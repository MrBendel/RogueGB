#pragma bank 255
// Title screen: logo, blinking "PRESS START". START (or A) opens the files: three
// save slots (save.h), each a character; picking one continues its expedition or
// starts one. SELECT opens the test lab, B the ice yard, and DOWN the combat
// playtests (minibosses, bosses, enemies, the movement lab); none of them saves.
#include <gb/gb.h>
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "chunks.h"
#include "text.h"
#include "save.h"
#include "settings.h"
#include "skilldata.h"

#define BLINK_MASK 0x20  // "PRESS START" blinks every 32 frames (its palette's ink)
#define FILE_ROW0 10     // the files' tile rows: 10, 12, 14
#define CURSOR_X 8       // px

uint8_t title_timer;
uint8_t title_files;   // the files are showing (2: a new file's class choice)
uint8_t title_class;   // the class under consideration
uint8_t title_choice;  // the file under the cursor
uint8_t title_cursor_tile;
// Tests set this (a memory write on the title) to start runs on a known seed.
uint16_t test_seed;
uint8_t title_next_tile;  // the first tile the logo and prompt left free
// The combat playtests: 0 home, 1 their categories, 2 a category's encounters.
uint8_t title_menu, title_category;

static const char *const categories[] = {"MINIBOSSES", "BOSSES", "ENEMIES", "MOVEMENT LAB"};
static const char *const enemies[] = {"ACID SKIMMER", "MERCENARY", "ACID WOLF"};
static const char *const minibosses[] = {"CLAIMJAW", "SCRAPBACK", "CRAGBACK"};
static const char *const bosses[] = {"WARDEN", "COLLECTOR SHIP", "SPROCKET + BLAST", "CAPTAIN FLYNT", "GLOBFATHER"};

// Text from this bank's constants (text.h: copied to RAM for text.c).
static uint8_t draw(uint8_t font, uint8_t x, uint8_t ty, const char *s, uint8_t tile, uint8_t pal) {
    char line[TEXT_MAX + 1];
    uint8_t n = 0;
    while (s[n] && n < TEXT_MAX) line[n] = s[n], n++;  // (no strncpy: it would land in bank 0)
    line[n] = 0;
    return text_draw(font, x, ty, line, tile, pal);
}

static void draw_home(void);

void title_enter(void) BANKED {
    if (!settings_loaded) settings_load();  // the options, once (the title is the first scene)
    title_timer = 0;
    title_files = 0;
    palettes_title();
    // Every cell on the text palette first (a scene before may have left others).
    VBK_REG = 1;
    fill_bkg_rect(0, 0, 32, 32, TITLE_PAL_TEXT);
    VBK_REG = 0;
    uint8_t t = SCENE_TILES;
    t += draw(FONT_DISPLAY, TEXT_CENTER, 4, "SKAVENGAR", t, TITLE_PAL_GOLD);
    t += draw(FONT_COMPACT, TEXT_CENTER, 7, "RUN.  GUN.  LOOT.", t, TITLE_PAL_TEXT);
    title_next_tile = t;
    title_menu = 0;
    draw_home();
}

// Under the logo: the prompt and the other ways in.
static void draw_home(void) {
    uint8_t t = title_next_tile;
    fill_bkg_rect(0, 9, SCREEN_TW, 9, 0);
    t += draw(FONT_COMPACT, TEXT_CENTER, 11, "PRESS START", t, TITLE_PAL_PROMPT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 14, "SELECT: test lab    B: ice yard", t, TITLE_PAL_HINT);
    t += draw(FONT_COMPACT, TEXT_CENTER, 15, "DOWN: combat playtests", t, TITLE_PAL_HINT);
    draw(FONT_COMPACT, 4, 16, "v0.1", t, TITLE_PAL_HINT);
}

// The prompt blinks by its palette: ink, or the background's color.
static void prompt_ink(uint8_t on) {
    pal_base_bg[(TITLE_PAL_PROMPT << 2) + 3] = pal_base_bg[(TITLE_PAL_TEXT << 2) + (on ? 3 : 0)];
    gfx_set_fade(0);
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

// The files, in place of the prompt (the display is on: a few hundred bytes of
// direct VRAM writes, once).
static void show_files(void) {
    title_files = 1;
    title_choice = 0;
    save_scan();
    fill_bkg_rect(0, 9, SCREEN_TW, 9, 0);
    uint8_t t = title_next_tile;
    char line[TEXT_MAX + 1];
    for (uint8_t s = 0; s < SAVE_SLOTS; s++) {
        char *p = put(line, "FILE ");
        *p++ = '1' + s;
        if (slot_used[s]) {
            p = put(p, "    LV ");
            p = put_num(p, slot_level[s], 1);
            p = put(p, "    DEPTH ");
            p = put_num(p, slot_depth[s], 1);
            p = put(p, "    ");
            p = put_num(p, slot_minutes[s] / 60, 1);
            *p++ = ':';
            p = put_num(p, slot_minutes[s] % 60, 2);
        } else {
            p = put(p, "    new");
        }
        *p = 0;
        t += text_draw(FONT_COMPACT, 20, FILE_ROW0 + (s << 1), line, t, TITLE_PAL_TEXT);
    }
    t += draw(FONT_COMPACT, TEXT_CENTER, 17, "A: PLAY     B: BACK", t, TITLE_PAL_HINT);
    title_cursor_tile = t;
    draw(FONT_ITALIC, CURSOR_X, FILE_ROW0, ">", t, TITLE_PAL_TEXT);
}

// A new file: which class? Its name, role and a line about it, from the skill data.
static void draw_class(void) {
    fill_bkg_rect(0, 9, SCREEN_TW, 9, 0);
    char line[TEXT_MAX + 1], buf[SKILL_LINE_MAX + 1];
    uint8_t t = title_next_tile;
    banked_memcpy(buf, class_name[title_class], CLASS_NAME_MAX + 1, BANK(skilldata));
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 10, buf, t, TITLE_PAL_TEXT);
    // Which of them: a dot each, this one lit.
    char *p = line;
    for (uint8_t c = 0; c < CLASS_COUNT; c++) *p++ = c == title_class ? '*' : '.', *p++ = ' ';
    p[-1] = 0;
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 16, line, t, TITLE_PAL_GOLD);
    banked_memcpy(buf, class_role[title_class], CLASS_ROLE_MAX + 1, BANK(skilldata));
    t += text_draw(FONT_COMPACT, TEXT_CENTER, 11, buf, t, TITLE_PAL_HINT);
    for (uint8_t i = 0; i < 2; i++) {
        banked_memcpy(buf, class_about[title_class][i], SKILL_LINE_MAX + 1, BANK(skilldata));
        if (buf[0]) t += text_draw(FONT_COMPACT, TEXT_CENTER, 13 + i, buf, t, TITLE_PAL_TEXT);
    }
    draw(FONT_COMPACT, TEXT_CENTER, 17, "D-PAD: CLASS    A: BEGIN    B: BACK", t, TITLE_PAL_HINT);
}

static void show_classes(void) {
    title_files = 2;
    title_class = 0;
    draw_class();
}

static uint8_t choice_row(uint8_t i) {
    return FILE_ROW0 + (title_menu==2 && title_category==1 ? i : i<<1);
}
static void move_cursor(uint8_t to) {
    set_bkg_tile_xy(CURSOR_X >> 3, choice_row(title_choice), 0);
    title_choice = to;
    set_bkg_tile_xy(CURSOR_X >> 3, choice_row(title_choice), title_cursor_tile);
    VBK_REG = 1;
    set_bkg_tile_xy(CURSOR_X >> 3, choice_row(title_choice), TITLE_PAL_TEXT);
    VBK_REG = 0;
}

static uint8_t choice_count(void) {
    return title_menu == 1 ? 4 : title_category == 1 ? 5 : title_category == 2 ? 3 : title_category == 0 ? 3 : 1;
}

// The playtest menu: a heading and its choices (rows 10, 12, ...), the cursor at one.
static void draw_playtests(void) {
    fill_bkg_rect(0, 9, SCREEN_TW, 9, 0);
    uint8_t t = title_next_tile;
    t += draw(FONT_COMPACT, TEXT_CENTER, 9, title_menu == 1 ? "COMBAT PLAYTESTS" : categories[title_category], t, TITLE_PAL_HINT);
    for (uint8_t i = 0; i < choice_count(); i++) {
        const char *s = title_menu == 1 ? categories[i] : title_category == 2 ? enemies[i] : title_category == 0 ? minibosses[i] : bosses[i];
        t += draw(FONT_COMPACT, 20, choice_row(i), s, t, TITLE_PAL_TEXT);
    }
    t += draw(FONT_COMPACT, TEXT_CENTER, 17, "A: GO     B: BACK", t, TITLE_PAL_HINT);
    title_cursor_tile = t;
    draw(FONT_ITALIC, CURSOR_X, FILE_ROW0, ">", t, TITLE_PAL_TEXT);
    uint8_t c = title_choice;
    title_choice = 0;
    move_cursor(c);
}

uint16_t run_new_seed(void) BANKED {
    // The frame the player presses the button is effectively random, so it
    // seeds the run. Replays press on the same frame and get the same run.
    uint16_t seed = ((uint16_t)DIV_REG << 8) ^ frame_count, tool;
    // The chunk editor can patch a seed into the ROM instead ("play this seed").
    banked_memcpy(&tool, &tool_seed, 2, BANK(chunks));
    return tool ? tool : test_seed ? test_seed : seed;
}

static void begin(uint8_t mode) {
    play_mode = mode;
    sfx_play(SFX_START);
    run_start(run_new_seed());
    state_change(STATE_PLAY);
}

void title_update(void) BANKED {
    title_timer++;
    if (title_menu) {
        uint8_t count = choice_count();
        if (joy_pressed & J_B) {
            title_menu--;
            title_choice = title_menu ? title_category : 0;
            if (title_menu) draw_playtests();
            else draw_home();
        } else if (joy_pressed & (J_UP | J_DOWN)) {
            // (wraps around)
            if (joy_pressed & J_DOWN) move_cursor(title_choice + 1 == count ? 0 : title_choice + 1);
            else move_cursor(title_choice ? title_choice - 1 : count - 1);
        } else if (joy_pressed & (J_START | J_A)) {
            if (title_menu == 1 && title_choice != 3) {
                title_category = title_choice;
                title_choice = 0;
                title_menu = 2;
                draw_playtests();
            } else {
                uint8_t mode = title_menu == 1 ? MODE_LAB
                             : title_category == 0 ? MODE_MINIBOSS : title_category == 1 ? MODE_BOSS : MODE_ENEMY;
                enemy_trial_kind = title_choice;
                if (title_menu == 2 && title_category == 1 && title_choice == 1) mode = MODE_GUNSHIP;
                save_none();
                begin(mode);
            }
        }
        return;
    }
    if (title_files == 2) {
        uint8_t c = title_class;
        if ((joy_pressed & J_LEFT) && c) c--;
        else if ((joy_pressed & J_RIGHT) && c < CLASS_COUNT - 1) c++;
        else if (joy_pressed & J_B) {
            show_files();
            return;
        }
        else if (joy_pressed & (J_A | J_START)) {
            save_new(title_choice, title_class);
            begin(MODE_YARD);  // a new character starts in the town
        }
        if (c != title_class) {
            title_class = c;
            draw_class();
        }
        return;
    }
    if (title_files) {
        if ((joy_pressed & J_UP) && title_choice > 0) move_cursor(title_choice - 1);
        else if ((joy_pressed & J_DOWN) && title_choice < SAVE_SLOTS - 1) move_cursor(title_choice + 1);
        else if (joy_pressed & J_B) {
            title_files = 0;
            draw_home();
            prompt_ink(1);
            return;
        }
        else if (joy_pressed & (J_A | J_START)) {
            if (slot_used[title_choice]) {
                // Mid-expedition: back to its map; otherwise the town.
                save_open(title_choice);
                begin(profile.exp_active ? MODE_DESCENT : MODE_YARD);
            } else {
                show_classes();
            }
        }
        return;
    }
    if ((title_timer & (BLINK_MASK - 1)) == 0) prompt_ink(!(title_timer & BLINK_MASK));
    if (joy_pressed & J_DOWN) {
        title_menu = 1;
        title_choice = 0;
        draw_playtests();
        prompt_ink(1);
    } else if (joy_pressed & (J_START | J_A)) {
        sfx_play(SFX_START);
        show_files();
    } else if (joy_pressed & (J_SELECT | J_B)) {
        save_none();
        begin(joy_pressed & J_SELECT ? MODE_LAB : MODE_YARD);
    }
}
