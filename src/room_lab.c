#pragma bank 255
// The movement lab room: its layout, tiles and loader. Banked: only needed when
// the scene starts. The map and lookups it fills in live in room.c (bank 0).
#include <gb/gb.h>
#include "room.h"
#include "gfx.h"

#define TILE_FLOOR (SCENE_TILES + 0)
#define TILE_WALL  (SCENE_TILES + 1)
#define TILE_RAISED (SCENE_TILES + 2)  // 4 tiles: TL, TR, BL, BR of one 16x16 block
#define TILE_PIT   (SCENE_TILES + 6)   // 4 tiles
#define TILE_FLOOR_CRACK (SCENE_TILES + 10)
#define TILE_FLOOR_SEAM  (SCENE_TILES + 11)
#define TILE_FLOOR_CHIP  (SCENE_TILES + 12)

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
    // floor cracked stone
    0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x10, 0x08,
    0x00, 0x08, 0x00, 0x04, 0x04, 0x02, 0x00, 0x02,
    // floor slab seam / border
    0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // floor chipped stone / gravel fleck
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x08, 0x14,
    0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
};
#define ROOM_TILE_COUNT 13

#define VIS_INIT_BLOCKS 12
// Background tile for each block type (BLK_*) and quadrant (TL, TR, BL, BR).
static const uint8_t block_tiles[VIS_INIT_BLOCKS * 4] = {
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_FLOOR (0)
    TILE_WALL, TILE_WALL, TILE_WALL, TILE_WALL,              // BLK_SOLID (1)
    TILE_PIT, TILE_PIT + 1, TILE_PIT + 2, TILE_PIT + 3,      // BLK_PIT (2)
    TILE_RAISED, TILE_RAISED + 1, TILE_RAISED + 2, TILE_RAISED + 3,  // BLK_RAISED (3)
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_PROP (4)
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_STAIRS (5)
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_DECK (6)
    // Randomized floor block variations:
    TILE_FLOOR_CRACK, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,    // 7: crack in TL
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR_CRACK,    // 8: crack in BR
    TILE_FLOOR_SEAM, TILE_FLOOR_SEAM, TILE_FLOOR, TILE_FLOOR,// 9: slab joint top
    TILE_FLOOR, TILE_FLOOR_CHIP, TILE_FLOOR, TILE_FLOOR_CHIP,// 10: chips
    TILE_FLOOR_CHIP, TILE_FLOOR, TILE_FLOOR_SEAM, TILE_FLOOR,// 11: varied
};

// The movement lab: 2x2 screens. # wall, o pit, = raised block, c the chest
// (chest.h: CHEST_BX, CHEST_BY).
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
    "#................c.#",
    "#..................#",
    "####################",
};


// The test tiles (floor, wall, raised, pit) and the visual block tables:
// visual block v is block type v, drawn with the scene's palette from VRAM bank
// 0. Generated maps (level.c) use them too until themes come in.
void room_load_tiles(void) BANKED {
    set_bkg_data(SCENE_TILES, ROOM_TILE_COUNT, room_tiles);
    for (uint8_t v = 0; v < VIS_BLOCKS; v++)
        for (uint8_t q = 0; q < 4; q++) {
            uint16_t i = (uint16_t)v * 4 + q;
            ROOM_TILE(v, q) = i < sizeof(block_tiles) ? block_tiles[i] : TILE_FLOOR;
            ROOM_ATTR(v, q) = BG_PAL_SCENE;
        }
    // One set of attributes for every tile: write them across the attribute map
    // now (the display is off), and scrolling needn't stream any.
    room_attr_uniform = 1;
    for (uint8_t v = 0; v < VIS_BLOCKS; v++)
        for (uint8_t q = 0; q < 4; q++)
            if (ROOM_ATTR(v, q) != ROOM_ATTR(0, 0)) room_attr_uniform = 0;
    if (room_attr_uniform) {
        VBK_REG = 1;
        fill_bkg_rect(0, 0, 32, 32, ROOM_ATTR(0, 0));
        VBK_REG = 0;
    }
}

void room_vis_from_map(void) BANKED {
    for (uint16_t i = 0; i < ROOM_MAX_W * ROOM_MAX_H; i++) room_vis[i] = room_map[i];
}

void room_randomize_floor(void) BANKED {
    for (uint8_t y = 0; y < ROOM_MAX_H; y++) {
        for (uint8_t x = 0; x < ROOM_MAX_W; x++) {
            uint16_t i = ((uint16_t)y << 5) | x;
            if (room_map[i] == BLK_FLOOR) {
                uint8_t h = (uint8_t)(x * 17 + y * 29 + ((x ^ y) << 1));
                uint8_t r = h & 7;
                if (r == 1) room_vis[i] = 7;
                else if (r == 2) room_vis[i] = 8;
                else if (r == 3) room_vis[i] = 9;
                else if (r == 4) room_vis[i] = 10;
                else if (r == 5) room_vis[i] = 11;
                else room_vis[i] = 0;
            }
        }
    }
}

void room_load_lab(void) BANKED {
    room_load_tiles();
    room_w = LAB_W;
    room_h = LAB_H;
    room_px_w = (uint16_t)LAB_W * BLOCK_SIZE;
    room_px_h = (uint16_t)LAB_H * BLOCK_SIZE;
    // Everything outside the room reads as wall, so drawing needs no bounds checks.
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID;
    for (uint8_t by = 0; by < LAB_H; by++)
        for (uint8_t bx = 0; bx < LAB_W; bx++)
        {
            char c = lab_layout[by][bx];
            room_map[((uint16_t)by << 5) | bx] = c == '#' ? BLK_SOLID : c == 'o' ? BLK_PIT : c == '=' ? BLK_RAISED : c == 'c' ? BLK_PROP : BLK_FLOOR;
        }
    room_vis_from_map();
}

