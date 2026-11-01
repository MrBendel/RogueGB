// Talking: things in a map you can speak to, and zones that start a script when
// you walk in (once). Beside something that talks, an "A" bubble shows over it
// and A opens its script (dialog.h) instead of firing; A stays away from the gun
// until it's let go. Each talker stands on a solid block.
//
// Talkers borrow the enemies' sprite palettes (slime, peashooter, bullet): a map
// has talkers or enemies, not both (towns and safe rooms talk; fields fight). The
// lift borrows the flash palette (put back when the map changes), so it can stand
// in the descent's start maps.
//
// A talker's "script" can instead be an action (TALK_DO_*): A on it sets
// talk_action for play.c (the shop, the lifts).
#ifndef TALK_H
#define TALK_H

#include <gb/gb.h>
#include <stdint.h>

#define TALK_MAX 12
#define TALK_ZONES 2
#define TALK_NONE 0xFF
// Kinds (their art: tools/assets.mjs spr_talk_*).
#define TALK_RELAY 0  // Echo's relay
#define TALK_POST 1   // a company post (the CEO's notices, Meridian's kiosks)
#define TALK_MARRO 2  // the scrap broker
#define TALK_LIFT 3   // the lift: town <-> descent
#define TALK_KINDS 4
#define TALK_COMMS 7  // drawn by the map (the entrance room's monitor): only its bubble
// People (rescue.h PERSON_*): TALK_PERSON + who. Drawn in the player's palette, their
// tiles from talk_people_at on (4 each, in the order they're added).
#define TALK_PERSON 8
#define TALK_PEOPLE 4
// Actions in place of a script.
#define TALK_DO 0x80
#define TALK_DO_SHOP 0x80  // Marro's shop
#define TALK_DO_DOWN 0x81  // the town's lift: down into the descent
#define TALK_DO_UP 0x82    // a start map's lift: back up to town
#define TALK_DO_VEND_AMMO 0x83    // a vending machine (descent.c descent_vend): every reserve full
#define TALK_DO_VEND_HEALTH 0x84  // ... health full
#define TALK_DO_VEND_GUN 0x85     // the town's: a random gun at your level, on the floor in front
#define TALK_DO_VEND_SHIELD 0x86  // ... the shield, then a bigger one (SHIELD_TIERS)
#define TALK_DO_BOARD 0x87  // the contracts board: missions (board.c)
#define TALK_DO_FREE 0x88   // a captive (rescue.c): free them

// --- watchable state ---
extern uint8_t talk_count;
extern uint8_t talk_bx[TALK_MAX], talk_by[TALK_MAX], talk_kind[TALK_MAX], talk_script[TALK_MAX];
extern uint8_t talk_near;   // the one in reach, or TALK_NONE
extern uint8_t zone_count;
extern uint8_t zones_done;  // a bit per zone: its script has played
extern uint16_t talks;      // conversations started by A
extern uint8_t talk_action; // TALK_DO_* asked for (play.c takes it), or 0
// Where people's tiles go: TALK_VRAM (the descent: the town's kinds aren't there),
// or a room with no enemies sets the enemies' tiles (actor.h) before talk_init.
extern uint8_t talk_people_at;

void talk_clear(void) BANKED;  // a map change: no talkers, no zones
void talk_add(uint8_t bx, uint8_t by, uint8_t kind, uint8_t script) BANKED;
void talk_remove(uint8_t i) BANKED;  // talker i gone (a captive freed): its block floor again
// A zone (blocks, inclusive) plays its script the first time you walk in; with a
// story bit (save.h STORY_*), once per file: skipped if set, set when it plays.
void talk_zone(uint8_t bx0, uint8_t by0, uint8_t bx1, uint8_t by1, uint8_t script, uint8_t story) BANKED;
void talk_init(void) BANKED;    // the talkers' tiles and palettes (after they're added)
void talk_update(void) BANKED;  // per frame: who's in reach; zones (may open a script)
void talk_start(void) BANKED;   // A in reach: open talk_near's script
uint8_t talk_draw(uint8_t oam) BANKED;  // talkers' sprites from oam on; returns the next free
// A person who isn't a talker (rescue.c: a follower): their tiles (display off),
// and their sprites at room pixels x, y in palette pal; returns the next free.
void talk_person_load(uint8_t who) BANKED;
uint8_t talk_person_draw(uint8_t oam, uint8_t who, uint16_t x, uint16_t y, uint8_t pal) BANKED;

#endif
