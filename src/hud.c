#pragma bank 255
// The HUD row: see hud.h. Banked (once a frame: a few compares unless a value
// changed). The bar tiles are background tiles in the Color's second VRAM bank
// (window tiles pick their bank in the attribute map), so they take none of
// the scene's tiles. Each bar's row is rebuilt in RAM when it changes and
// copied to the window map in vblank.
#include <gb/gb.h>
#include <gb/cgb.h>
#include "hud.h"
#include "health.h"
#include "gfx.h"
#include "weapon.h"

#define WIN_MAP ((uint8_t *)0x9C00)
#define BAR_COLS 11              // cells: 8 px each
#define BAR_PX (BAR_COLS * 8)    // the longest bar; past it, bars scale
#define ROW_LEN (BAR_COLS + 2)   // icon, cells, end cap
#define ATTR_BANK1 0x08          // attribute bit: the tile is in VRAM bank 1

// HUD tiles (VRAM bank 1, background tiles 0..)
#define T_BLANK 0
#define T_FILL 1    // + px filled, 0..8
#define T_CAP 10    // the bar's right end
#define T_SHIELD 11
#define T_HEART 12
#define T_BROKEN 13 // the shield icon, cracked: empty and recharging
#define HUD_TILE_COUNT 14

uint8_t hud_shield_px, hud_shield_len, hud_shield_icon;
uint8_t hud_hp_px, hud_hp_len;
uint8_t hud_fx;
uint8_t hud_tick;
uint8_t hud_rows[2][ROW_LEN];  // what the copies read: must last until vblank

extern uint16_t pal_bg[PAL_COUNT * 4], pal_obj[PAL_COUNT * 4];  // gfx.c: on screen
extern volatile uint8_t pal_dirty;

// The icons by color index (. = 0): the icon in columns 0-5, the bar's left
// edge in column 7.
static const char icons[3][8][9] = {
    {"........", "333333.3", "311123.3", "312223.3", "312223.3", ".3223..3", "..33...3", "........"},
    {"........", ".33.33.3", "312213.3", "322223.3", ".3223..3", "..33...3", ".......3", "........"},
    {"........", "333.33.3", "3..3.3.3", "3.3..3.3", ".3..3..3", "..33...3", ".......3", "........"},
};

void hud_init(void) BANKED {
    uint8_t tiles[HUD_TILE_COUNT * 16];
    uint8_t *t = tiles;
    for (uint8_t i = 0; i < 16; i++) *t++ = 0;  // T_BLANK
    // The bar cells: an ink outline (rows 1 and 6) around a fill of n px from
    // the left: a highlight row (color 1) over the body (color 2).
    for (uint8_t n = 0; n <= 8; n++) {
        uint8_t m = (uint8_t)(0xFF00 >> n);
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t lo = 0, hi = 0;
            if (r == 1 || r == 6) lo = hi = 0xFF;
            else if (r == 2) lo = m;
            else if (r > 2 && r < 6) hi = m;
            *t++ = lo;
            *t++ = hi;
        }
    }
    // T_CAP: the right edge, in column 0.
    for (uint8_t r = 0; r < 8; r++) {
        uint8_t edge = r && r < 7 ? 0x80 : 0;
        *t++ = edge;
        *t++ = edge;
    }
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t lo = 0, hi = 0;
            for (uint8_t x = 0; x < 8; x++) {
                char c = icons[i][r][x];
                uint8_t bit = 0x80 >> x;
                if (c == '1' || c == '3') lo |= bit;
                if (c == '2' || c == '3') hi |= bit;
            }
            *t++ = lo;
            *t++ = hi;
        }
    }
    VBK_REG = 1;
    set_bkg_data(0, HUD_TILE_COUNT, tiles);
    VBK_REG = 0;
}

// The outline's length for a max value: 1 px per point in whole cells, up to
// BAR_PX.
static uint8_t bar_len(uint8_t max) {
    if (max > BAR_PX) return BAR_PX;
    return (max + 7) & 0xF8;
}

// Pixels filled out of len for v of max (at least 1 while above 0). Divides,
// but only when a value changed.
static uint8_t bar_fill(uint8_t v, uint8_t max, uint8_t len) {
    if (v >= max) return len;
    if (len == max) return v;
    uint8_t px = (uint8_t)((uint16_t)v * len / max);
    return px || !v ? px : 1;
}

static void build_row(uint8_t *r, uint8_t icon, uint8_t px, uint8_t len) {
    uint8_t cells = len >> 3;
    r[0] = icon;
    for (uint8_t i = 0; i < BAR_COLS + 1; i++) {
        uint8_t t = T_BLANK;
        if (i < cells) {
            t = px >= 8 ? 8 : px;
            px -= t;
            t += T_FILL;
        } else if (i == cells && cells) {
            t = T_CAP;
        }
        r[1 + i] = t;
    }
}

void hud_draw(void) BANKED {
    fill_win_rect(0, 0, SCREEN_TW, 2, FONT_TILES);
    // The bars' tiles: palette and VRAM bank in the attribute map.
    VBK_REG = 1;
    fill_win_rect(0, 0, ROW_LEN, 1, BG_PAL_SHIELD | ATTR_BANK1);
    fill_win_rect(0, 1, ROW_LEN, 1, BG_PAL_HEALTH | ATTR_BANK1);
    fill_win_rect(ROW_LEN, 0, SCREEN_TW - ROW_LEN, 2, BG_PAL_UI);
    VBK_REG = 0;
    weapon_draw_name(HUD_GUN_X, 0);
    hud_shield_px = hud_hp_px = 0xFF;  // redraw both bars
    hud_update();
}

void hud_hide(void) BANKED {
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, 2, BG_PAL_UI);
    VBK_REG = 0;
}

static const uint16_t shield_tint[3] = {RGB8(216, 248, 255), RGB8(96, 176, 248), RGB8(24, 48, 128)};
static const uint16_t white_tint[3] = {RGB8(255, 255, 255), RGB8(255, 255, 255), RGB8(160, 200, 255)};
#define LOW_PULSE RGB8(255, 136, 120)

void hud_update(void) BANKED {
    uint8_t len = shield_cap ? bar_len(shield_cap) : 0;
    uint8_t px = len ? bar_fill(shield_hp, shield_cap, len) : 0;
    uint8_t icon = !shield_cap ? T_BLANK : shield_hp ? T_SHIELD : T_BROKEN;
    if (px != hud_shield_px || len != hud_shield_len || icon != hud_shield_icon) {
        hud_shield_px = px;
        hud_shield_len = len;
        hud_shield_icon = icon;
        build_row(hud_rows[0], icon, px, len);
        gfx_vram_copy(WIN_MAP, hud_rows[0], ROW_LEN, 0);
    }
    len = bar_len(player_hp_max);
    px = bar_fill(player_hp, player_hp_max, len);
    if (px != hud_hp_px || len != hud_hp_len) {
        hud_hp_px = px;
        hud_hp_len = len;
        build_row(hud_rows[1], T_HEART, px, len);
        gfx_vram_copy(WIN_MAP + 32, hud_rows[1], ROW_LEN, 0);
    }

    // Palette effects: the player's colors on a shield hit or break, and the
    // health bar pulsing at a quarter health or less.
    uint8_t fx = 0;
    if (shield_broke) fx = shield_broke & 2 ? HUD_FX_WHITE : HUD_FX_SHIELD;
    else if (shield_flash) fx = HUD_FX_SHIELD;
    hud_tick++;
    if (player_hp && player_hp <= (player_hp_max >> 2) && (hud_tick & 16)) fx |= HUD_FX_LOW;
    if (fx == hud_fx) return;
    hud_fx = fx;
    const uint16_t *c = fx & HUD_FX_WHITE ? white_tint
                      : fx & HUD_FX_SHIELD ? shield_tint
                      : &pal_base_obj[(SPR_PAL_PLAYER << 2) + 1];
    uint16_t *p = &pal_obj[(SPR_PAL_PLAYER << 2) + 1];
    p[0] = c[0];
    p[1] = c[1];
    p[2] = c[2];
    pal_bg[(BG_PAL_HEALTH << 2) + 2] = fx & HUD_FX_LOW ? LOW_PULSE : pal_base_bg[(BG_PAL_HEALTH << 2) + 2];
    pal_dirty = 1;
}
