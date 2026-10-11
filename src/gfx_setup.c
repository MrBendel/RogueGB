#pragma bank 255
// Screen setup and fades, banked: called between scenes, never per frame.
#include <gb/gb.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "state.h"
#include "font.h"
#include "frame.h"

extern volatile uint8_t vram_ready;  // gfx.c
extern uint8_t strip_count, copy_count;
void gfx_vbl(void);  // gfx.c: the vblank handler, in bank 0

// Normal palette is 0xE4 (shades 3,2,1,0); each step shifts one shade lighter.
static const uint8_t fade_pal[FADE_STEPS + 1] = {0xE4, 0x90, 0x40, 0x00};

void gfx_init(void) BANKED {
    CRITICAL {
        add_VBL(gfx_vbl);
    }
    // The font and frame are in switchable banks: switch in, copy, switch back.
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(font));
    set_bkg_data(FONT_TILES, FONT_TILE_COUNT, font_tiles);
    SWITCH_ROM(BANK(frame));
    set_bkg_data(FRAME_TILES, FRAME_TILE_COUNT, frame_tiles);
    SWITCH_ROM(saved);
    BGP_REG = 0xE4;
    OBP0_REG = 0xE4;
    OBP1_REG = FLASH_PAL;
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

static void set_fade(uint8_t step) {
    BGP_REG = OBP0_REG = fade_pal[step];
    // Sprites drawn with the second palette flash inverted; while fading they fade too.
    OBP1_REG = step ? fade_pal[step] : FLASH_PAL;
}

void fade_out(void) BANKED {
    for (uint8_t s = 1; s <= FADE_STEPS; s++) {
        set_fade(s);
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
    }
}

void fade_in(void) BANKED {
    for (uint8_t s = FADE_STEPS; s--;) {
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
        set_fade(s);
    }
}
