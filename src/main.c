// RogueGB: harness test scene.
//
// A placeholder program that exercises the build + test pipeline. It isn't
// the game yet: a ship moves with the d-pad and fires bullets with A. The
// globals below are deliberately non-static so the test harness and the
// playtest page can watch them by name (via build/roguegb.noi).
#include <gb/gb.h>
#include <stdint.h>

#define MAX_BULLETS 6
#define PLAYER_SPEED 1
#define BULLET_SPEED 3
#define FIRE_COOLDOWN 8

// --- tiles (2bpp, 16 bytes each) ---
const uint8_t sprite_tiles[] = {
    // 0: player ship
    0x18, 0x18, 0x3C, 0x24, 0x7E, 0x5A, 0xFF, 0x81,
    0xFF, 0xBD, 0x7E, 0x66, 0x24, 0x24, 0x66, 0x66,
    // 1: bullet
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x3C, 0x24,
    0x3C, 0x24, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t bkg_tiles[] = {
    // 0: empty
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 1: floor speckle
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
    // 2: wall
    0xFF, 0x00, 0x81, 0x7E, 0xBD, 0x42, 0xA5, 0x5A,
    0xA5, 0x5A, 0xBD, 0x42, 0x81, 0x7E, 0xFF, 0x00,
};

// --- watchable state ---
uint8_t player_x;
uint8_t player_y;
uint8_t joy;
uint16_t frame_count;
uint16_t bullets_fired;
uint8_t bullets_active;
uint8_t fire_cooldown;

uint8_t bullet_x[MAX_BULLETS];
uint8_t bullet_y[MAX_BULLETS];
uint8_t bullet_alive[MAX_BULLETS];

static void draw_room(void) {
    fill_bkg_rect(0, 0, 20, 18, 1);
    fill_bkg_rect(0, 0, 20, 1, 2);
    fill_bkg_rect(0, 17, 20, 1, 2);
    fill_bkg_rect(0, 0, 1, 18, 2);
    fill_bkg_rect(19, 0, 1, 18, 2);
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
                hide_sprite(1 + i);
                continue;
            }
            bullet_y[i] -= BULLET_SPEED;
            move_sprite(1 + i, bullet_x[i], bullet_y[i]);
            bullets_active++;
        }
    }
}

void main(void) {
    DISPLAY_OFF;
    set_bkg_data(0, 3, bkg_tiles);
    set_sprite_data(0, 2, sprite_tiles);
    draw_room();

    SPRITES_8x8;
    set_sprite_tile(0, 0);
    for (uint8_t i = 0; i < MAX_BULLETS; i++) set_sprite_tile(1 + i, 1);

    player_x = 84;   // sprite coords are offset by (8, 16): screen x 76
    player_y = 136;  // screen y 120
    move_sprite(0, player_x, player_y);

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    while (1) {
        joy = joypad();

        if ((joy & J_LEFT) && player_x > 8 + 8) player_x -= PLAYER_SPEED;
        if ((joy & J_RIGHT) && player_x < 8 + 144) player_x += PLAYER_SPEED;
        if ((joy & J_UP) && player_y > 16 + 8) player_y -= PLAYER_SPEED;
        if ((joy & J_DOWN) && player_y < 16 + 128) player_y += PLAYER_SPEED;

        if (fire_cooldown) fire_cooldown--;
        else if (joy & J_A) fire();

        move_sprite(0, player_x, player_y);
        update_bullets();

        frame_count++;
        vsync();
    }
}
