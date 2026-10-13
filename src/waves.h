// The arena's waves: see waves.c.
#ifndef WAVES_H
#define WAVES_H

#include <gb/gb.h>
#include <stdint.h>

// The arena is the screen below the lab (rows 16-22).
#define ARENA_TOP (16 * 16)
#define WAVE_BREAK 240  // frames between clearing a wave and the next (4 s)

// --- watchable state ---
extern uint8_t wave_spawned;   // a wave has ever spawned
extern uint8_t wave_num;       // the wave in progress or last spawned (0: none yet)
extern uint8_t waves_cleared;
extern uint16_t wave_break;    // frames left before the next wave
extern uint8_t wave_live;      // the current wave isn't cleared yet

void waves_init(void) BANKED;    // a new run
void waves_update(void) BANKED;  // per frame in play

#endif
