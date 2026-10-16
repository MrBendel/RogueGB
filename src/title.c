#pragma bank 255
// Title screen: logo, blinking "PRESS START". START (or A) opens the files: three
// save slots (save.h), each a character; picking one continues its expedition or
// starts one. SELECT opens the test lab and B the ice yard (neither saves).
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "chunks.h"
#include "text.h"
#include "save.h"

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)
#define FILE_ROW0 10     // the files' tile rows: 10, 12, 14
#define CURSOR_X 8       // px

uint8_t title_timer;
uint8_t title_files;   // the files are showing
uint8_t title_choice;  // the file under the cursor
uint8_t title_cursor_tile;
// Tests set this (a memory write on the title) to start runs on a known seed.
uint16_t test_seed;
uint8_t title_next_tile;  // the first tile the logo and prompt left free

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
    title_files = 0;
    palettes_title();
    // Every cell on the text palette first (a scene before may have left others).
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 4, "SKAVENGAR", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, TEXT_CENTER, 7, "RUN.  GUN.  LOOT.", t, TITLE_PAL_TEXT);
    title_next_tile = t;
    t += draw(FONT_COMPACT, TEXT_CENTER, 11, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 14, "SELECT: test lab    B: ice yard", t, TITLE_PAL_HINT);
    draw(FONT_COMPACT, 4, 16, "v0.1", t, TITLE_PAL_HINT);
}

// The prompt blinks by its palette: ink, or the background's color.
static void prompt_ink(uint8_t on) {
    pal_base_bg[(TITLE_PAL_PROMPT << 2) + 3] = pal_base_bg[(TITLE_PAL_TEXT << 2) + (on ? 3 : 0)];
    gfx_set_fade(0);
}

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, uint16_t n, uint8_t digits) {
    char d[5];
    uint8_t k = 0;
    do {
        d[k++] = '0' + n % 10;
        n /= 10;
    } while (n || k < digits);
    while (k) *p++ = d[--k];
    return p;
}

// The files, in place of the prompt (the display is on: a few hundred bytes of
// direct VRAM writes, once).
static void show_files(void) {
    title_files = 1;
    title_choice = 0;
    save_scan();
    fill_bkg_rect(0, 9, SCREEN_TW, 9, 0);
    uint8_t t = title_next_tile;
    char line[TEXT_MAX + 1];
    for (uint8_t s = 0; s < SAVE_SLOTS; s++) {
        char *p = put(line, "FILE ");
        *p++ = '1' + s;
        if (slot_used[s]) {
            p = put(p, "    LV ");
            p = put_num(p, slot_level[s], 1);
            p = put(p, "    DEPTH ");
            p = put_num(p, slot_depth[s], 1);
            p = put(p, "    ");
            p = put_num(p, slot_minutes[s] / 60, 1);
            *p++ = ':';
            p = put_num(p, slot_minutes[s] % 60, 2);
        } else {
            p = put(p, "    new");
        }
        *p = 0;
        t += text_draw(FONT_COMPACT, 20, FILE_ROW0 + (s << 1), line, t, TITLE_PAL_TEXT);
    }
    t += draw(FONT_COMPACT, TEXT_CENTER, 17, "A: PLAY     B: BACK", t, TITLE_PAL_HINT);
    title_cursor_tile = t;
    draw(FONT_ITALIC, CURSOR_X, FILE_ROW0, ">", t, TITLE_PAL_TEXT);
}

static void move_cursor(uint8_t to) {
    set_bkg_tile_xy(CURSOR_X >> 3, FILE_ROW0 + (title_choice << 1), 0);
    title_choice = to;
    set_bkg_tile_xy(CURSOR_X >> 3, FILE_ROW0 + (title_choice << 1), title_cursor_tile);
    VBK_REG = 1;
    set_bkg_tile_xy(CURSOR_X >> 3, FILE_ROW0 + (title_choice << 1), TITLE_PAL_TEXT);
    VBK_REG = 0;
}

static void begin(uint8_t mode) {
    play_mode = mode;
    sfx_play(SFX_START);
    // The frame the player presses the button is effectively random, so it
    // seeds the run. Replays press on the same frame and get the same run.
    uint16_t seed = ((uint16_t)DIV_REG << 8) ^ frame_count, tool;
    // The chunk editor can patch a seed into the ROM instead ("play this seed").
    banked_memcpy(&tool, &tool_seed, 2, BANK(chunks));
    run_start(tool ? tool : test_seed ? test_seed : seed);
    state_change(STATE_PLAY);
}

void title_update(void) BANKED {
    title_timer++;
    if (title_files) {
        if ((joy_pressed & J_UP) && title_choice > 0) move_cursor(title_choice - 1);
        else if ((joy_pressed & J_DOWN) && title_choice < SAVE_SLOTS - 1) move_cursor(title_choice + 1);
        else if (joy_pressed & J_B) state_change(STATE_TITLE);
        else if (joy_pressed & (J_A | J_START)) {
            save_open(title_choice);
            begin(MODE_DESCENT);
        }
        return;
    }
    if ((title_timer & (BLINK_MASK - 1)) == 0) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & (J_START | J_A)) {
        sfx_play(SFX_START);
        show_files();
    } else if (joy_pressed & (J_SELECT | J_B)) {
        save_none();
        begin(joy_pressed & J_SELECT ? MODE_LAB : MODE_YARD);
    }
}
