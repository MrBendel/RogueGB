// Talking: things in a map you can speak to, and zones that start a script when
// you walk in (once). Beside something that talks, an "A" bubble shows over it
// and A opens its script (dialog.h) instead of firing; A stays away from the gun
// until it's let go. Each talker stands on a solid block.
//
// Talkers borrow the enemies' sprite palettes (slime, peashooter, bullet): a map
// has talkers or enemies, not both (towns and safe rooms talk; fields fight).
#ifndef TALK_H
#define TALK_H

#include <gb/gb.h>
#include <stdint.h>

#define TALK_MAX 4
#define TALK_ZONES 2
#define TALK_NONE 0xFF
// Kinds (their art: tools/assets.mjs spr_talk_*).
#define TALK_RELAY 0  // Echo's relay
#define TALK_POST 1   // a company post (the CEO's notices, Meridian's kiosks)
#define TALK_MARRO 2  // the scrap broker
#define TALK_KINDS 3

// --- watchable state ---
extern uint8_t talk_count;
extern uint8_t talk_bx[TALK_MAX], talk_by[TALK_MAX], talk_kind[TALK_MAX], talk_script[TALK_MAX];
extern uint8_t talk_near;   // the one in reach, or TALK_NONE
extern uint8_t zone_count;
extern uint8_t zones_done;  // a bit per zone: its script has played
extern uint16_t talks;      // conversations started by A

void talk_clear(void) BANKED;  // a map change: no talkers, no zones
void talk_add(uint8_t bx, uint8_t by, uint8_t kind, uint8_t script) BANKED;
void talk_zone(uint8_t bx0, uint8_t by0, uint8_t bx1, uint8_t by1, uint8_t script) BANKED;  // blocks, inclusive
void talk_init(void) BANKED;    // the talkers' tiles and palettes (after they're added)
void talk_update(void) BANKED;  // per frame: who's in reach; zones (may open a script)
void talk_start(void) BANKED;   // A in reach: open talk_near's script
uint8_t talk_draw(uint8_t oam) BANKED;  // talkers' sprites from oam on; returns the next free

#endif
