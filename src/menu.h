// The pause menu's pages, on the window (play.c opens it with START): your
// profile, your class's skill tree, and the tuning panel (play.c draws that one).
// B turns the page. While it's open the window covers the screen, so background
// palettes 1-3 are borrowed for the menu's ink (put back on close), and the HUD's
// tiles in the second VRAM bank are borrowed for its text (hud_init rebuilds them).
#ifndef MENU_H
#define MENU_H

#include <gb/gb.h>
#include <stdint.h>

#define PAGE_PROFILE 0
#define PAGE_SKILLS 1
#define PAGE_TUNE 2
#define PAGES 3

// --- watchable state ---
extern uint8_t menu_page;
extern uint8_t menu_skill;  // the skill under the cursor (branch * 3 + tier)

void menu_open(void) BANKED;    // palettes; the page drawn (or the tuning page: play.c draws it)
void menu_close(void) BANKED;   // palettes back (the caller rebuilds the HUD)
void menu_draw(void) BANKED;    // the current page (not the tuning page)
void menu_tabs(void) BANKED;    // just the page tabs (the tuning page draws the rest)
void menu_update(void) BANKED;  // the skills page's cursor and A

#endif
