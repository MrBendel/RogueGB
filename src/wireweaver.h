#ifndef WIREWEAVER_H
#define WIREWEAVER_H
#include <gb/gb.h>
extern uint8_t web_slow;
void wireweaver_start(void) BANKED;
void wireweaver_pre(void) BANKED;
void wireweaver_update(void) BANKED;
void wireweaver_draw(void) BANKED;
#endif
