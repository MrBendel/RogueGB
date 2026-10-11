// A 16x16 character on screen: two 8x16 hardware sprites, drawing pre-rendered
// frames from a sprite converted by tools/assets.mjs (kind 'sprite').
//
// All frames stay in ROM. VRAM holds only the frame on screen: each slot owns
// 8 tiles, split into two 4-tile halves. A new frame is copied into the hidden
// half (queued for vblank) and the sprites are pointed at it in the same update,
// so tile data and flip flags change together (no torn or mis-flipped frame).
#ifndef SPRITE_SLOT_H
#define SPRITE_SLOT_H

#include <stdint.h>

#define SLOT_TILES 8  // VRAM tiles per slot
#define SLOT_NONE 0xFF

typedef struct {
    uint8_t oam;    // first hardware sprite (uses oam and oam + 1)
    uint8_t vram;   // first VRAM tile (uses SLOT_TILES); must be a multiple of 4
    uint8_t half;   // which 4-tile half is live
    uint8_t frame;  // frame currently loaded, or SLOT_NONE
} SpriteSlot;

void slot_init(SpriteSlot *s, uint8_t oam, uint8_t vram);
// Show a frame; copies 64 bytes into VRAM only when the frame changes.
void slot_set_frame(SpriteSlot *s, const uint8_t *tiles, const uint8_t *frames, uint8_t frame);
// Position by the frame's top-left in screen pixels.
void slot_move(SpriteSlot *s, uint8_t x, uint8_t y);
void slot_hide(SpriteSlot *s);

#endif
