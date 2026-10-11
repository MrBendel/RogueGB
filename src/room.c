#include <gb/gb.h>
#include "room.h"
#include "gfx.h"

#define TILE_FLOOR (SCENE_TILES + 0)
#define TILE_WALL  (SCENE_TILES + 1)
#define TILE_RAISED (SCENE_TILES + 2)  // 4 tiles: TL, TR, BL, BR of one 16x16 block
#define TILE_PIT   (SCENE_TILES + 6)   // 4 tiles

static const uint8_t room_tiles[] = {
    // floor speckle
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    // wall
    0xFF, 0x00, 0x81, 0x7E, 0xBD, 0x42, 0xA5, 0x5A,
    0xA5, 0x5A, 0xBD, 0x42, 0x81, 0x7E, 0xFF, 0x00,
    // raised block: light top face, dark rim, mid-gray front face (TL, TR, BL, BR)
    0xFF, 0xFF, 0xC0, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80,
    0xFF, 0xFF, 0x03, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01,
    0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xFF, 0xFF, 0xFF,
    0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0xFF, 0xFF,
    // pit: dark hole with a lit inner rim at the top (TL, TR, BL, BR)
    0x00, 0xFF, 0x80, 0xFF, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0xFF, 0x01, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
#define ROOM_TILE_COUNT 10

// Background tile for each block type (BLK_*) and quadrant (TL, TR, BL, BR).
static const uint8_t block_tiles[16] = {
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_FLOOR
    TILE_WALL, TILE_WALL, TILE_WALL, TILE_WALL,              // BLK_SOLID
    TILE_PIT, TILE_PIT + 1, TILE_PIT + 2, TILE_PIT + 3,      // BLK_PIT
    TILE_RAISED, TILE_RAISED + 1, TILE_RAISED + 2, TILE_RAISED + 3,  // BLK_RAISED
};
uint8_t __at(ROOM_TILE_LUT_ADDR) room_tile_lut[16];

// Parameters for the fill routines in room_fill.s.
uint8_t *fill_dst;
uint8_t fill_tx;
uint8_t fill_ty;
uint8_t fill_n;
void room_fill_row(void);
void room_fill_column(void);

// The movement lab: 2x2 screens. # wall, o pit, = raised block.
// - Top-left screen: the original lab's pillars (the movement tests rely on them);
//   the ledge below the start position at row 7 is where the old bottom wall was.
// - Top-right: a 2-block pit (columns 16-17) that needs a jump plus a dash.
// - Bottom-left: a raised platform and a 1-block pit trench (column 6) to jump.
// - Bottom-right: a raised platform.
// - Below, through the gap in row 15: the arena, where enemies spawn (see play.c).
// Column 1, column 13 and row 14 stay clear: tests walk along them.
#define LAB_W 20
#define LAB_H 24
static const char lab_layout[LAB_H][LAB_W + 1] = {
    "####################",
    "#..................#",
    "#.#....#...........#",
    "#...............oo.#",
    "#...##......#...oo.#",
    "#.#....#....#...oo.#",
    "#...........#...oo.#",
    "#..####............#",
    "#.....o...##.......#",
    "#..==.o............#",
    "#..==.o.#..#...===.#",
    "#..==.o.#..#...===.#",
    "#..##.o........===.#",
    "#.....o...#....#...#",
    "#..................#",
    "#########..#########",
    "#..................#",
    "#..................#",
    "#...#..........#...#",
    "#..................#",
    "#........##........#",
    "#..................#",
    "#..................#",
    "####################",
};

uint8_t room_w;
uint8_t room_h;
uint16_t room_px_w;
uint16_t room_px_h;
uint8_t __at(ROOM_MAP_ADDR) room_map[ROOM_MAX_W * ROOM_MAX_H];

void room_load_lab(void) {
    set_bkg_data(SCENE_TILES, ROOM_TILE_COUNT, room_tiles);
    room_w = LAB_W;
    room_h = LAB_H;
    room_px_w = (uint16_t)LAB_W * BLOCK_SIZE;
    room_px_h = (uint16_t)LAB_H * BLOCK_SIZE;
    // Everything outside the room reads as wall, so drawing needs no bounds checks.
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID;
    for (uint8_t i = 0; i < 16; i++) room_tile_lut[i] = block_tiles[i];
    for (uint8_t by = 0; by < LAB_H; by++)
        for (uint8_t bx = 0; bx < LAB_W; bx++)
        {
            char c = lab_layout[by][bx];
            room_map[((uint16_t)by << 5) | bx] = c == '#' ? BLK_SOLID : c == 'o' ? BLK_PIT : c == '=' ? BLK_RAISED : BLK_FLOOR;
        }
}

uint8_t room_block_at(uint16_t x, uint16_t y) {
    // Rooms are at most 512 px, so after this check the block indices fit 8 bits.
    if (x >= room_px_w || y >= room_px_h) return BLK_SOLID;
    return ROOM_BLOCK_IN(x, y);
}

uint8_t room_tile_at(uint8_t tx, uint8_t ty) {
    uint8_t blk = room_map[((uint16_t)((ty >> 1) & 31) << 5) | ((tx >> 1) & 31)];
    return block_tiles[(blk << 2) | ((ty & 1) << 1) | (tx & 1)];
}

// Rows and columns are drawn while scrolling, so the loops are in assembly (room_fill.s).
void room_draw_column(uint8_t tx, uint8_t ty, uint8_t h) {
    fill_dst = gfx_strip(tx, ty, 1, h);
    fill_tx = tx;
    fill_ty = ty;
    fill_n = h;
    room_fill_column();
}

void room_draw_row(uint8_t tx, uint8_t ty, uint8_t w) {
    fill_dst = gfx_strip(tx, ty, 0, w);
    fill_tx = tx;
    fill_ty = ty;
    fill_n = w;
    room_fill_row();
}
