// Loot in the world (the descent): the map's chests (level.c places them: one in
// each camp, unlocked when it's cleared, and hidden caches), and what lies on the
// floor: credits, picked up by walking over them, and guns. Standing on a gun
// shows a card comparing it with the one you hold; A takes it (into a free slot,
// or swapped for the held gun, which drops in its place). Enemies sometimes drop
// credits. Opened chests stay open for the expedition; the floor clears on a map
// change.
#ifndef PICKUPS_H
#define PICKUPS_H

#include <gb/gb.h>
#include <stdint.h>
#include "loot.h"

#define DROP_MAX 6
#define DROP_NONE 0
#define DROP_CREDITS 1
#define DROP_GUN 2
#define DROP_GUNS 3  // guns on the floor at once (their Gun records)
#define CARD_NONE 0xFF

// --- watchable state ---
extern uint8_t pickups_on;              // this map has chests or drops (play skips the calls otherwise)
extern uint8_t chest_open_bits;         // this map's chests opened (a bit each)
extern uint8_t drop_kind[DROP_MAX];
extern uint16_t drop_x[DROP_MAX], drop_y[DROP_MAX];
extern uint16_t drop_value[DROP_MAX];   // credits, or the gun's record (0..DROP_GUNS-1)
extern Gun floor_gun[DROP_GUNS];
extern uint8_t card_drop;               // the gun drop whose card shows, or CARD_NONE
extern uint16_t credits_picked, guns_taken, chests_opened;
extern uint16_t kill_x, kill_y;         // where the last enemy died (actor.c)

void pickups_init(void) BANKED;           // once per play scene: art
void pickups_load_map(uint8_t opened) BANKED;  // a map was built: its chests (opened bits), no drops
void pickups_update(void) BANKED;         // per frame: chests, picking up, the card
uint8_t pickups_draw(uint8_t oam) BANKED; // chests and drops from oam on; returns the next free
void pickups_take(void) BANKED;           // A on the card: take the gun
void pickups_card_refresh(void) BANKED;    // the held gun changed: the card's comparison again
void pickups_card_off(void) BANKED;       // put the card away (before the pause menu takes the window)
void loot_kill_drop(void) BANKED;         // maybe credits where the last enemy died

#endif
