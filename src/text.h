// Text in the fonts from artwork/fonts/font-sheet.png (tools/fonts.mjs):
// proportional letters drawn into fresh tiles, so they can sit at any pixel and
// pack tighter than one letter a tile.
//   FONT_COMPACT  small 3x7 letters: menus, the HUD, loot cards
//   FONT_DISPLAY  big 8x16 letters in three colors: titles and banners
// Glyph colors are indices (0 see-through, 1-3 ink); the palette picks the
// actual colors, so the same letters can take any theme's colors.
#ifndef TEXT_H
#define TEXT_H

#include <gb/gb.h>
#include <stdint.h>

#define FONT_COMPACT 0
#define FONT_DISPLAY 1

#define TEXT_CENTER 0xFF  // as text_draw's x: centered on the screen

// s must be in RAM (or bank 0): this code runs in its own ROM bank, so a string
// constant in the caller's bank isn't visible from here. Banked callers copy
// theirs to a buffer on the stack first (title.c's draw()).
#define TEXT_MAX 40  // characters in a line, at most

// Width of s in pixels.
uint8_t text_width(uint8_t font, const char *s) BANKED;

// Draw s at pixel x (or TEXT_CENTER) on background tile row ty, into new tiles
// from `tile` on, with background palette `pal`. A display line takes two tile
// rows. Writes VRAM directly: for screens being set up with the display off.
// Returns the number of tiles used (the next free tile is tile + that).
uint8_t text_draw(uint8_t font, uint8_t x, uint8_t ty, const char *s, uint8_t tile, uint8_t pal) BANKED;

#endif
