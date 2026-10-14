// State for the current run: reset on each start from the title screen,
// shown on the run-over screen. Meta-progression (between runs) goes elsewhere.
#ifndef RUN_H
#define RUN_H

#include <stdint.h>

extern uint16_t run_seed;     // RNG seed for this run (from title-screen timing)
extern uint16_t run_seconds;  // play time, excluding pause
extern uint8_t run_ticks;     // frames into the current second
// What START on the title begins: a generated descent, or (SELECT) the test lab.
#define MODE_LAB 0
#define MODE_DESCENT 1
#define MODE_MINIBOSS 2
#define MODE_BOSS 3
#define MODE_ENEMY 4
#define MODE_GUNSHIP 5
extern uint8_t play_mode;
extern uint8_t enemy_trial_kind;

#include <gb/gb.h>
void run_start(uint16_t seed) BANKED;  // in play.c (banked)

#endif
