#ifndef WARDEN_H
#define WARDEN_H
#include <gb/gb.h>
extern uint8_t warden_floor, warden_hud_dirty;
void warden_room(void) BANKED;
void warden_status(void) BANKED;
uint8_t warden_chase(void) BANKED;
uint8_t warden_near(void) BANKED;
void warden_start(void) BANKED;
void warden_update(void) BANKED;
void warden_draw(void) BANKED;
#endif
