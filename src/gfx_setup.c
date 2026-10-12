#pragma bank 255
// Screen setup and fades, banked: called between scenes, never per frame.
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "state.h"
#include "font.h"
#include "frame.h"

extern volatile uint8_t vram_ready;  // gfx.c
extern uint8_t strip_count, copy_count;
void gfx_vbl(void);  // gfx.c: the vblank handler, in bank 0

extern uint16_t pal_bg[PAL_COUNT * 4], pal_obj[PAL_COUNT * 4];  // gfx.c: on screen
extern volatile uint8_t pal_dirty;

uint16_t pal_base_bg[PAL_COUNT * 4], pal_base_obj[PAL_COUNT * 4];

// The starting palettes: the original Game Boy's four greys (color index i is
// shade i: white, light, dark, black), and the flash palette inverted. Real
// colors come per asset (Sprite Studio palettes) in the next steps.
#define GREY0 RGB(31, 31, 31)
#define GREY1 RGB(21, 21, 21)
#define GREY2 RGB(10, 10, 10)
#define GREY3 RGB(0, 0, 0)
static const uint16_t grey[4] = {GREY0, GREY1, GREY2, GREY3};
static const uint16_t inverted[4] = {GREY3, GREY2, GREY1, GREY0};

// How far toward white each fade step goes, in 1/256ths.
static const uint8_t fade_mix[FADE_STEPS + 1] = {0, 85, 171, 255};

static uint16_t faded(uint16_t c, uint8_t mix) {
    uint8_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    if (mix == 255) return RGB(31, 31, 31);
    r += (uint8_t)(((uint16_t)(31 - r) * mix) >> 8);
    g += (uint8_t)(((uint16_t)(31 - g) * mix) >> 8);
    b += (uint8_t)(((uint16_t)(31 - b) * mix) >> 8);
    return RGB(r, g, b);
}

void gfx_set_fade(uint8_t step) BANKED {
    uint8_t mix = fade_mix[step];
    for (uint8_t i = 0; i < PAL_COUNT * 4; i++) {
        pal_bg[i] = faded(pal_base_bg[i], mix);
        pal_obj[i] = faded(pal_base_obj[i], mix);
    }
    pal_dirty = 1;
}

void gfx_init(void) BANKED {
    CRITICAL {
        add_VBL(gfx_vbl);
    }
    // The font and frame are in switchable banks (banked_bkg_data: gfx.c, bank 0).
    banked_bkg_data(FONT_TILES, FONT_TILE_COUNT, font_tiles, BANK(font));
    banked_bkg_data(FRAME_TILES, FRAME_TILE_COUNT, frame_tiles, BANK(frame));
    // Tile attributes (VRAM bank 1: palette, flips, tile bank): all 0, so every
    // tile is drawn plainly with background palette 0 from tile bank 0.
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, 0);
    fill_win_rect(0, 0, 32, 32, 0);
    VBK_REG = 0;
    for (uint8_t p = 0; p < PAL_COUNT; p++) {
        for (uint8_t i = 0; i < 4; i++) {
            pal_base_bg[(p << 2) + i] = grey[i];
            pal_base_obj[(p << 2) + i] = p == SPR_PAL_FLASH ? inverted[i] : grey[i];
        }
    }
}

void gfx_clear(void) BANKED {
    fill_bkg_rect(0, 0, 32, 32, FONT_TILES);  // tile 0 = space
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
    HIDE_WIN;
    scroll_x = scroll_y = 0;
    strip_count = 0;
    copy_count = 0;
    vram_ready = 0;
    move_bkg(0, 0);
}


void fade_out(void) BANKED {
    for (uint8_t s = 1; s <= FADE_STEPS; s++) {
        gfx_set_fade(s);
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
    }
}

void fade_in(void) BANKED {
    for (uint8_t s = FADE_STEPS; s--;) {
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
        gfx_set_fade(s);
    }
}

// Frame tiles are row-major: 0 1 2 / 3 4 5 / 6 7 8.
#define FRAME_DRAW(fill, set)                                                    \
    fill(x, y, w, h, FRAME_TILES + 4);                                           \
    fill(x + 1, y, w - 2, 1, FRAME_TILES + 1);                                   \
    fill(x + 1, y + h - 1, w - 2, 1, FRAME_TILES + 7);                           \
    fill(x, y + 1, 1, h - 2, FRAME_TILES + 3);                                   \
    fill(x + w - 1, y + 1, 1, h - 2, FRAME_TILES + 5);                           \
    set(x, y, FRAME_TILES + 0);                                                  \
    set(x + w - 1, y, FRAME_TILES + 2);                                          \
    set(x, y + h - 1, FRAME_TILES + 6);                                          \
    set(x + w - 1, y + h - 1, FRAME_TILES + 8)

void bkg_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED {
    FRAME_DRAW(fill_bkg_rect, set_bkg_tile_xy);
}

void win_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) BANKED {
    FRAME_DRAW(fill_win_rect, set_win_tile_xy);
}
