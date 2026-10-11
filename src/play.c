// Gameplay scene. For now this is the movement lab: one room with pillars,
// a placeholder player, and movement presets to compare.
//   B             jump; B again in mid-air dashes
//   SELECT        cycle movement presets
//   START         pause; the pause panel tunes movement and jump values
//                 (UP/DOWN pick a value, LEFT/RIGHT change it)
//   SELECT paused end the run
#include <gb/gb.h>
#include <rand.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"
#include "room.h"
#include "player.h"
#include "camera.h"
#include "actor.h"

// HUD: one block row (2 tile rows) on the window layer under the 10x8-block room.
#define HUD_WY VIEW_H
// Pause panel: 15 tile rows on the window layer.
#define PAUSE_H 15
#define PAUSE_WY (144 - PAUSE_H * 8)
#define TUNE_ROW0 2  // window row of the first tunable

// Tunables shown on the pause panel. FX16 = uint16 8.8 shown as x.xx,
// FX8 = uint8 in 1/256ths shown as 0.xx, FRAMES = uint8 shown as a count.
#define KIND_FX16 0
#define KIND_FX8 1
#define KIND_FRAMES 2
#define KIND_NUM 3  // uint8 shown as a plain number
typedef struct {
    const char *label;
    uint8_t kind;
    uint8_t movement;  // changing it makes the movement preset CUSTOM
    void *value;
    uint16_t step, min, max;
} Tunable;

static const Tunable tunables[] = {
    {"SPEED", KIND_FX16, 1, &move_top, 64, 64, 768},
    {"ACCEL", KIND_FRAMES, 1, &move_accel_frames, 1, 1, 60},
    {"DECEL", KIND_FRAMES, 1, &move_decel_frames, 1, 1, 60},
    {"REVERSE", KIND_FRAMES, 1, &move_reverse_frames, 1, 1, 60},
    {"JUMP", KIND_FX16, 0, &jump_vel, 32, 128, 1024},
    {"GRAVITY", KIND_FX8, 0, &jump_grav, 4, 4, 128},
    {"DASH SPD", KIND_FX16, 0, &dash_speed, 64, 256, 1536},
    {"DASH FR", KIND_FRAMES, 0, &dash_frames, 1, 1, 30},
    {"CAM LEAD", KIND_FRAMES, 0, &cam_lead, 2, 0, 40},
    {"CAM EASE", KIND_NUM, 0, &cam_ease, 1, 0, 5},
};
#define TUNE_COUNT (sizeof(tunables) / sizeof(tunables[0]))

// Hardware sprites 0-3 are the player's (body and shadow); actors draw from here.
#define FIRST_ACTOR_OAM 4
// The arena is the screen below the lab (rows 16-22): entering it spawns a wave.
#define ARENA_TOP (16 * BLOCK_SIZE)

// Player start: on the ledge in the top-left screen.
#define START_X 72
#define START_Y 96

// --- watchable state ---
uint16_t run_seed;
uint16_t run_seconds;
uint8_t run_ticks;
uint8_t paused;
uint8_t tune_sel;
uint8_t wave_spawned;
uint8_t hud_hp;  // HP shown on the HUD, to redraw only on change

void run_start(uint16_t seed) {
    run_seed = seed;
    initrand(seed);
    run_seconds = 0;
    run_ticks = 0;
}

// HP is one digit: draw it directly (win_number divides, which is slow).
static void draw_hp(void) {
    set_win_tile_xy(17, 0, FONT_TILES + ('0' - ' ') + player_hp);
    hud_hp = player_hp;
}

static void draw_hud(void) {
    fill_win_rect(0, 0, SCREEN_TW, 2, FONT_TILES);
    win_text(0, 0, "MOVE:");
    win_text(6, 0, move_preset_name());
    win_text(14, 0, "HP:");
    draw_hp();
    win_text(0, 1, "SEL:NEXT  START:TUNE");
}

static uint16_t tune_get(const Tunable *t) {
    return t->kind == KIND_FX16 ? *(uint16_t *)t->value : *(uint8_t *)t->value;
}

static void tune_set(const Tunable *t, uint16_t v) {
    if (t->kind == KIND_FX16) *(uint16_t *)t->value = v;
    else *(uint8_t *)t->value = (uint8_t)v;
}

static void draw_tune_value(uint8_t i) {
    const Tunable *t = &tunables[i];
    uint8_t y = TUNE_ROW0 + i;
    uint16_t v = tune_get(t);
    win_text(1, y, i == tune_sel ? ">" : " ");
    if (t->kind == KIND_FRAMES || t->kind == KIND_NUM) {
        win_number(12, y, v, 3, 0);
        win_text(16, y, t->kind == KIND_FRAMES ? "FR" : "  ");
    } else {
        win_number(11, y, v >> 8, 1, 0);
        win_text(12, y, ".");
        win_number(13, y, ((v & 0xFF) * 100) >> 8, 2, 1);
        win_text(16, y, "PX");
    }
}

static void draw_pause(void) {
    win_frame(0, 0, SCREEN_TW, PAUSE_H);
    win_text(4, 1, "PAUSED: TUNE");
    for (uint8_t i = 0; i < TUNE_COUNT; i++) {
        win_text(2, TUNE_ROW0 + i, tunables[i].label);
        draw_tune_value(i);
    }
    win_text(2, TUNE_ROW0 + TUNE_COUNT, "START  RESUME");
    win_text(2, TUNE_ROW0 + TUNE_COUNT + 1, "SELECT END RUN");
}

void play_enter(void) {
    room_load_lab();
    player_init(START_X, START_Y);
    if (!move_top) move_apply_preset(0);  // first run; later runs keep the chosen movement
    camera_init(player_x + 8, player_y + 8);
    actors_init();
    wave_spawned = 0;
    paused = 0;
    tune_sel = 0;
    draw_hud();
    move_win(7, HUD_WY);
    SHOW_WIN;
    SHOW_SPRITES;
    player_draw();
}

// First wave: two peashooters in the top corners, three slimes below.
static void spawn_wave(void) {
    wave_spawned = 1;
    actor_spawn(ACTOR_PEASHOOTER, 2 * BLOCK_SIZE, 17 * BLOCK_SIZE);
    actor_spawn(ACTOR_PEASHOOTER, 17 * BLOCK_SIZE, 17 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 4 * BLOCK_SIZE, 20 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 15 * BLOCK_SIZE, 20 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 9 * BLOCK_SIZE, 22 * BLOCK_SIZE);
}

static void set_paused(uint8_t p) {
    paused = p;
    // Sprites would draw over the pause panel, so hide them while paused.
    if (p) {
        draw_pause();
        move_win(7, PAUSE_WY);
        HIDE_SPRITES;
    } else {
        draw_hud();
        move_win(7, HUD_WY);
        SHOW_SPRITES;
    }
}

static void tune_update(void) {
    uint8_t old = tune_sel;
    if ((joy_pressed & J_UP) && tune_sel > 0) tune_sel--;
    if ((joy_pressed & J_DOWN) && tune_sel < TUNE_COUNT - 1) tune_sel++;
    if (old != tune_sel) {
        draw_tune_value(old);
        draw_tune_value(tune_sel);
    }
    const Tunable *t = &tunables[tune_sel];
    uint16_t v = tune_get(t);
    if ((joy_pressed & J_RIGHT) && v + t->step <= t->max) v += t->step;
    else if ((joy_pressed & J_LEFT) && v >= t->min + t->step) v -= t->step;
    else return;
    tune_set(t, v);
    if (t->movement) {
        move_preset = MOVE_PRESET_CUSTOM;
        move_recalc();
    }
    draw_tune_value(tune_sel);
}

void play_update(void) {
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
    if (joy_pressed & J_SELECT) {
        move_apply_preset(move_preset < MOVE_PRESET_COUNT - 1 ? move_preset + 1 : 0);
        draw_hud();
    }

    if (++run_ticks == 60) {
        run_ticks = 0;
        run_seconds++;
    }

    player_update();
    if (!wave_spawned && player_y >= ARENA_TOP) spawn_wave();
    actors_update();
    camera_update(player_x + 8, player_y + 8, player_vx, player_vy);
    player_draw();
    actors_draw(FIRST_ACTOR_OAM);
    if (player_hp != hud_hp) draw_hp();
    if (!player_hp) state_change(STATE_RUN_OVER);
}
