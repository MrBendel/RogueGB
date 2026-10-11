// Actors: enemies (and later pickups) in a fixed pool, plus a pool of enemy
// bullets. Issue #4 (actor system) and #9 (first enemies).
//
// State is stored as parallel arrays (actor_x[i], actor_type[i], ...) rather than
// structs: cheaper array indexing on this CPU, and tests can read each field by name.
//
// Graphics: each enemy type's frames are loaded into sprite VRAM once (actors_init)
// and shared by every enemy of that type. Hardware sprites are handed out every
// frame by actors_draw, after the player's; the order rotates each frame so that
// when more than 10 sprites share a scanline (or OAM runs out), which ones the
// hardware drops changes from frame to frame (flicker) instead of one enemy
// staying invisible.
#ifndef ACTOR_H
#define ACTOR_H

#include <stdint.h>

#define ACTOR_MAX 8
#define BULLET_MAX 8

// Types (0 = free slot).
#define ACTOR_NONE 0
#define ACTOR_SLIME 1       // chaser: hops toward the player, hurts on contact
#define ACTOR_PEASHOOTER 2  // shooter: turns to face the player, telegraphs, fires
#define ACTOR_TYPE_COUNT 3

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
extern uint8_t actor_frame[ACTOR_MAX];  // frame index into the type's frame table
extern uint8_t actor_flash[ACTOR_MAX];  // frames left drawing with the flash palette

extern uint8_t bullet_alive[BULLET_MAX];
extern uint16_t bullet_x[BULLET_MAX];  // top-left of the 8x16 sprite, room pixels
extern uint16_t bullet_y[BULLET_MAX];
extern uint8_t bullet_xs[BULLET_MAX];
extern uint8_t bullet_ys[BULLET_MAX];
extern int16_t bullet_vx[BULLET_MAX];
extern int16_t bullet_vy[BULLET_MAX];

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

// Load enemy graphics into VRAM and clear the pools (call with the display off).
#include <gb/gb.h>
void actors_init(void) BANKED;  // actor_setup.c (banked)
// Spawn an actor with its frame's top-left at room pixel (x, y). Returns its index, or 0xFF if full.
uint8_t actor_spawn(uint8_t type, uint16_t x, uint16_t y) BANKED;
// Move, think and collide with the player.
void actors_update(void);
// Hand out hardware sprites from `first_oam` on and draw actors and bullets.
void actors_draw(uint8_t first_oam);

#endif
