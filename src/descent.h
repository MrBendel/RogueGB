// The descent: generated areas explored map by map (see descent.c).
#ifndef DESCENT_H
#define DESCENT_H

#include <gb/gb.h>
#include <stdint.h>

#define DESCENT_NO_DOOR 0  // descent_enter: arrive in the map's middle
#define DESCENT_SHAFT 0x10 // descent_door: down the exit map's shaft

#define CAMP_ASLEEP 0
#define CAMP_AWAKE 1
#define CAMP_CLEARED 2

// --- watchable state ---
extern uint8_t layer_depth;     // which area of the descent (1: the first)
extern uint16_t maps_entered;
extern uint8_t camp_state[];    // per camp of the current map

void descent_start(uint16_t seed) BANKED;             // a new descent, in the first area's start map (display off)
void descent_enter(uint8_t map, uint8_t side) BANKED; // build a map and place the player (display off)
void descent_update(void) BANKED;                     // per frame: camps wake and clear
uint8_t descent_door(void) BANKED;                    // per frame: the opening walked into (EDGE_*), the shaft, or 0
void descent_change_room(uint8_t door) BANKED;        // fade to the map through that opening, or down the shaft

#endif
