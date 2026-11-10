#ifndef ROCKLING_H
#define ROCKLING_H
#include <gb/gb.h>
// Dustwake's small Cragback relatives share the ordinary actor pool. Their
// otherwise-unused velocity fields hold the locked landing point (16-bit).
extern uint8_t rockling_on;
void rockling_load(void) BANKED;
void rockling_update(void) BANKED;
void rockling_emit(void) BANKED;
#endif
