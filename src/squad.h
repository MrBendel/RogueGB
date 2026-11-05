// The camp kinds made from the enemy trials: the spider, the bulwark, the rift
// stalker (squad.c). Actor kinds like the hound: any number, any squad.
#ifndef SQUAD_H
#define SQUAD_H

#include <gb/gb.h>
#include <stdint.h>

// Sprite tiles in the Color's second VRAM bank, after the guns' (weapon.c: 0-171)
// and before the menus' text (192+).
#define SPIDER_VRAM 172   // 8: two frames
#define BULWARK_VRAM 180  // 8: its walk, facing right (flipped for left)
#define STALKER_VRAM 188  // 4: one frame (flipped as it shimmers)
#define SPIDER_HP 5
#define BULWARK_HP 16
#define STALKER_HP 10
extern uint8_t spider_frames[4 * 2], bulwark_frames[4 * 4], stalker_frames[4 * 2];
extern uint16_t squad_blocks;

void squad_init(void) BANKED;            // the art, and frames (actors_init)
void squad_spawn(uint8_t i) BANKED;      // health and timers for a new one (actor_spawn)
void squad_update(uint8_t i) BANKED;     // a think step (actors_update)
// Would the bulwark at i's shield stop a swing from (x, y)?
uint8_t squad_blocks_melee(uint8_t i, uint16_t x, uint16_t y) BANKED;

#endif
