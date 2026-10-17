// The pause menu's pages, on the window (play.c opens it with START): your
// character (stats and loadout), your class's skill tree, the options, and in the
// lab and the yard the tuning panel (play.c draws that one). B turns the page.
// While it's open the window covers the screen, so background palettes 0-5 are
// borrowed for the menu's inks (put back on close), and the HUD's tiles in the
// second VRAM bank for its text (hud_init rebuilds them).
#ifndef MENU_H
#define MENU_H

#include <gb/gb.h>
#include <stdint.h>

#define PAGE_CHARACTER 0
#define PAGE_SKILLS 1
#define PAGE_OPTIONS 2
#define PAGE_TUNE 3  // only without a file (the lab, the yard)
#define PAGES 4
#define PAGE_SHOP 4  // Marro's shop (opened by talking to him in town; not one of B's pages)
#define SHOP_STOCK 3

// --- watchable state ---
extern uint8_t menu_page;
extern uint8_t menu_skill;  // the skill under the cursor (branch * 3 + tier)
extern uint8_t menu_option; // the option under the cursor
extern uint8_t shop_sel;    // the shop's gun under the cursor
extern uint8_t shop_sold;   // a bit per stock gun bought
extern uint16_t guns_bought;

void menu_open(void) BANKED;    // palettes; the page drawn (or the tuning page: play.c draws it)
void menu_close(void) BANKED;   // palettes back (the caller rebuilds the HUD)
void menu_draw(void) BANKED;    // the current page (not the tuning page)
void menu_tabs(void) BANKED;    // just the page tabs (the tuning page draws the rest)
void menu_update(void) BANKED;  // the skills and options pages' cursors
void menu_next_page(void) BANKED;  // B: the next page (the tuning page only without a file)
void shop_restock(void) BANKED;    // new guns at the shop (each time you come into town)

#endif
