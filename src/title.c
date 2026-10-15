#pragma bank 255
// Title screen: logo, blinking "PRESS START". START (or A) begins a run.
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "chunks.h"
#include "text.h"

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)

uint8_t title_timer;
// Tests set this (a memory write on the title) to start runs on a known seed.
uint16_t test_seed;

// Text from this bank's constants (text.h: copied to RAM for text.c).
static uint8_t draw(uint8_t font, uint8_t x, uint8_t ty, const char *s, uint8_t tile, uint8_t pal) {
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    while (s[n] && n < TEXT_MAX) line[n] = s[n], n++;  // (no strncpy: it would land in bank 0)
    line[n] = 0;
    return text_draw(font, x, ty, line, tile, pal);
}

void title_enter(void) BANKED {
    title_timer = 0;
    palettes_title();
    // Every cell on the text palette first (a scene before may have left others).
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 4, "SKAVENGAR", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, TEXT_CENTER, 7, "RUN.  GUN.  LOOT.", t, TITLE_PAL_TEXT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 11, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 14, "SELECT: test lab    B: ice yard", t, TITLE_PAL_HINT);
    draw(FONT_COMPACT, 4, 16, "v0.1", t, TITLE_PAL_HINT);
}

// The prompt blinks by its palette: ink, or the background's color.
static void prompt_ink(uint8_t on) {
    pal_base_bg[(TITLE_PAL_PROMPT << 2) + 3] = pal_base_bg[(TITLE_PAL_TEXT << 2) + (on ? 3 : 0)];
    gfx_set_fade(0);
}

void title_update(void) BANKED {
    title_timer++;
    if ((title_timer & (BLINK_MASK - 1)) == 0) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & (J_START | J_A | J_SELECT | J_B)) {
        play_mode = joy_pressed & J_SELECT ? MODE_LAB : joy_pressed & J_B ? MODE_YARD : MODE_DESCENT;
        sfx_play(SFX_START);
        // The frame the player presses START is effectively random, so it
        // seeds the run. Replays press on the same frame and get the same run.
        uint16_t seed = ((uint16_t)DIV_REG << 8) ^ frame_count, tool;
        // The chunk editor can patch a seed into the ROM instead ("play this seed").
        banked_memcpy(&tool, &tool_seed, 2, BANK(chunks));
        run_start(tool ? tool : test_seed ? test_seed : seed);
        state_change(STATE_PLAY);
    }
}
