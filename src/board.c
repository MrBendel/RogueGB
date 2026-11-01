#pragma bank 255
// The contracts board (board.h). The company's terminal in town posts a few jobs
// (mission.c); A takes one into a free slot (3 at most), and back here A turns a
// finished one in for its pay and XP, or pays a debt once you have the credits.
// SELECT gives a job up. Drawn with menu.c's text (its inks and tile pool).
#include <gb/gb.h>
#include "board.h"
#include "menu.h"
#include "mission.h"
#include "save.h"
#include "input.h"
#include "sfx.h"
#include "text.h"

#define OFFER_ROW 3   // the offers: rows 3-6
#define JOBS_HEAD 8
#define JOB_ROW 9     // your jobs: rows 9-11
#define DETAIL 13     // two lines
#define STATUS_X 128  // a job's progress, right of its name
#define ROWS (MISSION_OFFERS + MISSION_SLOTS)

uint8_t board_sel;
static uint8_t board_tiles;   // the detail's 2 lines (redrawn in place)
static uint8_t board_marker;  // the cursor's tile

static uint8_t row_of(uint8_t i) {
    return i < MISSION_OFFERS ? OFFER_ROW + i : JOB_ROW + i - MISSION_OFFERS;
}

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, uint32_t n) {
    // (By subtraction: no 32-bit division.)
    static const uint32_t pow10[] = {100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};
    uint8_t started = 0;
    if (n > 999999UL) n = 999999UL;
    for (uint8_t i = 0; i < 6; i++) {
        char d = '0';
        while (n >= pow10[i]) n -= pow10[i], d++;
        if (d != '0' || started || i == 5) *p++ = d, started = 1;
    }
    return p;
}

// Text from this bank's constants (copied to RAM: menu.c's bank can't see them).
static void say_c(uint8_t x, uint8_t ty, const char *s, uint8_t pal, uint8_t at) {
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    while (s[n] && n < TEXT_MAX) line[n] = s[n], n++;
    line[n] = 0;
    menu_say(x, ty, line, pal, at);
}

static void offer_row(uint8_t k) {
    char line[TEXT_MAX + 1];
    Mission m;
    mission_offer(k, &m);
    mission_text(line, &m);
    uint8_t taken = profile.mis_taken & (1 << k);
    menu_say(16, row_of(k), line, taken ? MENU_DIM : MENU_INK, 0xFF);
    if (taken) say_c(STATUS_X, row_of(k), "TAKEN", MENU_DIM, 0xFF);
}

static void job_row(uint8_t s) {
    char line[TEXT_MAX + 1];
    Mission m;
    mission_job(s, &m);
    uint8_t ty = row_of(MISSION_OFFERS + s);
    if (m.kind == MIS_NONE) {
        say_c(16, ty, "-", MENU_DIM, 0xFF);
        return;
    }
    mission_text(line, &m);
    uint8_t ready = mission_ready(s);
    menu_say(16, ty, line, ready ? MENU_LIT : MENU_INK, 0xFF);
    if (m.kind == MIS_DEBT) {
        say_c(STATUS_X, ty, ready ? "PAY" : "-", ready ? MENU_LIT : MENU_DIM, 0xFF);
    } else if (ready) {
        say_c(STATUS_X, ty, "DONE", MENU_LIT, 0xFF);
    } else {
        char *p = put_num(line, m.have);
        *p++ = '/';
        p = put_num(p, m.need);
        *p = 0;
        menu_say(STATUS_X, ty, line, MENU_INK, 0xFF);
    }
}

// Under the cursor: what it pays (or costs), and what A does.
static void detail(void) {
    char l1[TEXT_MAX + 1], l2[TEXT_MAX + 1];
    Mission m;
    menu_clear_row(DETAIL);
    menu_clear_row(DETAIL + 1);
    uint8_t job = board_sel >= MISSION_OFFERS, s = board_sel - MISSION_OFFERS;
    if (job) mission_job(s, &m);
    else mission_offer(board_sel, &m);
    if (m.kind == MIS_NONE) {
        say_c(8, DETAIL, "A free slot: take a job above.", MENU_DIM, board_tiles);
        return;
    }
    char *p = l1;
    if (m.kind == MIS_DEBT) {
        p = put(p, "Owed: ");
        p = put_num(p, mission_credits(&m));
        p = put(p, " CR. Pays ");
    } else {
        p = put(p, "Pays ");
        p = put_num(p, mission_credits(&m));
        p = put(p, " CR and ");
    }
    p = put_num(p, mission_xp(&m));
    p = put(p, " XP");
    *p = 0;
    menu_say(8, DETAIL, l1, MENU_INK, board_tiles);
    const char *what;
    if (!job) what = profile.mis_taken & (1 << board_sel) ? "Taken." : "A: take it (3 jobs at most).";
    else if (m.kind == MIS_DEBT) what = mission_ready(s) ? "A: pay it.  SELECT: drop" : "Earn the credits, then pay here.";
    else what = mission_ready(s) ? "A: turn it in." : m.kind == MIS_RETRIEVE ? "Cores are lost if you die. SELECT: drop" : "SELECT: drop it.";
    p = put(l2, what);
    *p = 0;
    menu_say(8, DETAIL + 1, l2, mission_ready(s) && job ? MENU_LIT : MENU_DIM, board_tiles + 19);
}

void board_draw(void) BANKED {
    char line[TEXT_MAX + 1];
    board_tiles = menu_alloc(19 * 2);  // the detail's 2 lines (redrawn in place)
    say_c(8, 1, "CONTRACTS", MENU_SEL, 0xFF);
    char *p = put_num(line, profile.credits);
    p = put(p, " CR");
    *p = 0;
    menu_say(104, 1, line, MENU_INK, 0xFF);
    for (uint8_t k = 0; k < MISSION_OFFERS; k++) offer_row(k);
    say_c(8, JOBS_HEAD, "YOUR JOBS", MENU_DIM, 0xFF);
    for (uint8_t s = 0; s < MISSION_SLOTS; s++) job_row(s);
    board_marker = menu_arrow_at(row_of(board_sel));
    say_c(8, 16, "A: take, turn in   B: leave", MENU_DIM, 0xFF);
    detail();
}

void board_update(void) BANKED {
    uint8_t old = board_sel;
    if ((joy_pressed & J_UP) && board_sel) board_sel--;
    else if ((joy_pressed & J_DOWN) && board_sel < ROWS - 1) board_sel++;
    else if (joy_pressed & (J_A | J_SELECT)) {
        uint8_t ok = 0;
        if (board_sel < MISSION_OFFERS) {
            if (joy_pressed & J_A) ok = mission_take(board_sel);
        } else {
            uint8_t s = board_sel - MISSION_OFFERS;
            if (joy_pressed & J_A) ok = mission_turn_in(s);
            else if (profile.mis_kind[s]) mission_drop(s), ok = 1;
        }
        if (!ok) {
            sfx_play(SFX_RELOAD);  // a click: can't
            return;
        }
        sfx_play(SFX_PICKUP);
        save_write();
        menu_draw();
        return;
    }
    if (old == board_sel) return;
    menu_move_arrow(board_marker, row_of(old), row_of(board_sel));
    detail();
}
