// The worlds (world.c): a run goes down through one world (LunarBase so far), and
// everything that differs between worlds comes from its row of the world table:
// its art, the enemies its camps wake, the minibosses at its shafts.
// docs/design/memory.md, "One world at a time".
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include <gb/gb.h>

#define WORLD_LUNAR 0
#define WORLD_COUNT 1

#define WORLD_ROSTER 8  // a camp's enemy kinds, one drawn per spawn (ACTOR_*: repeats weight them)
#define WORLD_TIERS 3   // rosters by depth: the first area's, the second's, then every deeper one's
#define WORLD_MINIBOSSES 3

typedef struct {
    uint8_t theme;                         // chunks.mjs CHUNK_THEME_*: its map pieces
    uint8_t roster[WORLD_TIERS][WORLD_ROSTER];  // ACTOR_*, by tier
    uint8_t minibosses[WORLD_MINIBOSSES];  // MODE_MINIBOSS kinds (play.c), in turn with depth
    // Its enemies' colours 1-3 (accent, body, outline): creatures' and machines'
    // (actor.h: odd and even kinds), and its bullets' and elites' (elite.h).
    uint16_t creature_pal[3], machine_pal[3], hot_pal[3];
} World;

extern uint8_t world_id;  // the world the run is in (WORLD_*)

void world_enter(uint8_t id) BANKED;         // a run into world id: its map pieces, its enemies' colours
void world_load_tiles(uint8_t indoor) BANKED;  // its outdoor or indoor tileset (the display off)
uint8_t world_roster_kind(uint8_t tier, uint8_t j) BANKED;  // the roster's j-th kind at a tier (missions)
uint8_t world_miniboss_kind(uint8_t i) BANKED;  // the world's i-th miniboss (MODE_MINIBOSS kind)
// A map's few kinds (2 in the first area, 3 deeper), drawn from the roster at depth the
// first time it's asked: every camp there, its reinforcements and an ambush use only
// those. world_room_clear forgets them (a new map, or a building), seeding the next
// draw from the map (the same kinds each time you're back).
#define ROOM_KINDS 3
extern uint8_t room_kinds[ROOM_KINDS], room_kind_n;
extern uint16_t room_seed;
void world_room_clear(uint16_t seed) BANKED;
uint8_t world_enemy(uint8_t depth) BANKED;    // one of the map's kinds at random (one rand)
uint8_t world_squad(uint8_t depth, uint8_t j) BANKED;  // the map's j-th kind (mod its count): a squad takes them in turn
uint8_t world_miniboss(uint8_t depth) BANKED; // the miniboss guarding depth's shaft

#endif
