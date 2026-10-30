// The loader: the descent's tank, an actor kind like the duelist (tank.c).
#ifndef TANK_H
#define TANK_H

#include <gb/gb.h>
#include <stdint.h>

#define TANK_FRAMES 4  // treads still and rolling, facing right then left
#define TANK_HP 14
extern uint8_t tank_frames[TANK_FRAMES * 4];

void tank_init(uint8_t v) BANKED;    // its 8 tiles at v, and its frames
void tank_update(uint8_t i) BANKED;  // a think step (actors_update)

#endif
