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

// Pillars to weave between and corners to slide around.
static const char lab_layout[ROOM_H][ROOM_W + 1] = {
    "##########",
    "#........#",
    "#.#....#.#",
    "#........#",
    "#...##...#",
    "#.#....#.#",
    "#........#",
    "##########",
};

uint8_t room_map[ROOM_H][ROOM_W];

void room_load_lab(void) {
    set_bkg_data(SCENE_TILES, 2, room_tiles);
    for (uint8_t by = 0; by < ROOM_H; by++) {
        for (uint8_t bx = 0; bx < ROOM_W; bx++) {
            uint8_t solid = lab_layout[by][bx] == '#';
            room_map[by][bx] = solid ? BLK_SOLID : BLK_FLOOR;
            fill_bkg_rect(bx * 2, by * 2, 2, 2, solid ? TILE_WALL : TILE_FLOOR);
        }
    }
}

uint8_t room_solid_at(uint8_t x, uint8_t y) {
    if (x >= ROOM_PX_W || y >= ROOM_PX_H) return 1;
    return room_map[y >> 4][x >> 4] == BLK_SOLID;
}
