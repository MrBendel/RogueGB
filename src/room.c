#include <gb/gb.h>
#include "room.h"
#include "gfx.h"

// The room's hot paths (bank 0): the block map, collision lookups and the
// background tiles for scrolling. Loading a room is in room_lab.c (banked).

uint8_t room_w;
uint8_t room_h;
uint16_t room_px_w;
uint16_t room_px_h;
uint8_t __at(ROOM_MAP_ADDR) room_map[ROOM_MAX_W * ROOM_MAX_H];
uint8_t __at(ROOM_TILE_LUT_ADDR) room_tile_lut[BLK_TYPES * 4];

// Parameters for the fill routines in room_fill.s.
uint8_t *fill_dst;
uint8_t fill_tx;
uint8_t fill_ty;
uint8_t fill_n;

void room_fill_row(void);
void room_fill_column(void);

// room_block_at is in assembly (room_fill.s): it's the most-called lookup.

uint8_t room_tile_at(uint8_t tx, uint8_t ty) {
    uint8_t blk = room_map[((uint16_t)((ty >> 1) & 31) << 5) | ((tx >> 1) & 31)];
    return room_tile_lut[(blk << 2) | ((ty & 1) << 1) | (tx & 1)];
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
