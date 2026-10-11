#include <gb/gb.h>
#include "sprite_slot.h"

#define COLUMN_BYTES 32  // one 8x16 column = two 8x8 tiles

void slot_init(SpriteSlot *s, uint8_t oam, uint8_t vram) {
    s->oam = oam;
    s->vram = vram;
    s->half = 0;
    s->frame = SLOT_NONE;
}

void slot_set_frame(SpriteSlot *s, const uint8_t *tiles, const uint8_t *frames, uint8_t frame) {
    if (frame == s->frame) return;
    s->frame = frame;
    s->half ^= 1;
    uint8_t base = s->vram + (s->half << 2);
    const uint8_t *f = frames + (frame << 2);  // left column, left flags, right column, right flags
    set_sprite_data(base, 2, tiles + (uint16_t)f[0] * COLUMN_BYTES);
    set_sprite_data(base + 2, 2, tiles + (uint16_t)f[2] * COLUMN_BYTES);
    set_sprite_tile(s->oam, base);
    set_sprite_prop(s->oam, f[1]);
    set_sprite_tile(s->oam + 1, base + 2);
    set_sprite_prop(s->oam + 1, f[3]);
}

void slot_move(SpriteSlot *s, uint8_t x, uint8_t y) {
    // Hardware sprite coordinates are offset by (8, 16).
    move_sprite(s->oam, x + 8, y + 16);
    move_sprite(s->oam + 1, x + 16, y + 16);
}

void slot_hide(SpriteSlot *s) {
    hide_sprite(s->oam);
    hide_sprite(s->oam + 1);
}
