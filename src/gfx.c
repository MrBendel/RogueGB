#include <gb/gb.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "state.h"

uint8_t scroll_x;
uint8_t scroll_y;

typedef struct {
    uint8_t x, y, vertical, len;
    uint8_t tiles[STRIP_MAX_LEN];
} Strip;
// Not static: vram_queue.s reads these.
Strip strips[STRIP_MAX];
uint8_t strip_count;

typedef struct {
    uint8_t *dst;
    const uint8_t *src;
    uint8_t len;
    uint8_t bank;  // ROM bank holding src (0: bank 0 or RAM)
} Copy;
Copy copies[COPY_MAX];
uint8_t copy_count;
void vram_queue_write(void);  // vram_queue.s

// Set by gfx_commit(); the vblank handler only touches the queues when set, so it
// never sees a half-built frame (e.g. when a frame runs long).
volatile uint8_t vram_ready;  // also cleared by gfx_clear (gfx_setup.c)

uint8_t *gfx_strip(uint8_t x, uint8_t y, uint8_t vertical, uint8_t len) {
    if (strip_count == STRIP_MAX || vram_ready) gfx_flush();
    Strip *s = &strips[strip_count++];
    s->x = x & 31;
    s->y = y & 31;
    s->vertical = vertical;
    s->len = len;
    return s->tiles;
}

void gfx_vram_copy(uint8_t *dst, const uint8_t *src, uint8_t len, uint8_t bank) {
    if (copy_count == COPY_MAX || vram_ready) gfx_flush();
    Copy *c = &copies[copy_count++];
    c->dst = dst;
    c->src = src;
    c->len = len;
    c->bank = bank;
}

void gfx_commit(void) {
    if (strip_count || copy_count) vram_ready = 1;
}

// Write everything queued, outside vblank: directly with the display off, or with
// each byte waiting for VRAM access (set_vram_byte) when the display is on (the
// rare fallback when a queue overflows). In vblank, vram_queue.s does this.
static void write_queued(uint8_t wait) {
    uint8_t saved = CURRENT_BANK;
    for (uint8_t i = 0; i < copy_count; i++) {
        if (copies[i].bank) SWITCH_ROM(copies[i].bank);
        uint8_t *d = copies[i].dst;
        const uint8_t *src = copies[i].src;
        uint8_t n = copies[i].len;
        if (wait) {
            while (n--) set_vram_byte(d++, *src++);
        } else {
            while (n--) *d++ = *src++;
        }
    }
    SWITCH_ROM(saved);
    copy_count = 0;
    for (uint8_t i = 0; i < strip_count; i++) {
        Strip *s = &strips[i];
        uint8_t *row = (uint8_t *)0x9800 + ((uint16_t)s->y << 5);
        uint8_t x = s->x;
        const uint8_t *t = s->tiles;
        uint8_t n = s->len;
        if (s->vertical) {
            uint8_t *p = row + x;
            while (n--) {
                if (wait) set_vram_byte(p, *t++);
                else *p = *t++;
                p += 32;
                if (p >= (uint8_t *)0x9C00) p -= 0x400;  // wrap to the top of the map
            }
        } else if (wait) {
            while (n--) {
                set_vram_byte(row + x, *t++);
                x = (x + 1) & 31;
            }
        } else {
            while (n--) {
                row[x] = *t++;
                x = (x + 1) & 31;  // wrap to the left edge
            }
        }
    }
    strip_count = 0;
    vram_ready = 0;
}

void gfx_flush(void) {
    write_queued(LCDC_REG & LCDCF_ON);
}

// Vblank handler, installed by gfx_init (after GBDK's sprite DMA). Scroll first: it must land inside
// vblank or the screen tears. Then the queued VRAM writes, in assembly so even a
// full queue (a row, a column and a sprite frame) fits in the rest of vblank.
void gfx_vbl(void) {
    SCX_REG = scroll_x;
    SCY_REG = scroll_y;
    if (vram_ready) {
        vram_queue_write();
        copy_count = 0;
        strip_count = 0;
        vram_ready = 0;
    }
}



static uint8_t glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c < ' ' || c > '_') c = ' ';
    return FONT_TILES + (uint8_t)(c - ' ');
}

void bkg_text(uint8_t x, uint8_t y, const char *s) {
    while (*s) set_bkg_tile_xy(x++, y, glyph(*s++));
}

void win_text(uint8_t x, uint8_t y, const char *s) {
    while (*s) set_win_tile_xy(x++, y, glyph(*s++));
}

static void draw_number(uint8_t win, uint8_t x, uint8_t y, uint16_t n, uint8_t width, uint8_t zero_pad) {
    x += width;
    do {
        uint8_t t = glyph('0' + (n % 10));
        if (win) set_win_tile_xy(--x, y, t);
        else set_bkg_tile_xy(--x, y, t);
        n /= 10;
        width--;
    } while (n && width);
    while (width--) {
        uint8_t t = glyph(zero_pad ? '0' : ' ');
        if (win) set_win_tile_xy(--x, y, t);
        else set_bkg_tile_xy(--x, y, t);
    }
}

void bkg_number(uint8_t x, uint8_t y, uint16_t n, uint8_t width, uint8_t zero_pad) {
    draw_number(0, x, y, n, width, zero_pad);
}

void win_number(uint8_t x, uint8_t y, uint16_t n, uint8_t width, uint8_t zero_pad) {
    draw_number(1, x, y, n, width, zero_pad);
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

void bkg_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    FRAME_DRAW(fill_bkg_rect, set_bkg_tile_xy);
}

void win_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    FRAME_DRAW(fill_win_rect, set_win_tile_xy);
}
