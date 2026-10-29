// The hound: the descent's flanker, an actor kind like the slime (hound.c).
#ifndef HOUND_H
#define HOUND_H

#include <gb/gb.h>
#include <stdint.h>

#define HOUND_FRAMES 4  // stride A and B, facing right then left
#define HOUND_HP 5
extern uint8_t hound_frames[HOUND_FRAMES * 4];

void hound_init(uint8_t v) BANKED;       // its tiles at sprite tile v (8), and its frames
void hound_update(uint8_t i) BANKED;     // a think step (actors_update)

#endif
