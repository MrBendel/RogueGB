#include <gb/gb.h>
#include "sprite_slot.h"
#include "gfx.h"

#define COLUMN_BYTES 32  // one 8x16 column = two 8x8 tiles

void slot_init(SpriteSlot *s, uint8_t oam, uint8_t vram) {
    s->oam = oam;
    s->vram = vram;
    s->half = 0;
    s->frame = SLOT_NONE;
}

void slot_set_frame(SpriteSlot *s, const uint8_t *tiles, uint8_t tiles_bank, const uint8_t *frames, uint8_t frame) {
    if (frame == s->frame) return;
    s->frame = frame;
    s->half ^= 1;
    uint8_t base = s->vram + (s->half << 2);
    const uint8_t *f = frames + (frame << 2);  // left column, left flags, right column, right flags
    // Queued for vblank (see gfx_vram_copy): copying during the frame waits per byte.
    uint8_t *vram = (uint8_t *)0x8000 + ((uint16_t)base << 4);
    gfx_vram_copy(vram, tiles + ((uint16_t)f[0] << 5), COLUMN_BYTES, tiles_bank);
    gfx_vram_copy(vram + COLUMN_BYTES, tiles + ((uint16_t)f[2] << 5), COLUMN_BYTES, tiles_bank);
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
