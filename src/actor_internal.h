// Shared internals of the actor system: used by actor.c (bank 0, runs every frame)
// and actor_setup.c (banked: loading graphics and spawning). Not for other modules.
#ifndef ACTOR_INTERNAL_H
#define ACTOR_INTERNAL_H

#include <stdint.h>
#include "actor.h"
#include "spr_slime.h"
#include "spr_peashooter.h"
#include "spr_pea.h"

// Sprite VRAM: the player owns tiles 0-9 (frame slot + shadow); enemy graphics
// are loaded from here on, one block per type. The hound's 8 fit between the
// pickups' (pickups.c) and the chest's (chest.h); the duelist's 8 in the last
// gaps: 6 after the player's, 2 after the chest's item. The loader's 8 after the
// reload bar's (aim.c).
#define ENEMY_VRAM 16
#define HOUND_VRAM 88
#define DUELIST_VRAM 10
#define DUELIST_VRAM2 106
#define TANK_VRAM 68

// Slime: rests (slime_rest frames), then hops toward the player.
#define SLIME_HOP 24
// Damage to the player (health is HEALTH_START = 60 at first: 6 touches).
#define TOUCH_DAMAGE 10   // walking into an enemy
#define BULLET_DAMAGE 8   // a peashooter's pea
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


extern uint8_t actor_count;
extern uint8_t bullet_count;
// Projectile settings for the current encounter; reset by actors_init.
extern uint8_t enemy_bullet_damage, enemy_bullet_cause;
extern uint8_t actor_t0[ACTOR_MAX];
extern uint8_t actor_p0[ACTOR_MAX];
extern uint8_t actor_t1[ACTOR_MAX];
extern uint8_t actor_p1[ACTOR_MAX];
extern uint8_t slime_frames[SPR_SLIME_FRAME_COUNT * 4];
extern uint8_t shooter_frames[SPR_PEASHOOTER_FRAME_COUNT * 4];
extern uint8_t pea_frames[SPR_PEA_FRAME_COUNT * 4];
extern uint8_t type_vram[ACTOR_TYPE_COUNT];
extern uint8_t bullet_vram;
extern uint8_t oam_end_last;

// Look up the sprite tiles and flags for an actor's current frame.
void actor_cache_frame(uint8_t i);
// Enemies only stand on plain floor: is the hitbox at (x, y) all on it? (actor.c)
uint8_t enemy_box_free(uint16_t x, uint16_t y);
// Velocity of `speed` along an 8-way direction; diag is its diagonal component (actor.c).
void set_velocity(int8_t sx, int8_t sy, int16_t speed, int16_t diag, int16_t *vx, int16_t *vy);
// A peashooter's think step (actor_shooter.c, banked).
void shooter_update(uint8_t i) BANKED;
// A peashooter's shot (actor_setup.c, banked: it's rare).
void fire_bullet(uint8_t i, int8_t sx, int8_t sy) BANKED;
// A peashooter's shot: one pea toward (sx, sy), or three with shooter_spread.
void fire_volley(uint8_t i, int8_t sx, int8_t sy) BANKED;

#endif
