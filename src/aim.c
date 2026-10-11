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

// Hardware sprites 4 and 5: the reticle's two halves (drawn over the enemies,
// which start at 6; a lower sprite index wins where sprites overlap).
#define RETICLE_OAM 4
// Scoring skips a frame when the screen is already drawing this line (a line is
// 456 cycles; scoring plus the rest of the frame's work needs about 12).
#define AIM_LATE_LINE 128
uint8_t aim_skipped;  // frames scoring was skipped for lack of time (watchable)
uint8_t aim_ly;       // the scanline aim_update last started on (watchable)

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
    SWITCH_ROM(saved);

    aim_target = AIM_NONE;
    for (uint8_t i = 0; i < ACTOR_MAX; i++) aim_score[i] = AIM_NONE;
    aim_eval = 0;
    fire_cooldown = 0;
    for (uint8_t i = 0; i < SHOT_MAX; i++) shots[i].alive = 0;
    shot_count = 0;
    shots_fired = shots_hit = enemies_killed = 0;
    shots_killed = 0;
    build_dir_table();  // here, with the display off: it's ~60,000 cycles
    shadow_OAM[RETICLE_OAM].y = 0;
    shadow_OAM[RETICLE_OAM + 1].y = 0;
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
// clockwise). In assembly: the compiled version, all 16-bit shifts, cost ~3,300
// cycles. Steps:
//   - |dx|, |dy| (remembering the signs), halved together until both fit 8 bits;
//   - within the octant, k = steps from the nearer axis (0..4): how many of
//     tan((k + 0.5) * 11.25 deg) = 25, 78, 137, 210 (/256) have min * 256 >= max * t;
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
    push hl                 ; flags (h) and lo (l)
    ld c, #0                ; c = k
    ld a, #25
    call 20$
    jr c, 6$
    inc c
    ld a, #78
    call 20$
    jr c, 6$
    inc c
    ld a, #137
    call 20$
    jr c, 6$
    inc c
    ld a, #210
    call 20$
    jr c, 6$
    inc c
6$:
    pop hl
    ld a, c                 ; q = k, or 8 - k when steep
    bit 2, h
    jr z, 7$
    ld a, #8
    sub a, c
7$:
    ld c, a
    ld a, h
    and a, #3
    jr z, 10$               ; right-down: q
    dec a
    jr z, 8$                ; dy < 0 only: right-up, (32 - q) & 31
    dec a
    jr z, 9$                ; dx < 0 only: left-down, 16 - q
    ld a, #16               ; both: left-up, 16 + q
    add a, c
    ret
8$:
    xor a, a
    sub a, c
    and a, #31
    ret
9$:
    ld a, #16
    sub a, c
    ret
10$:
    ld a, c
    ret

; carry clear if lo * 256 >= hi * a (lo on the stack below the return address,
; hi in d). hl = hi * a by shift-and-add, then compare lo:00 with it.
20$:
    push bc
    ld e, d
    ld b, #0
    ld c, d                 ; bc = hi
    ld hl, #0
    ld d, #8
21$:
    add hl, hl
    add a, a
    jr nc, 22$
    add hl, bc
22$:
    dec d
    jr nz, 21$
    ld d, e                 ; restore hi
    ; lo is at sp + 6, under the product, the saved bc and the return address
    push hl
    ld hl, #6
    add hl, sp
    ld a, (hl)              ; lo
    pop hl
    pop bc
    cp a, h                 ; lo vs the high byte of the product
    ret nz                  ; lo > h: carry clear (>=); lo < h: carry set
    ld a, l                 ; equal: lo:00 >= h:l only if l == 0
    or a, a
    ret z
    scf
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
        uint8_t dir;
        uint8_t t = aim_target;
        if (t != AIM_NONE) {
            int16_t dx = (int16_t)(actor_x[t] - player_x) + (ENEMY_MID_X - SHOT_FROM_X);
            int16_t dy = (int16_t)(actor_y[t] - player_y) + (ENEMY_MID_Y - SHOT_FROM_Y);
            dir = (dx || dy) ? direction(dx, dy) : facing_dir[player_facing];
        } else {
            dir = facing_dir[player_facing];
        }
        aim_dir = dir;
        if (speed_for != shot_speed) build_dir_table();
        int16_t vx = dir_vx[dir], vy = dir_vy[dir];
        if (life_for_range != aim_range || life_for_speed != shot_speed) {
            life_for_range = aim_range;
            life_for_speed = shot_speed;
            life = (uint8_t)(((uint16_t)aim_range << 8) / shot_speed) + 4;
        }
        b->alive = life;
        // The 8x16 sprite's top-left, so its hitbox (6x6 at 1, 5) centers on the
        // shot's start point.
        b->x = player_x + SHOT_FROM_X - 4;
        b->y = player_y + SHOT_FROM_Y - 8;
        b->xs = b->ys = 0x80;
        bullet_set_velocity(b, vx, vy);
        shot_count++;
        shots_fired++;
        return;
    }
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

    if (fire_cooldown) fire_cooldown--;
    if ((joy & J_A) && !fire_cooldown && !player_roll_len && !player_fall_timer) {
        fire();
        fire_cooldown = fire_interval;
    }
    aim_draw_reticle();
}
