#pragma bank 255
// Proportional text (text.h). Each line is drawn a tile row at a time into a
// RAM buffer of tiles, then copied into VRAM with its map and attributes.
// Banked: screens draw their text once, while they're set up.
#include <gb/gb.h>
#include <string.h>
#include "text.h"
#include "fonts_data.h"  // the glyph tables (in this file's bank)

#define LINE_TILES 21  // a screen width, plus one when the text starts mid-tile

uint8_t text_buf[LINE_TILES * 16];  // one tile row of a line, 2bpp

static const uint8_t *map, *widths, *rows;
static uint8_t height, gap, space;

static void use_font(uint8_t font) {
    if (font == FONT_DISPLAY) {
        map = font_display_map; widths = font_display_w; rows = font_display_rows;
        height = FONT_DISPLAY_H; gap = FONT_DISPLAY_GAP; space = FONT_DISPLAY_SPACE;
    } else {
        map = font_compact_map; widths = font_compact_w; rows = font_compact_rows;
        height = FONT_COMPACT_H; gap = FONT_COMPACT_GAP; space = FONT_COMPACT_SPACE;
    }
}

static uint8_t glyph_of(char c) {
    uint8_t u = (uint8_t)c;
    return u < 32 || u > 127 ? 255 : map[u - 32];
}

static uint8_t width_of(const char *s) {
    uint8_t w = 0;
    while (*s) {
        uint8_t g = glyph_of(*s++);
        w += g == 255 ? space : widths[g] + gap;
    }
    return w > gap ? w - gap : w;
}

uint8_t text_width(uint8_t font, const char *s) BANKED {
    use_font(font);
    return width_of(s);
}

uint8_t text_draw(uint8_t font, uint8_t x, uint8_t ty, const char *s, uint8_t tile, uint8_t pal) BANKED {
    use_font(font);
    uint8_t w = width_of(s);
    if (x == TEXT_CENTER) x = w < 160 ? (160 - w) >> 1 : 0;
    uint8_t tx = x >> 3, off = x & 7;
    uint8_t n = (off + w + 7) >> 3;
    if (n > LINE_TILES) n = LINE_TILES;
    uint8_t bands = (height + 7) >> 3;
    uint8_t cells[LINE_TILES];
    for (uint8_t band = 0; band < bands; band++) {
        memset(text_buf, 0, (uint16_t)n << 4);
        uint8_t lines = height - (band << 3);
        if (lines > 8) lines = 8;
        uint8_t px = off;
        for (const char *p = s; *p; p++) {
            uint8_t g = glyph_of(*p);
            if (g == 255) {
                px += space;
                continue;
            }
            uint8_t gw = widths[g];
            const uint8_t *src = rows + (uint16_t)g * (height << 1) + (band << 4);
            for (uint8_t r = 0; r < lines; r++) {
                uint16_t bits = ((uint16_t)src[r << 1] << 8) | src[(r << 1) + 1];
                for (uint8_t c = 0; c < gw; c++, bits <<= 2) {
                    uint8_t v = bits >> 14;
                    if (!v) continue;
                    uint8_t X = px + c;
                    if ((X >> 3) >= n) continue;
                    uint8_t *t = text_buf + ((uint16_t)(X >> 3) << 4) + (r << 1);
                    uint8_t m = 0x80 >> (X & 7);
                    if (v & 1) t[0] |= m;
                    if (v & 2) t[1] |= m;
                }
            }
            px += gw + gap;
        }
        uint8_t first = tile + band * n;
        set_bkg_data(first, n, text_buf);
        for (uint8_t i = 0; i < n; i++) cells[i] = first + i;
        set_bkg_tiles(tx, ty + band, n, 1, cells);
        memset(cells, pal, n);
        VBK_REG = 1;
        set_bkg_tiles(tx, ty + band, n, 1, cells);
        VBK_REG = 0;
    }
    return n * bands;
}
