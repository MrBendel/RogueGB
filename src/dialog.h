// The dialogue box: scripts from content/dialogue (tools/dialogue.mjs) shown a
// page at a time in a box over the bottom of the screen, typed out letter by
// letter in the italic font, the speaker's name above in their color. A or B
// finishes the page at once, then turns it (a blinking arrow waits). The world
// stands still while it's open (play_update returns early).
//
// Layout (the window, 20 x 6 tiles at y = DLG_WY; the HUD's rows give way):
//   row 0  frame          row 3  (between lines)
//   row 1  name           row 4  text line 2
//   row 2  text line 1    row 5  frame, the arrow at column 18
// Tiles: the Color's second VRAM bank, background tiles DLG_TILES.. (the HUD has
// 0-127 there; sprite tiles 128-191 are the weapons').
#ifndef DIALOG_H
#define DIALOG_H

#include <gb/gb.h>
#include <stdint.h>

#define DLG_WY (144 - 6 * 8)
#define DLG_PAGE_MAX 72  // bytes in a page (tools/dialogue.mjs checks)
#define DLG_FRAMES_PER_LETTER 2

// --- watchable state ---
extern uint8_t dlg_active;    // a conversation is open
extern uint8_t dlg_page_i;    // the page showing (an index into all pages)
extern uint8_t dlg_pos;       // letters of it out so far
extern uint8_t dlg_len;
extern uint8_t dlg_waiting;   // all out: waiting for A or B
extern uint8_t dlg_who;       // the speaker
extern uint16_t dlg_pages_shown;

void dlg_start(uint8_t script) BANKED;
void dlg_update(void) BANKED;  // per frame while dlg_active

#endif
