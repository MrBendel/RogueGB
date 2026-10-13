#pragma bank 255
// The arena's waves (play.c calls waves_update each frame). Entering the arena
// starts wave 1 (the same 5 enemies every run); clearing a wave starts a short
// break, and the next wave comes once it's over and you're in the arena (going
// back up to the lab holds it). Each wave is bigger, up to ACTOR_MAX, with more
// peashooters, at arena spots away from you, and tougher (the ramp below).
// Banked: a few compares a frame.
#include <gb/gb.h>
#include <rand.h>
#include "waves.h"
#include "actor.h"
#include "player.h"
#include "room.h"
#include "hud.h"

uint8_t wave_spawned;   // a wave has ever spawned (the chest unlocks after the first is cleared)
uint8_t wave_num;       // the wave in progress or last spawned (0: none yet)
uint8_t waves_cleared;
uint16_t wave_break;    // frames left before the next wave (tests set it high to hold waves)
uint8_t wave_live;      // the current wave isn't cleared yet
uint8_t wave_start = 1;

// How the enemies get tougher, per wave (1-6; later waves use the last): the
// peashooter's interval between shots, the pea's speed (px/frame 8.8), whether
// it fires a 3-way spread, the slime's rest between hops and hop speed, and HP
// added to every enemy.
#define RAMP 6
static const uint8_t ramp_interval[RAMP] = {80, 72, 64, 56, 50, 44};
static const uint16_t ramp_bullet[RAMP] = {384, 416, 448, 480, 512, 512};
static const uint8_t ramp_spread[RAMP] = {0, 0, 1, 1, 1, 1};
static const uint8_t ramp_rest[RAMP] = {26, 22, 20, 18, 16, 14};
static const uint16_t ramp_slime[RAMP] = {192, 208, 224, 240, 256, 256};
static const uint8_t ramp_hp[RAMP] = {0, 0, 1, 1, 2, 2};

static void set_difficulty(uint8_t n) {
    uint8_t r = n > RAMP ? RAMP - 1 : n - 1;
    shooter_interval = ramp_interval[r];
    bullet_speed = ramp_bullet[r];
    shooter_spread = ramp_spread[r];
    slime_rest = ramp_rest[r];
    slime_speed = ramp_slime[r];
    enemy_hp_bonus = ramp_hp[r];
}

// Where later waves spawn (blocks in the arena, rows 16-22), tried in turn from
// a random start; a spot near the player is skipped.
static const uint8_t spots[][2] = {
    {2, 17}, {17, 17}, {4, 20}, {15, 20}, {9, 22}, {2, 22}, {13, 17}, {6, 17},
    {2, 19}, {12, 22}, {6, 22}, {17, 19}, {11, 18}, {8, 19},
};
#define SPOT_COUNT (sizeof(spots) / sizeof(spots[0]))
#define SPOT_CLEAR 56  // px each way from the player a spot must be

void waves_init(void) BANKED {
    wave_spawned = wave_num = waves_cleared = wave_live = 0;
    wave_break = 0;
}

static void say_wave(void) {
    char t[8] = "WAVE   ";
    uint8_t n = wave_num;
    if (n >= 10) {
        t[5] = '0' + n / 10;
        t[6] = '0' + n % 10;
    } else {
        t[5] = '0' + n;
    }
    hud_say(t);
}

static void spawn_first(void) {
    // Two peashooters in the top corners, three slimes below.
    actor_spawn(ACTOR_PEASHOOTER, 2 * BLOCK_SIZE, 17 * BLOCK_SIZE);
    actor_spawn(ACTOR_PEASHOOTER, 17 * BLOCK_SIZE, 17 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 4 * BLOCK_SIZE, 20 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 15 * BLOCK_SIZE, 20 * BLOCK_SIZE);
    actor_spawn(ACTOR_SLIME, 9 * BLOCK_SIZE, 22 * BLOCK_SIZE);
}

// Wave n (2+): 4 + n enemies (up to ACTOR_MAX), 1 + n / 2 of them peashooters.
static void spawn_more(uint8_t n) {
    uint8_t count = 4 + n, shooters = 1 + (n >> 1);
    if (count > ACTOR_MAX) count = ACTOR_MAX;
    if (shooters > count - 2) shooters = count - 2;
    uint8_t s = (uint8_t)rand() % SPOT_COUNT;
    for (uint8_t tries = 0; tries < SPOT_COUNT && count; tries++) {
        uint16_t x = (uint16_t)spots[s][0] * BLOCK_SIZE, y = (uint16_t)spots[s][1] * BLOCK_SIZE;
        if (++s == SPOT_COUNT) s = 0;
        int16_t dx = (int16_t)(x - player_x), dy = (int16_t)(y - player_y);
        if (dx > -SPOT_CLEAR && dx < SPOT_CLEAR && dy > -SPOT_CLEAR && dy < SPOT_CLEAR) continue;
        actor_spawn(shooters ? ACTOR_PEASHOOTER : ACTOR_SLIME, x, y);
        if (shooters) shooters--;
        count--;
    }
}

void waves_update(void) BANKED {
    if (!wave_num) {
        if (player_y >= ARENA_TOP) {
            wave_spawned = 1;
            wave_num = wave_start ? wave_start : 1;
            wave_live = 1;
            set_difficulty(wave_num);
            if (wave_num == 1) spawn_first();
            else spawn_more(wave_num);
            say_wave();
        }
        return;
    }
    if (wave_live) {
        if (actors_active) return;
        wave_live = 0;
        waves_cleared++;
        wave_break = WAVE_BREAK;
        char t[8] = "CLEAR! ";
        hud_say(t);
        return;
    }
    if (player_y < ARENA_TOP) return;  // waits for you in the arena
    if (wave_break) {
        wave_break--;
        return;
    }
    if (wave_num < 99) wave_num++;
    wave_live = 1;
    set_difficulty(wave_num);
    spawn_more(wave_num);
    say_wave();
}
