#pragma bank 255
// Title screen: logo, blinking "PRESS START". START (or A) begins a run.
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"

#define BLINK_MASK 0x20  // "PRESS START" toggles every 32 frames
#define PROMPT_Y 12

uint8_t title_timer;
uint8_t title_menu; // 0: home, 1: categories, 2: encounters
uint8_t title_choice, title_category, title_redraw;

static const char *const categories[] = {"MINIBOSSES", "BOSSES", "ENEMIES", "MOVEMENT LAB"};
static const char *const enemies[] = {"ACID SKIMMER"};

static uint8_t choice_count(void) {
    return title_menu == 1 ? 4 : title_category == 1 ? 2 : 1;
}

// One queued row per frame keeps menu changes within the vblank budget.
static void menu_row(void) {
    uint8_t row = title_redraw++, n = row - 11;
    const char *s = "";
    if (!title_menu) {
        if (row == 12) s = "PRESS START";
        if (row == 13) s = "SELECT: TEST LAB";
        if (row == 14) s = "DOWN: PLAYTESTS";
        if (row == 16) s = "V0.1";
    } else {
        if (row == 10) s = title_menu == 1 ? "COMBAT PLAYTESTS" : categories[title_category];
        if (row >= 11 && row <= 14 && n < choice_count()) {
            if (title_menu == 1) s = categories[n];
            else if (title_category == 2) s = enemies[n];
            else s = title_category == 0 ? "CLAIMJAW" : n == 0 ? "WARDEN" : "COLLECTOR SHIP";
        }
        if (row == 15) s = "UP/DOWN: CHOOSE";
        if (row == 16) s = "A/START:GO B:BACK";
    }
    uint8_t *out = gfx_strip(1, row, 0, 18);
    for (uint8_t i = 0; i < 18; i++) out[i] = FONT_TILES;
    uint8_t x = title_menu && row >= 11 && row <= 14 ? 2 : 0;
    if (title_menu && x && n == title_choice) out[0] = FONT_TILES + ('>' - ' ');
    if (!title_menu) x = row == 12 ? 4 : row == 13 ? 2 : row == 14 ? 1 : 0;
    while (*s && x < 18) out[x++] = FONT_TILES + (*s++ - ' ');
    if (title_redraw == 17) title_redraw = 0;
}

void title_enter(void) BANKED {
    title_timer = 0;
    title_menu = title_choice = title_category = title_redraw = 0;
    palettes_menu();
    bkg_frame(2, 2, 16, 7);
    bkg_text(6, 4, "ROGUE GB");
    bkg_text(5, 6, "RUN.GUN.DIE");
    bkg_text(5, PROMPT_Y, "PRESS START");
    bkg_text(3, 13, "SELECT: TEST LAB");
    bkg_text(2, 14, "DOWN: PLAYTESTS");
    bkg_text(1, 16, "V0.1");
}

void title_update(void) BANKED {
    if (title_menu) {
        if (joy_pressed & J_B) {
            title_menu--;
            title_choice = title_menu ? title_category : 0;
            title_redraw = 10;
        } else if (joy_pressed & (J_UP | J_DOWN)) {
            uint8_t count = choice_count();
            if (joy_pressed & J_DOWN) title_choice = title_choice + 1 == count ? 0 : title_choice + 1;
            else title_choice = title_choice ? title_choice - 1 : count - 1;
            title_redraw = 10;
        } else if (joy_pressed & (J_START | J_A)) {
            if (title_menu == 1 && title_choice != 3) {
                title_category = title_choice;
                title_choice = 0;
                title_menu = 2;
                title_redraw = 10;
            } else {
                play_mode = title_menu == 1 ? MODE_LAB :
                    title_category == 0 ? MODE_MINIBOSS : title_category == 1 ? MODE_BOSS : MODE_ENEMY;
                enemy_trial_kind = title_choice;
                if (title_menu == 2 && title_category == 1 && title_choice == 1) play_mode = MODE_GUNSHIP;
                goto launch;
            }
        }
        if (title_redraw) menu_row();
        return;
    }
    if (joy_pressed & J_DOWN) {
        title_menu = 1;
        title_choice = 0;
        title_redraw = 10;
        menu_row();
        return;
    }
    if (title_redraw) { menu_row(); return; }
    title_timer++;
    if ((title_timer & (BLINK_MASK - 1)) == 0) {
        bkg_text(5, PROMPT_Y, (title_timer & BLINK_MASK) ? "           " : "PRESS START");
    }
    if (joy_pressed & (J_START | J_A | J_SELECT)) {
        play_mode = joy_pressed & J_SELECT ? MODE_LAB : MODE_DESCENT;
launch:
        sfx_play(SFX_START);
        // The frame the player presses START is effectively random, so it
        // seeds the run. Replays press on the same frame and get the same run.
        run_start(((uint16_t)DIV_REG << 8) ^ frame_count);
        state_change(STATE_PLAY);
    }
}
