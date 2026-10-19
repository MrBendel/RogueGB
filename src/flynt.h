#ifndef FLYNT_H
#define FLYNT_H
#include <gb/gb.h>
#include <stdint.h>

// Captain Flynt combat trial: Southern Shelf furnace deck
#define FLYNT_STALK   0
#define FLYNT_AIM     1
#define FLYNT_CLEAVE  2
#define FLYNT_RECOVER 3
#define FLYNT_CLOAK   4
#define FLYNT_LEAP    5
#define FLYNT_SLAM    6
#define FLYNT_STUNNED 7

extern uint8_t flynt_state, flynt_timer, flynt_phase;
extern uint8_t flynt_vent_cycle, flynt_vent_timer;
extern uint8_t flynt_minion_timer, flynt_z;
extern uint8_t vent_stage[4];
extern uint16_t flynt_cleaves, flynt_slams, flynt_stuns;

void flynt_room(void) BANKED;
void flynt_start(void) BANKED;
void flynt_update(void) BANKED;
void flynt_draw(void) BANKED;
void flynt_status(void) BANKED;

#endif
