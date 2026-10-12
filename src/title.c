#pragma bank 255
// Title screen: logo, blinking "PRESS START". START (or A) begins a run.
#include <gb/gb.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"

#define BLINK_MASK 0x20  // "PRESS START" toggles every 32 frames
#define PROMPT_Y 12

uint8_t title_timer;

void title_enter(void) BANKED {
    title_timer = 0;
    palettes_menu();
    bkg_frame(2, 2, 16, 7);
    bkg_text(6, 4, "ROGUE GB");
    bkg_text(5, 6, "RUN.GUN.DIE");
    bkg_text(5, PROMPT_Y, "PRESS START");
    bkg_text(1, 16, "V0.1");
}

void title_update(void) BANKED {
    title_timer++;
    if ((title_timer & (BLINK_MASK - 1)) == 0) {
        bkg_text(5, PROMPT_Y, (title_timer & BLINK_MASK) ? "           " : "PRESS START");
    }
    if (joy_pressed & (J_START | J_A)) {
        // The frame the player presses START is effectively random, so it
        // seeds the run. Replays press on the same frame and get the same run.
        run_start(((uint16_t)DIV_REG << 8) ^ frame_count);
        state_change(STATE_PLAY);
    }
}
