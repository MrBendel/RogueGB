#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
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

// The palettes on screen (gfx_set_fade fills them), and whether the vblank
// handler should upload them (pal_write, vram_queue.s).
uint16_t pal_bg[PAL_COUNT * 4], pal_obj[PAL_COUNT * 4];
volatile uint8_t pal_dirty;
void pal_write(void);

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
uint8_t obj_restore;

// LCD interrupt at the line above the HUD (gfx_sprite_split): wait for that
// line's horizontal blank, then sprites off for the rest of the screen.
void gfx_lcd(void) {
    if (!(LCDC_REG & LCDCF_OBJON)) return;
    while (STAT_REG & STATF_BUSY) {
    }
    LCDC_REG &= ~LCDCF_OBJON;
    obj_restore = 1;
}

void gfx_sprites(uint8_t on) {
    obj_restore = 0;
    if (on) SHOW_SPRITES;
    else HIDE_SPRITES;
}

void gfx_vbl(void) {
    if (obj_restore) {
        LCDC_REG |= LCDCF_OBJON;  // back on for the new frame (gfx_lcd turned them off)
        obj_restore = 0;
    }
    SCX_REG = scroll_x;
    SCY_REG = scroll_y;
    if (pal_dirty) {
        pal_write();
        pal_dirty = 0;
    }
    if (vram_ready) {
        // The queue writes VRAM bank 0; a load into bank 1 (weapon art) may be
        // what this interrupted, so switch to 0 and back.
        uint8_t vbk = VBK_REG;
        VBK_REG = 0;
        vram_queue_write();
        VBK_REG = vbk;
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

void banked_memcpy(void *dst, const void *src, uint16_t n, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    memcpy(dst, src, n);
    SWITCH_ROM(saved);
}

void banked_sprite_data(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    set_sprite_data(first, n, data);
    SWITCH_ROM(saved);
}

void banked_sprite_data_vbk1(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    VBK_REG = 1;
    set_sprite_data(first, n, data);
    VBK_REG = 0;
    SWITCH_ROM(saved);
}

void banked_bkg_data(uint8_t first, uint8_t n, const uint8_t *data, uint8_t bank) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    set_bkg_data(first, n, data);
    SWITCH_ROM(saved);
}
