#include <gb/gb.h>
#include <string.h>
#include "actor.h"
#include "player.h"
#include "room.h"
#include "camera.h"
#include "state.h"
#include "gfx.h"
#include "spr_slime.h"
#include "spr_peashooter.h"
#include "spr_pea.h"

#include "actor_internal.h"
#include "aim.h"
#include "progress.h"
#include "pickups.h"
#include "boss.h"
#include "cragback.h"
#include "globfather.h"
#include "hound.h"
#include "duelist.h"
#include "elite.h"

uint8_t actor_type[ACTOR_MAX];
uint8_t actor_state[ACTOR_MAX];
uint8_t actor_timer[ACTOR_MAX];
uint16_t actor_x[ACTOR_MAX];
uint16_t actor_y[ACTOR_MAX];
uint8_t actor_xs[ACTOR_MAX];
uint8_t actor_ys[ACTOR_MAX];
int16_t actor_vx[ACTOR_MAX];
int16_t actor_vy[ACTOR_MAX];
uint8_t actor_hp[ACTOR_MAX];
uint8_t actor_face[ACTOR_MAX];
uint8_t actor_try[ACTOR_MAX];  // slimes: which landing spot the next hop tries
uint8_t actor_hurt[ACTOR_MAX];  // think steps left flashing after a hit (set by shots.s)
uint8_t actor_frame[ACTOR_MAX];
uint8_t actor_flash[ACTOR_MAX];
uint8_t actor_scripted[ACTOR_MAX];
uint8_t actor_guard[ACTOR_MAX];
uint8_t actor_elite[ACTOR_MAX];
// Sprite data for the current frame (tile and flags per column), cached when the
// frame changes so drawing just copies bytes. Read by actor_draw.s.
uint8_t actor_t0[ACTOR_MAX];
uint8_t actor_p0[ACTOR_MAX];
uint8_t actor_t1[ACTOR_MAX];
uint8_t actor_p1[ACTOR_MAX];
uint8_t actor_count;   // slots in use
uint8_t bullet_count;  // bullets alive
uint8_t enemy_bullet_damage, enemy_bullet_cause;

Bullet bullets[BULLET_MAX];

uint8_t actors_active;
uint8_t bullets_active;
uint8_t sprites_used;
uint16_t bullets_fired;

uint16_t slime_speed = 192;     // 0.75 px/frame while hopping
uint8_t shooter_interval = 80;  // 1.33 s between shots (waves.c sets these per wave)
uint8_t shooter_telegraph = 24; // flash for 0.4 s first
uint16_t bullet_speed = 384;    // 1.5 px/frame
uint8_t slime_rest = 26;
uint8_t enemy_hp_bonus;
uint8_t shooter_spread;

// Per-type graphics: converted frame tables and where the tiles went in VRAM.
// Frame tables are copied to RAM at load: the graphics live in a switchable bank,
// and these are read every frame (cache_frame, bullets) while other banks may be in.
uint8_t slime_frames[SPR_SLIME_FRAME_COUNT * 4];
uint8_t shooter_frames[SPR_PEASHOOTER_FRAME_COUNT * 4];
uint8_t pea_frames[SPR_PEA_FRAME_COUNT * 4];
static const uint8_t *const type_frames[ACTOR_TYPE_COUNT] = {0, slime_frames, shooter_frames, hound_frames, duelist_frames};
// Per type: XP for a kill, and the cause of a touch (health.h CAUSE_*).
static const uint8_t type_xp[ACTOR_TYPE_COUNT] = {0, XP_SLIME, XP_PEASHOOTER, XP_HOUND, XP_DUELIST};
static const uint8_t type_cause[ACTOR_TYPE_COUNT] = {0, CAUSE_SLIME, CAUSE_PEASHOOTER, CAUSE_HOUND, CAUSE_DUELIST};
uint8_t type_vram[ACTOR_TYPE_COUNT];
uint8_t bullet_vram;

// Shared with actor_draw.s (hand-written sprite emission).
uint8_t draw_rotation;
uint8_t bullet_rotation;  // start slot for bullets (< BULLET_MAX)
uint8_t oam_next;
uint8_t bullet_draw_tile;
uint8_t bullet_draw_prop;
void actors_emit(void);
// bullets.s: per-frame bullet movement, collisions and drawing, and its inputs/outputs.
void bullets_update(void);
uint16_t bullet_hb_x;       // player hitbox top-left
uint16_t bullet_hb_y;
uint8_t bullet_hittable;
uint8_t bullet_parity;      // which half of the records checks walls this frame
uint8_t bullet_hit;         // set when a bullet hit the player
uint16_t bullet_hit_x;      // ... at this point
uint16_t bullet_hit_y;
Bullet *bullet_draw_start;  // rotating first slot to visit (and draw)
uint8_t oam_end_last;

// Look up the sprite tiles and flags for an actor's current frame.
void actor_cache_frame(uint8_t i) {
    uint8_t type = actor_type[i];
    const uint8_t *f = type_frames[type] + (actor_frame[i] << 2);
    uint8_t base = type_vram[type];
    actor_t0[i] = base + (f[0] << 1);
    uint8_t pal = actor_elite[i] ? SPR_PAL_BULLET : type & 1 ? SPR_PAL_SLIME : SPR_PAL_SHOOTER;  // (actor.h)
    actor_p0[i] = f[1] | pal;
    actor_t1[i] = base + (f[2] << 1);
    actor_p1[i] = f[3] | pal;
}

// Count a timer down by one AI step; returns the new value (stops at 0).
// Expensive terrain checks still allowed this frame (actor.h): a slime's landing
// spot, auto-aim's line of sight.
uint8_t terrain_checks_left;

// Position + velocity in 8/16-bit pieces (same as the player's).
static uint16_t moved(uint16_t pos, uint8_t frac, int16_t v, uint8_t *new_frac) {
    uint16_t f = (uint16_t)frac + (uint8_t)v;
    *new_frac = (uint8_t)f;
    return pos + (int8_t)((uint16_t)v >> 8) + (f >> 8);
}

static uint8_t tick(uint8_t t) {
    return t > AI_STEP ? t - AI_STEP : 0;
}

// Enemies only stand on plain floor: walls, pits and raised blocks all stop them.
uint8_t enemy_box_free(uint16_t x, uint16_t y) {
    uint16_t l = x + ENEMY_HB_X, r = x + ENEMY_HB_X + ENEMY_HB_W - 1;
    uint16_t t = y + ENEMY_HB_Y, b = y + ENEMY_HB_Y + ENEMY_HB_H - 1;
    return room_block_at(l, t) == BLK_FLOOR && room_block_at(r, t) == BLK_FLOOR &&
           room_block_at(l, b) == BLK_FLOOR && room_block_at(r, b) == BLK_FLOOR;
}


// Where a hop at velocity v (px/frame 8.8) ends, SLIME_HOP frames from pos.
static uint16_t hop_end(uint16_t pos, int16_t v) {
#if SLIME_HOP != 24
#error hop_end multiplies by 24 with shifts
#endif
    return pos + (int16_t)(((v << 4) + (v << 3)) >> 8);
}

// Sign of the direction from (ax, ay) to the player's feet, per axis, with a dead
// zone so an enemy lined up on one axis moves straight along the other.
static int8_t toward(uint16_t from, uint16_t to) {
    int16_t d = (int16_t)(to - from);
    return d > 4 ? 1 : d < -4 ? -1 : 0;
}

// Velocity of `speed` along an 8-way direction; diag is its diagonal component.
void set_velocity(int8_t sx, int8_t sy, int16_t speed, int16_t diag, int16_t *vx, int16_t *vy) {
    int16_t s = (sx && sy) ? diag : speed;
    *vx = sx > 0 ? s : sx < 0 ? -s : 0;
    *vy = sy > 0 ? s : sy < 0 ? -s : 0;
}

// Diagonal components of the speeds (diag_scale is two multiplies), redone only
// when the tunable changes.
static uint16_t slime_diag_for;
static int16_t slime_diag;

static void update_slime(uint8_t i) {
    if (actor_state[i] == STATE_REST) {
        actor_frame[i] = 0;
        if ((actor_timer[i] = tick(actor_timer[i]))) return;
        // Checking a landing spot is the costly part (4 block lookups), so it
        // takes the frame's terrain check (or waits), and each think step checks
        // one spot.
        if (!terrain_checks_left) return;
        terrain_checks_left--;
        // Hop toward where the player is now.
        int8_t sx = toward(actor_x[i] + 8, player_x + 8);
        int8_t sy = toward(actor_y[i] + 11, player_y + 12);
        int16_t vx, vy;
        if (slime_diag_for != slime_speed) slime_diag = diag_scale(slime_diag_for = slime_speed);
        set_velocity(sx, sy, slime_speed, slime_diag, &vx, &vy);
        // Check the landing spot once, instead of colliding every frame of the hop.
        // A hop (~18 px) is too short to pass through a wall or pit (16 px plus the
        // 12-px hitbox), so a free landing spot means a free path. If it's blocked,
        // the next think steps try hopping along x alone, then y alone, then the
        // slime hops in place.
        uint8_t t = actor_try[i];
        if (t == 1 && !vx) t = 2;
        if (t == 1) vy = 0;
        else if (t == 2) vx = 0;
        if ((vx || vy) && !enemy_box_free(hop_end(actor_x[i], vx), hop_end(actor_y[i], vy))) {
            if (t < 2) {
                actor_try[i] = t + 1;  // the timer stays at 0: try again next step
                return;
            }
            vx = vy = 0;
        }
        actor_try[i] = 0;
        actor_vx[i] = vx;
        actor_vy[i] = vy;
        actor_state[i] = STATE_HOP;
        actor_timer[i] = SLIME_HOP;
        return;
    }
    // Hopping: animate (squash and stretch over the hop) and move, stopping at walls.
    actor_frame[i] = 1 + ((uint8_t)(SLIME_HOP - actor_timer[i]) >> 3);  // 3 frames over 24
    // The path was checked when the hop started (see above), so just move.
    uint8_t nf;
    if (actor_vx[i]) {
        actor_x[i] = moved(actor_x[i], actor_xs[i], actor_vx[i] * AI_STEP, &nf);
        actor_xs[i] = nf;
    }
    if (actor_vy[i]) {
        actor_y[i] = moved(actor_y[i], actor_ys[i], actor_vy[i] * AI_STEP, &nf);
        actor_ys[i] = nf;
    }
    if (!(actor_timer[i] = tick(actor_timer[i]))) {
        actor_state[i] = STATE_REST;
        actor_timer[i] = slime_rest;
    }
}


// Do the spans [a, a+aw) and [b, b+bw) overlap? One unsigned compare: the
// difference a + aw - b must be in 1..aw+bw-1 (anything else wraps to a large value).
#define SPAN_OVERLAP(a, aw, b, bw) ((uint16_t)((a) + (aw) - (b) - 1) < (uint16_t)((aw) + (bw) - 1))

// Bullets move, collide and draw in assembly (bullets.s); a hit is handled here.
static void update_bullets(void) {
    bullets_active = 0;
    if (!bullet_count) return;
    bullet_hb_x = player_x + PLAYER_HB_X;
    bullet_hb_y = player_y + PLAYER_HB_Y;
    bullet_hittable = player_hittable();
    bullet_parity = (uint8_t)frame_count & 1;
    bullet_hit = 0;
    uint8_t anim = (uint8_t)(frame_count >> 3) & 1;
    bullet_draw_tile = bullet_vram + (pea_frames[anim << 2] << 1);
    bullet_draw_prop = pea_frames[(anim << 2) + 1] | SPR_PAL_BULLET;
    bullet_draw_start = &bullets[bullet_rotation];
    bullets_update();
    if (bullet_hit) {
        hurt_cause = enemy_bullet_cause;
        player_hurt(enemy_bullet_damage, bullet_hit_x, bullet_hit_y);
    }
}

// The player's shots (shots.s): set up its inputs, then remove enemies a shot
// finished off.
static void update_shots(void) {
    shot_parity = (uint8_t)frame_count & 1;
    uint8_t f = (((uint8_t)frame_count >> 2) & 1) << 2;  // the pellet turns every 4 frames
    shot_draw_tile = shot_vram + (shot_frames[f] << 1);
    shot_draw_prop = shot_frames[f + 1] | SPR_PAL_SHOT;
    shots_update();
}

// Remove enemies brought to 0 HP (by shots or the sword: either sets shots_killed).
static void reap(void) {
    shots_killed = 0;
    for (uint8_t i = 0; i < ACTOR_MAX; i++) {
        if (actor_type[i] == ACTOR_NONE || actor_hp[i]) continue;
        xp_pending += type_xp[actor_type[i]];
        if (actor_elite[i]) elite_fell(i);  // (play awards it)
        kill_x = actor_x[i];  // (loot may drop there: pickups.c)
        kill_y = actor_y[i];
        actor_type[i] = ACTOR_NONE;
        actor_count--;
        enemies_killed++;
        camera_shake(SHAKE_SMALL);
        aim_score[i] = AIM_NONE;
        if (aim_target == i) aim_target = AIM_NONE;
    }
}

void actors_update(uint8_t first_oam) {
    terrain_checks_left = 1;
    if (actor_count) {
        uint16_t px = player_x + PLAYER_HB_X, py = player_y + PLAYER_HB_Y;
        uint8_t hittable = player_hittable();
        if (boss_kind >= BOSS_CRAGBACK && (boss_kind == BOSS_GLOBFATHER || actor_state[boss_slot] == STATE_HOP)) hittable = 0;
        // Each enemy thinks and checks contact with the player on alternate frames
        // (half the slots each frame). At these speeds a touch registering one
        // frame later is imperceptible, and it halves the cost of this loop.
        for (uint8_t i = (uint8_t)frame_count & 1; i < ACTOR_MAX; i += 2) {
            uint8_t type = actor_type[i];
            if (type == ACTOR_NONE) continue;
            uint8_t frame = actor_frame[i];
            if (!actor_scripted[i]) {
                uint8_t n = actor_elite[i] == ELITE_SWIFT ? 2 : 1;  // (a swift elite: twice)
                do {
                    if (type == ACTOR_SLIME) update_slime(i);
                    else if (type == ACTOR_PEASHOOTER) shooter_update(i);
                    else if (type == ACTOR_HOUND) hound_update(i);
                    else duelist_update(i);
                } while (--n);
                if (actor_frame[i] != frame) actor_cache_frame(i);
            }
            // Hit by a shot: flash for a few think steps.
            if (actor_hurt[i]) {
                actor_hurt[i]--;
                actor_flash[i] = 1;
            } else if (type == ACTOR_SLIME) {
                actor_flash[i] = 0;  // (peashooters set their own: the warning flash)
            }
            // Scripted actors 15+ own their contact windows; others hurt on touch.
            if (hittable && actor_scripted[i] < 15 && SPAN_OVERLAP(actor_x[i] + ENEMY_HB_X, ENEMY_HB_W, px, PLAYER_HB_W) &&
                SPAN_OVERLAP(actor_y[i] + ENEMY_HB_Y, ENEMY_HB_H, py, PLAYER_HB_H)) {
                hurt_cause = actor_scripted[i] >= 2 ? actor_scripted[i] + 3 : type_cause[type];
                player_hurt(TOUCH_DAMAGE, actor_x[i] + 8, actor_y[i] + 11);
                hittable = 0;
            }
        }
    }
    // Draw: actors (actor_draw.s), then the player's shots and enemy bullets as
    // they move (shots.s, bullets.s).
    oam_next = first_oam;
    if (boss_kind == BOSS_GLOBFATHER) globfather_emit();
    else if (actor_count) actors_emit();
    if (boss_kind == BOSS_CRAGBACK) cragback_emit();
    // Warden reserves slots 15-22 for floor fields. Prioritize its enemy
    // volleys over player projectiles. Keep the ordinary path to one branch.
    if (first_oam == 23 || boss_kind == BOSS_GLOBFATHER) {
        update_bullets();
        if (shot_count) update_shots();
        if (shots_killed) reap();
        actors_active = actor_count;
    } else {
        if (shot_count) update_shots();
        if (shots_killed) reap();
        actors_active = actor_count;
        update_bullets();
    }
    uint8_t used = oam_next;
    sprites_used = used;
    // Hide whatever was drawn last frame beyond this frame's sprites.
    for (; used < oam_end_last; used++) shadow_OAM[used].y = 0;
    oam_end_last = sprites_used;
    // Steps coprime with the pool sizes visit every order over time.
    draw_rotation += 3;
    bullet_rotation += 7;
    if (bullet_rotation >= BULLET_MAX) bullet_rotation -= BULLET_MAX;
}
