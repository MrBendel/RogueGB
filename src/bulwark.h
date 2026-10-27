#ifndef BULWARK_H
#define BULWARK_H
#include <gb/gb.h>
void bulwark_start(void) BANKED;
void bulwark_update(void) BANKED;
void bulwark_draw(void) BANKED;
uint8_t bulwark_blocks_melee(uint16_t x,uint16_t y) BANKED;
#endif
