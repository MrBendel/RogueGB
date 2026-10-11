// RogueGB entry point: boot, then the main loop.
//
// The loop runs exactly one game_state update per frame. Scene changes are
// requested with state_change() and performed here between frames: fade out,
// display off, clear, enter() the new scene, display on, fade in.
//
// Globals are deliberately non-static so the test harness and the playtest
// page can watch them by name (via build/roguegb.noi).
#include <gb/gb.h>
#include "state.h"
#include "input.h"
#include "gfx.h"

#define STATE_NONE 0xFF

uint8_t game_state = STATE_NONE;
uint8_t next_state = STATE_TITLE;
uint16_t frame_count;
uint8_t in_transition;  // 1 while fading between scenes

void state_change(uint8_t state) {
    next_state = state;
}

void wait_frame(void) {
    frame_count++;
    vsync();
}

static void enter_next_state(void) {
    in_transition = 1;
    if (game_state != STATE_NONE) fade_out();
    DISPLAY_OFF;
    gfx_clear();
    game_state = next_state;
    switch (game_state) {
        case STATE_TITLE: title_enter(); break;
        case STATE_PLAY: play_enter(); break;
        case STATE_RUN_OVER: run_over_enter(); break;
    }
    DISPLAY_ON;
    fade_in();
    // Re-sample so a button held through the transition (e.g. the START that
    // caused it) doesn't count as a fresh press in the new scene.
    input_update();
    in_transition = 0;
}

void main(void) {
    DISPLAY_OFF;
    gfx_init();
    BGP_REG = OBP0_REG = OBP1_REG = 0x00;  // start faded out; first enter fades in
    SPRITES_8x8;
    SHOW_BKG;
    SHOW_SPRITES;

    while (1) {
        input_update();
        if (next_state != game_state) enter_next_state();

        switch (game_state) {
            case STATE_TITLE: title_update(); break;
            case STATE_PLAY: play_update(); break;
            case STATE_RUN_OVER: run_over_update(); break;
        }
        wait_frame();
    }
}
