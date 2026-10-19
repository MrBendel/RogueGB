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
// Work RAM layout (globals start at 0xC0A0 and grow up; tools/build.mjs fails the
// build if they reach ROOM_MAP_ADDR):
//   0xD400  room_map: each block's type (BLK_*): collision, pits, height
//   0xD800  room_vis: each block's look: a visual block id (0-63) into the LUTs
//   0xDC00  room_tile_lut: tile for (visual block << 2 | quadrant)
//   0xDD00  room_attr_lut: tile attributes for the same (palette, VRAM bank, flips)
//   0xDE00  the stack, growing down from 0xDFFF (512 bytes; ~220 used at most)
// The maps are 256-aligned with rows of 32, so a block's address is just
// ADDR | (by << 5) | bx (no tables); room_fill.s, bullets.s and shots.s rely on it.
#define ROOM_MAP_ADDR 0xD400
#define ROOM_VIS_ADDR 0xD800
#define ROOM_TILE_LUT_ADDR 0xDC00
#define ROOM_ATTR_LUT_ADDR 0xDD00
#define VIS_BLOCKS 64  // visual blocks per theme (4 quadrants each fill a 256-byte page)
// 1 when every tile of the theme has the same attributes: then they're written
// once when the room loads (room_load_tiles) and scrolling skips them.
extern uint8_t room_attr_uniform;
extern uint8_t __at(ROOM_MAP_ADDR) room_map[ROOM_MAX_W * ROOM_MAX_H];  // row-major, stride ROOM_MAX_W
extern uint8_t __at(ROOM_VIS_ADDR) room_vis[ROOM_MAX_W * ROOM_MAX_H];
extern uint8_t __at(ROOM_TILE_LUT_ADDR) room_tile_lut[VIS_BLOCKS * 4];  // filled by the room loader
extern uint8_t __at(ROOM_ATTR_LUT_ADDR) room_attr_lut[VIS_BLOCKS * 4];

// Load the movement-lab room (2x2 screens) and its tiles.
#include <gb/gb.h>
void room_load_lab(void) BANKED;  // room_lab.c (banked)
// Just the tiles and the visual block tables (generated rooms use the same set).
void room_load_tiles(void) BANKED;
// Each block's look from its type (one visual block per block type, for now:
// visual rules per theme come later). Call after the map is built or changed.
void room_vis_from_map(void) BANKED;
void room_randomize_floor(void) BANKED;
// The ice yard (room_yard.c): a test map in six palettes, with a walk-behind roof.
void room_load_yard(void) BANKED;
void room_load_town(void) BANKED;  // room_town.c: the town, drawn with the ice tileset (content/tilesets/ice.json)

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
