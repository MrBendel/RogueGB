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

// Block types.
#define BLK_FLOOR 0
#define BLK_SOLID 1   // wall: blocks everything
#define BLK_PIT 2     // hole: walkable, but standing over it (on the floor) makes you fall
#define BLK_RAISED 3  // platform RAISED_PX high: a wall from the floor, ground once you're on it
#define RAISED_PX 8
#define BLK_PROP 4    // under a prop (the chest): solid to the player, drawn as floor;
                      // shots, bullets and line of sight pass (they test SOLID/RAISED)
#define BLK_STAIRS 5 // traversable steps in authored terrace arenas
#define BLK_DECK 6   // traversable upper terrace surface
#define BLK_TYPES 7

extern uint8_t room_w;  // size in blocks
extern uint8_t room_h;
extern uint16_t room_px_w;  // size in pixels
extern uint16_t room_px_h;
// The block map lives at a fixed, 256-aligned address in WRAM, so a block's address
// is just ROOM_MAP_ADDR | (by << 5) | bx (no tables); room_fill.s relies on it.
// The stack grows down from 0xDFFF, far above it.
#define ROOM_MAP_ADDR 0xD000
// Tile for each (block type << 2 | quadrant), copied to RAM next to the map so the
// fill loops can index it with one byte.
#define ROOM_TILE_LUT_ADDR 0xD400
extern uint8_t __at(ROOM_MAP_ADDR) room_map[ROOM_MAX_W * ROOM_MAX_H];  // row-major, stride ROOM_MAX_W
extern uint8_t __at(ROOM_TILE_LUT_ADDR) room_tile_lut[BLK_TYPES * 4];  // filled by the room loader

// Load the movement-lab room (2x2 screens) and its tiles.
#include <gb/gb.h>
void room_load_lab(void) BANKED;  // room_lab.c (banked)
// Just the tiles and the block -> tile table (generated rooms use the same set).
void room_load_tiles(void) BANKED;

// Block at a pixel known to be inside the room (no bounds check), for hot loops.
#define ROOM_BLOCK_IN(x, y) \
    (*(const uint8_t *)(ROOM_MAP_ADDR | ((uint16_t)((uint8_t)((y) >> 4)) << 5) | (uint8_t)((x) >> 4)))

// Block type at the pixel (x, y). Anything outside the room counts as solid,
// including coordinates that wrapped below zero.
uint8_t room_block_at(uint16_t x, uint16_t y);
// Line of sight (room_fill.s): set the start (12.4 fixed point), the step to add
// before each sample and the number of samples (1+), then call. 1 if no wall or
// raised block is hit.
extern uint16_t line_fx, line_fy;
extern int16_t line_sx, line_sy;
extern uint8_t line_n;
uint8_t room_line_clear(void);

// Background tile for room tile coordinates (8x8 tiles; 2x2 per block).
uint8_t room_tile_at(uint8_t tx, uint8_t ty);
// Queue a column of `h` tiles / a row of `w` tiles for the background map; the
// vblank interrupt writes them (see gfx_strip). Rows longer than a strip are split.
void room_draw_column(uint8_t tx, uint8_t ty, uint8_t h);
void room_draw_row(uint8_t tx, uint8_t ty, uint8_t w);

#endif
