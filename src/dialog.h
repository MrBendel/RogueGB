// The dialogue box: scripts from content/dialogue (tools/dialogue.mjs) shown a
// page at a time in a box across the top of the screen (as in Star Ocean: Blue
// Sphere), the speaker's portrait at its left, their name and the text beside
// it, typed out letter by letter in the italic font. A or B finishes the page at
// once, then turns it (a blinking arrow waits). The world stands still while
// it's open (play_update returns early).
//
// Layout (the window, 20 x 6 tiles from the top; the HUD's rows give way, and a
// split shows the window only down to the box and sprites only below it):
//   row 0    frame
//   rows 1-4 the portrait (columns 1-4); beside it the name (row 1), the text
//            (rows 2 and 4); with no portrait, the text from column 1
//   row 5    frame, the arrow at column 18
// Tiles: the Color's second VRAM bank, background tiles 192-253 (the HUD has
// 0-127 there; sprite tiles 128-191 are the weapons').
#ifndef DIALOG_H
#define DIALOG_H

#include <gb/gb.h>
#include <stdint.h>

#define DLG_ROWS 6
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
