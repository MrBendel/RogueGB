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
static const uint8_t block_tiles[BLK_TYPES * 4] = {
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_FLOOR
    TILE_WALL, TILE_WALL, TILE_WALL, TILE_WALL,              // BLK_SOLID
    TILE_PIT, TILE_PIT + 1, TILE_PIT + 2, TILE_PIT + 3,      // BLK_PIT
    TILE_RAISED, TILE_RAISED + 1, TILE_RAISED + 2, TILE_RAISED + 3,  // BLK_RAISED
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_PROP (the prop is a sprite)
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_STAIRS: authored scenes replace art
    TILE_FLOOR, TILE_FLOOR, TILE_FLOOR, TILE_FLOOR,          // BLK_DECK
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
    for (uint16_t i = 0; i < VIS_BLOCKS * 4; i++) {
        room_tile_lut[i] = i < sizeof(block_tiles) ? block_tiles[i] : TILE_FLOOR;
        room_attr_lut[i] = BG_PAL_SCENE;
    }
    // One set of attributes for every tile: write them across the attribute map
    // now (the display is off), and scrolling needn't stream any.
    room_attr_uniform = 1;
    for (uint16_t i = 1; i < VIS_BLOCKS * 4; i++)
        if (room_attr_lut[i] != room_attr_lut[0]) room_attr_uniform = 0;
    if (room_attr_uniform) {
        VBK_REG = 1;
        fill_bkg_rect(0, 0, 32, 32, room_attr_lut[0]);
        VBK_REG = 0;
    }
}

void room_vis_from_map(void) BANKED {
    for (uint16_t i = 0; i < ROOM_MAX_W * ROOM_MAX_H; i++) room_vis[i] = room_map[i];
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

