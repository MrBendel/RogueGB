// Elites (elite.c): one in some camps, Borderlands' "badass". Twice the health,
// one modifier, drawn in the hot colours of the enemies' bullets (world.c), and a gun when
// it falls. docs/design/runs.md, "Enemies: a roster by role".
#ifndef ELITE_H
#define ELITE_H

#include <gb/gb.h>
#include <stdint.h>

// actor_elite (actor.h): its modifier, or none.
#define ELITE_NONE 0
#define ELITE_SWIFT 1   // thinks twice a step: everything it does, twice as fast
#define ELITE_BOMBER 2  // bursts into a ring of 8 peas when it falls
#define ELITE_SHIELDED 3  // its top half of health recharges (shimmering), until shot to half
#define ELITE_ACID 4    // leaves a trail of acid puddles: they hurt to stand in (jump them)
#define ELITE_KINDS 4
#define ELITE_BROKEN 5  // a shielded one whose shield broke: no modifier left

// Acid puddles: sprite tiles 252-255, the last of the shared background tiles (a
// scene's tileset stops short of them: tools/tileset.mjs MAX_TILES).
#define ACID_VRAM 252
#define ACID_MAX 4
extern uint8_t acid_puddles;
extern uint8_t elite_living;  // elites up in this room (elite_make; elite_fell, elite_clear)
extern char elite_name[8];    // the last one's name, 7 letters  // how many are down (play draws them while any are)

void elite_make(uint8_t i) BANKED;  // actor i becomes one (a random modifier); its name on the HUD
void elite_shield(uint8_t i) BANKED;  // a shielded one's think step (actor.c)
void elite_drip(uint8_t i) BANKED;    // an acid one's think step (actor.c)
void elite_init(void) BANKED;         // the puddles' tiles; none down (actors_init)
void elite_clear(void) BANKED;        // no puddles (a room change)
uint8_t elite_puddles(uint8_t oam) BANKED;  // age, draw and touch them (play.c); the next free sprite
void elite_fell(uint8_t i) BANKED;  // it's down (actor.c reap, before the slot is freed)

#endif
