// Auto-aim and the player's shots (#5). Holding A fires at the enemy auto-aim
// picks; a reticle marks it. See ROADMAP.md, "Auto-aim".
//
// Targeting (aim.c, banked): one enemy slot at a time is scored (distance, less
// a bias if it's in front of the player, or AIM_NONE if out of range or out of line
// of sight), round-robin on odd frames, so every enemy is rescored about every 16
// frames (a turn is skipped when a slime took the frame's terrain check, see
// actor.h, or the frame is running late). The target is
// the best score, but it's sticky: another enemy takes over only when it's clearly
// better (aim_sticky), or when the target dies or can't be seen.
//
// Shots (shots.s, bank 0): a small pool moving, hitting walls and enemies, and
// drawing in one pass, like enemy bullets. A hit takes 1 HP and flashes the enemy;
// actors_update removes enemies at 0 HP.
#ifndef AIM_H
#define AIM_H

#include <gb/gb.h>
#include <stdint.h>
#include "actor.h"

#define AIM_NONE 0xFF
#define SHOT_MAX 6  // also in shots.s

// --- watchable state ---
extern uint8_t aim_target;              // enemy slot, or AIM_NONE
extern uint8_t aim_score[ACTOR_MAX];    // lower is better; AIM_NONE = can't be targeted
extern uint8_t aim_eval;                // slot scored next
extern uint8_t fire_cooldown;           // frames until the next shot can fire
extern uint8_t aim_dir;                 // direction of the last shot, 0-31 (0 = right, clockwise)
extern uint16_t shots_fired;
extern uint16_t shots_hit;              // shots that hit an enemy
extern uint16_t enemies_killed;
extern uint8_t aim_dir32;               // where the gun points now (0-31)
extern uint8_t gun_dir16;               // the gun sprite's direction (0-15)
extern uint8_t aim_facing;              // art direction while shooting (player.c), or 0xFF

// The pool uses the enemy bullet record (actor.h); `alive` counts the frames of
// flight left (0 = free slot).
extern Bullet shots[SHOT_MAX];
extern uint8_t shot_count;

// --- tunables ---
extern uint8_t aim_range;      // px: farthest target (octagonal distance)
extern uint8_t aim_bias;       // px taken off the distance of enemies in front of the player
extern uint8_t aim_sticky;     // px better another enemy must be to take the target over
extern uint8_t fire_interval;  // frames between shots while A is held
extern uint16_t shot_speed;    // px/frame 8.8

// Load the shot and reticle graphics into sprite VRAM from tile `vram` on, and
// clear targeting and the shot pool. Called by actors_init (display off).
void aim_init(uint8_t vram) BANKED;
// Score one enemy, pick the target, fire if A is held, draw the reticle. Call
// after actors_update (enemies have moved and drawn for this frame).
void aim_update(void) BANKED;

// Bank 0, from actors_update: move, collide and draw the shots (shots.s), and
// its inputs and outputs.
void shots_update(void);
extern uint8_t shot_parity, shot_draw_tile, shot_draw_prop, shots_killed;
extern uint8_t shot_vram, shot_frames[];

#endif
