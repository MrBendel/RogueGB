#pragma bank 255
// The pause menu's pages (menu.h). Banked: only while paused. The display is on,
// so text goes straight to VRAM (a few frames of writes when a page is drawn).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "menu.h"
#include "text.h"
#include "gfx.h"
#include "input.h"
#include "save.h"
#include "progress.h"
#include "skills.h"
#include "skilldata.h"
#include "sfx.h"
#include "settings.h"
#include "health.h"
#include "loot.h"

// Ink palettes (background palettes 0-5, borrowed; 6 is the panel's own).
#define P_INK BG_PAL_UI
#define P_EPIC 0    // a purple gun
#define P_DIM 1     // locked skills, labels
#define P_LIT 2     // learned skills, the XP bar
#define P_SEL 3     // the cursor, the page's tab; an orange (legendary) gun
#define P_GREEN 4   // an uncommon gun
#define P_BLUE 5    // a rare gun
#define BORROWED 6  // palettes 0-5
#define C(r, g, b) RGB8(r, g, b)
#define PAPER C(248, 240, 216)
static const uint16_t inks[BORROWED][4] = {
    {PAPER, C(220, 180, 240), C(170, 110, 220), C(130, 50, 180)},
    {PAPER, C(200, 184, 152), C(176, 164, 140), C(150, 136, 116)},
    {PAPER, C(152, 232, 216), C(32, 150, 140), C(16, 96, 92)},
    {PAPER, C(255, 200, 160), C(208, 96, 40), C(184, 64, 24)},
    {PAPER, C(180, 230, 170), C(70, 160, 70), C(40, 120, 40)},
    {PAPER, C(170, 200, 250), C(70, 120, 220), C(40, 80, 190)},
};
static const uint8_t rarity_pal[RARITIES] = {P_INK, P_GREEN, P_BLUE, P_EPIC, P_SEL};
// HUD bar tiles in the second VRAM bank (hud.c): T_FILL + n = a cell n px full.
#define T_FILL 1
extern uint16_t enemies_killed;  // aim.c: this run's
#define XP_CELLS 10

uint8_t menu_page;
uint8_t menu_skill;
uint8_t menu_option;
Gun shop_stock[SHOP_STOCK];
uint8_t shop_sel, shop_sold, shop_mode;
uint8_t sell_top;    // the sell list's first row (it scrolls)
uint8_t gear_sel, gear_pick = GEAR_NONE, gear_top;  // gear_top: the backpack list's first row
uint8_t gear_tiles;  // the gear detail's 2 lines (redrawn in place)
uint16_t guns_bought;
uint8_t shop_tiles;  // where the detail lines are drawn (redrawn in place)
uint8_t shop_marker; // the cursor's tile
uint16_t menu_saved_pal[BORROWED * 4];  // background palettes 0-5 as they were
// Text tiles (second VRAM bank): the HUD's 12-127, then 192-253.
uint8_t tiles_a, tiles_b;
uint8_t detail_tiles;  // the skill detail's 4 lines, 19 tiles each (redrawn in place)
uint8_t cell_tiles[SKILLS_PER_CLASS];  // each grid label's width in tiles (to recolor just those)

static uint8_t alloc(uint8_t n) {
    uint8_t t;
    if (tiles_a + n <= 128) {
        t = tiles_a;
        tiles_a += n;
    } else {
        if (tiles_b + n > 254) return 0;  // full: drawn over (never, for these pages)
        t = tiles_b;
        tiles_b += n;
    }
    return t;
}

// Text in a RAM buffer at pixel x, tile row ty, in palette pal; tiles from the pool
// (or `at`, if not 0xFF). Returns its width in tiles.
static uint8_t say(uint8_t x, uint8_t ty, const char *s, uint8_t pal, uint8_t at) {
    uint8_t w = text_width(FONT_COMPACT, s);
    if (!w) return 0;
    uint8_t n = ((x & 7) + w + 7) >> 3;
    text_on_window = 1;
    text_draw(FONT_COMPACT, x, ty, s, at != 0xFF ? at : alloc(n), pal | TEXT_BANK1);
    text_on_window = 0;
    return n;
}

// The same, from this bank's constants (copied to RAM for text.c).
static void say_c(uint8_t x, uint8_t ty, const char *s, uint8_t pal) {
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    while (s[n] && n < TEXT_MAX) line[n] = s[n], n++;
    line[n] = 0;
    say(x, ty, line, pal, 0xFF);
}

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, uint16_t n, uint8_t digits) {
    char d[5];
    uint8_t k = 0;
    do {
        d[k++] = '0' + n % 10;
        n /= 10;
    } while (n || k < digits);
    while (k) *p++ = d[--k];
    return p;
}

// A 32-bit number by subtraction (no 32-bit division: the library's would land in bank 0).
static char *put_u32(char *p, uint32_t n) {
    static const uint32_t pow10[] = {1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};
    uint8_t started = 0;
    for (uint8_t i = 0; i < 10; i++) {
        char d = '0';
        while (n >= pow10[i]) n -= pow10[i], d++;
        if (d != '0' || started || i == 9) *p++ = d, started = 1;
    }
    return p;
}

static void clear_inside(void) {
    fill_win_rect(1, 2, SCREEN_TW - 2, SCREEN_TH - 3, FRAME_TILES + 4);
    VBK_REG = 1;
    fill_win_rect(1, 2, SCREEN_TW - 2, SCREEN_TH - 3, P_INK);
    VBK_REG = 0;
}

void menu_tabs(void) BANKED {
    tiles_a = 12;
    tiles_b = 192;
    fill_win_rect(1, 1, SCREEN_TW - 2, 1, FRAME_TILES + 4);
    VBK_REG = 1;
    fill_win_rect(1, 1, SCREEN_TW - 2, 1, P_INK);
    VBK_REG = 0;
    // (Each tab starts in its own tile: a tile is one palette.)
    say_c(8, 1, "CHARACTER", menu_page == PAGE_CHARACTER ? P_SEL : P_DIM);
    say_c(48, 1, "GEAR", menu_page == PAGE_GEAR ? P_SEL : P_DIM);
    say_c(72, 1, "SKILLS", menu_page == PAGE_SKILLS ? P_SEL : P_DIM);
    say_c(104, 1, "OPTIONS", menu_page == PAGE_OPTIONS ? P_SEL : P_DIM);
    if (save_slot == SAVE_NONE) say_c(136, 1, "TUNE", menu_page == PAGE_TUNE ? P_SEL : P_DIM);
}

void menu_open(void) BANKED {
    memcpy(menu_saved_pal, pal_base_bg, sizeof(menu_saved_pal));
    memcpy(pal_base_bg, inks, sizeof(inks));
    gfx_set_fade(0);
    win_frame(0, 0, SCREEN_TW, SCREEN_TH);
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, SCREEN_TH, BG_PAL_UI);  // (a dialogue may have left bank-1 cells)
    VBK_REG = 0;
    if (menu_page != PAGE_TUNE) menu_draw();
}

void menu_close(void) BANKED {
    memcpy(pal_base_bg, menu_saved_pal, sizeof(menu_saved_pal));
    gfx_set_fade(0);
}

// --- the character page ---

static void draw_character(void) {
    char line[TEXT_MAX + 1], name[CLASS_ROLE_MAX + 1];
    char *p;
    // The class, and its role.
    banked_memcpy(name, class_name[profile.cls], CLASS_NAME_MAX + 1, BANK(skilldata));
    p = put(line, name);
    p = put(p, " - ");
    banked_memcpy(name, class_role[profile.cls], CLASS_ROLE_MAX + 1, BANK(skilldata));
    p = put(p, name);
    *p = 0;
    say(8, 3, line, P_INK, 0xFF);
    // Level and XP, with a bar.
    p = put(line, "LEVEL ");
    p = put_num(p, profile.level, 1);
    *p = 0;
    say(8, 4, line, P_INK, 0xFF);
    uint16_t need = xp_need(profile.level);
    if (need) {
        p = put(line, "XP ");
        p = put_num(p, profile.xp, 1);
        p = put(p, " of ");
        p = put_num(p, need, 1);
    } else {
        p = put(line, "MAX LEVEL");
    }
    *p = 0;
    say(64, 4, line, P_DIM, 0xFF);
    // The bar's pixels: XP_CELLS * 8 of them, in 16-bit math.
    uint16_t px = !need ? XP_CELLS * 8 : need < 800 ? profile.xp * (XP_CELLS * 8) / need : profile.xp / (need / (XP_CELLS * 8));
    if (px > XP_CELLS * 8) px = XP_CELLS * 8;
    uint8_t cells[XP_CELLS];
    for (uint8_t i = 0; i < XP_CELLS; i++) {
        uint8_t f = px >= 8 ? 8 : (uint8_t)px;
        cells[i] = T_FILL + f;
        px -= f;
    }
    set_win_tiles(1, 5, XP_CELLS, 1, cells);
    memset(cells, P_LIT | TEXT_BANK1, XP_CELLS);
    VBK_REG = 1;
    set_win_tiles(1, 5, XP_CELLS, 1, cells);
    VBK_REG = 0;
    // Health and shield; credits and time; depth and kills (two columns).
    p = put(line, "HEALTH ");
    p = put_num(p, player_hp, 1);
    p = put(p, " of ");
    p = put_num(p, player_hp_max, 1);
    *p = 0;
    say(8, 7, line, P_INK, 0xFF);
    p = put(line, "SHIELD ");
    p = shield_cap ? put_num(p, shield_cap, 1) : put(p, "none");
    *p = 0;
    say(96, 7, line, P_INK, 0xFF);
    p = put(line, "CREDITS ");
    p = put_u32(p, profile.credits);
    *p = 0;
    say(8, 8, line, P_INK, 0xFF);
    p = put(line, "TIME ");
    p = put_num(p, profile.play_minutes / 60, 1);
    *p++ = ':';
    p = put_num(p, profile.play_minutes % 60, 2);
    *p = 0;
    say(96, 8, line, P_INK, 0xFF);
    p = put(line, "DEEPEST ");
    p = put_num(p, profile.best_depth, 1);
    *p = 0;
    say(8, 9, line, P_INK, 0xFF);
    p = put(line, "KILLS ");
    p = put_num(p, profile.kills + enemies_killed, 1);  // (the file's, plus this run's)
    *p = 0;
    say(96, 9, line, P_INK, 0xFF);
    p = put(line, "SKILL POINTS ");
    p = put_num(p, profile.skill_points, 1);
    *p = 0;
    say(8, 10, line, profile.skill_points ? P_LIT : P_DIM, 0xFF);
    // The loadout: each gun in its rarity's color, the one held marked.
    say_c(8, 12, "LOADOUT", P_DIM);
    for (uint8_t s = 0; s < LOADOUT; s++) {
        const Gun *g = &profile.guns[s];
        if (g->type == GUN_NONE) {
            say_c(16, 13 + s, "-", P_DIM);
            continue;
        }
        p = line;
        gun_name(p, g);
        while (*p) p++;
        p = put(p, "  LV ");
        p = put_num(p, g->level, 1);
        *p = 0;
        say(16, 13 + s, line, rarity_pal[g->rarity], 0xFF);
        if (s == profile.gun_sel && save_slot != SAVE_NONE) say_c(120, 13 + s, "held", P_DIM);
    }
}

// --- the options page ---

#define OPTIONS 3
#define OPT_VALUE_X 96
static const char *const text_speeds[TEXT_SPEEDS] = {"SLOW", "NORMAL", "FAST", "INSTANT"};
static const char *const sounds[SOUND_LEVELS] = {"OFF", "LOW", "FULL"};
uint8_t opt_tiles[OPTIONS];  // where each option's value is drawn (redrawn in place)

static void draw_option(uint8_t i) {
    static const char *const labels[OPTIONS] = {"TEXT SPEED", "SCREEN SHAKE", "SOUND"};
    uint8_t row = 4 + (i << 1), pal = i == menu_option ? P_SEL : P_INK;
    fill_win_rect(1, row, SCREEN_TW - 2, 1, FRAME_TILES + 4);
    VBK_REG = 1;
    fill_win_rect(1, row, SCREEN_TW - 2, 1, P_INK);
    VBK_REG = 0;
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    const char *l = labels[i];
    while (l[n]) line[n] = l[n], n++;
    line[n] = 0;
    say(16, row, line, pal, opt_tiles[i]);
    const char *v = i == 0 ? text_speeds[settings.text_speed] : i == 1 ? (settings.shake ? "ON" : "OFF") : sounds[settings.sound];
    n = 0;
    while (v[n]) line[n] = v[n], n++;
    line[n] = 0;
    say(OPT_VALUE_X, row, line, pal, opt_tiles[i] + 8);
}

static void draw_options(void) {
    for (uint8_t i = 0; i < OPTIONS; i++) {
        opt_tiles[i] = alloc(14);  // a label (up to 8 tiles) and its value (6)
        draw_option(i);
    }
    say_c(16, 12, "UP DOWN: choose", P_DIM);
    say_c(16, 13, "LEFT RIGHT: change", P_DIM);
    say_c(16, 15, "START: back to the game", P_DIM);
    say_c(16, 16, save_slot != SAVE_NONE ? "SELECT: save and quit" : "SELECT: end the run", P_DIM);
}

static void options_update(void) {
    uint8_t old = menu_option;
    if ((joy_pressed & J_UP) && menu_option) menu_option--;
    else if ((joy_pressed & J_DOWN) && menu_option < OPTIONS - 1) menu_option++;
    if (old != menu_option) {
        draw_option(old);
        draw_option(menu_option);
        return;
    }
    int8_t d = joy_pressed & J_RIGHT ? 1 : joy_pressed & J_LEFT ? -1 : 0;
    if (!d) return;
    if (menu_option == 0) settings.text_speed = (settings.text_speed + TEXT_SPEEDS + d) % TEXT_SPEEDS;
    else if (menu_option == 1) settings.shake ^= 1;
    else settings.sound = (settings.sound + SOUND_LEVELS + d) % SOUND_LEVELS;
    settings_apply();
    settings_save();
    sfx_play(SFX_TEXT);  // (heard at the new volume)
    draw_option(menu_option);
}

// --- the skills page ---

static uint8_t cell_x(uint8_t branch) {
    return 8 + branch * 48;  // on tile edges: a tile is one palette, so cells must not share one
}

static uint8_t cell_pal(uint8_t k) {
    if (k == menu_skill) return P_SEL;
    uint8_t s = skill_state(k);
    return s == SKILL_LEARNED ? P_LIT : s == SKILL_OPEN ? P_INK : P_DIM;
}

static void draw_detail(void) {
    char line[TEXT_MAX + 1];
    for (uint8_t r = 0; r < 4; r++) {
        fill_win_rect(1, 13 + r, SCREEN_TW - 2, 1, FRAME_TILES + 4);
        VBK_REG = 1;
        fill_win_rect(1, 13 + r, SCREEN_TW - 2, 1, P_INK);
        VBK_REG = 0;
    }
    banked_memcpy(line, skill_name[profile.cls][menu_skill], SKILL_NAME_MAX + 1, BANK(skilldata));
    say(12, 13, line, P_SEL, detail_tiles);
    for (uint8_t i = 0; i < 2; i++) {
        banked_memcpy(line, skill_text[profile.cls][menu_skill][i], SKILL_LINE_MAX + 1, BANK(skilldata));
        say(12, 14 + i, line, P_INK, detail_tiles + 19 * (i + 1));
    }
    uint8_t s = skill_state(menu_skill);
    const char *status = s == SKILL_LEARNED ? "Learned."
                       : s == SKILL_LOCKED ? "Learn the one above it first."
                       : profile.skill_points ? "A: learn it (1 point)."
                                              : "No skill points: level up.";
    char st[TEXT_MAX + 1];
    uint8_t n = 0;
    while (status[n]) st[n] = status[n], n++;
    st[n] = 0;
    say(12, 16, st, s == SKILL_OPEN && profile.skill_points ? P_LIT : P_DIM, detail_tiles + 57);
}

static void draw_skills(void) {
    char line[TEXT_MAX + 1], name[CLASS_NAME_MAX + 1];
    detail_tiles = alloc(19 * 4);
    banked_memcpy(name, class_name[profile.cls], CLASS_NAME_MAX + 1, BANK(skilldata));
    say(12, 3, name, P_INK, 0xFF);
    char *p = put(line, "POINTS ");
    p = put_num(p, profile.skill_points, 1);
    *p = 0;
    say(100, 3, line, profile.skill_points ? P_LIT : P_DIM, 0xFF);
    for (uint8_t b = 0; b < 3; b++) {
        banked_memcpy(line, branch_name[profile.cls][b], BRANCH_NAME_MAX + 1, BANK(skilldata));
        say(cell_x(b), 5, line, P_DIM, 0xFF);
        for (uint8_t t = 0; t < 3; t++) {
            uint8_t k = b * 3 + t;
            banked_memcpy(line, skill_label[profile.cls][k], SKILL_NAME_MAX + 1, BANK(skilldata));
            cell_tiles[k] = say(cell_x(b), 7 + (t << 1), line, cell_pal(k), 0xFF);
        }
    }
    draw_detail();
}

// A cell's palette, after the cursor or a skill changed: its tiles' attributes.
static void repaint(uint8_t k) {
    uint8_t b = k / 3, t = k % 3;
    VBK_REG = 1;
    fill_win_rect(cell_x(b) >> 3, 7 + (t << 1), cell_tiles[k], 1, cell_pal(k) | TEXT_BANK1);
    VBK_REG = 0;
}

static void clear_row(uint8_t row);
static void draw_shop(void);
static void draw_gear(void);

void menu_draw(void) BANKED {
    clear_inside();
    if (menu_page == PAGE_SHOP) {
        clear_row(1);
        draw_shop();
        return;
    }
    menu_tabs();
    if (menu_page == PAGE_CHARACTER) draw_character();
    else if (menu_page == PAGE_GEAR) draw_gear();
    else if (menu_page == PAGE_OPTIONS) draw_options();
    else draw_skills();
}

// --- lists of guns (the gear page, the shop) ---

// A gun's name and level at pixel 16 of row ty (in its rarity's ink, or pal); "-": none.
static void gun_row(uint8_t ty, const Gun *g, uint8_t pal) {
    char line[TEXT_MAX + 1];
    if (g->type == GUN_NONE) {
        say_c(16, ty, "-", P_DIM);
        return;
    }
    gun_name(line, g);
    char *p = line;
    while (*p) p++;
    p = put(p, "  LV ");
    p = put_num(p, g->level, 1);
    *p = 0;
    say(16, ty, line, pal == 0xFF ? rarity_pal[g->rarity] : pal, 0xFF);
}

// The cursor: an italic arrow in a row's first column. Returns its tile.
static uint8_t arrow_at(uint8_t ty) {
    uint8_t t = alloc(1);
    char arrow[2] = ">";
    text_on_window = 1;
    text_draw(FONT_ITALIC, 8, ty, arrow, t, P_SEL | TEXT_BANK1);
    text_on_window = 0;
    return t;
}

static void move_arrow(uint8_t marker, uint8_t from, uint8_t to) {
    set_win_tile_xy(1, from, FRAME_TILES + 4);
    VBK_REG = 1;
    set_win_tile_xy(1, from, P_INK);
    set_win_tile_xy(1, to, P_SEL | TEXT_BANK1);
    VBK_REG = 0;
    set_win_tile_xy(1, to, marker);
}

// say_c into fixed tiles (redrawn in place).
static void say_at(uint8_t x, uint8_t ty, const char *s, uint8_t pal, uint8_t at) {
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    while (s[n] && n < TEXT_MAX) line[n] = s[n], n++;
    line[n] = 0;
    say(x, ty, line, pal, at);
}

// Arrows on the frame when a list has more above or below.
static void scroll_marks(uint8_t row0, uint8_t rows, uint8_t top, uint8_t count) {
    set_win_tile_xy(SCREEN_TW - 1, row0, top ? FONT_TILES + ('^' - ' ') : FRAME_TILES + 5);
    set_win_tile_xy(SCREEN_TW - 1, row0 + rows - 1, top + rows < count ? FONT_TILES + ('V' - ' ') : FRAME_TILES + 5);
}

// --- the gear page: the loadout and the backpack ---

#define GEAR_LOAD_ROW 3   // the loadout's rows: 3-6
#define GEAR_PACK_HEAD 8
#define GEAR_PACK_ROW 9   // the backpack's visible rows: 9-12
#define GEAR_ROWS 4
#define GEAR_DETAIL 14    // two lines, then the help line (16)

uint8_t gear_marker;

static Gun *gear_gun(uint8_t i) {
    return i < LOADOUT ? &profile.guns[i] : &profile.pack[i - LOADOUT];
}

static uint8_t gear_row(uint8_t i) {
    return i < LOADOUT ? GEAR_LOAD_ROW + i : GEAR_PACK_ROW + (i - LOADOUT - gear_top);
}

static void gear_detail(void) {
    char l1[TEXT_MAX + 1], l2[TEXT_MAX + 1];
    clear_row(GEAR_DETAIL);
    clear_row(GEAR_DETAIL + 1);
    const Gun *g = gear_gun(gear_sel);
    if (g->type == GUN_NONE) {
        say_at(8, GEAR_DETAIL, "Empty.", P_DIM, gear_tiles);
        return;
    }
    gun_card_lines(g, l1, l2);  // (against the gun held)
    say(8, GEAR_DETAIL, l1, P_INK, gear_tiles);
    say(8, GEAR_DETAIL + 1, l2, P_INK, gear_tiles + 19);
}

static void draw_gear(void) {
    char line[TEXT_MAX + 1];
    char *p;
    uint8_t total = LOADOUT + profile.pack_size;
    if (gear_sel >= total) gear_sel = 0;
    if (gear_pick >= total) gear_pick = GEAR_NONE;
    gear_tiles = alloc(19 * 2);
    say_c(8, 2, "LOADOUT", P_DIM);
    uint8_t used = 0;
    for (uint8_t s = 0; s < profile.pack_size; s++) if (profile.pack[s].type != GUN_NONE) used++;
    p = put(line, "BACKPACK ");
    p = put_num(p, used, 1);
    p = put(p, " of ");
    p = put_num(p, profile.pack_size, 1);
    *p = 0;
    say(8, GEAR_PACK_HEAD, line, P_DIM, 0xFF);
    for (uint8_t i = 0; i < total; i++) {
        if (i >= LOADOUT && (i - LOADOUT < gear_top || i - LOADOUT >= gear_top + GEAR_ROWS)) continue;
        gun_row(gear_row(i), gear_gun(i), i == gear_pick ? P_SEL : 0xFF);
        if (i == profile.gun_sel && save_slot != SAVE_NONE) say_c(128, gear_row(i), "held", P_DIM);
    }
    scroll_marks(GEAR_PACK_ROW, GEAR_ROWS, gear_top, profile.pack_size);
    gear_marker = arrow_at(gear_row(gear_sel));
    gear_detail();
    say_c(8, 16, gear_pick == GEAR_NONE ? "A: move a gun    B: next page" : "A: swap with it    B: put back", P_DIM);
}

// Swap two slots' guns (loadout or backpack), keeping a gun in your hands.
static void gear_swap(uint8_t a, uint8_t b) {
    Gun *ga = gear_gun(a), *gb = gear_gun(b);
    Gun t = *ga;
    *ga = *gb;
    *gb = t;
    if (loadout_free() == 0 && profile.guns[1].type == GUN_NONE && profile.guns[2].type == GUN_NONE &&
        profile.guns[3].type == GUN_NONE) {
        *gb = *ga;  // that was the loadout's last gun: no
        *ga = t;
        sfx_play(SFX_RELOAD);
        return;
    }
    // Magazines: loadout guns take theirs along; one out of the backpack comes loaded.
    if (gun_mods) slot_ammo[profile.gun_sel] = weapon_ammo[weapon_cur];
    if (a < LOADOUT && b < LOADOUT) {
        uint8_t m = slot_ammo[a];
        slot_ammo[a] = slot_ammo[b];
        slot_ammo[b] = m;
    } else {
        slot_ammo[a < LOADOUT ? a : b] = 255;
    }
    // The held slot again (its gun may have changed); emptied: the next one.
    gun_mods = 0;
    if (profile.guns[profile.gun_sel].type == GUN_NONE) loadout_next();
    else loadout_select(profile.gun_sel);
    sfx_play(SFX_PICKUP);
    save_write();
}

static void gear_update(void) {
    uint8_t total = LOADOUT + profile.pack_size, old = gear_sel;
    if ((joy_pressed & J_UP) && gear_sel) gear_sel--;
    else if ((joy_pressed & J_DOWN) && gear_sel < total - 1) gear_sel++;
    else if (joy_pressed & J_A) {
        if (gear_pick == GEAR_NONE) {
            if (gear_gun(gear_sel)->type == GUN_NONE) {
                sfx_play(SFX_RELOAD);  // nothing there to move
                return;
            }
            gear_pick = gear_sel;
            sfx_play(SFX_TEXT);
        } else {
            if (gear_pick != gear_sel) gear_swap(gear_pick, gear_sel);
            gear_pick = GEAR_NONE;
        }
        menu_draw();
        return;
    }
    if (old == gear_sel) return;
    // Scrolled the backpack: the page again; else just the arrow and the detail.
    if (gear_sel >= LOADOUT) {
        uint8_t j = gear_sel - LOADOUT;
        if (j < gear_top || j >= gear_top + GEAR_ROWS) {
            gear_top = j < gear_top ? j : j - GEAR_ROWS + 1;
            menu_draw();
            return;
        }
    }
    move_arrow(gear_marker, gear_row(old), gear_row(gear_sel));
    gear_detail();
}

uint8_t menu_back(void) BANKED {
    if (menu_page != PAGE_GEAR || gear_pick == GEAR_NONE) return 0;
    gear_pick = GEAR_NONE;
    menu_draw();
    return 1;
}

// --- Marro's shop ---
// Buy: his stock (a row each, double-spaced), then the backpack upgrade.
// Sell: your backpack's guns, for a quarter of his price (a scrolling list).

#define SHOP_ROW0 4       // buy rows: 4, 6, 8, 10 (name and price on one line)
#define SELL_ROWS 5       // sell rows: 4-8
#define SHOP_DETAIL 12    // the row under the cursor: 2 lines, then what A does (15)
#define SHOP_PRICE_X 112

void shop_restock(void) BANKED {
    // At your level; deeper explorers get luckier stock.
    for (uint8_t i = 0; i < SHOP_STOCK; i++) loot_roll(&shop_stock[i], profile.level, profile.best_depth * 3);
    shop_sold = 0;
    shop_sel = 0;
    shop_mode = SHOP_BUY;
    sell_top = 0;
}

static void clear_row(uint8_t row) {
    fill_win_rect(1, row, SCREEN_TW - 2, 1, FRAME_TILES + 4);
    VBK_REG = 1;
    fill_win_rect(1, row, SCREEN_TW - 2, 1, P_INK);
    VBK_REG = 0;
}

static uint8_t shop_rows(void) {
    return shop_mode == SHOP_SELL ? profile.pack_size : SHOP_STOCK + (pack_upgrade_price() ? 1 : 0);
}

static uint8_t shop_row(uint8_t i) {
    return shop_mode == SHOP_SELL ? SHOP_ROW0 + i - sell_top : SHOP_ROW0 + (i << 1);
}

static uint16_t sell_price(const Gun *g) {
    return gun_price(g) >> 2;
}

static void shop_credits(void) {
    char line[TEXT_MAX + 1];
    char *p = put(line, "CREDITS ");
    p = put_u32(p, profile.credits);
    *p = 0;
    clear_row(2);
    say(8, 2, line, P_INK, shop_tiles + 57);
}

// The row under the cursor: a gun against the one held, and what A would do.
static void shop_detail(void) {
    char l1[TEXT_MAX + 1], l2[TEXT_MAX + 1];
    for (uint8_t r = SHOP_DETAIL; r <= SHOP_DETAIL + 3; r++) clear_row(r);
    char *p;
    uint8_t ok;
    if (shop_mode == SHOP_SELL) {
        const Gun *g = &profile.pack[shop_sel];
        if (g->type == GUN_NONE) {
            say_at(8, SHOP_DETAIL, "Empty.", P_DIM, shop_tiles);
            return;
        }
        gun_card_lines(g, l1, l2);
        say(8, SHOP_DETAIL, l1, P_INK, shop_tiles);
        say(8, SHOP_DETAIL + 1, l2, P_INK, shop_tiles + 19);
        p = put(l1, "A: sell it for ");
        p = put_num(p, sell_price(g), 1);
        p = put(p, " CR");
        *p = 0;
        say(8, SHOP_DETAIL + 3, l1, P_LIT, shop_tiles + 38);
        return;
    }
    if (shop_sel == SHOP_STOCK) {  // the backpack upgrade
        p = put(l1, "A bigger backpack: ");
        p = put_num(p, profile.pack_size, 1);
        p = put(p, " slots, then ");
        p = put_num(p, profile.pack_size + PACK_STEP, 1);
        *p = 0;
        say(8, SHOP_DETAIL, l1, P_INK, shop_tiles);
        ok = profile.credits >= pack_upgrade_price();
        say_at(8, SHOP_DETAIL + 3, ok ? "A: buy" : "Not enough credits.", ok ? P_LIT : P_DIM, shop_tiles + 38);
        return;
    }
    const Gun *g = &shop_stock[shop_sel];
    uint16_t price = gun_price(g);
    gun_card_lines(g, l1, l2);
    say(8, SHOP_DETAIL, l1, P_INK, shop_tiles);
    say(8, SHOP_DETAIL + 1, l2, P_INK, shop_tiles + 19);
    if (shop_sold & (1 << shop_sel)) p = put(l1, "Sold.");
    else if (profile.credits < price) p = put(l1, "Not enough credits.");
    else if (loadout_free() < LOADOUT) p = put(l1, "A: buy");
    else if (pack_free() != GUN_NONE) p = put(l1, "A: buy, into the backpack");
    else p = put(l1, "A: buy, trading in yours");
    *p = 0;
    ok = !(shop_sold & (1 << shop_sel)) && profile.credits >= price;
    say(8, SHOP_DETAIL + 3, l1, ok ? P_LIT : P_DIM, shop_tiles + 38);
}

static void draw_shop(void) {
    tiles_a = 12;
    tiles_b = 192;
    char line[TEXT_MAX + 1];
    char *p;
    shop_tiles = alloc(19 * 3 + 12);  // the detail's 3 lines and the credits
    say_c(8, 1, "MARRO'S SALVAGE", P_SEL);
    say_c(104, 1, "BUY", shop_mode == SHOP_BUY ? P_SEL : P_DIM);
    say_c(128, 1, "SELL", shop_mode == SHOP_SELL ? P_SEL : P_DIM);
    shop_credits();
    uint8_t rows = shop_rows();
    if (shop_sel >= rows) shop_sel = rows - 1;
    for (uint8_t i = 0; i < rows; i++) {
        if (shop_mode == SHOP_SELL) {
            if (i < sell_top || i >= sell_top + SELL_ROWS) continue;
            const Gun *g = &profile.pack[i];
            gun_row(shop_row(i), g, 0xFF);
            if (g->type == GUN_NONE) continue;
            p = put_num(line, sell_price(g), 1);
        } else if (i == SHOP_STOCK) {
            p = put(line, "BACKPACK +");
            p = put_num(p, PACK_STEP, 1);
            p = put(p, " SLOTS");
            *p = 0;
            say(16, shop_row(i), line, P_INK, 0xFF);
            p = put_num(line, pack_upgrade_price(), 1);
        } else {
            const Gun *g = &shop_stock[i];
            uint8_t sold = shop_sold & (1 << i);
            gun_row(shop_row(i), g, sold ? P_DIM : 0xFF);
            if (sold) {
                say_c(SHOP_PRICE_X, shop_row(i), "SOLD", P_DIM);
                continue;
            }
            p = put_num(line, gun_price(g), 1);
        }
        p = put(p, " CR");
        *p = 0;
        say(SHOP_PRICE_X, shop_row(i), line, P_INK, 0xFF);
    }
    if (shop_mode == SHOP_SELL) scroll_marks(SHOP_ROW0, SELL_ROWS, sell_top, profile.pack_size);
    shop_marker = arrow_at(shop_row(shop_sel));
    shop_detail();
    say_c(8, 16, "LEFT RIGHT: buy, sell    B: leave", P_DIM);
}

static void buy(void) {
    if (shop_sel == SHOP_STOCK) {
        uint16_t price = pack_upgrade_price();
        if (profile.credits < price) {
            sfx_play(SFX_RELOAD);
            return;
        }
        profile.credits -= price;
        profile.pack_size += PACK_STEP;
        if (!pack_upgrade_price()) shop_sel = 0;  // (the row's gone)
        sfx_play(SFX_PICKUP);
        save_write();
        menu_draw();
        return;
    }
    const Gun *g = &shop_stock[shop_sel];
    uint16_t price = gun_price(g);
    if ((shop_sold & (1 << shop_sel)) || profile.credits < price) {
        sfx_play(SFX_RELOAD);  // a click: can't
        return;
    }
    profile.credits -= price;
    uint8_t s = loadout_free();
    if (s == LOADOUT && pack_add(g)) {
        s = GUN_NONE;  // into the backpack
    } else if (s == LOADOUT) {
        // Full up: Marro takes the held gun for a quarter of what he'd ask for it.
        s = profile.gun_sel;
        profile.credits += gun_price(&profile.guns[s]) >> 2;
        gun_mods = 0;  // (its magazine isn't kept for the new gun)
    }
    if (s != GUN_NONE) {
        profile.guns[s] = *g;
        slot_ammo[s] = 255;
        loadout_select(s);
    }
    shop_sold |= 1 << shop_sel;
    guns_bought++;
    sfx_play(SFX_PICKUP);
    save_write();
    menu_draw();
}

static void sell(void) {
    Gun *g = &profile.pack[shop_sel];
    if (g->type == GUN_NONE) {
        sfx_play(SFX_RELOAD);
        return;
    }
    profile.credits += sell_price(g);
    g->type = GUN_NONE;
    sfx_play(SFX_PICKUP);
    save_write();
    menu_draw();
}

static void shop_update(void) {
    uint8_t old = shop_sel, rows = shop_rows();
    if (joy_pressed & (J_LEFT | J_RIGHT)) {
        shop_mode ^= 1;
        shop_sel = 0;
        sell_top = 0;
        sfx_play(SFX_TEXT);
        menu_draw();
        return;
    }
    if ((joy_pressed & J_UP) && shop_sel) shop_sel--;
    else if ((joy_pressed & J_DOWN) && shop_sel < rows - 1) shop_sel++;
    else if (joy_pressed & J_A) {
        if (shop_mode == SHOP_SELL) sell();
        else buy();
        return;
    }
    if (old == shop_sel) return;
    if (shop_mode == SHOP_SELL && (shop_sel < sell_top || shop_sel >= sell_top + SELL_ROWS)) {
        sell_top = shop_sel < sell_top ? shop_sel : shop_sel - SELL_ROWS + 1;
        menu_draw();
        return;
    }
    move_arrow(shop_marker, shop_row(old), shop_row(shop_sel));
    shop_detail();
}

void menu_next_page(void) BANKED {
    menu_page++;
    if (menu_page == PAGE_TUNE && save_slot != SAVE_NONE) menu_page++;  // (tuning: the lab and the yard)
    if (menu_page >= PAGES) menu_page = 0;
}

void menu_update(void) BANKED {
    if (menu_page == PAGE_SHOP) {
        shop_update();
        return;
    }
    if (menu_page == PAGE_OPTIONS) {
        options_update();
        return;
    }
    if (menu_page == PAGE_GEAR) {
        gear_update();
        return;
    }
    if (menu_page != PAGE_SKILLS) return;
    uint8_t k = menu_skill, b = k / 3, t = k % 3;
    if ((joy_pressed & J_LEFT) && b) b--;
    else if ((joy_pressed & J_RIGHT) && b < 2) b++;
    else if ((joy_pressed & J_UP) && t) t--;
    else if ((joy_pressed & J_DOWN) && t < 2) t++;
    else if (joy_pressed & J_A) {
        if (skill_learn(k)) menu_draw();  // points, the cell, the detail
        else sfx_play(SFX_RELOAD);         // a click: can't
        return;
    } else {
        return;
    }
    menu_skill = b * 3 + t;
    repaint(k);
    repaint(menu_skill);
    draw_detail();
}
