#ifndef OPENING_H
#define OPENING_H
#include <gb/gb.h>
extern uint8_t opening_timer, opening_notice;
void opening_setup(void) BANKED;
uint8_t opening_update(void) BANKED;
void opening_arrive(void) BANKED;
uint8_t opening_shield(uint8_t chest) BANKED;
#endif
