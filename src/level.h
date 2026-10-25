// Generated areas (M2, docs/design/level-generation.md).
//
// An area is up to AREA_W x AREA_H linked maps: level_generate grows it outward
// from the start (so every map is reachable), adds a link or two for loops, and
// puts the way onward in the map farthest from the start. Each map keeps its
// type, size (in chunks), links and a seed; level_build_map assembles it into
// the room map from chunks (content/chunks), the same way every time it's
// entered: open ground with rock clusters, enemy camps around chests, maybe a
// hidden cache, and openings onto the linked maps. Everything follows from the seed.
#ifndef LEVEL_H
#define LEVEL_H

#include <gb/gb.h>
#include <stdint.h>

#define AREA_W 4
#define AREA_H 3
#define AREA_CELLS (AREA_W * AREA_H)
// The entrance room (made by hand: content/tilesets/lunar_int.json, map "entrance"):
// two rooms deep, 2 x 3 chunks. Its comms device (the monitor, 2x2 blocks) is on the
// upper room's back wall, so its north door is beside it, not in the middle.
#define COMMS_X 4  // the monitor's top-left block
#define COMMS_Y 1
#define ENTRANCE_DOOR_N 7

// Map types (0: no map in that slot).
#define MAP_NONE 0
#define MAP_START 1
#define MAP_FIELD 2
#define MAP_EXIT 3

// Links, a bit per side.
#define EDGE_N 1
#define EDGE_E 2
#define EDGE_S 4
#define EDGE_W 8

#define OPENING_W 4    // blocks wide
#define LEVEL_CAMPS 2  // camps per map at most
#define CAMP_SLOTS 5   // enemies per camp at most
#define LEVEL_CHESTS 3

// --- watchable state ---
extern uint8_t area_type[AREA_CELLS];
extern uint8_t area_links[AREA_CELLS];
extern uint8_t area_cw[AREA_CELLS], area_ch[AREA_CELLS];  // size in chunks
extern uint8_t area_camps_cleared[AREA_CELLS];            // a bit per camp
extern uint8_t area_chests_opened[AREA_CELLS];            // a bit per chest (pickups.c)
extern uint8_t area_start, area_exit, area_maps;
extern uint8_t level_map;  // the map the player is in
// The built map: its openings (block offset along each side, N E S W), camps
// (centre, enemy slots, room pixels) and chests.
extern uint8_t level_open[4];
extern uint8_t level_camps;
extern uint16_t camp_x[LEVEL_CAMPS], camp_y[LEVEL_CAMPS];
extern uint8_t camp_slots[LEVEL_CAMPS];
extern uint16_t camp_slot_x[LEVEL_CAMPS][CAMP_SLOTS], camp_slot_y[LEVEL_CAMPS][CAMP_SLOTS];
extern uint8_t level_chests;
extern uint16_t chest_x[LEVEL_CHESTS], chest_y[LEVEL_CHESTS];
extern uint8_t chest_camp[LEVEL_CHESTS];  // the camp guarding it, or 0xFF (a cache)
extern uint8_t level_carved;  // the last build had to carve paths

void level_generate(uint16_t seed) BANKED;
// Build map i into the room map (and set room_w/room_h).
void level_build_map(uint8_t i) BANKED;
void level_look_exterior(void) BANKED;
// The set pieces stamped on the current map (level.c, plan_set_pieces).
#define PF_MAX 16
extern uint8_t pf_count, pf_xid[PF_MAX], pf_y[PF_MAX];  // x | set piece << 5, and y
// ... and the pipes run between them: x | y << 5 | (length - 1) << 10 | down << 14.
#define PIPE_MAX 6
extern uint8_t pipe_count;
extern uint16_t pipes[PIPE_MAX];
#define CHUNK_ROWS 6  // the most chunks down a map (24 blocks: room.h ROOM_MAX_H)  // an exterior map's look: the theme's rules, then buildings' bases

#endif
