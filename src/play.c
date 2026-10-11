// Gameplay scene. For now this is still the placeholder room from the harness
// test scene: a ship moves with the d-pad and fires with A. START pauses;
// SELECT while paused ends the run.
#include <gb/gb.h>
#include <rand.h>
#include "state.h"
#include "input.h"
#include "gfx.h"
#include "run.h"

#define MAX_BULLETS 6
#define PLAYER_SPEED 1
#define BULLET_SPEED 3
#define FIRE_COOLDOWN 8

#define SPR_PLAYER 0
#define SPR_BULLET 1  // sprites 1..MAX_BULLETS

#define TILE_FLOOR (SCENE_TILES + 0)
#define TILE_WALL  (SCENE_TILES + 1)

// Pause panel on the window layer, shown at the bottom of the screen.
#define PAUSE_H 5
#define PAUSE_WY (144 - PAUSE_H * 8)

// --- tiles (2bpp, 16 bytes each) ---
static const uint8_t sprite_tiles[] = {
    // 0: player ship
    0x18, 0x18, 0x3C, 0x24, 0x7E, 0x5A, 0xFF, 0x81,
    0xFF, 0xBD, 0x7E, 0x66, 0x24, 0x24, 0x66, 0x66,
    // 1: bullet
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x3C, 0x24,
    0x3C, 0x24, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t room_tiles[] = {
    // floor speckle
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    // wall
    0xFF, 0x00, 0x81, 0x7E, 0xBD, 0x42, 0xA5, 0x5A,
    0xA5, 0x5A, 0xBD, 0x42, 0x81, 0x7E, 0xFF, 0x00,
};

// --- watchable state ---
uint16_t run_seed;
uint16_t run_seconds;
uint8_t run_ticks;

uint8_t paused;
uint8_t player_x;
uint8_t player_y;
uint16_t bullets_fired;
uint8_t bullets_active;
uint8_t fire_cooldown;

uint8_t bullet_x[MAX_BULLETS];
uint8_t bullet_y[MAX_BULLETS];
uint8_t bullet_alive[MAX_BULLETS];

void run_start(uint16_t seed) {
    run_seed = seed;
    initrand(seed);
    run_seconds = 0;
    run_ticks = 0;
    bullets_fired = 0;
}

static void draw_room(void) {
    fill_bkg_rect(0, 0, SCREEN_TW, SCREEN_TH, TILE_FLOOR);
    fill_bkg_rect(0, 0, SCREEN_TW, 1, TILE_WALL);
    fill_bkg_rect(0, SCREEN_TH - 1, SCREEN_TW, 1, TILE_WALL);
    fill_bkg_rect(0, 0, 1, SCREEN_TH, TILE_WALL);
    fill_bkg_rect(SCREEN_TW - 1, 0, 1, SCREEN_TH, TILE_WALL);
}

void play_enter(void) {
    set_bkg_data(SCENE_TILES, 2, room_tiles);
    set_sprite_data(0, 2, sprite_tiles);
    draw_room();

    win_frame(0, 0, SCREEN_TW, PAUSE_H);
    win_text(7, 1, "PAUSED");
    win_text(2, 2, "START  RESUME");
    win_text(2, 3, "SELECT END RUN");
    move_win(7, PAUSE_WY);

    set_sprite_tile(SPR_PLAYER, 0);
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        set_sprite_tile(SPR_BULLET + i, 1);
        bullet_alive[i] = 0;
    }
    bullets_active = 0;
    fire_cooldown = 0;
    paused = 0;
    SHOW_SPRITES;

    player_x = 84;   // sprite coords are offset by (8, 16): screen x 76
    player_y = 136;  // screen y 120
    move_sprite(SPR_PLAYER, player_x, player_y);
}

static void set_paused(uint8_t p) {
    paused = p;
    // Sprites would draw over the pause panel, so hide them while paused.
    if (p) {
        SHOW_WIN;
        HIDE_SPRITES;
    } else {
        HIDE_WIN;
        SHOW_SPRITES;
    }
}

static void fire(void) {
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (!bullet_alive[i]) {
            bullet_alive[i] = 1;
            bullet_x[i] = player_x;
            bullet_y[i] = player_y - 6;
            bullets_fired++;
            fire_cooldown = FIRE_COOLDOWN;
            return;
        }
    }
}

static void update_bullets(void) {
    bullets_active = 0;
    for (uint8_t i = 0; i < MAX_BULLETS; i++) {
        if (bullet_alive[i]) {
            if (bullet_y[i] < 16 + 8 + BULLET_SPEED) {
                bullet_alive[i] = 0;
                hide_sprite(SPR_BULLET + i);
                continue;
            }
            bullet_y[i] -= BULLET_SPEED;
            move_sprite(SPR_BULLET + i, bullet_x[i], bullet_y[i]);
            bullets_active++;
        }
    }
}

void play_update(void) {
    if (paused) {
        if (joy_pressed & J_START) set_paused(0);
        else if (joy_pressed & J_SELECT) state_change(STATE_RUN_OVER);
        return;
    }
    if (joy_pressed & J_START) {
        set_paused(1);
        return;
    }

    if (++run_ticks == 60) {
        run_ticks = 0;
        run_seconds++;
    }

    if ((joy & J_LEFT) && player_x > 8 + 8) player_x -= PLAYER_SPEED;
    if ((joy & J_RIGHT) && player_x < 8 + 144) player_x += PLAYER_SPEED;
    if ((joy & J_UP) && player_y > 16 + 8) player_y -= PLAYER_SPEED;
    if ((joy & J_DOWN) && player_y < 16 + 128) player_y += PLAYER_SPEED;

    if (fire_cooldown) fire_cooldown--;
    else if (joy & J_A) fire();

    move_sprite(SPR_PLAYER, player_x, player_y);
    update_bullets();
}
