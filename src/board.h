// The contracts board (board.c): the pause menu's page for missions (mission.h),
// opened by its terminal in town. Up: the offers; down: the jobs you carry.
#ifndef BOARD_H
#define BOARD_H

#include <gb/gb.h>
#include <stdint.h>

// --- watchable state ---
extern uint8_t board_sel;  // the cursor: offers 0-3, then your jobs 4-6

void board_draw(void) BANKED;    // the page (menu_draw: the panel cleared)
void board_update(void) BANKED;  // UP/DOWN, A (take, turn in, pay), SELECT (drop a job)

#endif
