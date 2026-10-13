// Generated expedition layers (M2, docs/design/level-generation.md).
//
// A layer is a LAYER_W x LAYER_H grid of room slots. level_generate lays out a
// main path from the start (top row) down to the exit (bottom row), so the exit
// is always reachable, then adds branches until the layer has 8-12 rooms. Each
// room keeps its type, size (in chunks), doors and a seed; level_build_room
// assembles it into the room map from chunks (content/chunks), the same way
// every time it's entered. Everything follows from the seed.
#ifndef LEVEL_H
#define LEVEL_H

#include <gb/gb.h>
#include <stdint.h>

#define LAYER_W 4
#define LAYER_H 4
#define LAYER_CELLS (LAYER_W * LAYER_H)

// Room types (0: no room in that slot).
#define ROOM_NONE 0
#define ROOM_START 1
#define ROOM_COMBAT 2
#define ROOM_CONNECTOR 3
#define ROOM_CACHE 4
#define ROOM_EXIT 5

// Doors, a bit per side.
#define DOOR_N 1
#define DOOR_E 2
#define DOOR_S 4
#define DOOR_W 8

#define LEVEL_SLOTS 8  // enemy slots kept per room
#define LEVEL_ITEMS 4  // item slots

// --- watchable state ---
extern uint8_t layer_type[LAYER_CELLS];
extern uint8_t layer_doors[LAYER_CELLS];
extern uint8_t layer_cw[LAYER_CELLS], layer_ch[LAYER_CELLS];  // size in chunks
extern uint8_t layer_cleared[LAYER_CELLS];
extern uint8_t layer_start, layer_exit, layer_rooms;
extern uint8_t level_room;  // the room the player is in
// The built room's slots (room pixels of each 16x16 block's top-left).
extern uint8_t level_slots, level_items;
extern uint16_t level_slot_x[LEVEL_SLOTS], level_slot_y[LEVEL_SLOTS];
extern uint16_t level_item_x[LEVEL_ITEMS], level_item_y[LEVEL_ITEMS];
extern uint8_t level_carved;  // the last build had to carve a path between doors

void level_generate(uint16_t seed) BANKED;
// Build room i into the room map (and set room_w/room_h).
void level_build_room(uint8_t i) BANKED;

#endif
