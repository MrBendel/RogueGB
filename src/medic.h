// The medic drone: the descent's support, an actor kind like the loader (medic.c).
#ifndef MEDIC_H
#define MEDIC_H

#include <gb/gb.h>
#include <stdint.h>

#define MEDIC_FRAMES 2  // hovering: up, then down a pixel (it's symmetrical: no facing)
#define MEDIC_HP 5
extern uint8_t medic_frames[MEDIC_FRAMES * 4];

void medic_init(uint8_t v) BANKED;    // its 8 tiles at v, and its frames
void medic_update(uint8_t i) BANKED;  // a think step (actors_update)

#endif
