// The pause menu's pages, on the window (play.c opens it with START): your
// character (stats and loadout), your gear (loadout and backpack), your class's skill tree, the options, and in the
// lab and the yard the tuning panel (play.c draws that one). B turns the page.
// While it's open the window covers the screen, so background palettes 0-5 are
// borrowed for the menu's inks (put back on close), and the HUD's tiles in the
// second VRAM bank for its text (hud_init rebuilds them).
#ifndef MENU_H
#define MENU_H

#include <gb/gb.h>
#include <stdint.h>

#define PAGE_CHARACTER 0
#define PAGE_GEAR 1  // the loadout and the backpack: A picks a gun up, A on another slot swaps them
#define PAGE_SKILLS 2
#define PAGE_OPTIONS 3
#define PAGE_TUNE 4  // only without a file (the lab, the yard)
#define PAGES 5
#define PAGE_SHOP 5  // Marro's shop (opened by talking to him in town; not one of the pages)
#define PAGE_BOARD 6  // the contracts board (talking to its terminal in town; board.c)
#define SHOP_STOCK 3  // guns; then the backpack upgrade, while there is one

// --- watchable state ---
extern uint8_t menu_page;
extern uint8_t menu_skill;  // the skill under the cursor (branch * 3 + tier)
extern uint8_t menu_option; // the option under the cursor
extern uint8_t shop_sel;    // the shop's gun under the cursor
extern uint8_t shop_sold;   // a bit per stock gun bought
extern uint16_t guns_bought;
extern uint8_t shop_mode;   // SHOP_BUY or SHOP_SELL (LEFT/RIGHT)
#define SHOP_BUY 0
#define SHOP_SELL 1
extern uint8_t gear_sel;    // the gear page's cursor: loadout slots 0-3, then the backpack's
extern uint8_t gear_pick;   // the slot picked up to move (GEAR_NONE: none)
#define GEAR_NONE 0xFF

void menu_open(void) BANKED;    // palettes; the page drawn (or the tuning page: play.c draws it)
void menu_close(void) BANKED;   // palettes back (the caller rebuilds the HUD)
void menu_draw(void) BANKED;    // the current page (not the tuning page)
void menu_tabs(void) BANKED;    // just the page tabs (the tuning page draws the rest)
void menu_update(void) BANKED;  // the skills and options pages' cursors
uint8_t menu_turn(void) BANKED;  // LEFT/RIGHT: the page before or after (tuning only without a file); 1 if turned
void shop_restock(void) BANKED;
// For the board's page (board.c): text from RAM at pixel x, row ty, in ink pal, into
// tiles `at` (0xFF: the next free); a row cleared; the cursor arrow, and moved.
uint8_t menu_say(uint8_t x, uint8_t ty, const char *s, uint8_t pal, uint8_t at) BANKED;
uint8_t menu_alloc(uint8_t n) BANKED;  // n text tiles from the pool
void menu_clear_row(uint8_t row) BANKED;
uint8_t menu_arrow_at(uint8_t ty) BANKED;
void menu_move_arrow(uint8_t marker, uint8_t from, uint8_t to) BANKED;
#define MENU_INK 6  // menu.c's inks, for the board
#define MENU_DIM 1
#define MENU_LIT 2
#define MENU_SEL 3    // new guns at the shop (each time you come into town)

#endif
