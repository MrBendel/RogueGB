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
#define ELITE_KINDS 2

void elite_make(uint8_t i) BANKED;  // actor i becomes one (a random modifier); its name on the HUD
void elite_fell(uint8_t i) BANKED;  // it's down (actor.c reap, before the slot is freed)

#endif
