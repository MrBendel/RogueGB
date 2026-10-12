#pragma bank 255
// Auto-aim: picking a target, firing, and the reticle (#5). Banked: it runs once a
// frame, but the per-frame work is in assembly in bank 0 (aim_score.s: scoring,
// target choice, reticle); the shots move in shots.s. See aim.h.
#include <gb/gb.h>
#include "aim.h"
#include "actor.h"
#include "player.h"
#include "room.h"
#include "camera.h"
#include "input.h"
#include "state.h"
#include "spr_shot.h"
#include "spr_reticle.h"
#include "spr_flash.h"
#include "spr_pistol.h"
#include "weapon_pistol.h"
#include "aim_dir_table.h"
#include "oam.h"

// --- watchable state ---
uint8_t aim_target = AIM_NONE;
uint8_t aim_score[ACTOR_MAX];
uint8_t aim_eval;
uint8_t fire_cooldown;
uint8_t aim_dir;
uint16_t shots_fired;
uint16_t shots_hit;
uint16_t enemies_killed;
Bullet shots[SHOT_MAX];
uint8_t shot_count;

// --- tunables ---
uint8_t aim_range = 112;    // 7 blocks
uint8_t aim_bias = 24;      // 1.5 blocks
uint8_t aim_sticky = 16;    // 1 block
uint8_t fire_interval = 10; // 6 shots a second
uint16_t shot_speed = 768;  // 3 px/frame

// Shared with shots.s and actor.c.
uint8_t shot_parity;
uint8_t shot_draw_tile;
uint8_t shot_draw_prop;
uint8_t shots_killed;
// Graphics: where the tiles went and the frame tables (copied to RAM: the assets
// are in a switchable bank). Set by aim_init.
uint8_t shot_vram;
uint8_t shot_frames[SPR_SHOT_FRAME_COUNT * 4];
uint8_t reticle_vram;
uint8_t reticle_frames[SPR_RETICLE_FRAME_COUNT * 4];

#define RETICLE_OAM OAM_RETICLE
// Scoring skips a frame when the screen is already drawing this line (a line is
// 456 cycles; scoring plus the rest of the frame's work needs about 12).
#define AIM_LATE_LINE 128
uint8_t aim_skipped;  // frames scoring was skipped for lack of time (watchable)
uint8_t aim_ly;       // the scanline aim_update last started on (watchable)

// --- the held weapon (for now always the pistol; the loadout comes with M3) ---
uint8_t aim_dir32;       // where the gun points: at the target, or the facing (0-31)
uint8_t gun_dir16;       // the gun sprite's direction (0-15, 0 = right, clockwise)
uint8_t gun_kick;        // frames of recoil left after a shot
uint8_t gun_flash;       // frames of muzzle flash left
uint8_t aim_hold;        // frames the body keeps facing the aim after a shot
uint8_t aim_facing = 0xFF;  // art direction the body faces while shooting (player.c), or 0xFF
#define KICK_FRAMES 4    // recoil: 2 px for 2 frames, then 1 px for 2
#define FLASH_FRAMES 4   // muzzle flash: the big burst for 2 frames, then the small one
#define HOLD_FRAMES 20   // keep facing the aim for 1/3 s after a shot
// Graphics, set by aim_init: tiles in sprite VRAM, frame tables copied to RAM.
uint8_t gun_vram, gun_blank;  // gun_blank: the empty column's index, not drawn
uint8_t gun_frames[SPR_PISTOL_FRAME_COUNT * 4];
uint8_t flash_vram;
uint8_t flash_frames[SPR_FLASH_FRAME_COUNT * 4];

// Each facing as a 32-step direction, for firing with no target (0 = right, clockwise; y grows downward).
static const uint8_t facing_dir[8] = {24, 28, 0, 4, 8, 12, 16, 20};

// cos(k * 11.25 deg) * 256 for k = 0..8: one quadrant of the 32 directions.
static const uint16_t unit[9] = {256, 251, 237, 213, 181, 142, 98, 50, 0};

static void build_dir_table(void);

void aim_init(uint8_t v) BANKED {
    uint8_t saved = CURRENT_BANK;
    shot_vram = v;
    SWITCH_ROM(BANK(spr_shot));
    set_sprite_data(v, SPR_SHOT_COLUMN_COUNT * 2, spr_shot_tiles);
    for (uint8_t i = 0; i < sizeof(shot_frames); i++) shot_frames[i] = spr_shot_frames[i];
    v += SPR_SHOT_COLUMN_COUNT * 2;
    reticle_vram = v;
    SWITCH_ROM(BANK(spr_reticle));
    set_sprite_data(v, SPR_RETICLE_COLUMN_COUNT * 2, spr_reticle_tiles);
    for (uint8_t i = 0; i < sizeof(reticle_frames); i++) reticle_frames[i] = spr_reticle_frames[i];
    v += SPR_RETICLE_COLUMN_COUNT * 2;
    flash_vram = v;
    SWITCH_ROM(BANK(spr_flash));
    set_sprite_data(v, SPR_FLASH_COLUMN_COUNT * 2, spr_flash_tiles);
    for (uint8_t i = 0; i < sizeof(flash_frames); i++) flash_frames[i] = spr_flash_frames[i];
    v += SPR_FLASH_COLUMN_COUNT * 2;
    gun_vram = v;
    SWITCH_ROM(BANK(spr_pistol));
    set_sprite_data(v, SPR_PISTOL_COLUMN_COUNT * 2, spr_pistol_tiles);
    for (uint8_t i = 0; i < sizeof(gun_frames); i++) gun_frames[i] = spr_pistol_frames[i];
    // A frame with an empty column (a short gun pointing up or down fits in one
    // 8-px column) doesn't need its second sprite: find the empty column.
    gun_blank = 0xFF;
    for (uint8_t c = 0; c < SPR_PISTOL_COLUMN_COUNT && gun_blank == 0xFF; c++) {
        const uint8_t *t = spr_pistol_tiles + ((uint16_t)c << 5);
        uint8_t any = 0;
        for (uint8_t i = 0; i < 32; i++) any |= t[i];
        if (!any) gun_blank = c;
    }
    SWITCH_ROM(saved);

    aim_target = AIM_NONE;
    for (uint8_t i = 0; i < ACTOR_MAX; i++) aim_score[i] = AIM_NONE;
    aim_eval = 0;
    fire_cooldown = 0;
    for (uint8_t i = 0; i < SHOT_MAX; i++) shots[i].alive = 0;
    shot_count = 0;
    shots_fired = shots_hit = enemies_killed = 0;
    shots_killed = 0;
    gun_kick = gun_flash = aim_hold = 0;
    aim_facing = 0xFF;
    build_dir_table();  // here, with the display off: it's ~60,000 cycles
    shadow_OAM[RETICLE_OAM].y = 0;
    shadow_OAM[RETICLE_OAM + 1].y = 0;
    shadow_OAM[OAM_FLASH].y = 0;
    for (uint8_t i = 0; i < 2; i++) shadow_OAM[OAM_GUN_FRONT + i].y = shadow_OAM[OAM_GUN_BACK + i].y = 0;
}

// Offsets from the player's top-left to where shots start (the body's middle),
// and from an enemy's top-left to the middle of its hitbox (also in aim_score.s).
#define SHOT_FROM_X 8
#define SHOT_FROM_Y 8
#define ENEMY_MID_X 8
#define ENEMY_MID_Y 11

// Score enemy i (aim_score.s): octagonal distance, less aim_bias in front of the
// player; AIM_NONE if it's gone, out of range or out of line of sight.
uint8_t aim_score_of(uint8_t i);
// The best score becomes the target unless the current one is still visible and
// within aim_sticky of it (aim_score.s).
void aim_select(void);
// Hardware sprites 4-5 around the target, or hidden (aim_score.s).
void aim_draw_reticle(void);

// The nearest of the 32 directions to (dx, dy), not both 0 (0 = right,
// clockwise). In assembly (the compiled version, all 16-bit shifts, cost ~3,300
// cycles; this is ~300). Steps:
//   - |dx|, |dy| (remembering the signs), scaled together so the larger ("hi")
//     is 16-31 and the smaller ("lo") follows;
//   - k = steps from the nearer axis (0..4), from the table at (hi - 16) * 32 + lo
//     (aim_dir_table.h, tools/make-aim-table.mjs);
//   - q = steps below the x axis in the quadrant (k, or 8 - k when steep), then
//     into the circle: right-down q, left-down 16 - q, left-up 16 + q, right-up 32 - q.
// dx in de, dy in bc, result in a (the compiler's convention).
static uint8_t direction(int16_t dx, int16_t dy) __naked {
    (void)dx;
    (void)dy;
    __asm
    ld h, #0                ; h: bit 1 = dx < 0, bit 0 = dy < 0, bit 2 = steep
    bit 7, d
    jr z, 1$
    xor a, a
    sub a, e
    ld e, a
    ld a, #0
    sbc a, d
    ld d, a
    set 1, h
1$:
    bit 7, b
    jr z, 2$
    xor a, a
    sub a, c
    ld c, a
    ld a, #0
    sbc a, b
    ld b, a
    set 0, h
2$:
    ld a, d                 ; halve both until they fit 8 bits
    or a, b
    jr z, 3$
    srl d
    rr e
    srl b
    rr c
    jr 2$
3$:
    ld a, e                 ; e = |dx|, c = |dy|
    cp a, c
    jr nc, 4$
    set 2, h                ; steep: lo = |dx|, hi = |dy|
    ld l, e
    ld d, c
    jr 5$
4$:
    ld l, c                 ; lo = |dy|, hi = |dx|
    ld d, e
5$:
    ld a, d                 ; scale so hi is 16-31: up while < 16 ...
6$:
    cp a, #16
    jr nc, 7$
    add a, a
    sla l
    jr 6$
7$:
    cp a, #32               ; ... down while >= 32
    jr c, 8$
    srl a
    srl l
    jr 7$
8$:
    ; k = aim_dir_k[(hi - 16) * 32 + lo]
    sub a, #16
    ld c, a
    ld b, #0
    swap c                  ; bc = (hi - 16) * 32 = (hi - 16) << 5
    sla c
    rl b
    ld a, c
    or a, l
    ld c, a
    push hl
    ld hl, #_aim_dir_k
    add hl, bc
    ld c, (hl)              ; c = k
    pop hl
    ld a, c                 ; q = k, or 8 - k when steep
    bit 2, h
    jr z, 10$
    ld a, #8
    sub a, c
10$:
    ld c, a
    ld a, h
    and a, #3
    jr z, 13$               ; right-down: q
    dec a
    jr z, 11$               ; dy < 0 only: right-up, (32 - q) & 31
    dec a
    jr z, 12$               ; dx < 0 only: left-down, 16 - q
    ld a, #16               ; both: left-up, 16 + q
    add a, c
    ret
11$:
    xor a, a
    sub a, c
    and a, #31
    ret
12$:
    ld a, #16
    sub a, c
    ret
13$:
    ld a, c
    ret
    __endasm;
}

// speed * unit / 256, in two 16-bit products (no 32-bit multiply).
static int16_t scaled(uint16_t speed, uint16_t u) {
    return (int16_t)((speed >> 8) * u + (((speed & 0xFF) * u) >> 8));
}

// Shot velocity for each of the 32 directions at shot_speed. Built by aim_init,
// and again on the next shot if the tunable changes (multiplies are slow here).
static int16_t dir_vx[32], dir_vy[32];
static uint16_t speed_for;

static void build_dir_table(void) {
    speed_for = shot_speed;
    for (uint8_t d = 0; d < 32; d++) {
        // Quadrant and steps below the x axis (0..8). Right-down: x = cos, y = sin
        // = cos(8 - q); each further quadrant turns 90 degrees: (x, y) -> (-y, x).
        uint8_t q = d & 7, quad = d >> 3;
        int16_t c = scaled(shot_speed, unit[q]), s = scaled(shot_speed, unit[8 - q]);
        int16_t vx, vy;
        if (quad == 0) { vx = c; vy = s; }
        else if (quad == 1) { vx = -s; vy = c; }
        else if (quad == 2) { vx = -c; vy = -s; }
        else { vx = s; vy = -c; }
        dir_vx[d] = vx;
        dir_vy[d] = vy;
    }
}

// Frames a shot flies: aim range / speed, plus a little. A division, so it's only
// redone when either tunable changes.
static uint8_t life_for_range, life;
static uint16_t life_for_speed;

static void fire(void) {
    Bullet *b = shots;
    for (uint8_t k = 0; k < SHOT_MAX; k++, b++) {
        if (b->alive) continue;
        uint8_t dir = aim_dir32;
        aim_dir = dir;
        if (speed_for != shot_speed) build_dir_table();
        int16_t vx = dir_vx[dir], vy = dir_vy[dir];
        if (life_for_range != aim_range || life_for_speed != shot_speed) {
            life_for_range = aim_range;
            life_for_speed = shot_speed;
            life = (uint8_t)(((uint16_t)aim_range << 8) / shot_speed) + 4;
        }
        b->alive = life;
        // Out of the gun's muzzle: the shot's 8x16 sprite top-left, so its hitbox
        // (6x6 at 1, 5) centres on the muzzle.
        uint8_t d = gun_dir16;
        b->x = player_x + weapon_pistol_mx[d] - 4;
        b->y = player_y + weapon_pistol_my[d] - 8;
        gun_kick = KICK_FRAMES;
        gun_flash = FLASH_FRAMES;
        aim_hold = HOLD_FRAMES;
        b->xs = b->ys = 0x80;
        bullet_set_velocity(b, vx, vy);
        shot_count++;
        shots_fired++;
        return;
    }
}

// Where the gun points this frame: at the target (exactly, in 32 steps), or the
// way the player faces. The sprite shows the nearest of its 16 directions. In
// assembly, straight into direction() (the compiled version cost ~740 cycles).
// The offsets between the shot's start and the enemy's middle: x cancels
// (ENEMY_MID_X - SHOT_FROM_X = 0), y is ENEMY_MID_Y - SHOT_FROM_Y = 3.
static void update_aim_dir(void) __naked {
    __asm
    ld a, (_aim_target)
    cp a, #0xFF
    jr z, 8$
    add a, a
    ld c, a
    ld b, #0                ; bc = byte offset of the target 16-bit fields
    ld hl, #_actor_y        ; dy = actor_y - player_y + 3, kept on the stack
    add hl, bc
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld a, (_player_y)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (_player_y + 1)
    ld l, a
    ld a, d
    sbc a, l
    ld d, a
    ld hl, #3
    add hl, de
    push hl
    ld hl, #_actor_x        ; dx = actor_x - player_x
    add hl, bc
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld a, (_player_x)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (_player_x + 1)
    ld l, a
    ld a, d
    sbc a, l
    ld d, a                 ; de = dx
    pop bc                  ; bc = dy
    ld a, d
    or a, e
    or a, b
    or a, c
    jr z, 8$                ; on top of it: use the facing
    call _direction
    jr 9$
8$:
    ld a, (_player_facing)
    ld e, a
    ld d, #0
    ld hl, #_facing_dir
    add hl, de
    ld a, (hl)
9$:
    ld (_aim_dir32), a
    inc a                   ; nearest of 16: (d32 + 1) / 2, wrapped
    srl a
    and a, #15
    ld (_gun_dir16), a
    ret
    __endasm;
}

// While shooting (A held, or just after a shot), the body faces the aim instead
// of the way it walks, so you can back away while firing. 32 directions to the
// art's 4: right, down, left, up (ANIM_RIGHT, ANIM_DOWN, ANIM_LEFT, ANIM_UP).
static const uint8_t aim_art_dir[4] = {2, 0, 3, 1};

// The held weapon (2 sprites, in front of or behind the body) and the muzzle
// flash, placed from the per-direction table (weapon_pistol.h), raised with the
// body in a jump, kicked back after a shot, and hidden while rolling, falling or
// blinking. In assembly: the compiled version cost ~3,000 cycles.
// Hardware sprite coordinates are the screen position + (8, 16); 8-bit wrap-around
// is what the hardware does with them too.
uint8_t gun_px, gun_py;  // the player's frame in hardware coordinates (before the recoil)
static void draw_gun(void) __naked {
    __asm
    xor a, a                ; hide all five, then show the ones needed
    ld (_shadow_OAM + 0), a         ; OAM_FLASH
    ld (_shadow_OAM + 4), a         ; OAM_GUN_FRONT
    ld (_shadow_OAM + 8), a
    ld (_shadow_OAM + 20), a        ; OAM_GUN_BACK
    ld (_shadow_OAM + 24), a
    ld a, (_player_roll_len)
    ld hl, #_player_fall_timer
    or a, (hl)
    ret nz
    ld a, (_player_blink)
    and a, #4
    ret nz
    ld a, (_gun_dir16)
    ld e, a
    ld d, #0                ; de = direction, an index into the tables
    ; px = player_x - cam_x + 8, py = player_y - cam_y + 16 - height
    ld a, (_player_x)
    ld hl, #_cam_x
    sub a, (hl)
    add a, #8
    ld b, a
    ld a, (_player_y)
    ld hl, #_cam_y
    sub a, (hl)
    add a, #16
    ld hl, #_player_z + 1
    sub a, (hl)
    ld c, a
    ld a, b                 ; the muzzle flash goes where the shot started: no kick
    ld (_gun_px), a
    ld a, c
    ld (_gun_py), a
    ; recoil: the kick for 2 frames, then 1 px the same way
    ld a, (_gun_kick)
    or a, a
    jr z, 3$
    ld hl, #_weapon_pistol_kx
    add hl, de
    ld a, (hl)
    call 50$
    add a, b
    ld b, a
    ld hl, #_weapon_pistol_ky
    add hl, de
    ld a, (hl)
    call 50$
    add a, c
    ld c, a
3$:
    ; the two gun columns: front or behind the body
    ld hl, #_weapon_pistol_behind
    add hl, de
    ld a, (hl)
    or a, a
    ld hl, #_shadow_OAM + 4
    jr z, 4$
    ld hl, #_shadow_OAM + 20
4$:
    push hl                 ; OAM entry
    ld hl, #_weapon_pistol_gx
    add hl, de
    ld a, (hl)
    add a, b
    ld b, a                 ; b = x
    ld hl, #_weapon_pistol_gy
    add hl, de
    ld a, (hl)
    add a, c
    ld c, a                 ; c = y
    ld hl, #_gun_frames     ; hl = gun_frames + direction * 4
    sla e
    sla e
    add hl, de
    ld d, h
    ld e, l                 ; de -> left column, flags, right column, flags
    pop hl
    call 60$                ; left column at x
    ld a, b
    add a, #8
    ld b, a
    call 60$                ; right column at x + 8
    ; the muzzle flash
    ld a, (_gun_flash)
    or a, a
    ret z
    cp a, #3                ; the big burst for the first 2 frames, then the small one
    ld hl, #_flash_frames
    jr nc, 5$
    ld hl, #_flash_frames + 4
5$:
    ld a, (_gun_dir16)
    ld e, a
    ld d, #0
    push hl
    ld hl, #_weapon_pistol_my
    add hl, de
    ld a, (_gun_py)
    add a, (hl)
    sub a, #8
    ld (_shadow_OAM + 0), a
    ld hl, #_weapon_pistol_mx
    add hl, de
    ld a, (_gun_px)
    add a, (hl)
    sub a, #4
    ld (_shadow_OAM + 1), a
    pop hl
    ld a, (hl+)
    add a, a
    ld b, a
    ld a, (_flash_vram)
    add a, b
    ld (_shadow_OAM + 2), a
    ld a, (hl)
    ld (_shadow_OAM + 3), a
    ret

; a = a (full kick) for the first frames, its sign (1 px) after (gun_kick <= 2)
50$:
    push af
    ld a, (_gun_kick)
    cp a, #3
    jr c, 51$
    pop af
    ret
51$:
    pop af
    or a, a
    ret z
    bit 7, a
    ld a, #1
    ret z
    ld a, #0xFF
    ret

; one column: entry hl (y, x, tile, flags) <- b = x, c = y, (de) = column, flags;
; skipped if it is the empty column. hl and de step to the next.
60$:
    ld a, (_gun_blank)
    push hl
    ld h, d
    ld l, e
    cp a, (hl)
    pop hl
    jr z, 61$
    ld a, c
    ld (hl+), a
    ld a, b
    ld (hl+), a
    ld a, (de)
    inc de
    add a, a
    push hl
    ld hl, #_gun_vram
    add a, (hl)
    pop hl
    ld (hl+), a
    ld a, (de)
    inc de
    ld (hl+), a
    ret
61$:
    inc de
    inc de
    inc hl
    inc hl
    inc hl
    inc hl
    ret
    __endasm;
}

void aim_update(void) BANKED {
    // Score one enemy (round-robin), then pick the target (aim_score.s). Scoring
    // is the optional part (~3,500 cycles with line of sight), so it runs on odd
    // frames only (each enemy is rescored about every 16 frames, a quarter of a
    // second), and waits a frame if a slime took this frame's terrain check
    // (actor.h) or if the frame is running late: the update starts at vblank
    // (lines 144-153) and must finish before line 144 comes round again, so past
    // AIM_LATE_LINE there isn't room.
    uint8_t ly = aim_ly = LY_REG;
    if (((uint8_t)frame_count & 1) && terrain_checks_left && (ly < AIM_LATE_LINE || ly >= 144)) {
        terrain_checks_left--;
        aim_score[aim_eval] = aim_score_of(aim_eval);
        aim_eval = (aim_eval + 1) & (ACTOR_MAX - 1);
        aim_select();
    } else if (((uint8_t)frame_count & 1) && terrain_checks_left) {
        aim_skipped++;
    }

    // The gun's direction is redone on even frames, opposite the scoring (odd
    // frames), so the two don't add up in one frame; a frame late doesn't show.
    // Pressing A always gets a fresh one, so a first shot goes the right way.
    if (!((uint8_t)frame_count & 1) || (joy_pressed & J_A)) update_aim_dir();
    if (fire_cooldown) fire_cooldown--;
    if (gun_kick) gun_kick--;
    if (gun_flash) gun_flash--;
    if (aim_hold) aim_hold--;
    if ((joy & J_A) && !fire_cooldown && !player_roll_len && !player_fall_timer) {
        fire();
        fire_cooldown = fire_interval;
    }
    aim_facing = ((joy & J_A) || aim_hold) ? aim_art_dir[((aim_dir32 + 4) >> 3) & 3] : 0xFF;
    aim_draw_reticle();
    draw_gun();
}
