// The descent: generated areas explored map by map (see descent.c).
#ifndef DESCENT_H
#define DESCENT_H

#include <gb/gb.h>
#include <stdint.h>

#define DESCENT_NO_DOOR 0  // descent_enter: arrive in the map's middle
#define DESCENT_SHAFT 0x10 // descent_door: down the exit map's shaft
#define DESCENT_TOWN 0x20  // descent_door: out of the entrance room's south door, to town
#define DESCENT_INSIDE 0x40  // descent_door: | k, in through building door k (level.h door_bx)
#define DESCENT_OUTSIDE 0x80 // descent_door: out of a building's south door, onto its step
#define DESCENT_ARENA 0x30  // descent_door: up to an exit map's shaft, its miniboss not beaten yet
// An area's miniboss beaten: bit 7 of its exit map's area_chests_opened (so it's
// saved with the expedition: profile.exp_chests).
#define AREA_BOSS_BEATEN 0x80
void descent_save(void) BANKED;          // the expedition into the file now (before leaving for an arena)
uint8_t descent_miniboss(void) BANKED;   // the miniboss guarding this area's shaft (play.c's MODE_MINIBOSS kind)
void descent_vend(uint8_t what) BANKED;  // A on a vending machine (TALK_DO_VEND_*): pay, and fill up (or the town's: a gun, a shield)
extern uint8_t level_inside;  // the door of the building the player is in, or 0xFF (outside)

#define CAMP_ASLEEP 0
#define CAMP_AWAKE 1
#define CAMP_CLEARED 2

// --- watchable state ---
extern uint8_t layer_depth;     // which area of the descent (1: the first)
extern uint16_t maps_entered;
extern uint8_t camp_state[];    // per camp of the current map
extern uint16_t saved_seconds;   // run_seconds at the last save (save and quit adds the rest)

void descent_start(uint16_t seed) BANKED;             // a new descent, in the first area's start map (display off)
void descent_resume(void) BANKED;                     // the profile's expedition (save.h), at its map
void descent_enter(uint8_t map, uint8_t side) BANKED; // build a map and place the player (display off)
void descent_update(void) BANKED;                     // per frame: camps wake and clear
uint8_t descent_door(void) BANKED;                    // per frame: the opening walked into (EDGE_*), the shaft, or 0
void descent_change_room(uint8_t door) BANKED;        // fade to the map through that opening, or down the shaft

#endif
