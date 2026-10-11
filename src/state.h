// Game state machine. Each state is a scene with an enter() (runs once with
// the display faded out, loads tiles/maps) and an update() (runs once per
// frame). main.c owns the loop and performs transitions between frames.
#ifndef STATE_H
#define STATE_H

#include <stdint.h>

#define STATE_TITLE    0
#define STATE_PLAY     1
#define STATE_RUN_OVER 2

// Watchable: the state currently running, and the one requested for next frame.
extern uint8_t game_state;
extern uint8_t next_state;

// Counts every main-loop iteration (one per frame, including transitions).
extern uint16_t frame_count;

// Request a transition. It happens at the start of the next frame, with a fade.
void state_change(uint8_t state);

// End the frame: count it and wait for vblank. Anything that spends frames
// outside the main loop (fades, etc.) should call this instead of vsync().
void wait_frame(void);

void title_enter(void);
void title_update(void);
void play_enter(void);
void play_update(void);
void run_over_enter(void);
void run_over_update(void);

#endif
