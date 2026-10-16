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

// Ink palettes (background palettes 1-3, borrowed; 6 is the panel's own).
#define P_INK BG_PAL_UI
#define P_DIM 1  // locked skills, labels
#define P_LIT 2  // learned skills, the XP bar
#define P_SEL 3  // the cursor, the page's tab
#define C(r, g, b) RGB8(r, g, b)
#define PAPER C(248, 240, 216)
static const uint16_t inks[3][4] = {
    {PAPER, C(200, 184, 152), C(176, 164, 140), C(150, 136, 116)},
    {PAPER, C(152, 232, 216), C(32, 150, 140), C(16, 96, 92)},
    {PAPER, C(255, 200, 160), C(208, 96, 40), C(184, 64, 24)},
};
// HUD bar tiles in the second VRAM bank (hud.c): T_FILL + n = a cell n px full.
#define T_FILL 1
extern uint16_t enemies_killed;  // aim.c: this run's
#define XP_CELLS 10

uint8_t menu_page;
uint8_t menu_skill;
uint16_t menu_saved_pal[12];  // background palettes 1-3 as they were
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
    say_c(12, 1, "PROFILE", menu_page == PAGE_PROFILE ? P_SEL : P_DIM);
    say_c(64, 1, "SKILLS", menu_page == PAGE_SKILLS ? P_SEL : P_DIM);
    say_c(116, 1, "TUNE", menu_page == PAGE_TUNE ? P_SEL : P_DIM);
}

void menu_open(void) BANKED {
    memcpy(menu_saved_pal, &pal_base_bg[1 << 2], sizeof(menu_saved_pal));
    memcpy(&pal_base_bg[P_DIM << 2], inks, sizeof(inks));
    gfx_set_fade(0);
    win_frame(0, 0, SCREEN_TW, SCREEN_TH);
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, SCREEN_TH, BG_PAL_UI);  // (a dialogue may have left bank-1 cells)
    VBK_REG = 0;
    if (menu_page != PAGE_TUNE) menu_draw();
}

void menu_close(void) BANKED {
    memcpy(&pal_base_bg[1 << 2], menu_saved_pal, sizeof(menu_saved_pal));
    gfx_set_fade(0);
}

// --- the profile page ---

static void draw_profile(void) {
    char line[TEXT_MAX + 1], name[CLASS_ROLE_MAX + 1];
    char *p;
    // The class, and its role.
    banked_memcpy(name, class_name[profile.cls], CLASS_NAME_MAX + 1, BANK(skilldata));
    p = put(line, name);
    p = put(p, " - ");
    banked_memcpy(name, class_role[profile.cls], CLASS_ROLE_MAX + 1, BANK(skilldata));
    p = put(p, name);
    *p = 0;
    say(12, 3, line, P_INK, 0xFF);
    // Level and XP, with a bar.
    p = put(line, "LEVEL ");
    p = put_num(p, profile.level, 1);
    *p = 0;
    say(12, 5, line, P_INK, 0xFF);
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
    say(72, 5, line, P_DIM, 0xFF);
    // The bar's pixels: XP_CELLS * 8 of them, in 16-bit math.
    uint16_t px = !need ? XP_CELLS * 8 : need < 800 ? profile.xp * (XP_CELLS * 8) / need : profile.xp / (need / (XP_CELLS * 8));
    if (px > XP_CELLS * 8) px = XP_CELLS * 8;
    uint8_t cells[XP_CELLS];
    for (uint8_t i = 0; i < XP_CELLS; i++) {
        uint8_t f = px >= 8 ? 8 : (uint8_t)px;
        cells[i] = T_FILL + f;
        px -= f;
    }
    set_win_tiles(2, 6, XP_CELLS, 1, cells);
    memset(cells, P_LIT | TEXT_BANK1, XP_CELLS);
    VBK_REG = 1;
    set_win_tiles(2, 6, XP_CELLS, 1, cells);
    VBK_REG = 0;
    // The rest of the file.
    p = put(line, "CREDITS ");
    p = put_u32(p, profile.credits);
    *p = 0;
    say(12, 8, line, P_INK, 0xFF);
    p = put(line, "TIME ");
    p = put_num(p, profile.play_minutes / 60, 1);
    *p++ = ':';
    p = put_num(p, profile.play_minutes % 60, 2);
    *p = 0;
    say(12, 9, line, P_INK, 0xFF);
    p = put(line, "DEEPEST DEPTH ");
    p = put_num(p, profile.best_depth, 1);
    *p = 0;
    say(12, 10, line, P_INK, 0xFF);
    p = put(line, "KILLS ");
    p = put_num(p, profile.kills + enemies_killed, 1);  // (the file's, plus this run's)
    *p = 0;
    say(12, 11, line, P_INK, 0xFF);
    p = put(line, "SKILL POINTS ");
    p = put_num(p, profile.skill_points, 1);
    *p = 0;
    say(12, 13, line, profile.skill_points ? P_LIT : P_INK, 0xFF);
    if (save_slot == SAVE_NONE) say_c(12, 15, "Test play: nothing is saved.", P_DIM);
    say_c(12, 16, "B: next page    SELECT: end run", P_DIM);
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

void menu_draw(void) BANKED {
    clear_inside();
    menu_tabs();
    if (menu_page == PAGE_PROFILE) draw_profile();
    else draw_skills();
}

void menu_update(void) BANKED {
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
