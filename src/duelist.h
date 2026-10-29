// The duelist: the descent's merc, an actor kind like the hound (duelist.c).
#ifndef DUELIST_H
#define DUELIST_H

#include <gb/gb.h>
#include <stdint.h>

#define DUELIST_FRAMES 4  // standing and a step, facing right then left
#define DUELIST_HP 6
extern uint8_t duelist_frames[DUELIST_FRAMES * 4];

void duelist_init(uint8_t v, uint8_t v2) BANKED;  // its tiles at v (6) and v2 (2), and its frames
void duelist_update(uint8_t i) BANKED;            // a think step (actors_update)

#endif
