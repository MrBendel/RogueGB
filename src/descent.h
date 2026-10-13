// The descent: a generated layer played room by room (see descent.c).
#ifndef DESCENT_H
#define DESCENT_H

#include <gb/gb.h>
#include <stdint.h>

#define DESCENT_NO_DOOR 0  // descent_enter: arrive in the room's middle

// --- watchable state ---
extern uint8_t layer_depth;
extern uint16_t rooms_entered;
extern uint8_t room_live;

void descent_start(uint16_t seed) BANKED;              // a new layer, in its start room (display off)
void descent_enter(uint8_t room, uint8_t side) BANKED; // build a room and place the player (display off)
void descent_update(void) BANKED;                      // per frame: a room cleared
uint8_t descent_door(void) BANKED;                     // per frame: the door being walked through (DOOR_*), or 0
void descent_change_room(uint8_t door) BANKED;         // fade to the room through that door

#endif
