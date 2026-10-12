#pragma bank 255
// Run-over screen: stats for the run that just ended. START returns to the title.
// This is where meta-progression (spending gold, picking an heir) will hook in.
#include <gb/gb.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"

// Ignore input briefly so a held or mashed button doesn't skip the screen.
#define INPUT_DELAY 30

uint8_t run_over_timer;

void run_over_enter(void) BANKED {
    run_over_timer = 0;
    palettes_menu();
    bkg_frame(2, 3, 16, 9);
    bkg_text(6, 4, "RUN OVER");
    bkg_text(4, 7, "TIME");
    bkg_number(11, 7, run_seconds / 60, 2, 1);
    bkg_text(13, 7, ":");
    bkg_number(14, 7, run_seconds % 60, 2, 1);
    bkg_text(5, 14, "PRESS START");
}

void run_over_update(void) BANKED {
    if (run_over_timer < INPUT_DELAY) {
        run_over_timer++;
        return;
    }
    if (joy_pressed & (J_START | J_A)) state_change(STATE_TITLE);
}
