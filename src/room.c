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
uint8_t __at(ROOM_VIS_ADDR) room_vis[ROOM_MAX_W * ROOM_MAX_H];
uint8_t __at(ROOM_LUT_ADDR) room_lut[4][256];

// Parameters for the fill routines in room_fill.s.
uint8_t *fill_dst;
uint8_t fill_tx;
uint8_t fill_ty;
uint8_t fill_n;
uint8_t fill_lut;  // which half of the LUT to fill from: tiles (0) or attributes (LUT_ATTR)
uint8_t room_attr_uniform;

void room_fill_row(void);
void room_fill_column(void);

// room_block_at is in assembly (room_fill.s): it's the most-called lookup.

// Rows and columns are drawn while scrolling, so the loops are in assembly (room_fill.s).
// Each strip is filled twice from the same visual blocks: tiles, then their
// attributes (written to VRAM bank 1 by the vblank queue).
void room_draw_column(uint8_t tx, uint8_t ty, uint8_t h) {
    if (room_attr_uniform) {
        fill_dst = gfx_strip(tx, ty, 1, h);
        fill_tx = tx;
        fill_ty = ty;
        fill_n = h;
        fill_lut = 0;
        room_fill_column();
        return;
    }
    uint8_t *tiles = gfx_strip_attr(tx, ty, 1, h);
    fill_dst = tiles;
    fill_tx = tx;
    fill_ty = ty;
    fill_n = h;
    fill_lut = 0;
    room_fill_column();
    fill_dst = tiles + STRIP_MAX_LEN;
    fill_tx = tx;
    fill_ty = ty;
    fill_n = h;
    fill_lut = LUT_ATTR;
    room_fill_column();
}

void room_draw_row(uint8_t tx, uint8_t ty, uint8_t w) {
    if (room_attr_uniform) {
        fill_dst = gfx_strip(tx, ty, 0, w);
        fill_tx = tx;
        fill_ty = ty;
        fill_n = w;
        fill_lut = 0;
        room_fill_row();
        return;
    }
    uint8_t *tiles = gfx_strip_attr(tx, ty, 0, w);
    fill_dst = tiles;
    fill_tx = tx;
    fill_ty = ty;
    fill_n = w;
    fill_lut = 0;
    room_fill_row();
    fill_dst = tiles + STRIP_MAX_LEN;
    fill_tx = tx;
    fill_ty = ty;
    fill_n = w;
    fill_lut = LUT_ATTR;
    room_fill_row();
}
