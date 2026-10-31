#pragma bank 255
// Health and the shield layer: see health.h. Banked (a call per frame and per
// hit); the hit checks that call it are in bank 0 (player.c, actor.c).
#include <gb/gb.h>
#include "health.h"
#include "elite.h"

// How long the HUD's effects last (frames).
#define SHIELD_FLASH_FRAMES 8
#define SHIELD_BROKE_FRAMES 24

uint8_t player_hp, player_hp_max;
uint8_t shield_hp, shield_cap;
uint8_t shield_delay, shield_rate;
uint8_t shield_wait;
uint8_t shield_tick;   // frames until the next point recharged
uint8_t health_regen;
uint8_t regen_tick;
uint16_t player_hits;
uint16_t shield_hits;
uint8_t shield_flash, shield_broke;
uint8_t hurt_cause, death_cause, death_led;

void health_start(void) BANKED {
    if (!player_hp_max) player_hp_max = HEALTH_START;  // later runs keep a tuned max
    player_hp = player_hp_max;
    shield_hp = shield_cap = 0;
    shield_delay = SHIELD_START_DELAY;  // what the tuner starts from
    shield_rate = SHIELD_START_RATE;
    shield_wait = shield_tick = 0;
    shield_flash = shield_broke = 0;
    regen_tick = 0;
    player_hits = shield_hits = 0;
    hurt_cause = death_cause = CAUSE_NONE;
    death_led = 0;
}

void health_give_shield(uint8_t cap, uint8_t delay, uint8_t rate) BANKED {
    shield_cap = shield_hp = cap;
    shield_delay = delay;
    shield_rate = rate;
    shield_wait = 0;
}

uint8_t health_damage(uint8_t dmg, uint8_t flags) BANKED {
    if (shield_cap) shield_wait = shield_delay;  // any hit restarts the delay
    if (!(flags & DMG_DIRECT) && shield_hp) {
        if (dmg < shield_hp) {
            shield_hp -= dmg;
            shield_hits++;
            shield_flash = SHIELD_FLASH_FRAMES;
            return 0;
        }
        // Broken: the overflow spills into health.
        dmg -= shield_hp;
        shield_hp = 0;
        shield_broke = SHIELD_BROKE_FRAMES;
        if (!dmg) {
            shield_hits++;
            return 0;
        }
    }
    player_hits++;
    if (dmg >= player_hp) {
        player_hp = 0;
        death_cause = hurt_cause;
        death_led = elite_living;
    } else {
        player_hp -= dmg;
    }
    return 1;
}

void health_update(void) BANKED {
    if (shield_flash) shield_flash--;
    if (shield_broke) shield_broke--;
    if (shield_hp < shield_cap) {
        if (shield_wait) {
            shield_wait--;
        } else if (shield_tick) {
            shield_tick--;
        } else {
            shield_hp++;
            shield_tick = shield_rate ? shield_rate - 1 : 0;
        }
    }
    if (health_regen && player_hp && player_hp < player_hp_max) {
        if (regen_tick) {
            regen_tick--;
        } else {
            player_hp++;
            regen_tick = health_regen - 1;
        }
    }
}
