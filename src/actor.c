#include <gb/gb.h>
#include "actor.h"
#include "player.h"
#include "room.h"
#include "camera.h"
#include "state.h"
#include "spr_slime.h"
#include "spr_peashooter.h"
#include "spr_pea.h"

// Sprite VRAM: the player owns tiles 0-9 (frame slot + shadow); enemy graphics
// are loaded from here on, one block per type.
#define ENEMY_VRAM 16

// Slime: rests, then hops toward the player.
#define SLIME_REST 30
#define SLIME_HOP 24
#define SLIME_HP 3
#define STATE_REST 0
#define STATE_HOP 1
// Peashooter: stationary, faces the player, flashes, fires.
#define SHOOTER_HP 4

// Hitboxes inside the 16x16 frame: x offset, width, y offset, height.
#define ENEMY_HB_X 2
#define ENEMY_HB_W 12
#define ENEMY_HB_Y 6
#define ENEMY_HB_H 10
// Bullet hitbox inside its 8x16 sprite (the art is roughly rows 3-12).
#define BULLET_HB_X 1
#define BULLET_HB_W 6
#define BULLET_HB_Y 5
#define BULLET_HB_H 6

#define DIAG 181  // 1/sqrt(2) in 0.8 fixed point

// Enemies think at 30 Hz: each runs its AI every other frame (alternating by
// slot, so half the enemies update each frame) and covers two frames' worth of
// movement and timers when it does. A standard Game Boy saving; bullets still
// move every frame. Timers are therefore kept even.
#define AI_STEP 2

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
uint8_t actor_frame[ACTOR_MAX];
uint8_t actor_flash[ACTOR_MAX];
// Sprite data for the current frame (tile and flags per column), cached when the
// frame changes so drawing just copies bytes. Read by actor_draw.s.
uint8_t actor_t0[ACTOR_MAX];
uint8_t actor_p0[ACTOR_MAX];
uint8_t actor_t1[ACTOR_MAX];
uint8_t actor_p1[ACTOR_MAX];
static uint8_t actor_count;   // slots in use (enemies aren't removed yet)
static uint8_t bullet_count;  // bullets alive

uint8_t bullet_alive[BULLET_MAX];
uint16_t bullet_x[BULLET_MAX];
uint16_t bullet_y[BULLET_MAX];
uint8_t bullet_xs[BULLET_MAX];
uint8_t bullet_ys[BULLET_MAX];
int16_t bullet_vx[BULLET_MAX];
int16_t bullet_vy[BULLET_MAX];

uint8_t actors_active;
uint8_t bullets_active;
uint8_t sprites_used;
uint16_t bullets_fired;

uint16_t slime_speed = 192;     // 0.75 px/frame while hopping
uint8_t shooter_interval = 90;  // 1.5 s between shots
uint8_t shooter_telegraph = 24; // flash for 0.4 s first
uint16_t bullet_speed = 384;    // 1.5 px/frame

// Per-type graphics: converted frame tables and where the tiles went in VRAM.
static const uint8_t *type_frames[ACTOR_TYPE_COUNT] = {0, spr_slime_frames, spr_peashooter_frames};
static uint8_t type_vram[ACTOR_TYPE_COUNT];
static uint8_t bullet_vram;

// Shared with actor_draw.s (hand-written sprite emission).
uint8_t draw_rotation;
uint8_t oam_next;
uint8_t bullet_draw_tile;
uint8_t bullet_draw_prop;
void actors_emit(void);
void bullets_emit(void);
void bullets_move(void);
static uint8_t oam_end_last;

void actors_init(void) {
    // Load each type's columns (2 tiles each) into its own block of sprite VRAM.
    uint8_t v = ENEMY_VRAM;
    type_vram[ACTOR_SLIME] = v;
    set_sprite_data(v, SPR_SLIME_COLUMN_COUNT * 2, spr_slime_tiles);
    v += SPR_SLIME_COLUMN_COUNT * 2;
    type_vram[ACTOR_PEASHOOTER] = v;
    set_sprite_data(v, SPR_PEASHOOTER_COLUMN_COUNT * 2, spr_peashooter_tiles);
    v += SPR_PEASHOOTER_COLUMN_COUNT * 2;
    bullet_vram = v;
    set_sprite_data(v, SPR_PEA_COLUMN_COUNT * 2, spr_pea_tiles);

    for (uint8_t i = 0; i < ACTOR_MAX; i++) actor_type[i] = ACTOR_NONE;
    for (uint8_t i = 0; i < BULLET_MAX; i++) bullet_alive[i] = 0;
    actors_active = bullets_active = 0;
    actor_count = bullet_count = 0;
    bullets_fired = 0;
    oam_end_last = 0;
}

// Look up the sprite tiles and flags for an actor's current frame.
static void cache_frame(uint8_t i) {
    uint8_t type = actor_type[i];
    const uint8_t *f = type_frames[type] + (actor_frame[i] << 2);
    uint8_t base = type_vram[type];
    actor_t0[i] = base + (f[0] << 1);
    actor_p0[i] = f[1];
    actor_t1[i] = base + (f[2] << 1);
    actor_p1[i] = f[3];
}

// Count a timer down by one AI step; returns the new value (stops at 0).
static uint8_t tick(uint8_t t) {
    return t > AI_STEP ? t - AI_STEP : 0;
}

uint8_t actor_spawn(uint8_t type, uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < ACTOR_MAX; i++) {
        if (actor_type[i] != ACTOR_NONE) continue;
        actor_type[i] = type;
        actor_x[i] = x;
        actor_y[i] = y;
        actor_xs[i] = actor_ys[i] = 0;
        actor_vx[i] = actor_vy[i] = 0;
        actor_face[i] = FACE_DOWN;
        actor_frame[i] = 0;
        actor_flash[i] = 0;
        actor_state[i] = STATE_REST;
        actor_count++;
        // Stagger timers by slot so a wave doesn't move in lockstep.
        if (type == ACTOR_SLIME) {
            actor_hp[i] = SLIME_HP;
            actor_timer[i] = SLIME_REST + (i << 3);
        } else {
            actor_hp[i] = SHOOTER_HP;
            actor_timer[i] = shooter_interval + (i << 4);
        }
        cache_frame(i);
        return i;
    }
    return 0xFF;
}

// Position + velocity in 8/16-bit pieces (same as the player's).
static uint16_t moved(uint16_t pos, uint8_t frac, int16_t v, uint8_t *new_frac) {
    uint16_t f = (uint16_t)frac + (uint8_t)v;
    *new_frac = (uint8_t)f;
    return pos + (int8_t)((uint16_t)v >> 8) + (f >> 8);
}

// Enemies only stand on plain floor: walls, pits and raised blocks all stop them.
static uint8_t enemy_box_free(uint16_t x, uint16_t y) {
    uint16_t l = x + ENEMY_HB_X, r = x + ENEMY_HB_X + ENEMY_HB_W - 1;
    uint16_t t = y + ENEMY_HB_Y, b = y + ENEMY_HB_Y + ENEMY_HB_H - 1;
    return room_block_at(l, t) == BLK_FLOOR && room_block_at(r, t) == BLK_FLOOR &&
           room_block_at(l, b) == BLK_FLOOR && room_block_at(r, b) == BLK_FLOOR;
}

// Where a hop at velocity v (px/frame 8.8) ends, SLIME_HOP frames from pos.
static uint16_t hop_end(uint16_t pos, int16_t v) {
    return pos + (int16_t)((v * SLIME_HOP) >> 8);
}

// Sign of the direction from (ax, ay) to the player's feet, per axis, with a dead
// zone so an enemy lined up on one axis moves straight along the other.
static int8_t toward(uint16_t from, uint16_t to) {
    int16_t d = (int16_t)(to - from);
    return d > 4 ? 1 : d < -4 ? -1 : 0;
}

// Velocity of `speed` along an 8-way direction.
static void set_velocity(int8_t sx, int8_t sy, uint16_t speed, int16_t *vx, int16_t *vy) {
    int16_t s = (sx && sy) ? (int16_t)((speed >> 8) * DIAG + (((speed & 0xFF) * DIAG) >> 8)) : (int16_t)speed;
    *vx = sx > 0 ? s : sx < 0 ? -s : 0;
    *vy = sy > 0 ? s : sy < 0 ? -s : 0;
}

static void update_slime(uint8_t i) {
    if (actor_state[i] == STATE_REST) {
        actor_frame[i] = 0;
        if ((actor_timer[i] = tick(actor_timer[i]))) return;
        // Hop toward where the player is now.
        int8_t sx = toward(actor_x[i] + 8, player_x + 8);
        int8_t sy = toward(actor_y[i] + 11, player_y + 12);
        int16_t vx, vy;
        set_velocity(sx, sy, slime_speed, &vx, &vy);
        // Check the landing spot once, instead of colliding every frame of the hop.
        // A hop (~18 px) is too short to pass through a wall or pit (16 px plus the
        // 12-px hitbox), so a free landing spot means a free path. If it's blocked,
        // try hopping along just one axis, or stay put.
        uint16_t ex = hop_end(actor_x[i], vx), ey = hop_end(actor_y[i], vy);
        if (!enemy_box_free(ex, ey)) {
            if (vx && enemy_box_free(ex, actor_y[i])) vy = 0;
            else if (vy && enemy_box_free(actor_x[i], ey)) vx = 0;
            else vx = vy = 0;
        }
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
        actor_timer[i] = SLIME_REST;
    }
}

static void fire_bullet(uint8_t i, int8_t sx, int8_t sy) {
    for (uint8_t b = 0; b < BULLET_MAX; b++) {
        if (bullet_alive[b]) continue;
        bullet_alive[b] = 1;
        // The bullet's 8x16 sprite, centered on the shooter.
        bullet_x[b] = actor_x[i] + 4;
        bullet_y[b] = actor_y[i];
        bullet_xs[b] = bullet_ys[b] = 0;
        set_velocity(sx, sy, bullet_speed, &bullet_vx[b], &bullet_vy[b]);
        bullets_fired++;
        bullet_count++;
        return;
    }
}

static void update_shooter(uint8_t i) {
    // Face the player along the larger axis.
    int16_t dx = (int16_t)(player_x - actor_x[i]);
    int16_t dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    actor_face[i] = ax > ay ? (dx > 0 ? FACE_RIGHT : FACE_LEFT) : (dy > 0 ? FACE_DOWN : FACE_UP);

    uint8_t t = actor_timer[i] = tick(actor_timer[i]);
    uint8_t telegraph = t < shooter_telegraph;
    // Warning: flash and open up before firing.
    actor_flash[i] = telegraph && (t & 4);
    actor_frame[i] = (actor_face[i] << 1) + telegraph;
    if (t) return;
    // Aim at the player in 8 directions: diagonal when the axes are within 2:1.
    int8_t sx = (ax << 1) > ay ? (dx > 0 ? 1 : -1) : 0;
    int8_t sy = (ay << 1) > ax ? (dy > 0 ? 1 : -1) : 0;
    fire_bullet(i, sx, sy);
    actor_timer[i] = shooter_interval;
}

// Do the spans [a, a+aw) and [b, b+bw) overlap? One unsigned compare: the
// difference a + aw - b must be in 1..aw+bw-1 (anything else wraps to a large value).
#define SPAN_OVERLAP(a, aw, b, bw) ((uint16_t)((a) + (aw) - (b) - 1) < (uint16_t)((aw) + (bw) - 1))

static void kill_bullet(uint8_t b) {
    bullet_alive[b] = 0;
    bullet_count--;
}

static void update_bullets(void) {
    bullets_active = 0;
    if (!bullet_count) return;
    uint16_t px = player_x + PLAYER_HB_X, py = player_y + PLAYER_HB_Y;
    uint8_t hittable = player_hittable();
    uint8_t parity = (uint8_t)frame_count & 1;
    bullets_move();  // every frame, in assembly (actor_draw.s)
    for (uint8_t b = 0; b < BULLET_MAX; b++) {
        if (!bullet_alive[b]) continue;
        bullets_active++;
        // Collisions run at 30 Hz per bullet (staggered): at 1.5 px/frame a bullet
        // can't cross a 16-px wall or the player's hitbox between checks.
        if ((b & 1) != parity) continue;
        uint16_t x = bullet_x[b], y = bullet_y[b];
        // Walls and raised blocks stop bullets (they fly at ground level). Bullets
        // stay inside the room because its edge is wall, so no bounds check is needed.
        uint8_t blk = ROOM_BLOCK_IN(x + 4, y + 8);
        if (blk == BLK_SOLID || blk == BLK_RAISED) {
            kill_bullet(b);
            bullets_active--;
            continue;
        }
        if (hittable && SPAN_OVERLAP(x + BULLET_HB_X, BULLET_HB_W, px, PLAYER_HB_W) &&
            SPAN_OVERLAP(y + BULLET_HB_Y, BULLET_HB_H, py, PLAYER_HB_H)) {
            player_hurt(1, x + 4, y + 8);
            kill_bullet(b);
            bullets_active--;
            hittable = 0;
        }
    }
}

void actors_update(void) {
    actors_active = 0;
    if (actor_count) {
        uint16_t px = player_x + PLAYER_HB_X, py = player_y + PLAYER_HB_Y;
        uint8_t hittable = player_hittable();
        uint8_t parity = (uint8_t)frame_count & 1;
        for (uint8_t i = 0; i < ACTOR_MAX; i++) {
        uint8_t type = actor_type[i];
        if (type == ACTOR_NONE) continue;
        actors_active++;
        if ((i & 1) == parity) {
            uint8_t frame = actor_frame[i];
            if (type == ACTOR_SLIME) update_slime(i);
            else update_shooter(i);
            if (actor_frame[i] != frame) cache_frame(i);
        }
        // Touching an enemy hurts (unless you're jumping over it).
        if (hittable && SPAN_OVERLAP(actor_x[i] + ENEMY_HB_X, ENEMY_HB_W, px, PLAYER_HB_W) &&
            SPAN_OVERLAP(actor_y[i] + ENEMY_HB_Y, ENEMY_HB_H, py, PLAYER_HB_H)) {
            player_hurt(1, actor_x[i] + 8, actor_y[i] + 11);
            hittable = 0;
        }
        }
    }
    update_bullets();
}

void actors_draw(uint8_t first_oam) {
    oam_next = first_oam;
    // The per-sprite work is in assembly (actor_draw.s): two sprites per visible
    // actor, then one per visible bullet, starting at a rotating slot.
    if (actor_count) actors_emit();
    if (bullet_count) {
        uint8_t anim = (uint8_t)(frame_count >> 3) & 1;
        bullet_draw_tile = bullet_vram + (spr_pea_frames[anim << 2] << 1);
        bullet_draw_prop = spr_pea_frames[(anim << 2) + 1];
        bullets_emit();
    }
    uint8_t used = oam_next;
    sprites_used = used;
    // Hide whatever was drawn last frame beyond this frame's sprites.
    for (; used < oam_end_last; used++) shadow_OAM[used].y = 0;
    oam_end_last = sprites_used;
    // A step coprime with the pool size visits every order over time.
    draw_rotation += 3;
}
