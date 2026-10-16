// The arena's chest (issue #10): it holds the first shield. Locked until the
// arena's wave is cleared, then it flashes; walking into it opens it, the shield
// pops out and lands in front, and walking over it equips it (the bar fills up
// from empty). A placeholder for the loot chests of M3 (item card, fountain).
#ifndef CHEST_H
#define CHEST_H

#define CHEST_VRAM 96  // sprite tiles (bank 0): the chest's art, loaded every scene (chest_init)

#include <gb/gb.h>
#include <stdint.h>

#define CHEST_LOCKED 0  // the wave isn't cleared yet
#define CHEST_READY 1   // flashing: walk into it
#define CHEST_OPEN 2    // the shield is out (flying, then on the floor)
#define CHEST_TAKEN 3   // the shield is equipped

// Where it stands (blocks): the arena's bottom-right, a row clear of the wall so
// the shield can land in front (room_lab.c marks the block).
#define CHEST_BX 17
#define CHEST_BY 21

// --- watchable state ---
extern uint8_t chest_state;
extern uint16_t item_x, item_y;  // the shield's 16x16 frame, room pixels
extern int16_t item_z;           // its height while it flies (px 8.8)
extern uint8_t item_landed;
extern uint8_t chest_frames[];  // the chest's frames (closed at 0, open at 4): pickups.c draws the descent's

void chest_init(void) BANKED;    // once per play scene (tiles, state)
void chest_update(void) BANKED;  // per frame, after the camera moved
void chest_hide(void) BANKED;

#endif
