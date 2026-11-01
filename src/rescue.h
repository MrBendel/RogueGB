// Rescue jobs (mission.h MIS_RESCUE; rescue.c): someone from town is held at a
// camp in the area the job names. Clear the camp and walk up: depending on the
// job, A frees them (they head home), or they follow you back to the area's
// entrance, or you hold the camp while they cut themselves loose. Once the job's
// turned in at the board they stand in town (room_town.c). docs/design/runs.md,
// "Missions".
#ifndef RESCUE_H
#define RESCUE_H

#include <gb/gb.h>
#include <stdint.h>

// Who (a job's target: low nibble; talk.h TALK_PERSON + who).
#define PERSON_TESS 0
#define PERSON_OKAFOR 1
#define PERSON_NELL 2
#define PERSON_PIP 3
#define PEOPLE 4
// How (a job's target: high nibble).
#define RESCUE_FREE 0    // A frees them; they head home on their own
#define RESCUE_ESCORT 1  // they follow you to the area's entrance; they can be hurt
#define RESCUE_HOLD 2    // they cut themselves loose while you hold the camp
#define RESCUE_STYLES 3
#define RESCUE_WHO(t) ((t) & 15)
#define RESCUE_HOW(t) ((t) >> 4)

#define RS_NONE 0    // nobody held on this map (or no rescue in this area)
#define RS_HELD 1    // the captive stands at the camp (a talker)
#define RS_FOLLOW 2  // ... walking behind you (RESCUE_ESCORT)
#define RS_HOLD 3    // ... cutting loose while you hold (RESCUE_HOLD)

#define ESCORT_HP 8
#define HOLD_T 20       // seconds of holding, counted while you're near
#define HOLD_NEAR 80    // px from the captive that counts as near
#define HOLD_SPAWN 150  // frames between enemies coming at the camp

// --- watchable state ---
extern uint8_t rescue_slot;   // the job slot of this area's rescue, or 0xFF
extern uint8_t rescue_map;    // the map they're held in, or 0xFF
extern uint8_t rescue_state;  // RS_*
extern uint16_t rescue_x, rescue_y;  // a follower's top-left, room pixels
extern uint8_t rescue_hp;     // a follower's health
extern uint16_t rescue_timer; // RS_HOLD: seconds left

void rescue_area(void) BANKED;           // a new area (or one resumed): is someone held here?
void rescue_enter(uint8_t map) BANKED;   // descent_enter: the captive here, or a follower along (display off)
void rescue_free(void) BANKED;           // A on the captive (talk.h TALK_DO_FREE)
void rescue_update(void) BANKED;         // per frame while following or holding
uint8_t rescue_draw(uint8_t oam) BANKED; // a follower's sprites; returns the next free

#endif
