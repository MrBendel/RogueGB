#pragma bank 255
// Gameplay scene. For now this is the movement lab: one room with pillars,
// a placeholder player, and movement presets to compare.
//   A (hold)      fire at the auto-aim target (aim.c)
//   B             jump; B again in mid-air dashes
//   SELECT (tap)  switch weapon (weapon.c), on release; the loadout menu later
//   SELECT+B      reload (aim.c)
//   SELECT+A      reserved: a grenade or a mine (later)
//   START         pause; the pause panel picks the movement preset and tunes
//                 movement, camera and the held weapon (UP/DOWN pick a value,
//                 LEFT/RIGHT change it)
//   SELECT paused end the run
#include <gb/gb.h>
#include <rand.h>
#include "waves.h"
#include "descent.h"
#include "level.h"
#include "sfx.h"
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "room.h"
#include "player.h"
#include "camera.h"
#include "actor.h"
#include "aim.h"
#include "oam.h"
#include "weapon.h"
#include "health.h"
#include "hud.h"
#include "chest.h"
#include "dialog.h"
#include "talk.h"
#include "dialogue.h"

// HUD: one block row (2 tile rows) on the window layer under the 10x8-block room.
#define HUD_WY VIEW_H
// Pause panel: the whole screen (18 tile rows) on the window layer.
#define PAUSE_H 18
#define PAUSE_WY (144 - PAUSE_H * 8)
#define TUNE_ROW0 2  // window row of the first tunable shown
#define TUNE_ROWS 14 // tunables shown at once; the list scrolls

// Tunables shown on the pause panel. FX16 = uint16 8.8 shown as x.xx,
// FX8 = uint8 in 1/256ths shown as 0.xx, FRAMES = uint8 shown as a count.
#define KIND_FX16 0
#define KIND_FX8 1
#define KIND_FRAMES 2
#define KIND_NUM 3  // uint8 shown as a plain number
#define KIND_PRESET 4  // the movement preset, by name
#define KIND_RIFLE 5   // the rifle's trigger: TAP, BURST, AUTO
// What else changing a value does.
#define TUNE_MOVEMENT 1  // makes the movement preset CUSTOM
#define TUNE_WEAPON 2    // the held weapon's: reload it (its recoil table is scaled)
#define TUNE_HEALTH 4    // max health: refill to it
#define TUNE_SHIELD 8    // the shield's: re-equip it (full)
typedef struct {
    const char *label;
    uint8_t kind;
    uint8_t flags;  // TUNE_*
    void *value;
    uint16_t step, min, max;
} Tunable;

static const Tunable tunables[] = {
    {"MOVE", KIND_PRESET, 0, &move_preset, 1, 0, MOVE_PRESET_COUNT - 1},
    {"SPEED", KIND_FX16, TUNE_MOVEMENT, &move_top, 64, 64, 768},
    {"ACCEL", KIND_FRAMES, TUNE_MOVEMENT, &move_accel_frames, 1, 1, 60},
    {"DECEL", KIND_FRAMES, TUNE_MOVEMENT, &move_decel_frames, 1, 1, 60},
    {"REVERSE", KIND_FRAMES, TUNE_MOVEMENT, &move_reverse_frames, 1, 1, 60},
    {"JUMP", KIND_FX16, 0, &jump_vel, 32, 128, 1024},
    {"GRAVITY", KIND_FX8, 0, &jump_grav, 4, 4, 128},
    {"DASH SPD", KIND_FX16, 0, &dash_speed, 64, 256, 1536},
    {"DASH FR", KIND_FRAMES, 0, &dash_frames, 1, 1, 30},
    {"CAM LEAD", KIND_FRAMES, 0, &cam_lead, 2, 0, 40},
    {"LEAD SPD", KIND_FX16, 0, &cam_lead_rate, 32, 32, 1024},  // at most 64/16 px: camera_lead.s
    {"RIFLE", KIND_RIFLE, 0, &rifle_mode, 1, 0, RIFLE_MODES - 1},
    // The held weapon (weapon.h; SELECT switches it).
    {"FIRE RATE", KIND_FRAMES, TUNE_WEAPON, &wpn.fire_interval, 1, 2, 90},
    {"TAP RATE", KIND_FRAMES, TUNE_WEAPON, &wpn.tap_interval, 1, 1, 60},
    {"PELLETS", KIND_NUM, TUNE_WEAPON, &wpn.pellets, 1, 1, 9},
    {"SPREAD", KIND_NUM, TUNE_WEAPON, &wpn.spread, 1, 0, 4},
    {"JITTER", KIND_NUM, TUNE_WEAPON, &wpn.jitter, 1, 0, 4},
    {"SHOT SPD", KIND_FX16, TUNE_WEAPON, &wpn.speed, 32, 128, 1792},
    {"RANGE", KIND_NUM, TUNE_WEAPON, &wpn.range, 8, 16, 248},
    {"DAMAGE", KIND_NUM, TUNE_WEAPON, &wpn.damage, 1, 1, 9},
    {"KICK", KIND_NUM, TUNE_WEAPON, &wpn.kick, 1, 1, 3},
    {"WINDUP", KIND_FRAMES, TUNE_WEAPON, &wpn.windup, 2, 2, 90},
    {"MAG", KIND_NUM, TUNE_WEAPON, &wpn.mag, 1, 0, 99},
    {"RELOAD", KIND_FRAMES, TUNE_WEAPON, &wpn.reload, 5, 10, 250},
    {"AIM RANGE", KIND_NUM, TUNE_WEAPON, &wpn.aim_range, 8, 32, 160},
    {"AIM BIAS", KIND_NUM, 0, &aim_bias, 4, 0, 64},
    {"HITSTOP", KIND_FRAMES, 0, &hitstop_frames, 1, 0, 10},
    {"SHAKE", KIND_NUM, 0, &cam_shake_max, 1, 0, 4},
    {"START WAVE", KIND_NUM, 0, &wave_start, 1, 1, 20},
    {"HEALTH", KIND_NUM, TUNE_HEALTH, &player_hp_max, 10, 10, 250},
    {"SHIELD", KIND_NUM, TUNE_SHIELD, &shield_cap, 10, 0, 250},
    {"SH DELAY", KIND_FRAMES, TUNE_SHIELD, &shield_delay, 10, 0, 250},
    {"SH RATE", KIND_FRAMES, TUNE_SHIELD, &shield_rate, 1, 0, 30},
};
#define TUNE_COUNT (sizeof(tunables) / sizeof(tunables[0]))

// Sprites before this are the player's, the held weapon's and the reticle's (oam.h).
#define FIRST_ACTOR_OAM OAM_FIRST_ACTOR

// Player start: on the ledge in the top-left screen.
#define START_X 72
#define START_Y 96

// --- watchable state ---
uint16_t run_seed;
uint8_t play_mode;
uint16_t run_seconds;
uint8_t run_ticks;
uint8_t paused;
uint8_t select_chord;  // SELECT is down and was used with A or B (no switch on release)
uint8_t chord_mask;    // A/B kept from the game: while SELECT is down, then until let go
uint16_t chord_throws; // SELECT+A presses (the grenade/mine to come; watchable)
uint8_t tune_sel;
uint8_t tune_top;  // the first tunable shown (the list scrolls)
// Debug/test hook: tests set this through emulator memory writes; the next frame
// runs it and clears it. 1 = stress wave (actors_debug_stress); 2 = spawn an actor
// of debug_type at (debug_x, debug_y), its slot in debug_slot; 3 = equip the
// first shield (what the chest will hold); 4 = hurt the player for debug_type
// damage from (debug_x, debug_y); 5 = generate an area from seed debug_x and
// build its map debug_type into the room map (tests check the generator); 6 =
// open dialogue script debug_type (dialogue.h's DLG_ ids).
uint8_t debug_cmd;
uint8_t debug_type;
uint16_t debug_x, debug_y;
uint8_t debug_slot;

void run_start(uint16_t seed) BANKED {
    run_seed = seed;
    initrand(seed);
    run_seconds = 0;
    run_ticks = 0;
}

static uint16_t tune_get(const Tunable *t) {
    return t->kind == KIND_FX16 ? *(uint16_t *)t->value : *(uint8_t *)t->value;
}

static void tune_set(const Tunable *t, uint16_t v) {
    if (t->kind == KIND_FX16) *(uint16_t *)t->value = v;
    else *(uint8_t *)t->value = (uint8_t)v;
}

// Tunable i's row, if it's in view.
static void draw_tune_row(uint8_t i) {
    if (i < tune_top || i >= tune_top + TUNE_ROWS) return;
    const Tunable *t = &tunables[i];
    uint8_t y = TUNE_ROW0 + i - tune_top;
    uint16_t v = tune_get(t);
    fill_win_rect(1, y, SCREEN_TW - 2, 1, FONT_TILES);
    win_text(1, y, i == tune_sel ? ">" : " ");
    win_text(2, y, t->label);
    if (t->kind == KIND_PRESET) {
        win_text(11, y, move_preset_name());
    } else if (t->kind == KIND_RIFLE) {
        win_text(13, y, v == RIFLE_TAP ? "TAP  " : v == RIFLE_BURST ? "BURST" : "AUTO ");
    } else if (t->kind == KIND_FRAMES || t->kind == KIND_NUM) {
        win_number(12, y, v, 3, 0);
        win_text(16, y, t->kind == KIND_FRAMES ? "FR" : "  ");
    } else {
        win_number(11, y, v >> 8, 1, 0);
        win_text(12, y, ".");
        win_number(13, y, ((v & 0xFF) * 100) >> 8, 2, 1);
        win_text(16, y, "PX");
    }
}

static void draw_tune_rows(void) {
    for (uint8_t i = tune_top; i < tune_top + TUNE_ROWS && i < TUNE_COUNT; i++) draw_tune_row(i);
    // Arrows on the frame when there's more above or below.
    set_win_tile_xy(SCREEN_TW - 1, TUNE_ROW0, tune_top ? FONT_TILES + ('^' - ' ') : FRAME_TILES + 5);
    set_win_tile_xy(SCREEN_TW - 1, TUNE_ROW0 + TUNE_ROWS - 1,
                    tune_top + TUNE_ROWS < TUNE_COUNT ? FONT_TILES + ('V' - ' ') : FRAME_TILES + 5);
}

static void draw_pause(void) {
    win_frame(0, 0, SCREEN_TW, PAUSE_H);
    win_text(2, 1, "TUNE   GUN:");
    weapon_draw_name(13, 1);
    draw_tune_rows();
    win_text(1, TUNE_ROW0 + TUNE_ROWS, "START:BACK SEL:END");
}

void play_enter(void) BANKED {
    palettes_play();
    talk_clear();  // a room adds its own
    if (play_mode == MODE_DESCENT) {
        room_load_tiles();
        player_init(0, 0);
        descent_start(run_seed);  // builds the start room and places the player
    } else if (play_mode == MODE_YARD) {
        room_load_yard();
        player_init(5 * BLOCK_SIZE, 4 * BLOCK_SIZE);
    } else {
        room_load_lab();
        player_init(START_X, START_Y);
    }
    if (!move_top) move_apply_preset(0);  // first run; later runs keep the chosen movement
    camera_init(player_x + 8, player_y + 8);
    actors_init();
    hud_init();
    chest_init();
    sfx_watch_reset();
    waves_init();
    paused = 0;
    select_chord = chord_mask = 0;
    chord_throws = 0;
    tune_sel = tune_top = 0;
    hud_draw();
    move_win(7, HUD_WY);
    SHOW_WIN;
    gfx_sprites(1);
    gfx_sprite_split(HUD_WY);  // no sprites over the HUD
    player_draw();
    talk_init();  // after the room: it added its talkers
}

static void set_paused(uint8_t p) {
    paused = p;
    // Sprites would draw over the pause panel, so hide them while paused.
    if (p) {
        hud_hide();
        draw_pause();
        move_win(7, PAUSE_WY);
        gfx_sprites(0);
    } else {
        hud_draw();
        move_win(7, HUD_WY);
        gfx_sprites(1);
    }
}

static void tune_update(void) {
    uint8_t old = tune_sel;
    if ((joy_pressed & J_UP) && tune_sel > 0) tune_sel--;
    if ((joy_pressed & J_DOWN) && tune_sel < TUNE_COUNT - 1) tune_sel++;
    if (old != tune_sel) {
        if (tune_sel < tune_top || tune_sel >= tune_top + TUNE_ROWS) {
            tune_top = tune_sel < tune_top ? tune_sel : tune_sel - TUNE_ROWS + 1;
            draw_tune_rows();
        } else {
            draw_tune_row(old);
            draw_tune_row(tune_sel);
        }
    }
    const Tunable *t = &tunables[tune_sel];
    if (t->kind == KIND_PRESET) {
        // LEFT/RIGHT step through the presets (from CUSTOM, back to the first).
        uint8_t p = move_preset;
        if (joy_pressed & J_RIGHT) p = p >= MOVE_PRESET_COUNT - 1 ? 0 : p + 1;
        else if (joy_pressed & J_LEFT) p = p == 0 || p >= MOVE_PRESET_COUNT ? MOVE_PRESET_COUNT - 1 : p - 1;
        else return;
        move_apply_preset(p);
        // It and the movement values under it change (rows 0-4); redrawing just
        // those keeps the panel responsive (numbers are slow to draw).
        for (uint8_t i = 0; i <= 4; i++) draw_tune_row(i);
        return;
    }
    uint16_t v = tune_get(t);
    if (t->kind == KIND_RIFLE) {
        // LEFT/RIGHT cycle the three.
        if (joy_pressed & J_RIGHT) v = v >= RIFLE_MODES - 1 ? 0 : v + 1;
        else if (joy_pressed & J_LEFT) v = v == 0 ? RIFLE_MODES - 1 : v - 1;
        else return;
        tune_set(t, v);
        draw_tune_row(tune_sel);
        return;
    }
    if ((joy_pressed & J_RIGHT) && v + t->step <= t->max) v += t->step;
    else if ((joy_pressed & J_LEFT) && v >= t->min + t->step) v -= t->step;
    else return;
    tune_set(t, v);
    if (t->flags & TUNE_MOVEMENT) {
        move_preset = MOVE_PRESET_CUSTOM;
        move_recalc();
        draw_tune_row(0);
    }
    if (t->flags & TUNE_WEAPON) weapon_select(weapon_cur);
    if (t->flags & TUNE_HEALTH) player_hp = player_hp_max;
    if (t->flags & TUNE_SHIELD) health_give_shield(shield_cap, shield_delay, shield_rate);
    draw_tune_row(tune_sel);
}

void play_update(void) BANKED {
    if (dlg_active) {  // the world waits while someone talks
        dlg_update();
        return;
    }
    if (paused) {
        if (joy_pressed & J_START) set_paused(0);
        else if (joy_pressed & J_SELECT) state_change(STATE_RUN_OVER);
        else tune_update();
        return;
    }
    if (joy_pressed & J_START) {
        set_paused(1);
        return;
    }
    // SELECT is a modifier: with B it reloads, with A (later) it throws a
    // grenade or drops a mine; a tap on its own switches weapons, on release so
    // a chord never switches too. While it's down, A and B don't reach the
    // player or the gun; a button still held when SELECT is let go stays out
    // until it's let go too (no jump or shot from the end of a chord).
    if (joy & J_SELECT) {
        if (joy_pressed & J_SELECT) select_chord = 0;
        if (joy_pressed & J_B) {
            aim_reload();
            select_chord = 1;
        }
        if (joy_pressed & J_A) {
            chord_throws++;
            select_chord = 1;
        }
        chord_mask = J_A | J_B;
    } else {
        if ((joy_released & J_SELECT) && !select_chord) {
            weapon_select(weapon_cur + 1 < WEAPON_COUNT ? weapon_cur + 1 : 0);  // the HUD shows it
        }
        chord_mask &= joy;
    }
    // A beside someone who talks: talk, don't fire (A stays out until let go).
    if ((joy_pressed & J_A) && !(chord_mask & J_A) && talk_near != TALK_NONE) {
        chord_mask |= J_A;
        talk_start();
        return;
    }
    joy &= ~chord_mask;
    joy_pressed &= ~chord_mask;

    if (++run_ticks == 60) {
        run_ticks = 0;
        run_seconds++;
    }

    // Hitstop: a hit that lands freezes everything for a few frames (the sprites
    // stay where they are), which gives it weight.
    if (hitstop) {
        hitstop--;
        return;
    }

    if (debug_cmd) {
        if (debug_cmd == 1) actors_debug_stress(player_x, player_y);
        else if (debug_cmd == 2) debug_slot = actor_spawn(debug_type, debug_x, debug_y);
        else if (debug_cmd == 3) health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
        else if (debug_cmd == 5) {
            level_generate(debug_x);
            level_build_map(debug_type);
        } else if (debug_cmd == 6) {
            dlg_start(debug_type);
        } else if (debug_cmd == 4) {
            hurt_cause = CAUSE_NONE;
            player_hurt(debug_type, debug_x, debug_y);
        }
        debug_cmd = 0;
    }
    player_update();
    if (talk_count | zone_count) talk_update();  // (a banked call costs: skip it in maps with neither)
    if (play_mode == MODE_DESCENT) {
        uint8_t door = descent_door();
        if (door) descent_change_room(door);
        descent_update();
    } else if (play_mode == MODE_LAB) {
        waves_update();
    }
    // The camera only follows the player, so it moves first; then actors and
    // bullets can move and draw in one pass against this frame's camera.
    camera_update(player_x + 8, player_y + 8, player_vx, player_vy);
    actors_update(talk_count ? talk_draw(FIRST_ACTOR_OAM) : FIRST_ACTOR_OAM);  // talkers first: few, standing still
    if (play_mode == MODE_LAB) chest_update();
    aim_update();
    player_draw();
    health_update();
    hud_update();
    sfx_watch();
    if (!player_hp) {
        sfx_play(SFX_DEATH);
        state_change(STATE_RUN_OVER);
    }
}
