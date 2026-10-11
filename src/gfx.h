// Shared graphics: font, text and frame drawing, screen fades.
//
// Background tile layout (BG tiles live at 0x9000/0x8800, separate from sprites):
//   0..63    font, ASCII 32..95, so tile = char - 32 and tile 0 is a blank space
//   64..72   dialog frame (3x3)
//   80..     owned by the current scene (SCENE_TILES)
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define FONT_TILES  0
#define FRAME_TILES 64
#define SCENE_TILES 80

#define SCREEN_TW 20  // screen size in tiles
#define SCREEN_TH 18

// Load the font and frame tiles. Call once at boot; scenes never overwrite them.
void gfx_init(void);

// Clear the background map to blank, hide every sprite, hide the window.
void gfx_clear(void);

// Draw text on the background (or window, with win_text) at tile x, y.
// Lowercase is drawn as uppercase; anything outside the font is a space.
void bkg_text(uint8_t x, uint8_t y, const char *s);
void win_text(uint8_t x, uint8_t y, const char *s);
// Draw an unsigned number, right-aligned in width tiles (zero-padded if zero_pad).
void bkg_number(uint8_t x, uint8_t y, uint16_t n, uint8_t width, uint8_t zero_pad);
void win_number(uint8_t x, uint8_t y, uint16_t n, uint8_t width, uint8_t zero_pad);

// Draw a framed box on the background or the window.
void bkg_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void win_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

// Fade the background and sprite palettes to white / back to normal.
// Each takes FADE_STEPS * FADE_FRAMES frames.
#define FADE_STEPS  3
#define FADE_FRAMES 4
void fade_out(void);
void fade_in(void);

#endif
