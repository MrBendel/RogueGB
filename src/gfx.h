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

// Background scroll, applied by the vblank interrupt (together with the sprite
// update), so the background and sprites always move in the same frame.
extern uint8_t scroll_x;
extern uint8_t scroll_y;

// VRAM updates queued during the frame and written by the vblank interrupt, when
// VRAM can be written without waiting. Writing while the screen is drawing costs a
// wait per byte, which made streaming a row, or swapping a sprite's animation
// frame, take a large part of the frame. wait_frame() calls gfx_commit(), which
// hands everything queued this frame to the next vblank.
//
// Background map strips (a row or a column of tiles):
#define STRIP_MAX_LEN 21
#define STRIP_MAX 4
// x, y: background map tile coordinates (wrapped to 0-31). vertical: column if 1.
// Returns the buffer to fill with `len` tiles (flushing first if the queue is full).
uint8_t *gfx_strip(uint8_t x, uint8_t y, uint8_t vertical, uint8_t len);
// Byte copies into VRAM (tile data). `src` must stay valid until the next vblank.
#define COPY_MAX 4
void gfx_vram_copy(uint8_t *dst, const uint8_t *src, uint8_t len);
// Mark this frame's queued updates ready for the next vblank.
void gfx_commit(void);
// Write everything queued now (waits for VRAM access if the display is on).
void gfx_flush(void);

// Load the font and frame tiles and install the vblank scroll handler.
// Call once at boot; scenes never overwrite these tiles.
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
// Second sprite palette (S_PALETTE in a sprite's flags): colors inverted, used to
// flash a sprite as a warning or when hit.
#define FLASH_PAL 0x1B

#define FADE_STEPS  3
#define FADE_FRAMES 4
void fade_out(void);
void fade_in(void);

#endif
