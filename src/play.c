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
#include "tiles_lunar_ext.h"
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
#include "save.h"
#include "progress.h"
#include "skills.h"
#include "menu.h"
#include "loot.h"
#include "pickups.h"
#include "dialogue.h"
#include "boss.h"
#include "enemy_trial.h"
#include "gunship.h"
#include "warden.h"
#include "acid_skimmer.h"
#include "mercenary.h"
#include "acid_wolf.h"
#include "scrapback.h"
#include "flynt.h"

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
uint8_t play_actor_oam;
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

// The tuning page of the pause menu (menu.h: the profile and skills pages are menu.c's).
static void draw_pause(void) {
    win_frame(0, 0, SCREEN_TW, PAUSE_H);
    VBK_REG = 1;
    fill_win_rect(0, 0, SCREEN_TW, PAUSE_H, BG_PAL_UI);
    VBK_REG = 0;
    menu_tabs();
    draw_tune_rows();
    win_text(1, TUNE_ROW0 + TUNE_ROWS, "B:PAGE SEL:END RUN");
}

static void draw_page(void) {
    if (menu_page == PAGE_TUNE) draw_pause();
    else menu_draw();
}

void play_enter(void) BANKED {
    play_actor_oam = play_mode == MODE_GUNSHIP ? FIRST_ACTOR_OAM + 10 : FIRST_ACTOR_OAM;
    palettes_play();
    talk_clear();    // a room adds its own
    saved_seconds = 0;  // (run_start began the clock again)
    pickups_init();  // a descent map loads its chests (descent.c)
    if (play_mode == MODE_DESCENT) {
        tiles_lunar_ext_load();  // the descent's theme (docs/design/lunar-base.md)
        player_init(0, 0);
        // A file (save.h): its expedition, if one is under way.
        if (save_slot != SAVE_NONE && profile.exp_active) descent_resume();
        else descent_start(run_seed);  // builds the start room and places the player
    } else if (play_mode == MODE_YARD) {
        room_load_yard();
        player_init(5 * BLOCK_SIZE, 4 * BLOCK_SIZE);
    } else if (play_mode >= MODE_MINIBOSS) {
        if (play_mode == MODE_BOSS && enemy_trial_kind == 0) { warden_room(); player_init(72,184); }
        else if (play_mode == MODE_BOSS && enemy_trial_kind == 3) { flynt_room(); player_init(72,96); }
        else { boss_room(); player_init(72,96); }
    } else {
        room_load_lab();
        player_init(START_X, START_Y);
    }
    if (!move_top) move_apply_preset(0);  // first run; later runs keep the chosen movement
    skills_refresh();  // this file's skills, before anything loads a gun (weapon_select applies them)
    gun_mods = 0;      // the plain guns, until a file's loadout is picked up (below)
    camera_init(player_x + 8, player_y + 8);
    actors_init();
    if (save_slot != SAVE_NONE) {
        // The file's loadout, magazines full.
        for (uint8_t s = 0; s < LOADOUT; s++) slot_ammo[s] = 255;
        loadout_select(profile.guns[profile.gun_sel].type != GUN_NONE ? profile.gun_sel : 0);
    }
    if (play_mode != MODE_DESCENT) layer_depth = 1;  // (XP's depth bonus)
    progress_apply();  // max health from the level
    // XP and levels: a file's descent, the lab and the yard; the combat playtests
    // (bosses, enemies) are tuned arenas: a level up's full heal would skew them.
    xp_on = play_mode == MODE_DESCENT || play_mode == MODE_LAB || play_mode == MODE_YARD;
    hud_init();
    boss_kind = BOSS_NONE;
    if (play_mode < MODE_MINIBOSS || play_mode == MODE_YARD) chest_init();  // (the yard is no boss room)
    else if (play_mode == MODE_ENEMY) {
        play_actor_oam = enemy_trial_kind==2 ? FIRST_ACTOR_OAM : FIRST_ACTOR_OAM + 8;
        enemy_trial_start();
    }
    else {
        if (play_mode == MODE_BOSS && enemy_trial_kind == 0) play_actor_oam = FIRST_ACTOR_OAM + 8;
        boss_start(play_mode == MODE_MINIBOSS ? (enemy_trial_kind==2 ? BOSS_CRAGBACK : enemy_trial_kind==1 ? BOSS_SCRAPBACK : BOSS_CLAIMJAW) :
                   play_mode == MODE_GUNSHIP ? BOSS_GUNSHIP :
                   enemy_trial_kind == 2 ? BOSS_DUO :
                   enemy_trial_kind == 3 ? BOSS_FLYNT : BOSS_WARDEN);
    }
    sfx_watch_reset();
    waves_init();
    paused = 0;
    // The pause menu opens on your profile; in the lab and the yard (no file),
    // where tuning is the point, on the tuning page.
    menu_page = save_slot == SAVE_NONE ? PAGE_TUNE : PAGE_CHARACTER;
    select_chord = chord_mask = 0;
    chord_throws = 0;
    tune_sel = tune_top = 0;
    hud_draw();
    uint8_t hud_y = (play_mode == MODE_BOSS && enemy_trial_kind == 0) ? 120 : HUD_WY;
    move_win(7, hud_y);
    SHOW_WIN;
    gfx_sprites(1);
    gfx_sprite_split(hud_y);  // no sprites over the HUD
    player_draw();
    talk_init();  // after the room: it added its talkers
    if (play_mode == MODE_YARD && save_slot != SAVE_NONE) shop_restock();  // new stock each time you're in town
}

// Leaving this scene for another (a lift): the file gets the time and kills so far.
static void leave_scene(void) {
    if (save_slot == SAVE_NONE) return;
    save_add_time(run_seconds - saved_seconds);
    saved_seconds = run_seconds;
    profile.kills += enemies_killed;
    enemies_killed = 0;
    save_write();
}

// Play again in another mode (play -> play: the main loop only changes scenes
// when the next differs, so fade out here and leave none; it then enters play).
static void go(uint8_t mode) {
    leave_scene();
    play_mode = mode;
    sfx_play(SFX_START);
    run_start(run_new_seed());
    fade_out();
    game_state = STATE_NONE;
    state_change(STATE_PLAY);
}

static void set_paused(uint8_t p);

// A talker's action (talk.h): the lifts, the shop.
static void take_action(void) {
    uint8_t a = talk_action;
    talk_action = 0;
    if (a == TALK_DO_DOWN) {
        go(MODE_DESCENT);  // a new expedition
    } else if (a == TALK_DO_UP) {
        profile.exp_active = 0;  // back up with everything: the expedition is over (no fee)
        go(MODE_YARD);
    } else if (a == TALK_DO_SHOP) {
        menu_page = PAGE_SHOP;  // the pause menu's shop page: the world waits while you browse
        set_paused(1);
    }
}

static void set_paused(uint8_t p) {
    paused = p;
    // Sprites would draw over the pause panel, so hide them while paused.
    if (p) {
        pickups_card_off();
        hud_hide();
        menu_open();
        if (menu_page == PAGE_TUNE) draw_pause();
        move_win(7, PAUSE_WY);
        gfx_sprites(0);
    } else {
        menu_close();
        hud_init();  // the menu's text borrowed the HUD's tiles
        hud_draw();
        move_win(7, play_mode == MODE_BOSS ? 120 : HUD_WY);
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
    if (paused && menu_page == PAGE_SHOP) {
        if (joy_pressed & (J_START | J_B)) {
            set_paused(0);
            menu_page = PAGE_CHARACTER;
        } else {
            menu_update();
        }
        return;
    }
    if (paused) {
        if (joy_pressed & J_START) set_paused(0);
        else if (joy_pressed & J_SELECT) {
            // A file: save and quit (the expedition goes on from this map's entrance
            // next time); the lab and the yard: end the run.
            if (save_slot != SAVE_NONE) {
                save_add_time(run_seconds - saved_seconds);
                saved_seconds = run_seconds;
                profile.kills += enemies_killed;
                enemies_killed = 0;  // (counted now: not again when a run ends)
                save_write();
                state_change(STATE_TITLE);
            } else {
                state_change(STATE_RUN_OVER);
            }
        } else if (joy_pressed & J_B) {
            if (!menu_back()) {
                menu_next_page();
                draw_page();
            }
        } else if (menu_page == PAGE_TUNE) tune_update();
        else menu_update();
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
            // The file's loadout; the lab and the yard cycle the five plain guns.
            if (save_slot != SAVE_NONE) {
                loadout_next();
                pickups_card_refresh();
            }
            else weapon_select(weapon_cur + 1 < WEAPON_COUNT ? weapon_cur + 1 : 0);  // the HUD shows it
        }
        chord_mask &= joy;
    }
    // A on a gun's card: take it, don't fire (A stays out until let go).
    if ((joy_pressed & J_A) && !(chord_mask & J_A) && card_drop != CARD_NONE) {
        chord_mask |= J_A;
        pickups_take();
        return;
    }
    // A beside someone who talks: talk, don't fire (A stays out until let go).
    if ((joy_pressed & J_A) && !(chord_mask & J_A) && talk_near != TALK_NONE) {
        chord_mask |= J_A;
        talk_start();
        if (talk_action) take_action();
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
    if (pickups_on) pickups_update();
    if (play_mode == MODE_LAB) {
        waves_update();
    } else if (play_mode == MODE_DESCENT) {
        uint8_t door = descent_door();
        if (door) descent_change_room(door);
        descent_update();
    } else if (play_mode == MODE_ENEMY) {
        enemy_trial_update();
    } else if (play_mode >= MODE_MINIBOSS && play_mode != MODE_YARD) {
        boss_update();
    }
    // The camera only follows the player, so it moves first; then actors and
    // bullets can move and draw in one pass against this frame's camera.
    camera_update(player_x + 8, player_y + 8, player_vx, player_vy);
    // Boss hazards reserve entries ahead of projectiles so crowded shot pools
    // cannot hide damaging beams or floor claims. Talkers and loot come next (few,
    // standing still; never in a boss's room), then actors and shots.
    uint8_t first = talk_count ? talk_draw(play_actor_oam) : play_actor_oam;
    if (pickups_on) first = pickups_draw(first);
    actors_update(first);
    // Kills' XP (actor.c), every 8th frame: not on the kill's own frame, the busiest.
    if (xp_pending && (run_ticks & 7) == 3) progress_award();
    if (play_mode == MODE_ENEMY) {
        if (enemy_trial_kind==2) acid_wolf_draw();
        else if (enemy_trial_kind==1) mercenary_draw();
        else acid_skimmer_draw();
    } else if (play_mode == MODE_LAB) {
        chest_update();
    }
    aim_update();
    player_draw();
    health_update();
    if (card_drop == CARD_NONE) hud_update();  // (a gun's card has the window: the HUD redraws when it closes)
    if (boss_kind != BOSS_NONE) {
        boss_status();
        if (boss_kind == BOSS_GUNSHIP) gunship_draw();
        else if (boss_kind == BOSS_WARDEN) warden_draw();
        else if (boss_kind == BOSS_DUO) duo_draw();
        else if (boss_kind == BOSS_SCRAPBACK) scrapback_draw();
        else if (boss_kind == BOSS_FLYNT) flynt_draw();
    }
    sfx_watch();
    if (!player_hp) {
        sfx_play(SFX_DEATH);
        state_change(STATE_RUN_OVER);
    }
}
