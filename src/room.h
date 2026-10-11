// The current room: a grid of 16x16 blocks, collision queries against it, and
// drawing it into the background map as the camera scrolls.
//
// Rooms can be larger than the screen. The hardware background map is only
// 32x32 tiles (16x16 blocks), so room_draw_* write just the tiles the camera can
// see; camera.c streams new columns and rows in as it moves.
// For now there is one hand-written test room; M2 replaces this with authored rooms.
#ifndef ROOM_H_INCLUDED
#define ROOM_H_INCLUDED

#include <stdint.h>

#define BLOCK_SIZE 16
#define SCREEN_BLOCKS_W 10  // one screen is 10x8 blocks above the HUD row
#define SCREEN_BLOCKS_H 8
#define VIEW_W 160          // visible playfield in pixels
#define VIEW_H 128
#define ROOM_MAX_W 32       // blocks; the map's row stride, so indexing is a shift
#define ROOM_MAX_H 32

#define BLK_FLOOR 0
#define BLK_SOLID 1

extern uint8_t room_w;  // size in blocks
extern uint8_t room_h;
extern uint16_t room_px_w;  // size in pixels
extern uint16_t room_px_h;
extern uint8_t room_map[ROOM_MAX_W * ROOM_MAX_H];  // row-major, stride ROOM_MAX_W

// Load the movement-lab room (2x2 screens) and its tiles.
void room_load_lab(void);

// Is the pixel (x, y) inside a solid block? Anything outside the room counts as
// solid, including coordinates that wrapped below zero.
uint8_t room_solid_at(uint16_t x, uint16_t y);

// Background tile for room tile coordinates (8x8 tiles; 2x2 per block).
uint8_t room_tile_at(uint8_t tx, uint8_t ty);
// Draw a column of `h` tiles / a row of `w` tiles into the background map,
// wrapping at 32 like the hardware does.
void room_draw_column(uint8_t tx, uint8_t ty, uint8_t h);
void room_draw_row(uint8_t tx, uint8_t ty, uint8_t w);

#endif
