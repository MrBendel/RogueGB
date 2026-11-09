#ifndef DESERT_H
#define DESERT_H
#include <gb/gb.h>
#define DESERT_CROSSROADS 5
#define DESERT_RUSTYARD 6
#define DESERT_WAKE 9
extern uint8_t desert_return;
void desert_generate(void) BANKED;
void desert_build(uint8_t map) BANKED;
#endif
