#ifndef TOWN_H
#define TOWN_H
#include <gb/gb.h>
#define TOWN_OUTSIDE 0xff
#define TOWN_QUARTERS 0
#define TOWN_RELAY 1
#define TOWN_MARKET 2
#define TOWN_BUILDINGS 3
extern uint8_t town_inside, town_changing;
void town_reset(void) BANKED;
// Walk up into a facade, down through the interior's south doorway.
// Returns 1 after changing rooms; the caller stops the old frame.
uint8_t town_update(void) BANKED;
// Returns 1 while a committed downward ledge hop owns movement.
uint8_t town_move(void) BANKED;
#endif
