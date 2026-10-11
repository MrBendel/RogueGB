// State for the current run: reset on each start from the title screen,
// shown on the run-over screen. Meta-progression (between runs) goes elsewhere.
#ifndef RUN_H
#define RUN_H

#include <stdint.h>

extern uint16_t run_seed;     // RNG seed for this run (from title-screen timing)
extern uint16_t run_seconds;  // play time, excluding pause
extern uint8_t run_ticks;     // frames into the current second
extern uint16_t bullets_fired; // shots fired this run

void run_start(uint16_t seed);

#endif
