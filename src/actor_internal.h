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


extern uint8_t actor_count;
extern uint8_t bullet_count;
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

#endif
