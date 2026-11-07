// What level.c and level_indoor.c share while a map is built.
#ifndef LEVEL_INTERNAL_H
#define LEVEL_INTERNAL_H

#include <gb/gb.h>
#include <stdint.h>
#include "room.h"

#define MAP(x, y) room_map[((uint16_t)(y) << 5) | (x)]

// The generator's random numbers (xorshift on level_rng: level.c).
extern uint16_t level_rng;
// Points every build must connect: openings, camps, chests, the middle (blocks).
#define MUST_MAX 16
extern uint8_t must_x[MUST_MAX], must_y[MUST_MAX], musts;
// Everything that must connect still does (a flood over floor from the first point).
uint8_t level_still_connected(void) BANKED;
// Indoors: halls and buildings (level_indoor.c), from the links' openings.
void level_build_indoor(uint8_t links) BANKED;

#endif
