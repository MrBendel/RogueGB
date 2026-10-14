#pragma bank 255
// The HUD row: see hud.h. Banked (once a frame: a few compares unless a value
// changed). Every HUD tile is a background tile in the Color's second VRAM bank
// (window tiles pick their bank in the attribute map), so the HUD takes none of
// the scene's tiles. Rows are rebuilt in RAM when something changes and copied
// to the window map in vblank.
//
// Layout (20 columns, 2 rows; a 1 px ink line runs along the top):
//   0       shield icon (row 0) / heart (row 1); column 7 is the bars' left edge
//   1-11    the bars: shield (row 0, hanging from the line) over health (row 1)
//   12      the end caps (at most)
//   14-15   the held weapon's icon (16x16, its right-facing frame)
//   17-19   rounds left, in tall 8x16 digits
// Switching weapons shows its name over columns 13-19 for a second.
#include <gb/gb.h>
#include <gb/cgb.h>
#include "hud.h"
#include "health.h"
#include "gfx.h"
#include "weapon.h"
#include "font.h"

#define WIN_MAP ((uint8_t *)0x9C00)
#define BAR_COLS 11              // cells: 8 px each
#define BAR_PX (BAR_COLS * 8)    // the longest bar; past it, bars scale
#define ROW_LEN (BAR_COLS + 2)   // icon, cells, end cap
#define RIGHT_X ROW_LEN          // the weapon side
#define RIGHT_LEN (SCREEN_TW - ROW_LEN)
#define ATTR_BANK1 0x08          // attribute bits: the tile is in VRAM bank 1 ...
#define ATTR_FLIPX 0x20          // ... drawn mirrored
#define SLOT_CELLS 5             // the empty shield slot's length (the first shield's)
#define NAME_FRAMES 60           // how long a switched-to weapon's name shows

// HUD tiles (VRAM bank 1, background tiles 0..)
#define T_BLANK 0
#define T_FILL 1        // + px filled, 0..8 (an ink edge on rows 0 and 7)
#define T_CAP 10        // a bar's right end
#define T_SHIELD 11
#define T_HEART 12
#define T_BROKEN 13     // the shield icon, cracked: empty and recharging
#define T_LINE 14       // just the top line
#define T_CAP_LINE 15   // the shield bar's right end (with the line)
#define T_SLOT 16       // no shield yet: a dashed empty cell ...
#define T_SLOT_CAP 17   // ... its end ...
#define T_SLOT_ICON 18  // ... and the shield's outline
#define T_GUN 20        // 4: left column top, bottom, right column top, bottom
#define T_DIGIT 24      // 20: digit d's top half at T_DIGIT + 2d, bottom half next
#define T_FONT 64       // 64: the font (ASCII 32-95) with the top line, for names

uint8_t hud_shield_px, hud_shield_len, hud_shield_icon;
// The values the bars were last built from: when none changed (most frames),
// the bars aren't recomputed (one of them divides). 0xFF forces a rebuild.
uint8_t hud_raw_shield, hud_raw_cap = 0xFF, hud_raw_hp, hud_raw_max = 0xFF;
uint8_t hud_hp_px, hud_hp_len;
uint8_t hud_fx;
uint8_t hud_tick;
uint8_t hud_ammo = 0xFF;       // rounds shown (0xFF: none, a weapon without a magazine)
uint8_t hud_gun = 0xFF;        // the weapon whose icon is loaded
uint8_t hud_name_t;            // frames left showing hud_text (a weapon's name, a message)
char hud_text[8];
uint8_t hud_rows[2][ROW_LEN];  // what the copies read: must last until vblank
uint8_t hud_right[2][RIGHT_LEN];

extern uint16_t pal_bg[PAL_COUNT * 4], pal_obj[PAL_COUNT * 4];  // gfx.c: on screen
extern volatile uint8_t pal_dirty;

// 8x8 drawings by color index (. = 0), with the top line where the row has it.
static const char art[8][8][9] = {
    // T_SHIELD
    {"33333333", ".33333.3", "311123.3", "312223.3", "312223.3", ".3223..3", "..33...3", ".......3"},
    // T_HEART
    {".......3", ".33.33.3", "312213.3", "322223.3", ".3223..3", "..33...3", ".......3", ".......3"},
    // T_BROKEN
    {"33333333", "333.33.3", "3..3.3.3", "3.3..3.3", ".3..3..3", "..33...3", ".......3", ".......3"},
    // T_LINE
    {"33333333", "........", "........", "........", "........", "........", "........", "........"},
    // T_CAP_LINE
    {"33333333", "3.......", "3.......", "3.......", "3.......", "3.......", "3.......", "3......."},
    // T_SLOT
    {"33333333", "........", "........", "........", "........", "........", "........", "2.2.2.2."},
    // T_SLOT_CAP
    {"33333333", "........", "2.......", "........", "2.......", "........", "2.......", "........"},
    // T_SLOT_ICON
    {"33333333", ".22222.2", "2....2..", "2....2.2", ".2..2...", "..22...2", "........", ".......2"},
};

static void put_tiles(uint8_t first, uint8_t n, const uint8_t *data) {
    VBK_REG = 1;
    set_bkg_data(first, n, data);
    VBK_REG = 0;
}

void hud_init(void) BANKED {
    uint8_t t[16 * 10];
    // T_BLANK and the bar cells: an ink edge (rows 0, 7) around n px filled from
    // the left: a highlight row (color 1) over the body (color 2).
    for (uint8_t i = 0; i < 16; i++) t[i] = 0;
    put_tiles(T_BLANK, 1, t);
    uint8_t *p = t;
    for (uint8_t n = 0; n <= 8; n++) {
        uint8_t m = (uint8_t)(0xFF00 >> n);
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t lo = 0, hi = 0;
            if (r == 0 || r == 7) lo = hi = 0xFF;
            else if (r == 1) lo = m;
            else hi = m;
            *p++ = lo;
            *p++ = hi;
        }
    }
    put_tiles(T_FILL, 9, t);
    // T_CAP: the right edge, column 0, full height.
    for (uint8_t i = 0; i < 16; i++) t[i] = 0x80;
    put_tiles(T_CAP, 1, t);
    // The drawings.
    for (uint8_t i = 0; i < 8; i++) {
        p = t;
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t lo = 0, hi = 0;
            for (uint8_t x = 0; x < 8; x++) {
                char c = art[i][r][x];
                uint8_t bit = 0x80 >> x;
                if (c == '1' || c == '3') lo |= bit;
                if (c == '2' || c == '3') hi |= bit;
            }
            *p++ = lo;
            *p++ = hi;
        }
        put_tiles(i < 3 ? T_SHIELD + i : T_LINE + i - 3, 1, t);
    }
    // Tall digits: the font's, each row doubled; the top half gets the line.
    for (uint8_t d = 0; d < 10; d++) {
        uint8_t g[16];
        banked_memcpy(g, font_tiles + (uint16_t)('0' - ' ' + d) * 16, 16, BANK(font));
        for (uint8_t r = 0; r < 16; r++) {
            uint8_t src = (r >> 1) << 1;  // glyph row r/2
            t[r * 2] = g[src];
            t[r * 2 + 1] = g[src + 1];
        }
        t[0] = t[1] = 0xFF;
        put_tiles(T_DIGIT + d * 2, 2, t);
    }
    // The font with the top line, for weapon names.
    for (uint8_t c = 0; c < FONT_TILE_COUNT; c++) {
        banked_memcpy(t, font_tiles + (uint16_t)c * 16, 16, BANK(font));
        t[0] = t[1] = 0xFF;
        put_tiles(T_FONT + c, 1, t);
    }
    hud_gun = 0xFF;
}

// The held weapon's icon: its right-facing frame (direction 0), both 8x16
// columns copied from its art into T_GUN, with the line over the top. Only on a
// switch (direct VRAM writes: a few hundred cycles of waiting).
static void load_gun_icon(void) {
    uint8_t t[32];
    for (uint8_t col = 0; col < 2; col++) {
        uint8_t c = gun_frames[col << 1];
        if (c == gun_blank) {
            for (uint8_t i = 0; i < 32; i++) t[i] = 0;
        } else {
            banked_memcpy(t, gun_tiles + (uint16_t)c * 32, 32, gun_tiles_bank);
        }
        t[0] = t[1] = 0xFF;
        put_tiles(T_GUN + (col << 1), 2, t);
    }
    // Mirrored columns (the frame tables reuse art flipped).
    VBK_REG = 1;
    for (uint8_t col = 0; col < 2; col++) {
        uint8_t a = BG_PAL_GUN | ATTR_BANK1 | (gun_frames[(col << 1) + 1] & S_FLIPX ? ATTR_FLIPX : 0);
        fill_win_rect(RIGHT_X + 1 + col, 0, 1, 2, a);
    }
    VBK_REG = 0;
}

// Cells of outline for a max value: 1 px per point, in whole cells, up to BAR_PX.
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

// One bar's row: icon, cells, the end cap; past it, empty (with the line on top
// for the shield's row).
static void build_row(uint8_t *r, uint8_t icon, uint8_t px, uint8_t len, uint8_t line) {
    uint8_t cells = len >> 3;
    r[0] = icon;
    for (uint8_t i = 0; i < BAR_COLS + 1; i++) {
        uint8_t t = line ? T_LINE : T_BLANK;
        if (i < cells) {
            t = px >= 8 ? 8 : px;
            px -= t;
            t += T_FILL;
        } else if (i == cells) {
            t = line ? T_CAP_LINE : T_CAP;
        }
        r[1 + i] = t;
    }
}

// The weapon side: the name for a moment after a switch, else the icon and the
// rounds left.
static void build_right(void) {
    uint8_t *top = hud_right[0], *bot = hud_right[1];
    for (uint8_t i = 0; i < RIGHT_LEN; i++) {
        top[i] = T_LINE;
        bot[i] = T_BLANK;
    }
    if (hud_name_t) {
        for (uint8_t i = 0; i < RIGHT_LEN && hud_text[i]; i++) top[i] = T_FONT + (hud_text[i] - ' ');
        return;
    }
    top[1] = T_GUN;
    bot[1] = T_GUN + 1;
    top[2] = T_GUN + 2;
    bot[2] = T_GUN + 3;
    uint8_t ammo = hud_ammo;
    if (ammo == 0xFF) return;
    uint8_t h = 0, tens = 0;
    while (ammo >= 100) { ammo -= 100; h++; }
    while (ammo >= 10) { ammo -= 10; tens++; }
    if (h) {
        top[4] = T_DIGIT + (h << 1);
        bot[4] = T_DIGIT + (h << 1) + 1;
    }
    if (h || tens) {
        top[5] = T_DIGIT + (tens << 1);
        bot[5] = T_DIGIT + (tens << 1) + 1;
    }
    top[6] = T_DIGIT + (ammo << 1);
    bot[6] = T_DIGIT + (ammo << 1) + 1;
}

void hud_draw(void) BANKED {
    palettes_window_hud();
    // Palette and VRAM bank in the attribute map: every HUD tile is in bank 1.
    VBK_REG = 1;
    fill_win_rect(0, 0, ROW_LEN, 1, BG_PAL_SHIELD | ATTR_BANK1);
    fill_win_rect(0, 1, ROW_LEN, 1, BG_PAL_HEALTH | ATTR_BANK1);
    fill_win_rect(RIGHT_X, 0, RIGHT_LEN, 2, BG_PAL_GUN | ATTR_BANK1);
    VBK_REG = 0;
    hud_shield_px = hud_hp_px = 0xFF;  // redraw everything
    hud_raw_cap = hud_raw_max = 0xFF;
    hud_ammo = 0xFE;
    hud_gun = 0xFF;
    hud_update();  // (hud_gun 0xFF: the icon loads without showing the name)
}

void hud_say(const char *s) BANKED {
    uint8_t i = 0;
    for (; i < 7 && s[i]; i++) hud_text[i] = s[i];
    for (; i < 8; i++) hud_text[i] = i < 7 ? ' ' : 0;
    hud_name_t = NAME_FRAMES;
    hud_ammo = 0xFE;  // the weapon side redraws at the next update
}

void hud_hide(void) BANKED {
    palettes_window_ui();
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, 2, BG_PAL_UI);
    VBK_REG = 0;
}

static const uint16_t shield_tint[3] = {RGB8(216, 248, 255), RGB8(96, 176, 248), RGB8(24, 48, 128)};
static const uint16_t white_tint[3] = {RGB8(255, 255, 255), RGB8(255, 255, 255), RGB8(160, 200, 255)};
#define LOW_PULSE RGB8(255, 136, 120)

void hud_update(void) BANKED {
    // The shield's row: an empty dashed slot until there's a shield.
    uint8_t len, px;
    if (shield_hp == hud_raw_shield && shield_cap == hud_raw_cap) goto health;
    hud_raw_shield = shield_hp;
    hud_raw_cap = shield_cap;
    len = shield_cap ? bar_len(shield_cap) : 0;
    px = len ? bar_fill(shield_hp, shield_cap, len) : 0;
    uint8_t icon = !shield_cap ? T_SLOT_ICON : shield_hp ? T_SHIELD : T_BROKEN;
    if (px != hud_shield_px || len != hud_shield_len || icon != hud_shield_icon) {
        hud_shield_px = px;
        hud_shield_len = len;
        hud_shield_icon = icon;
        uint8_t *r = hud_rows[0];
        if (shield_cap) {
            build_row(r, icon, px, len, 1);
        } else {
            r[0] = T_SLOT_ICON;
            for (uint8_t i = 1; i < ROW_LEN; i++) r[i] = i <= SLOT_CELLS ? T_SLOT : i == SLOT_CELLS + 1 ? T_SLOT_CAP : T_LINE;
        }
        gfx_vram_copy(WIN_MAP, r, ROW_LEN, 0);
    }
health:
    if (player_hp == hud_raw_hp && player_hp_max == hud_raw_max) goto weapon;
    hud_raw_hp = player_hp;
    hud_raw_max = player_hp_max;
    len = bar_len(player_hp_max);
    px = bar_fill(player_hp, player_hp_max, len);
    if (px != hud_hp_px || len != hud_hp_len) {
        hud_hp_px = px;
        hud_hp_len = len;
        build_row(hud_rows[1], T_HEART, px, len, 0);
        gfx_vram_copy(WIN_MAP + 32, hud_rows[1], ROW_LEN, 0);
    }

weapon:;
    // The weapon side: a new weapon loads its icon and shows its name a while.
    uint8_t right = 0;
    if (weapon_cur != hud_gun) {
        if (hud_gun != 0xFF) {
            weapon_get_name(hud_text);
            hud_name_t = NAME_FRAMES;
        }
        hud_gun = weapon_cur;
        load_gun_icon();
        right = 1;
    }
    if (hud_name_t && !--hud_name_t) right = 1;
    uint8_t ammo = wpn.mag ? weapon_ammo[weapon_cur] : 0xFF;
    if (ammo != hud_ammo) {
        hud_ammo = ammo;
        right = 1;
    }
    if (right) {
        build_right();
        gfx_vram_copy(WIN_MAP + RIGHT_X, hud_right[0], RIGHT_LEN, 0);
        gfx_vram_copy(WIN_MAP + 32 + RIGHT_X, hud_right[1], RIGHT_LEN, 0);
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
