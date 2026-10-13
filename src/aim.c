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
#include "weapon.h"
#include <rand.h>
#include "aim_dir_table.h"
#include "oam.h"
#include "gfx.h"

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

// --- tunables (the weapon's own are in wpn, weapon.h) ---
uint8_t aim_range = 112;    // copied from the weapon every frame (aim_score.s reads it)
uint8_t aim_bias = 24;      // 1.5 blocks
uint8_t aim_sticky = 16;    // 1 block

// Shared with shots.s and actor.c.
uint8_t shot_parity;
uint8_t shot_draw_tile;
uint8_t shot_draw_prop;
uint8_t shots_killed;
uint8_t shot_damage;      // HP the shots in flight take off (shots.s); the last shot's
uint8_t weapon_windup;    // steadying progress, frames (FIRE_CHARGE)
uint8_t burst_left;       // shots left in the rifle's current burst
uint8_t reticle_tight;    // the reticle closes in (steadying past halfway) (aim_score.s)
uint16_t crits;           // critical shots fired

// --- melee (FIRE_MELEE): see swing() ---
uint8_t swing_t;          // frames into the current swing (0 = not swinging)
uint8_t swing_step;       // combo step: 1 and 2 sweep (opposite ways), 3 thrusts
uint8_t swing_dir;        // the aim (0-31) when the swing started
uint8_t swing_hit;        // enemies already hit this swing (a bit per slot)
uint8_t combo_left;       // frames left to chain the next swing
uint16_t melee_hits;      // enemies hit by swings
uint16_t deflects;        // enemy bullets swatted back
uint8_t hitstop;          // frames the game freezes for (play.c), on a hit that lands
uint8_t hitstop_frames = 3;  // tunable
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

// --- the held weapon (weapon.h: its stats, art and placement tables) ---
uint8_t aim_dir32;       // where the gun points: at the target, or the facing (0-31)
uint8_t gun_dir16;       // the gun sprite's direction (0-15, 0 = right, clockwise)
uint8_t gun_kick;        // frames of recoil left after a shot
uint8_t gun_flash;       // frames of muzzle flash left
uint8_t aim_hold;        // frames the body keeps facing the aim after a shot
uint8_t aim_facing = 0xFF;  // art direction the body faces while shooting (player.c), or 0xFF
#define KICK_FRAMES 4    // recoil: 2 px for 2 frames, then 1 px for 2
#define FLASH_FRAMES 4   // muzzle flash: the big burst for 2 frames, then the small one
#define HOLD_FRAMES 20   // keep facing the aim for 1/3 s after a shot
uint8_t reload_vram;      // the reload bar's tiles: 8 left halves, then 8 right halves
uint16_t reload_fill;     // the bar's fill, px 8.8
uint16_t reload_step;     // ... added each frame
uint8_t flash_vram;
uint8_t flash_frames[SPR_FLASH_FRAME_COUNT * 4];

// Each facing as a 32-step direction, for firing with no target (0 = right, clockwise; y grows downward).
static const uint8_t facing_dir[8] = {24, 28, 0, 4, 8, 12, 16, 20};

// cos(k * 11.25 deg) * 256 for k = 0..8: one quadrant of the 32 directions.
static const uint16_t unit[9] = {256, 251, 237, 213, 181, 142, 98, 50, 0};

static void build_dir_table(uint8_t w);

// shots.s: writes the pellets of a shot into the pool (see fire()).
void shots_spawn(void);
extern uint8_t spawn_n, spawn_dir, spawn_spread, spawn_life;
extern uint16_t spawn_x, spawn_y;
extern int16_t *spawn_vx, *spawn_vy;

// --- Reloading (see weapon.h) ---
// The bar over the player's head while reloading: 2 sprites (the reticle's: no
// target is shown while you can't shoot), 14 px inside an ink outline, white
// filling a grey track, in the top 4 rows of 8x16 sprites. Tiles: for each fill
// 0-7, a left half then (8 pairs on) a right half; each sprite's lower tile is
// blank.
#define RELOAD_BAR_PX 14

static void reload_bar_tiles(uint8_t v) {
    uint8_t t[32];
    for (uint8_t i = 0; i < 32; i++) t[i] = 0;
    for (uint8_t half = 0; half < 2; half++) {
        for (uint8_t n = 0; n < 8; n++) {
            // The ink edge and the n px filled from the left (inside the edge).
            uint8_t ink = half ? 0x01 : 0x80;
            uint8_t fill = half ? 0xFF ^ (0xFF >> n) : 0x7F ^ (0x7F >> n);
            uint8_t track = (uint8_t)~(ink | fill);
            t[0] = t[1] = t[6] = t[7] = 0xFF;  // the top and bottom edges (rows 0, 3)
            t[2] = t[4] = ink | fill;          // rows 1-2: color 1 = fill, 2 = track, 3 = ink
            t[3] = t[5] = ink | track;
            set_sprite_data(v, 2, t);  // from RAM: no bank to switch
            v += 2;
        }
    }
}

static void reload_start(void) {
    reload_t = wpn.reload ? wpn.reload : 1;
    reload_idle = 0;
    weapon_windup = 0;
    reload_fill = 0;
    reload_step = ((uint16_t)RELOAD_BAR_PX << 8) / reload_t;  // once per reload
    reloads++;
}

static void draw_reload_bar(void) {
    uint8_t px = reload_fill >> 8;
    if (px > RELOAD_BAR_PX) px = RELOAD_BAR_PX;
    uint8_t l = px > 7 ? 7 : px;
    // Hardware coordinates: (8, 16) offset; the bar's 4 rows sit 2 px above the head.
    uint8_t x = (uint8_t)(player_x - cam_x) + 8;
    uint8_t y = (uint8_t)(player_y - cam_y) - (uint8_t)(player_z >> 8) + 16 - 6;
    set_sprite_tile(OAM_RETICLE, reload_vram + (l << 1));
    set_sprite_tile(OAM_RETICLE + 1, reload_vram + 16 + ((px - l) << 1));
    set_sprite_prop(OAM_RETICLE, SPR_PAL_UI);
    set_sprite_prop(OAM_RETICLE + 1, SPR_PAL_UI);
    move_sprite(OAM_RETICLE, x, y);
    move_sprite(OAM_RETICLE + 1, x + 8, y);
}

void aim_init(uint8_t v) BANKED {
    // Graphics in switchable banks: the banked_* helpers (gfx.c, bank 0) switch
    // to them and back (this code is banked too, so it can't switch itself).
    shot_vram = v;
    banked_sprite_data(v, SPR_SHOT_COLUMN_COUNT * 2, spr_shot_tiles, BANK(spr_shot));
    banked_memcpy(shot_frames, spr_shot_frames, sizeof(shot_frames), BANK(spr_shot));
    v += SPR_SHOT_COLUMN_COUNT * 2;
    reticle_vram = v;
    banked_sprite_data(v, SPR_RETICLE_COLUMN_COUNT * 2, spr_reticle_tiles, BANK(spr_reticle));
    banked_memcpy(reticle_frames, spr_reticle_frames, sizeof(reticle_frames), BANK(spr_reticle));
    for (uint8_t i = 1; i < sizeof(reticle_frames); i += 2) reticle_frames[i] |= SPR_PAL_UI;
    v += SPR_RETICLE_COLUMN_COUNT * 2;
    flash_vram = v;
    banked_sprite_data(v, SPR_FLASH_COLUMN_COUNT * 2, spr_flash_tiles, BANK(spr_flash));
    banked_memcpy(flash_frames, spr_flash_frames, sizeof(flash_frames), BANK(spr_flash));
    for (uint8_t i = 1; i < sizeof(flash_frames); i += 2) flash_frames[i] |= SPR_PAL_SHOT;
    v += SPR_FLASH_COLUMN_COUNT * 2;
    reload_vram = v;
    reload_bar_tiles(v);
    v += 32;
    weapon_init();  // the weapons' art (VRAM bank 1)

    aim_target = AIM_NONE;
    for (uint8_t i = 0; i < ACTOR_MAX; i++) aim_score[i] = AIM_NONE;
    aim_eval = 0;
    fire_cooldown = 0;
    for (uint8_t i = 0; i < SHOT_MAX; i++) shots[i].alive = 0;
    shot_count = 0;
    shots_fired = shots_hit = enemies_killed = 0;
    shots_killed = 0;
    gun_kick = gun_flash = aim_hold = 0;
    weapon_windup = reticle_tight = 0;
    burst_left = 0;
    crits = 0;
    swing_t = swing_step = combo_left = 0;
    melee_hits = deflects = 0;
    hitstop = 0;
    aim_facing = 0xFF;
    move_slow = 0;
    // Here, with the display off: it's ~60,000 cycles per weapon.
    for (uint8_t w = 0; w < WEAPON_COUNT; w++) build_dir_table(w);
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

// Shot velocity for each weapon and each of the 32 directions at its speed.
// Built by aim_init, and again on the next shot if the speed is tuned
// (multiplies are slow here). 512 bytes of RAM.
static int16_t dir_vx[WEAPON_COUNT][32], dir_vy[WEAPON_COUNT][32];
static uint16_t speed_for[WEAPON_COUNT];

static void build_dir_table(uint8_t w) {
    uint16_t speed = w == weapon_cur ? wpn.speed : weapon_stats[w].speed;
    speed_for[w] = speed;
    for (uint8_t d = 0; d < 32; d++) {
        // Quadrant and steps below the x axis (0..8). Right-down: x = cos, y = sin
        // = cos(8 - q); each further quadrant turns 90 degrees: (x, y) -> (-y, x).
        uint8_t q = d & 7, quad = d >> 3;
        int16_t c = scaled(speed, unit[q]), s = scaled(speed, unit[8 - q]);
        int16_t vx, vy;
        if (quad == 0) { vx = c; vy = s; }
        else if (quad == 1) { vx = -s; vy = c; }
        else if (quad == 2) { vx = -c; vy = -s; }
        else { vx = s; vy = -c; }
        dir_vx[w][d] = vx;
        dir_vy[w][d] = vy;
    }
}

// Frames a shot flies: range / speed, plus a little. A division, so it's only
// redone when either changes.
static uint8_t life_for_range, life;
static uint16_t life_for_speed;

// Fire the held weapon: its pellets fanned around the aim (plus any jitter), out
// of the muzzle, each taking `damage` HP on a hit. Kicks the gun back, flashes
// the muzzle, and (shotgun) shoves the player back. The pellets themselves are
// written into the pool by shots_spawn (shots.s).
static void fire(uint8_t damage) {
    uint8_t w = weapon_cur;
    if (speed_for[w] != wpn.speed) build_dir_table(w);
    if (life_for_range != wpn.range || life_for_speed != wpn.speed) {
        life_for_range = wpn.range;
        life_for_speed = wpn.speed;
        life = (uint8_t)(((uint16_t)wpn.range << 8) / wpn.speed) + 2;
    }
    shot_damage = damage;
    uint8_t aim = aim_dir32;
    uint8_t j = wpn.jitter;
    if (j) {
        // A random step in -j..j, without a division: fold 0..15 into 0..2j.
        uint8_t r = (uint8_t)rand() & 15, n = (j << 1) + 1;
        while (r >= n) r -= n;
        aim += r - j;
    }
    aim_dir = aim & 31;
    // The fan: pellet p at aim + (p - (pellets - 1) / 2) * spread.
    uint8_t half = 0;
    for (uint8_t p = 1; p < wpn.pellets; p++) half += wpn.spread;
    uint8_t d16 = gun_dir16;
    spawn_n = wpn.pellets;
    spawn_dir = aim - (half >> 1);
    spawn_spread = wpn.spread;
    spawn_life = life;
    spawn_x = player_x + gun_place[PLACE_MX][d16] - 4;  // the shot's 8x16 sprite top-left,
    spawn_y = player_y + gun_place[PLACE_MY][d16] - 8;  // so its hitbox centres on the muzzle
    spawn_vx = dir_vx[w];
    spawn_vy = dir_vy[w];
    shots_spawn();
    if (wpn.mag && !--weapon_ammo[w]) reload_start();  // empty: reload at once
    gun_kick = KICK_FRAMES;
    gun_flash = FLASH_FRAMES;
    aim_hold = HOLD_FRAMES;
    fire_cooldown = wpn.fire_interval;
    if (wpn.push) {
        // Shoved back along the aim: the shot's velocity, scaled down.
        player_vx -= dir_vx[w][aim_dir] >> wpn.push;
        player_vy -= dir_vy[w][aim_dir] >> wpn.push;
    }
}

extern uint8_t bullet_count;  // enemy bullets alive (actor.c)

// --- Melee ---
// A swing lasts SWING_FRAMES: a short windup, SWING_ACTIVE frames that hit, then
// recovery. The sword sprite sweeps through its directions around the aim (the
// offsets below, in 16ths of a turn); step 2 sweeps the other way; step 3 is a
// thrust straight at the aim, with a longer lunge and double damage. While the
// swing is active, a box in front of the player (reach wpn.range) hits each enemy
// once (knocking it back wpn.kick px) and turns enemy bullets into the player's.
#define SWING_FRAMES 14
#define SWING_ACTIVE_FROM 3
#define SWING_ACTIVE_TO 6    // inclusive
#define SWING_HALF_W 14      // half the hit box: width ...
#define SWING_HALF_H 12      // ... and height (enemy hitbox middles inside it are hit)
static const int8_t sweep[SWING_FRAMES] = {-4, -4, -3, -2, -1, 1, 2, 3, 3, 3, 3, 3, 3, 3};
int16_t swing_cx, swing_cy;  // the hit box's centre offset from the player's middle
int8_t swing_kx, swing_ky;   // the knockback, px

// px along direction d (0-31): the sword's table is unit vectors (speed 256),
// so |u| * px fits 16 bits (no 32-bit multiply).
static int16_t along(uint8_t d, uint8_t px, uint8_t axis) {
    int16_t u = axis ? dir_vy[WEAPON_SWORD][d & 31] : dir_vx[WEAPON_SWORD][d & 31];
    uint16_t m = ((uint16_t)(u < 0 ? -u : u) * px) >> 8;
    return u < 0 ? -(int16_t)m : (int16_t)m;
}

static void swing_start(uint8_t step) {
    uint8_t d = aim_dir32;
    swing_t = 1;
    swing_step = step;
    swing_dir = d;
    swing_hit = 0;
    swing_cx = along(d, wpn.range, 0);
    swing_cy = along(d, wpn.range, 1);
    swing_kx = (int8_t)along(d, wpn.kick, 0);
    swing_ky = (int8_t)along(d, wpn.kick, 1);
    // A lunge toward the aim: 2 px/frame (3 for the thrust), eased off by friction.
    int16_t lx = dir_vx[WEAPON_SWORD][d], ly = dir_vy[WEAPON_SWORD][d];
    player_vx += step == 3 ? (lx << 1) + lx : lx << 1;
    player_vy += step == 3 ? (ly << 1) + ly : ly << 1;
    aim_hold = HOLD_FRAMES;
}

// The active frames: enemies and enemy bullets in the box. melee_scan (shots.s)
// finds them; this acts on the few it finds.
void melee_scan(void);
extern uint16_t melee_ex, melee_ey, melee_bx, melee_by;
extern uint8_t melee_enemies, melee_bullets[3];

static void swing_hits(void) {
    uint16_t l = player_x + 8 + swing_cx - SWING_HALF_W, t = player_y + 10 + swing_cy - SWING_HALF_H;
    melee_ex = l - 8;   // an enemy's hitbox middle is at (8, 11)
    melee_ey = t - 11;
    melee_bx = l - 4;   // a bullet's at (4, 8)
    melee_by = t - 8;
    melee_scan();
    uint8_t dmg = swing_step == 3 ? wpn.damage << 1 : wpn.damage;
    uint8_t landed = 0;
    uint8_t hits = melee_enemies & ~swing_hit;  // each enemy once per swing
    for (uint8_t i = 0, bit = 1; hits; i++, bit <<= 1) {
        if (!(hits & bit)) continue;
        hits &= ~bit;
        swing_hit |= bit;
        actor_hp[i] = actor_hp[i] > dmg ? actor_hp[i] - dmg : 0;
        if (!actor_hp[i]) shots_killed = 1;  // actors_update removes it next frame
        actor_hurt[i] = 4;
        actor_push(i, swing_kx, swing_ky);
        melee_hits++;
        landed = 1;
    }
    // Swat enemy bullets back: each becomes one of the player's shots, flying
    // along the swing at pistol speed.
    for (uint8_t i = 0; i < BULLET_MAX; i++) {
        if (!(melee_bullets[i >> 3] & (1 << (i & 7)))) continue;
        Bullet *b = &bullets[i];
        b->alive = 0;
        bullet_count--;
        shot_damage = dmg;
        spawn_n = 1;
        spawn_dir = swing_dir;
        spawn_spread = 0;
        spawn_life = 40;
        spawn_x = b->x;
        spawn_y = b->y;
        spawn_vx = dir_vx[WEAPON_PISTOL];
        spawn_vy = dir_vy[WEAPON_PISTOL];
        shots_spawn();
        deflects++;
        landed = 1;
    }
    if (landed && !hitstop) hitstop = hitstop_frames;
}

// FIRE_MELEE: a press swings; pressed again before the combo window closes (or
// during the recovery of a swing), it chains the next step. Returns whether a
// swing is under way.
static uint8_t swing(uint8_t pressed, uint8_t able) {
    if (swing_t) {
        if (pressed && able && swing_t > SWING_ACTIVE_TO) {
            swing_start(swing_step == 3 ? 1 : swing_step + 1);
        } else if (++swing_t > SWING_FRAMES) {
            swing_t = 0;
            combo_left = wpn.tap_interval;
        } else if (swing_t >= SWING_ACTIVE_FROM && swing_t <= SWING_ACTIVE_TO) {
            swing_hits();
        }
    } else if (pressed && able) {
        swing_start(combo_left && swing_step < 3 ? swing_step + 1 : 1);
    } else if (combo_left && !--combo_left) {
        swing_step = 0;
    }
    if (swing_t) {
        // The sword sweeps through its directions around the aim (step 2 the
        // other way); the thrust points straight at it.
        int8_t o = swing_step == 3 ? 0 : sweep[swing_t - 1];
        if (swing_step == 2) o = -o;
        gun_dir16 = (((swing_dir + 1) >> 1) + o) & 15;
    }
    return swing_t != 0;
}

// How holding and tapping A fire, per mode (weapon.h). Returns whether A counts
// as "firing" this frame (the body faces the aim; heavy weapons slow you down).
static uint8_t trigger(void) {
    uint8_t held = joy & J_A, pressed = joy_pressed & J_A;
    uint8_t able = !player_roll_len && !player_fall_timer;
    if (fire_cooldown) fire_cooldown--;
    if (wpn.mag) {
        if (reload_t) {
            if (pressed && weapon_ammo[weapon_cur]) {
                reload_t = 0;  // an idle reload, cut short: fire what's left
            } else {
                reload_fill += reload_step;
                if (!--reload_t) {
                    weapon_ammo[weapon_cur] = wpn.mag;
                    burst_left = 0;  // a burst cut off by the reload doesn't resume
                }
                reticle_tight = 0;
                return 0;
            }
        }
        if (held || burst_left) reload_idle = 0;
        else if (weapon_ammo[weapon_cur] < wpn.mag && ++reload_idle >= RELOAD_IDLE) {
            reload_start();
            return 0;
        }
    }
    if (wpn.mode == FIRE_MELEE) return swing(pressed, able);
    switch (wpn.mode) {
    case FIRE_SEMI:
        // A press fires once the tap interval has passed; holding, the full interval.
        if (able && ((pressed && fire_cooldown <= wpn.fire_interval - wpn.tap_interval) ||
                     (held && !fire_cooldown)))
            fire(wpn.damage);
        break;
    case FIRE_AUTO:
        if (able && held && !fire_cooldown) fire(wpn.damage);
        break;
    case FIRE_RIFLE:
        if (rifle_mode == RIFLE_AUTO) {
            if (able && held && !fire_cooldown) fire(wpn.damage);
        } else if (rifle_mode == RIFLE_TAP) {
            // One shot per press, as fast as you can tap (but tap_interval apart).
            if (able && pressed && fire_cooldown <= wpn.fire_interval - wpn.tap_interval) fire(wpn.damage);
        } else if (burst_left) {
            // A burst finishes even if A is let go.
            if (able && !fire_cooldown) {
                fire(wpn.damage);
                fire_cooldown = --burst_left ? BURST_GAP : wpn.windup;
            }
        } else if (able && pressed && !fire_cooldown) {
            fire(wpn.damage);
            burst_left = BURST_SHOTS - 1;
            fire_cooldown = BURST_GAP;
        }
        return (held || burst_left) && able;
    case FIRE_CHARGE:
        if (able && held) {
            if (fire_cooldown) break;  // the next steadying starts after the cooldown
            if (++weapon_windup >= wpn.windup) {
                fire(wpn.damage << 1);  // a critical
                crits++;
                weapon_windup = 0;
            }
        } else {
            if (weapon_windup && able) fire(wpn.damage);  // let go early: a normal shot
            weapon_windup = 0;
        }
        break;
    }
    reticle_tight = wpn.mode == FIRE_CHARGE && weapon_windup >= (wpn.windup >> 1);
    return held && able;
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
    ld hl, #_gun_place + 64
    add hl, de
    ld a, (hl)
    call 50$
    add a, b
    ld b, a
    ld hl, #_gun_place + 80
    add hl, de
    ld a, (hl)
    call 50$
    add a, c
    ld c, a
3$:
    ; the two gun columns: front or behind the body
    ld hl, #_gun_place + 96
    add hl, de
    ld a, (hl)
    or a, a
    ld hl, #_shadow_OAM + 4
    jr z, 4$
    ld hl, #_shadow_OAM + 20
4$:
    push hl                 ; OAM entry
    ld hl, #_gun_place + 0
    add hl, de
    ld a, (hl)
    add a, b
    ld b, a                 ; b = x
    ld hl, #_gun_place + 16
    add hl, de
    ld a, (hl)
    add a, c
    ld c, a                 ; c = y
    ld a, (_draw_frames)    ; hl = draw_frames + direction * 4
    ld l, a
    ld a, (_draw_frames + 1)
    ld h, a
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
    ld hl, #_gun_place + 48
    add hl, de
    ld a, (_gun_py)
    add a, (hl)
    sub a, #8
    ld (_shadow_OAM + 0), a
    ld hl, #_gun_place + 32
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
    ld a, (_draw_blank)
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
    ld hl, #_draw_vram
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
    aim_range = wpn.aim_range;
    if (gun_kick) gun_kick--;
    if (gun_flash) gun_flash--;
    if (aim_hold) aim_hold--;
    uint8_t firing = trigger();
    move_slow = firing && wpn.slow;
    aim_facing = (firing || aim_hold) ? aim_art_dir[((aim_dir32 + 4) >> 3) & 3] : 0xFF;
    if (reload_t) draw_reload_bar();
    else aim_draw_reticle();
    // The sword shows its smear while a swing can hit (the thrust too). Other
    // weapons keep what weapon_select set.
    if (wpn.mode == FIRE_MELEE) {
        uint8_t smear = swing_t >= SWING_ACTIVE_FROM && swing_t <= SWING_ACTIVE_TO;
        draw_frames = smear ? smear_frames : gun_frames;
        draw_vram = smear ? smear_vram : gun_vram;
        draw_blank = smear ? smear_blank : gun_blank;
    }
    draw_gun();
}
