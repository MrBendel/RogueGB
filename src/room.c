#include <gb/gb.h>
#include "room.h"
#include "gfx.h"

#define TILE_FLOOR (SCENE_TILES + 0)
#define TILE_WALL  (SCENE_TILES + 1)

static const uint8_t room_tiles[] = {
    // floor speckle
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    // wall
    0xFF, 0x00, 0x81, 0x7E, 0xBD, 0x42, 0xA5, 0x5A,
    0xA5, 0x5A, 0xBD, 0x42, 0x81, 0x7E, 0xFF, 0x00,
};

// The movement lab: 2x2 screens. The top-left screen keeps the original lab's
// pillars (the movement tests rely on them); the ledge below the start position
// at row 7 is where the old bottom wall was.
#define LAB_W 20
#define LAB_H 16
static const char lab_layout[LAB_H][LAB_W + 1] = {
    "####################",
    "#..................#",
    "#.#....#.......##..#",
    "#..................#",
    "#...##......#......#",
    "#.#....#....#..#...#",
    "#...........#......#",
    "#..####............#",
    "#.........##.......#",
    "#..#...............#",
    "#..#....#..#...##..#",
    "#.......#..#.......#",
    "#..##..............#",
    "#.........#....#...#",
    "#..................#",
    "####################",
};

uint8_t room_w;
uint8_t room_h;
uint16_t room_px_w;
uint16_t room_px_h;
uint8_t room_map[ROOM_MAX_W * ROOM_MAX_H];

void room_load_lab(void) {
    set_bkg_data(SCENE_TILES, 2, room_tiles);
    room_w = LAB_W;
    room_h = LAB_H;
    room_px_w = (uint16_t)LAB_W * BLOCK_SIZE;
    room_px_h = (uint16_t)LAB_H * BLOCK_SIZE;
    for (uint8_t by = 0; by < LAB_H; by++)
        for (uint8_t bx = 0; bx < LAB_W; bx++)
            room_map[((uint16_t)by << 5) | bx] = lab_layout[by][bx] == '#' ? BLK_SOLID : BLK_FLOOR;
}

uint8_t room_solid_at(uint16_t x, uint16_t y) {
    uint16_t bx = x >> 4, by = y >> 4;
    if (bx >= room_w || by >= room_h) return 1;
    return room_map[(by << 5) | bx] == BLK_SOLID;
}

uint8_t room_tile_at(uint8_t tx, uint8_t ty) {
    uint8_t bx = tx >> 1, by = ty >> 1;
    if (bx >= room_w || by >= room_h) return TILE_WALL;
    // Every block is currently one tile repeated 2x2; real tilesets (M2) will
    // pick the quadrant from (tx & 1, ty & 1).
    return room_map[((uint16_t)by << 5) | bx] == BLK_SOLID ? TILE_WALL : TILE_FLOOR;
}

void room_draw_column(uint8_t tx, uint8_t ty, uint8_t h) {
    while (h--) {
        set_bkg_tile_xy(tx & 31, ty & 31, room_tile_at(tx, ty));
        ty++;
    }
}

void room_draw_row(uint8_t tx, uint8_t ty, uint8_t w) {
    while (w--) {
        set_bkg_tile_xy(tx & 31, ty & 31, room_tile_at(tx, ty));
        tx++;
    }
}
