#include <gb/gb.h>
#include <gb/metasprites.h>
#include "gfx.h"
#include "state.h"
#include "font.h"
#include "frame.h"

// Normal palette is 0xE4 (shades 3,2,1,0); each step shifts one shade lighter.
static const uint8_t fade_pal[FADE_STEPS + 1] = {0xE4, 0x90, 0x40, 0x00};

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
} Copy;
Copy copies[COPY_MAX];
uint8_t copy_count;
void vram_queue_write(void);  // vram_queue.s

// Set by gfx_commit(); the vblank handler only touches the queues when set, so it
// never sees a half-built frame (e.g. when a frame runs long).
static volatile uint8_t vram_ready;

uint8_t *gfx_strip(uint8_t x, uint8_t y, uint8_t vertical, uint8_t len) {
    if (strip_count == STRIP_MAX || vram_ready) gfx_flush();
    Strip *s = &strips[strip_count++];
    s->x = x & 31;
    s->y = y & 31;
    s->vertical = vertical;
    s->len = len;
    return s->tiles;
}

void gfx_vram_copy(uint8_t *dst, const uint8_t *src, uint8_t len) {
    if (copy_count == COPY_MAX || vram_ready) gfx_flush();
    Copy *c = &copies[copy_count++];
    c->dst = dst;
    c->src = src;
    c->len = len;
}

void gfx_commit(void) {
    if (strip_count || copy_count) vram_ready = 1;
}

// Write everything queued, outside vblank: directly with the display off, or with
// each byte waiting for VRAM access (set_vram_byte) when the display is on (the
// rare fallback when a queue overflows). In vblank, vram_queue.s does this.
static void write_queued(uint8_t wait) {
    for (uint8_t i = 0; i < copy_count; i++) {
        uint8_t *d = copies[i].dst;
        const uint8_t *src = copies[i].src;
        uint8_t n = copies[i].len;
        if (wait) {
            while (n--) set_vram_byte(d++, *src++);
        } else {
            while (n--) *d++ = *src++;
        }
    }
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

// Vblank handler (after GBDK's sprite DMA). Scroll first: it must land inside
// vblank or the screen tears. Then the queued VRAM writes, in assembly so even a
// full queue (a row, a column and a sprite frame) fits in the rest of vblank.
static void vbl_scroll(void) {
    SCX_REG = scroll_x;
    SCY_REG = scroll_y;
    if (vram_ready) {
        vram_queue_write();
        copy_count = 0;
        strip_count = 0;
        vram_ready = 0;
    }
}

void gfx_init(void) {
    CRITICAL {
        add_VBL(vbl_scroll);
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

void gfx_clear(void) {
    fill_bkg_rect(0, 0, 32, 32, FONT_TILES);  // tile 0 = space
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
    HIDE_WIN;
    scroll_x = scroll_y = 0;
    strip_count = 0;
    copy_count = 0;
    vram_ready = 0;
    move_bkg(0, 0);
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

static void set_fade(uint8_t step) {
    BGP_REG = OBP0_REG = fade_pal[step];
    // Sprites drawn with the second palette flash inverted; while fading they fade too.
    OBP1_REG = step ? fade_pal[step] : FLASH_PAL;
}

void fade_out(void) {
    for (uint8_t s = 1; s <= FADE_STEPS; s++) {
        set_fade(s);
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
    }
}

void fade_in(void) {
    for (uint8_t s = FADE_STEPS; s--;) {
        for (uint8_t f = 0; f < FADE_FRAMES; f++) wait_frame();
        set_fade(s);
    }
}
