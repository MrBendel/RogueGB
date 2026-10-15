#pragma bank 255
// The dialogue box (dialog.h). Banked: it runs only while a conversation is
// open, and then the world is paused, so a letter's few hundred cycles are free.
#include <gb/gb.h>
#include <string.h>
#include "dialog.h"
#include "dialogue.h"
#include "text.h"
#include "gfx.h"
#include "hud.h"
#include "input.h"
#include "room.h"
#include "sfx.h"
#include "frame.h"

#define DLG_TILES 192            // second VRAM bank
#define T_LINE0 DLG_TILES        // 18 tiles each
#define T_LINE1 (DLG_TILES + 18)
#define T_NAME (DLG_TILES + 36)  // up to 8
#define T_ARROW (DLG_TILES + 44)  // the frame's bottom edge with the arrow on it ...
#define T_EDGE (DLG_TILES + 45)   // ... and without (a copy: the arrow's cell is in this bank)
#define LINE_W 18                // tiles, from column 1
#define PEN_X 2                  // the text's left margin in its first tile
#define ROW_NAME 1
#define ROW_LINE0 2
#define ROW_LINE1 4
#define ARROW_X 18
#define ARROW_Y 5
#define ATTR_TEXT (BG_PAL_UI | TEXT_BANK1)
#define ATTR_NAME (BG_PAL_HEALTH | TEXT_BANK1)  // palette 7 (the HUD's, hidden now): the speaker's color
#define NAME_PAL (BG_PAL_HEALTH << 2)

uint8_t dlg_active;
uint8_t dlg_page_i, dlg_page_end;
uint8_t dlg_pos, dlg_len;
uint8_t dlg_waiting;
uint8_t dlg_who = 0xFF;
uint8_t dlg_line;
uint8_t dlg_timer;
uint16_t dlg_pages_shown;
char dlg_buf[DLG_PAGE_MAX];
uint16_t dlg_saved_pal[4];  // palette 7 as it was (the HUD's red)

// The arrow that waits for a button: an ink triangle pointing down (rows 0-3).
static const uint8_t arrow_rows[4] = {0x7E, 0x3C, 0x18, 0x00};

static void upload(uint8_t first_tile, const uint8_t *data, uint8_t n) {
    VBK_REG = 1;
    set_bkg_data(first_tile, n, data);
    VBK_REG = 0;
}

static uint8_t line_tile(void) {
    return dlg_line ? T_LINE1 : T_LINE0;
}

static void read_page(void) {
    uint16_t at;
    banked_memcpy(&at, &dlg_page_at[dlg_page_i], 2, BANK(dialogue));
    banked_memcpy(&dlg_len, &dlg_page_len[dlg_page_i], 1, BANK(dialogue));
    banked_memcpy(dlg_buf, &dlg_text[at], dlg_len, BANK(dialogue));
}

static void show_speaker(uint8_t who) {
    char name[DLG_NAME_MAX + 1];
    uint16_t color;
    uint8_t sound;
    banked_memcpy(name, dlg_speaker_name[who], DLG_NAME_MAX + 1, BANK(dialogue));
    banked_memcpy(&color, &dlg_speaker_color[who], 2, BANK(dialogue));
    banked_memcpy(&sound, &dlg_speaker_sound[who], 1, BANK(dialogue));
    // Clear the last name (frame fill, the UI palette), then draw this one.
    fill_win_rect(1, ROW_NAME, LINE_W, 1, FRAME_TILES + 4);
    VBK_REG = 1;
    fill_win_rect(1, ROW_NAME, LINE_W, 1, BG_PAL_UI);
    VBK_REG = 0;
    text_on_window = 1;
    text_draw(FONT_COMPACT, 8 + PEN_X, ROW_NAME, name, T_NAME, ATTR_NAME);
    text_on_window = 0;
    pal_base_bg[NAME_PAL + 3] = color;
    gfx_set_fade(0);
    if (sound != 0xFF) sfx_play(sound);
    dlg_who = who;
}

static void start_page(void) {
    read_page();
    uint8_t who;
    banked_memcpy(&who, &dlg_page_who[dlg_page_i], 1, BANK(dialogue));
    if (who != dlg_who) show_speaker(who);
    // Blank both lines (their map cells stay put; only the tiles change).
    memset(text_buf, 0, LINE_W * 16);
    upload(T_LINE0, text_buf, LINE_W);
    upload(T_LINE1, text_buf, LINE_W);
    set_win_tile_xy(ARROW_X, ARROW_Y, T_EDGE);
    dlg_line = 0;
    text_line_start(FONT_ITALIC, PEN_X);
    dlg_pos = 0;
    dlg_timer = 0;
    dlg_waiting = 0;
    dlg_pages_shown++;
}

// The next letter into the line's buffer; returns 1 if it drew ink.
static uint8_t put_next(void) {
    char c = dlg_buf[dlg_pos++];
    if (c == '\n') {
        upload(line_tile(), text_buf, LINE_W);  // (already up, but cheap and certain)
        dlg_line = 1;
        text_line_start(FONT_ITALIC, PEN_X);
        return 0;
    }
    text_put(c);
    return text_dirty_n != 0;
}

void dlg_start(uint8_t script) BANKED {
    uint16_t first;
    uint8_t pages;
    banked_memcpy(&first, &dlg_script_first[script], 2, BANK(dialogue));
    banked_memcpy(&pages, &dlg_script_pages[script], 1, BANK(dialogue));
    dlg_page_i = (uint8_t)first;
    dlg_page_end = (uint8_t)first + pages;
    memcpy(dlg_saved_pal, &pal_base_bg[NAME_PAL], sizeof(dlg_saved_pal));
    // The box takes the window: the HUD's rows give way (as for the pause panel).
    hud_hide();
    win_frame(0, 0, SCREEN_TW, 6);
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, 6, BG_PAL_UI);
    VBK_REG = 0;
    uint8_t cells[LINE_W];
    for (uint8_t i = 0; i < LINE_W; i++) cells[i] = T_LINE0 + i;
    set_win_tiles(1, ROW_LINE0, LINE_W, 1, cells);
    for (uint8_t i = 0; i < LINE_W; i++) cells[i] = T_LINE1 + i;
    set_win_tiles(1, ROW_LINE1, LINE_W, 1, cells);
    memset(cells, ATTR_TEXT, LINE_W);
    VBK_REG = 1;
    set_win_tiles(1, ROW_LINE0, LINE_W, 1, cells);
    set_win_tiles(1, ROW_LINE1, LINE_W, 1, cells);
    set_win_tile_xy(ARROW_X, ARROW_Y, ATTR_TEXT);
    VBK_REG = 0;
    // The arrow's two looks: the frame's bottom edge, plain and with the arrow.
    uint8_t edge[32];
    banked_memcpy(edge, frame_tiles + 7 * 16, 16, BANK(frame));
    memcpy(edge + 16, edge, 16);
    for (uint8_t r = 0; r < 4; r++) edge[16 + (r << 1)] |= arrow_rows[r], edge[17 + (r << 1)] |= arrow_rows[r];
    upload(T_EDGE, edge, 1);
    upload(T_ARROW, edge + 16, 1);
    set_win_tile_xy(ARROW_X, ARROW_Y, T_EDGE);
    move_win(7, DLG_WY);
    gfx_sprite_split(DLG_WY);
    dlg_who = 0xFF;
    dlg_active = 1;
    start_page();
}

static void close(void) {
    memcpy(&pal_base_bg[NAME_PAL], dlg_saved_pal, sizeof(dlg_saved_pal));
    gfx_set_fade(0);
    hud_draw();
    move_win(7, VIEW_H);
    gfx_sprite_split(VIEW_H);
    dlg_active = 0;
}

void dlg_update(void) BANKED {
    if (dlg_waiting) {
        // The arrow blinks until a button turns the page.
        dlg_timer++;
        if ((dlg_timer & 15) == 0) set_win_tile_xy(ARROW_X, ARROW_Y, dlg_timer & 16 ? T_ARROW : T_EDGE);
        if (joy_pressed & (J_A | J_B)) {
            if (++dlg_page_i >= dlg_page_end) close();
            else start_page();
        }
        return;
    }
    if (joy_pressed & (J_A | J_B)) {
        // The rest of the page at once.
        while (dlg_pos < dlg_len) put_next();
        upload(line_tile(), text_buf, LINE_W);
    } else if (++dlg_timer >= DLG_FRAMES_PER_LETTER) {
        dlg_timer = 0;
        if (put_next()) {
            upload(line_tile() + text_dirty_first, text_buf + ((uint16_t)text_dirty_first << 4), text_dirty_n);
            if (dlg_pos & 1) sfx_play(SFX_TEXT);
        }
    }
    if (dlg_pos >= dlg_len) {
        dlg_waiting = 1;
        dlg_timer = 15;  // the arrow shows on the next frame
    }
}
