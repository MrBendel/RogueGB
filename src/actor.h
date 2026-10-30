// Actors: enemies (and later pickups) in a fixed pool, plus a pool of enemy
// bullets. Issue #4 (actor system) and #9 (first enemies).
//
// State is stored as parallel arrays (actor_x[i], actor_type[i], ...) rather than
// structs: cheaper array indexing on this CPU, and tests can read each field by name.
//
// Graphics: each enemy type's frames are loaded into sprite VRAM once (actors_init)
// and shared by every enemy of that type. Hardware sprites are handed out every
// frame by actors_update, after the player's; the order rotates each frame so that
// when more than 10 sprites share a scanline (or OAM runs out), which ones the
// hardware drops changes from frame to frame (flicker) instead of one enemy
// staying invisible.
#ifndef ACTOR_H
#define ACTOR_H

#include <stdint.h>

#define ACTOR_MAX 8
#define BULLET_MAX 20  // also in actor_draw.s

// Types (0 = free slot).
#define ACTOR_NONE 0
#define ACTOR_SLIME 1       // chaser: hops toward the player, hurts on contact
#define ACTOR_PEASHOOTER 2  // shooter: turns to face the player, telegraphs, fires
#define ACTOR_HOUND 3       // flanker: circles, warns, lunges, rests (hound.c)
#define ACTOR_DUELIST 4     // duelist: sidesteps, aims, fires a burst, reloads (duelist.c)
#define ACTOR_TYPE_COUNT 5
// Odd kinds (slime, hound) are creatures, drawn in SPR_PAL_SLIME; even ones
// (peashooter, duelist) machines and mercs, in SPR_PAL_SHOOTER: a world sets both
// (world.c).

// Directions for facing (art order).
#define FACE_DOWN 0
#define FACE_UP 1
#define FACE_RIGHT 2
#define FACE_LEFT 3

extern uint8_t actor_type[ACTOR_MAX];
extern uint8_t actor_state[ACTOR_MAX];
extern uint8_t actor_timer[ACTOR_MAX];
extern uint16_t actor_x[ACTOR_MAX];  // top-left of the 16x16 frame, room pixels
extern uint16_t actor_y[ACTOR_MAX];
extern uint8_t actor_xs[ACTOR_MAX];  // pixel fractions
extern uint8_t actor_ys[ACTOR_MAX];
extern int16_t actor_vx[ACTOR_MAX];  // px/frame 8.8
extern int16_t actor_vy[ACTOR_MAX];
extern uint8_t actor_hp[ACTOR_MAX];
extern uint8_t actor_face[ACTOR_MAX];
extern uint8_t actor_try[ACTOR_MAX];
extern uint8_t actor_hurt[ACTOR_MAX];
extern uint8_t actor_frame[ACTOR_MAX];  // frame index into the type's frame table
extern uint8_t actor_flash[ACTOR_MAX];  // frames left drawing with the flash palette
extern uint8_t actor_scripted[ACTOR_MAX]; // encounter controller owns AI/art; still collides
extern uint8_t actor_guard[ACTOR_MAX];    // excluded from aim and weapon damage while protected
extern uint8_t actor_elite[ACTOR_MAX];    // elite.h ELITE_*: its modifier, or none
extern uint8_t actor_count;

// One enemy bullet. The layout is used by bullets.s (offsets in its header): per
// axis, velocity then position, so moving is one straight walk through the record.
typedef struct {
    uint8_t alive;   // 0
    int16_t vx;      // 1: px/frame 8.8
    int8_t vx_sign;  // 3: 0 or -1, vx's sign extension (set with bullet_set_velocity)
    uint8_t xs;      // 4: fraction of a pixel
    uint16_t x;      // 5: top-left of the 8x16 sprite, room pixels
    int16_t vy;      // 7
    int8_t vy_sign;  // 9
    uint8_t ys;      // 10
    uint16_t y;      // 11
} Bullet;            // 13 bytes
extern Bullet bullets[BULLET_MAX];
// Set a bullet's velocity (px/frame 8.8), keeping the sign bytes in step.
#define bullet_set_velocity(b, vx_, vy_) do { \
        (b)->vx = (vx_); (b)->vx_sign = (b)->vx < 0 ? -1 : 0; \
        (b)->vy = (vy_); (b)->vy_sign = (b)->vy < 0 ? -1 : 0; \
    } while (0)

// One expensive terrain check per frame (a slime's landing spot, or auto-aim
// scoring an enemy with its line of sight): set to 1 by actors_update; whoever
// needs one takes it, or waits a frame. Spreads the cost so they never pile up.
extern uint8_t terrain_checks_left;

// Watchable counters.
extern uint8_t actors_active;
extern uint8_t bullets_active;
extern uint8_t sprites_used;    // hardware sprites drawn last frame (all, player included)
extern uint16_t bullets_fired;  // by enemies

// Tunables.
extern uint16_t slime_speed;      // px/frame 8.8 while hopping
extern uint8_t shooter_interval;  // frames between shots
extern uint8_t shooter_telegraph; // frames of warning flash before a shot
extern uint16_t bullet_speed;     // px/frame 8.8
extern uint8_t slime_rest;        // frames a slime rests between hops
extern uint8_t enemy_hp_bonus;    // HP added to each enemy spawned
extern uint8_t shooter_spread;    // peashooters fire 3 peas (the aim and 45 degrees either side)

// Debug/test: fill the enemy pool around room pixel (px, py) and the bullet pool
// with slow bullets nearby (the M1 stress target: 8 enemies, 20 bullets).
void actors_debug_stress(uint16_t px, uint16_t py) BANKED;
// Load enemy graphics into VRAM and clear the pools (call with the display off).
#include <gb/gb.h>
void actors_init(void) BANKED;  // actor_setup.c (banked)
void actors_clear(void) BANKED;  // no enemies, bullets or shots (a room change)
// Spawn an actor with its frame's top-left at room pixel (x, y). Returns its index, or 0xFF if full.
uint8_t actor_spawn(uint8_t type, uint16_t x, uint16_t y) BANKED;
// Move, think, collide with the player, and draw: actors and bullets get hardware
// sprites from `first_oam` on. Call after the camera has moved for this frame.
void actors_update(uint8_t first_oam);
// Knock enemy i back by (dx, dy) px, if it lands on plain floor (else half way,
// or not at all). Banked (actor_setup.c): it's only on a sword hit.
void actor_push(uint8_t i, int8_t dx, int8_t dy) BANKED;

#endif
