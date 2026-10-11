// Gameplay scene. For now this is the movement lab: one room with pillars,
// a placeholder player, and movement presets to compare.
//   SELECT        cycle movement presets
//   START         pause; the pause panel tunes the movement values
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

// HUD: one block row (2 tile rows) on the window layer under the 10x8-block room.
#define HUD_WY (ROOM_PX_H)
// Pause panel: 9 tile rows on the window layer.
#define PAUSE_H 9
#define PAUSE_WY (144 - PAUSE_H * 8)

#define TUNE_SPEED 0
#define TUNE_ACCEL 1
#define TUNE_DECEL 2
#define TUNE_REVERSE 3
#define TUNE_COUNT 4
#define TUNE_ROW0 2  // window row of the first tunable

#define SPEED_STEP 64  // 0.25 px/frame
#define SPEED_MIN 64
#define SPEED_MAX 768
#define FRAMES_MAX 60

// Player start: middle of the bottom corridor.
#define START_X 72
#define START_Y 96

// --- watchable state ---
uint16_t run_seed;
uint16_t run_seconds;
uint8_t run_ticks;
uint8_t paused;
uint8_t tune_sel;

void run_start(uint16_t seed) {
    run_seed = seed;
    initrand(seed);
    run_seconds = 0;
    run_ticks = 0;
}

static void draw_hud(void) {
    fill_win_rect(0, 0, SCREEN_TW, 2, FONT_TILES);
    win_text(1, 0, "MOVE:");
    win_text(7, 0, move_preset_name());
    win_text(0, 1, "SEL:NEXT  START:TUNE");
}

static void draw_tune_value(uint8_t i) {
    uint8_t y = TUNE_ROW0 + i;
    win_text(1, y, i == tune_sel ? ">" : " ");
    if (i == TUNE_SPEED) {
        win_number(11, y, move_top >> 8, 1, 0);
        win_text(12, y, ".");
        win_number(13, y, ((move_top & 0xFF) * 100) >> 8, 2, 1);
        win_text(16, y, "PX");
    } else {
        uint8_t f = i == TUNE_ACCEL ? move_accel_frames : i == TUNE_DECEL ? move_decel_frames : move_reverse_frames;
        win_number(12, y, f, 3, 0);
        win_text(16, y, "FR");
    }
}

static void draw_pause(void) {
    win_frame(0, 0, SCREEN_TW, PAUSE_H);
    win_text(4, 1, "PAUSED: TUNE");
    win_text(2, TUNE_ROW0 + TUNE_SPEED, "SPEED");
    win_text(2, TUNE_ROW0 + TUNE_ACCEL, "ACCEL");
    win_text(2, TUNE_ROW0 + TUNE_DECEL, "DECEL");
    win_text(2, TUNE_ROW0 + TUNE_REVERSE, "REVERSE");
    for (uint8_t i = 0; i < TUNE_COUNT; i++) draw_tune_value(i);
    win_text(2, 6, "START  RESUME");
    win_text(2, 7, "SELECT END RUN");
}

void play_enter(void) {
    room_load_lab();
    player_init(START_X, START_Y);
    if (!move_top) move_apply_preset(0);  // first run; later runs keep the chosen movement
    paused = 0;
    tune_sel = 0;
    draw_hud();
    move_win(7, HUD_WY);
    SHOW_WIN;
    SHOW_SPRITES;
    player_draw();
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

static void adjust_frames(uint8_t *f, int8_t d) {
    if (d < 0 && *f > 1) (*f)--;
    if (d > 0 && *f < FRAMES_MAX) (*f)++;
}

static void tune_update(void) {
    uint8_t old = tune_sel;
    if ((joy_pressed & J_UP) && tune_sel > 0) tune_sel--;
    if ((joy_pressed & J_DOWN) && tune_sel < TUNE_COUNT - 1) tune_sel++;
    if (old != tune_sel) {
        draw_tune_value(old);
        draw_tune_value(tune_sel);
    }
    int8_t d = (joy_pressed & J_RIGHT) ? 1 : (joy_pressed & J_LEFT) ? -1 : 0;
    if (!d) return;
    if (tune_sel == TUNE_SPEED) {
        if (d < 0 && move_top > SPEED_MIN) move_top -= SPEED_STEP;
        if (d > 0 && move_top < SPEED_MAX) move_top += SPEED_STEP;
    } else if (tune_sel == TUNE_ACCEL) adjust_frames(&move_accel_frames, d);
    else if (tune_sel == TUNE_DECEL) adjust_frames(&move_decel_frames, d);
    else adjust_frames(&move_reverse_frames, d);
    move_preset = MOVE_PRESET_CUSTOM;
    move_recalc();
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
    player_draw();
}
