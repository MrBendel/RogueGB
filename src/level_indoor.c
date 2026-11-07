#pragma bank 255
// Maps indoors, in the world's interior tileset (content/tilesets/lunar_int.json):
// a hab's interior behind its door, the halls and buildings an area's maps can be
// (level.h MAP_HALL, MAP_HOUSE), and a miniboss's arena (outdoors, but the same
// one-screen shape). Banked: runs as a map is entered, never per frame.
#include <gb/gb.h>
#include "level.h"
#include "level_internal.h"
#include "room.h"
#include "tiles_lunar_ext.h"
#include "tiles_lunar_int.h"
#include "world.h"
#include <string.h>

// The generator's numbers (level.c's xorshift, on the same level_rng).
static uint8_t rnd(void) {
    uint16_t x = level_rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    level_rng = x;
    return (uint8_t)x ^ (uint8_t)(x >> 8);
}

static uint8_t below(uint8_t n) {
    uint8_t m = 1, r;
    if (n < 2) return 0;
    while (m < n - 1) m = (m << 1) | 1;
    do r = rnd() & m; while (r >= n);
    return r;
}

// --- Building interiors ---
// One screen (10x8 blocks) in the interior tileset, after the artist's reference
// interior (docs/design/lunar-base.md, "The interior"): white walls seen from
// above; under the top wall a face strip, mirrored (lights, plates, screens,
// vents, sometimes a pair of starfield windows); one floor for the room (plates,
// or studs with a wall's shadow and cyan lights); couches and counters in mirrored
// pairs either side of a clear walkway up from the south door; a chest beside it.
#define IN_W 10
#define IN_H 8
#define IN_DOOR 4  // the south door's column

static void in_put(uint8_t x, uint8_t y, uint8_t v, uint8_t t) {
    uint16_t k = ((uint16_t)y << 5) | x;
    room_vis[k] = v;
    room_map[k] = t;
}

// A block and its mirror image across the walkway.
static void in_pair(uint8_t x, uint8_t y, uint8_t v, uint8_t t) {
    in_put(x, y, v, t);
    in_put(IN_W - 1 - x, y, v, t);
}

void level_build_interior(uint16_t seed) BANKED {
    static const uint8_t faces[4] = {TS_LUNAR_INT_FACE_LIGHTS, TS_LUNAR_INT_FACE_PLATE, TS_LUNAR_INT_FACE_SCREEN, TS_LUNAR_INT_FACE_VENT};
    level_rng = seed ? seed : 0xACE1;
    level_roofed = 1;
    room_w = IN_W;
    room_h = IN_H;
    room_px_w = IN_W * BLOCK_SIZE;
    room_px_h = IN_H * BLOCK_SIZE;
    level_camps = level_chests = level_carved = 0;
    pf_count = pipe_count = level_doors = level_conts = level_vends = 0;
    for (uint8_t s = 0; s < 4; s++) level_open[s] = 0xFF;
    level_open[2] = IN_DOOR;
    uint8_t plates = below(2), floor = plates ? TS_LUNAR_INT_PLATES : TS_LUNAR_INT_FLOOR;
    // The walls and the floor.
    for (uint8_t y = 0; y < IN_H; y++)
        for (uint8_t x = 0; x < IN_W; x++) {
            uint8_t v = floor, t = BLK_FLOOR;
            if (!y || y == IN_H - 1) {
                t = BLK_SOLID;
                v = !x ? (y ? TS_LUNAR_INT_CORNER_SW : TS_LUNAR_INT_CORNER_NW)
                  : x == IN_W - 1 ? (y ? TS_LUNAR_INT_CORNER_SE : TS_LUNAR_INT_CORNER_NE) : TS_LUNAR_INT_WALL_N;
            } else if (!x) {  // (side walls: a strip in the outer half, its shadow in the inner)
                t = BLK_WALL_W;
                v = plates ? TS_LUNAR_INT_WALL_W_PLATES : TS_LUNAR_INT_WALL_W;
            } else if (x == IN_W - 1) {
                t = BLK_WALL_E;
                v = plates ? TS_LUNAR_INT_WALL_E_PLATES : TS_LUNAR_INT_WALL_E;
            } else if (y == 2) {
                v = plates ? TS_LUNAR_INT_PLATES_SHADOW : TS_LUNAR_INT_FLOOR_SHADOW;  // (the top wall's shadow)
            }
            in_put(x, y, v, t);
        }
    // The face strip under the top wall: a pair of windows, or face pieces.
    uint8_t windows = !below(3);
    for (uint8_t x = 1; x < IN_W / 2; x++) {
        if (windows && (x == 2 || x == 3)) {
            uint8_t b = TS_LUNAR_INT_WINDOW + (x - 2);
            in_put(x, 1, b, BLK_SOLID);
            in_put(x, 2, b + 2, BLK_SOLID);
            in_put(x + 4, 1, b, BLK_SOLID);  // (the second window: the same way round)
            in_put(x + 4, 2, b + 2, BLK_SOLID);
            continue;
        }
        in_pair(x, 1, faces[below(4)], BLK_SOLID);
    }
    // Couches in the middle of the room, counters along the bottom, lights on studs.
    if (below(4)) {
        in_put(1, 4, TS_LUNAR_INT_COUCH, BLK_SOLID);
        in_put(2, 4, TS_LUNAR_INT_COUCH + 1, BLK_SOLID);
        in_put(IN_W - 3, 4, TS_LUNAR_INT_COUCH, BLK_SOLID);  // (both the same way round)
        in_put(IN_W - 2, 4, TS_LUNAR_INT_COUCH + 1, BLK_SOLID);
    }
    if (below(4)) {
        in_put(1, 6, TS_LUNAR_INT_COUNTER_L, BLK_SOLID);
        in_put(2, 6, TS_LUNAR_INT_COUNTER, BLK_SOLID);
        in_put(3, 6, TS_LUNAR_INT_COUNTER, BLK_SOLID);
        in_put(IN_W - 4, 6, TS_LUNAR_INT_COUNTER, BLK_SOLID);
        in_put(IN_W - 3, 6, TS_LUNAR_INT_COUNTER, BLK_SOLID);
        in_put(IN_W - 2, 6, TS_LUNAR_INT_COUNTER_R, BLK_SOLID);
    }
    if (!plates) in_pair(3, 3, TS_LUNAR_INT_FLOOR_LIGHT, BLK_FLOOR);
    // A locker pair at the ends of the face strip, one building in two.
    if (below(2)) {
        for (uint8_t k = 0; k < 2; k++) {
            cont_bx[k] = k ? IN_W - 2 : 1;
            cont_by[k] = 1;
            cont_kind[k] = CONT_LOCKER;
            in_put(cont_bx[k], 1, 0, BLK_SOLID);
            in_put(cont_bx[k], 2, 0, BLK_SOLID);
        }
        level_conts = 2;
        for (uint8_t k = 0; k < 2; k++) level_draw_container(k);
    }
    // A guard, one building in two: a camp in the upper room (its enemies at the ends
    // of the shadow row and the middle; the chest waits for it, descent.c).
    if (below(2)) {
        static const uint8_t gx[3] = {2, IN_W - 3, IN_W / 2}, gy[3] = {2, 2, 3};
        level_camps = 1;
        camp_x[0] = (IN_W / 2) * BLOCK_SIZE;
        camp_y[0] = 3 * BLOCK_SIZE;
        camp_slots[0] = 3;
        for (uint8_t s = 0; s < 3; s++) {
            camp_slot_x[0][s] = (uint16_t)gx[s] * BLOCK_SIZE;
            camp_slot_y[0][s] = (uint16_t)gy[s] * BLOCK_SIZE;
        }
    }
    // The way out: the south door, its jambs either side.
    in_put(IN_DOOR, IN_H - 1, TS_LUNAR_INT_DOOR_NS, BLK_FLOOR);
    in_put(IN_DOOR - 1, IN_H - 1, TS_LUNAR_INT_JAMB_NS_L, BLK_SOLID);
    in_put(IN_DOOR + 1, IN_H - 1, TS_LUNAR_INT_JAMB_NS_R, BLK_SOLID);
    // The chest, on one side, above the couches.
    level_chests = 1;
    chest_x[0] = (uint16_t)(below(2) ? 2 : IN_W - 3) * BLOCK_SIZE;  // (not under a locker: you walk up to those)
    chest_y[0] = 3 * BLOCK_SIZE;
    chest_camp[0] = level_camps ? 0 : 0xFF;  // (guarded: opens once they're cleared; else a cache)
}

// --- A miniboss's arena ---
// The combat trials' room (boss.c boss_room: 10x8, a rim, open ground: the fights
// are tuned to it) drawn in the world's exterior art instead of the test tiles.
void level_build_arena(void) BANKED {
    world_load_tiles(0);
    level_roofed = 0;
    room_w = IN_W;
    room_h = IN_H;
    room_px_w = IN_W * BLOCK_SIZE;
    room_px_h = IN_H * BLOCK_SIZE;
    for (uint16_t k = 0; k < sizeof(room_map); k++) room_map[k] = BLK_SOLID;
    for (uint8_t y = 1; y + 1 < IN_H; y++)
        for (uint8_t x = 1; x + 1 < IN_W; x++) MAP(x, y) = BLK_FLOOR;
    pf_count = pipe_count = level_doors = level_conts = level_vends = 0;
    tiles_lunar_ext_apply();
}

// --- Halls and buildings ---
// An area's indoor maps (level.h), so its maps are told apart: a hall is one long
// room, 20 or 30 blocks, under a wall with its machinery face; a building is 10
// wide and 12 or 16 tall, its rooms stacked, each wall across with a doorway in
// it, as in the entrance room. Doors out onto the linked maps go anywhere along
// their walls, so you come out somewhere else.
// A band: a wall across, the face strip under it, the wall's shadow, then floor
// down to the next wall. Its floor is plates or studs (the side walls match;
// each 10 blocks of a hall its own). Windows in the top face strip, couches,
// counters and lockers are kept only if every door and doorway still connects;
// then the camps and a chest.
#define FURN 0x40  // (while building: a furnishing's block, its look in the low bits)
#define BANDS_MAX 3

static uint8_t bands, wall_y[BANDS_MAX + 1], gap_x[BANDS_MAX], plates[BANDS_MAX][3];

static uint8_t band_of(uint8_t y) {
    uint8_t b = 0;
    while (b + 1 < bands && y >= wall_y[b + 1]) b++;
    return b;
}

static void must(uint8_t x, uint8_t y) {
    if (musts < MUST_MAX) {
        must_x[musts] = x;
        must_y[musts++] = y;
    }
}

// w x h blocks at x, y clear of every point that must connect and the blocks
// round it (where you come in, and walk up to a door).
static uint8_t clear_of_musts(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t k = 0; k < musts; k++)
        if ((uint8_t)(must_x[k] + 1 - x) < w + 2 && (uint8_t)(must_y[k] + 1 - y) < h + 2) return 0;
    return 1;
}

// Furnish w x 1 blocks of floor at x, y with looks v0, v0 + 1... (v0 0: a
// counter run, its ends rounded); kept if everything still connects.
static void furnish(uint8_t x, uint8_t y, uint8_t w, uint8_t v0) {
    if (x + w > room_w - 1 || !clear_of_musts(x, y, w, 1)) return;
    for (uint8_t i = 0; i < w; i++)
        if (MAP(x + i, y) != BLK_FLOOR) return;
    for (uint8_t i = 0; i < w; i++)
        MAP(x + i, y) = FURN | (v0 ? v0 + i : !i ? TS_LUNAR_INT_COUNTER_L : i + 1 == w ? TS_LUNAR_INT_COUNTER_R : TS_LUNAR_INT_COUNTER);
    if (level_still_connected()) return;
    for (uint8_t i = 0; i < w; i++) MAP(x + i, y) = BLK_FLOOR;
}

// A starfield window (2x2 blocks) in the top face strip and the shadow under it.
static void window(uint8_t x) {
    if (!clear_of_musts(x, 2, 2, 1)) return;
    for (uint8_t i = 0; i < 2; i++)
        if (MAP(x + i, 1) != BLK_SOLID || MAP(x + i, 2) != BLK_FLOOR) return;
    for (uint8_t i = 0; i < 2; i++) {
        MAP(x + i, 1) = FURN | (TS_LUNAR_INT_WINDOW + i);
        MAP(x + i, 2) = FURN | (TS_LUNAR_INT_WINDOW + 2 + i);
    }
    if (level_still_connected()) return;
    for (uint8_t i = 0; i < 2; i++) MAP(x + i, 1) = BLK_SOLID, MAP(x + i, 2) = BLK_FLOOR;
}

// A locker in face strip f and the shadow under it (level_draw_container draws it).
static void locker(uint8_t x, uint8_t f) {
    if (level_conts >= CONT_MAX || !clear_of_musts(x, f + 1, 1, 1) || MAP(x, f) != BLK_SOLID || MAP(x, f + 1) != BLK_FLOOR
        || MAP(x, f + 2) != BLK_FLOOR) return;  // (and the floor in front: you walk up to it)
    MAP(x, f + 1) = FURN | TS_LUNAR_INT_CONT_LOCKER;
    if (!level_still_connected()) {
        MAP(x, f + 1) = BLK_FLOOR;
        return;
    }
    MAP(x, f) = FURN | TS_LUNAR_INT_CONT_LOCKER;
    cont_bx[level_conts] = x;
    cont_by[level_conts] = f;
    cont_kind[level_conts++] = CONT_LOCKER;
}

// A camp in band b, about column cx: up to 4 slots on its floor, none near a door
// (you come in there); its middle is its first slot.
static void camp(uint8_t cx, uint8_t b) {
    uint8_t c = level_camps, y0 = wall_y[b] + 2, n = wall_y[b + 1] - y0;
    if (c >= LEVEL_CAMPS) return;
    camp_slots[c] = 0;
    for (uint8_t tries = 0; tries < 32 && camp_slots[c] < 4; tries++) {
        uint8_t x = cx - 3 + below(7), y = y0 + below(n), near = 0;
        if (x < 1 || x > room_w - 2 || MAP(x, y) != BLK_FLOOR) continue;
        for (uint8_t k = 0; k < musts; k++)
            if ((uint8_t)(must_x[k] + 2 - x) < 5 && (uint8_t)(must_y[k] + 2 - y) < 5) near = 1;
        for (uint8_t s = 0; s < camp_slots[c]; s++)
            if (camp_slot_x[c][s] == (uint16_t)x * BLOCK_SIZE && camp_slot_y[c][s] == (uint16_t)y * BLOCK_SIZE) near = 1;
        if (near) continue;
        camp_slot_x[c][camp_slots[c]] = (uint16_t)x * BLOCK_SIZE;
        camp_slot_y[c][camp_slots[c]++] = (uint16_t)y * BLOCK_SIZE;
    }
    if (camp_slots[c] < 2) return;
    camp_x[c] = camp_slot_x[c][0] + BLOCK_SIZE / 2;
    camp_y[c] = camp_slot_y[c][0] + BLOCK_SIZE / 2;
    level_camps++;
}

// A door's look: across a wall (N, S, a doorway between rooms), or in a side wall.
static void door_ns(uint8_t x, uint8_t y) {
    uint16_t k = ((uint16_t)y << 5) | x;
    room_vis[k] = TS_LUNAR_INT_DOOR_NS;
    room_vis[k - 1] = TS_LUNAR_INT_JAMB_NS_L;
    room_vis[k + 1] = TS_LUNAR_INT_JAMB_NS_R;
}

static void door_side(uint8_t x, uint8_t y, uint8_t v) {
    uint16_t k = ((uint16_t)y << 5) | x;
    room_vis[k] = v;
    room_vis[k - 32] = v + 1;  // (its jambs: up, down)
    room_vis[k + 32] = v + 2;
}

void level_build_indoor(uint8_t links) BANKED {
    static const uint8_t faces[4] = {TS_LUNAR_INT_FACE_LIGHTS, TS_LUNAR_INT_FACE_PLATE, TS_LUNAR_INT_FACE_SCREEN, TS_LUNAR_INT_FACE_VENT};
    uint8_t w = room_w, h = room_h, segs = w >= 30 ? 3 : w >= 20 ? 2 : 1;
    level_roofed = 1;
    level_camps = level_chests = level_carved = 0;
    pf_count = pipe_count = level_doors = level_conts = level_vends = 0;
    musts = 0;
    // The walls across: the top, and in a building one or two between its rooms.
    bands = 1;
    wall_y[0] = 0;
    if (h == 12) wall_y[bands++] = 5 + below(2);
    else if (h >= 16) {
        if (below(2)) wall_y[bands++] = 5, wall_y[bands++] = 10;
        else wall_y[bands++] = 7 + below(2);
    }
    wall_y[bands] = h - 1;
    for (uint8_t b = 0; b < bands; b++) {
        gap_x[b] = 2 + below(w - 4);
        for (uint8_t s = 0; s < 3; s++) plates[b][s] = below(2);
    }
    for (uint8_t y = 0; y < h; y++) {
        uint8_t b = band_of(y), across = y == wall_y[b] || y == h - 1, face = y == wall_y[b] + 1;
        for (uint8_t x = 0; x < w; x++) {
            uint8_t t = across || face ? BLK_SOLID : BLK_FLOOR;
            if (!across && !x) t = BLK_WALL_W;
            else if (!across && x == w - 1) t = BLK_WALL_E;
            MAP(x, y) = t;
        }
    }
    // The doorways between rooms, and the doors out (level_open; descent.c comes
    // in and goes out by them as by the entrance room's).
    for (uint8_t b = 1; b < bands; b++) {
        MAP(gap_x[b], wall_y[b]) = MAP(gap_x[b], wall_y[b] + 1) = BLK_FLOOR;
        must(gap_x[b], wall_y[b]);
    }
    for (uint8_t s = 0; s < 4; s++) level_open[s] = 0xFF;
    if (links & EDGE_N) {
        uint8_t x = 2 + below(w - 4);
        level_open[0] = x;
        MAP(x, 0) = MAP(x, 1) = BLK_FLOOR;
        must(x, 0);
        must(x, 2);
    }
    if (links & EDGE_S) {
        uint8_t x = 2 + below(w - 4);
        level_open[2] = x;
        MAP(x, h - 1) = BLK_FLOOR;
        must(x, h - 1);
        must(x, h - 3);
    }
    for (uint8_t s = 1; s < 4; s += 2) {
        if (!(links & (s == 1 ? EDGE_E : EDGE_W))) continue;
        uint8_t b = below(bands), y = wall_y[b] + 2 + below(wall_y[b + 1] - wall_y[b] - 3), x = s == 1 ? w - 1 : 0;
        level_open[s] = y;
        MAP(x, y) = BLK_FLOOR;
        must(x, y);
        must(s == 1 ? w - 3 : 2, y);
    }
    // Windows (one stretch of the top wall in three), furniture, lockers.
    for (uint8_t s = 0; s < segs; s++)
        if (!below(3)) window(s * 10 + 2), window(s * 10 + 6);
    for (uint8_t b = 0; b < bands; b++) {
        uint8_t f0 = wall_y[b] + 3, f1 = wall_y[b + 1];  // (its floor rows, below the shadow)
        for (uint8_t s = 0; s < segs; s++)
            for (uint8_t tries = 0; tries < 3; tries++) {
                uint8_t x = s * 10 + 1 + below(8), k = below(4);
                if (k == 0) furnish(x, f0 + below(f1 - f0), 2, TS_LUNAR_INT_COUCH);
                else if (k == 1) furnish(x, f1 - 1, 2 + below(3), 0);  // (a counter along its bottom)
                else if (k == 2) furnish(x, f0 + below(f1 - f0), 3, 0);  // (an island)
            }
        if (below(2)) locker(below(2) ? 1 : w - 2, wall_y[b] + 1);
        if (segs > 1 && below(2)) locker(1 + below(w - 2), wall_y[b] + 1);
    }
    // Camps: one in a building (two in one of three rooms, half the time), one
    // in a hall (two in a long one).
    uint8_t camp_band[LEVEL_CAMPS];
    if (segs > 1) {
        for (uint8_t k = 0; k + 1 < segs; k++) {
            camp_band[level_camps] = 0;
            camp(k ? 19 + below(5) : (segs > 2 ? 7 : 8) + below(5), 0);
        }
    } else {
        uint8_t n = 1 + (bands > 2 && below(2)), first = below(bands);
        for (uint8_t k = 0; k < n; k++) {
            uint8_t b = first + k < bands ? first + k : first + k - bands;
            camp_band[level_camps] = b;
            camp(3 + below(4), b);
        }
    }
    // A chest in two of three: in a corner of a room's floor (or anywhere along a
    // hall's), where it shuts nothing off; guarded by a camp in its room.
    if (below(3))
        for (uint8_t tries = 0; tries < 8; tries++) {
            uint8_t b = below(bands), y = wall_y[b + 1] - 1, x = segs > 1 ? 1 + below(w - 2) : below(2) ? 1 : w - 2, taken = 0, ok;
            if (MAP(x, y) != BLK_FLOOR || !clear_of_musts(x, y, 1, 1)) continue;
            for (uint8_t c = 0; c < level_camps; c++)
                for (uint8_t s = 0; s < camp_slots[c]; s++)
                    if ((camp_slot_x[c][s] >> 4) == x && (camp_slot_y[c][s] >> 4) == y) taken = 1;
            if (taken) continue;
            MAP(x, y) = BLK_SOLID;  // (it stands there: solid)
            ok = level_still_connected();
            MAP(x, y) = BLK_FLOOR;
            if (!ok) continue;
            chest_x[0] = (uint16_t)x * BLOCK_SIZE;
            chest_y[0] = (uint16_t)y * BLOCK_SIZE;
            chest_camp[0] = 0xFF;
            for (uint8_t c = 0; c < level_camps; c++)
                if (camp_band[c] == b) chest_camp[0] = c;
            level_chests = 1;
            break;
        }
    // The look: each block from its band (wall, face, shadow, floor) and its
    // stretch's floor; furnishings as placed; then the doors over it.
    uint8_t pick[5];
    memset(room_vis, 0, sizeof(room_vis));  // (past the room too: the same every build)
    for (uint8_t y = 0; y < h; y++) {
        uint8_t b = band_of(y), across = y == wall_y[b] || y == h - 1, face = y == wall_y[b] + 1, shadow = y == wall_y[b] + 2;
        for (uint8_t x = 0; x < w; x++) {
            uint16_t k = ((uint16_t)y << 5) | x;
            uint8_t m = room_map[k], seg = x >= 20 ? 2 : x >= 10 ? 1 : 0, p = plates[b][seg], q = x - seg * 10, v;
            if (q > 4) q = 9 - q;  // (mirrored across each stretch)
            if (face && x == seg * 10)
                for (uint8_t i = 0; i < 5; i++) pick[i] = below(4);
            if (m & FURN) {
                v = m & 0x3F;
                m = BLK_SOLID;
            } else if (across) {
                v = !x ? (!y ? TS_LUNAR_INT_CORNER_NW : y == h - 1 ? TS_LUNAR_INT_CORNER_SW : TS_LUNAR_INT_WALL_W_TEE)
                  : x == w - 1 ? (!y ? TS_LUNAR_INT_CORNER_NE : y == h - 1 ? TS_LUNAR_INT_CORNER_SE : TS_LUNAR_INT_WALL_E_TEE)
                  : TS_LUNAR_INT_WALL_N;
            } else if (!x) {
                v = p ? TS_LUNAR_INT_WALL_W_PLATES : TS_LUNAR_INT_WALL_W;
            } else if (x == w - 1) {
                v = p ? TS_LUNAR_INT_WALL_E_PLATES : TS_LUNAR_INT_WALL_E;
            } else if (face) {
                v = faces[pick[q]];
            } else if (shadow) {
                v = p ? TS_LUNAR_INT_PLATES_SHADOW : TS_LUNAR_INT_FLOOR_SHADOW;
            } else {
                v = p ? TS_LUNAR_INT_PLATES : y == wall_y[b] + 3 && q == 3 ? TS_LUNAR_INT_FLOOR_LIGHT : TS_LUNAR_INT_FLOOR;
            }
            room_vis[k] = v;
            room_map[k] = m;
        }
    }
    for (uint8_t b = 1; b < bands; b++) {
        door_ns(gap_x[b], wall_y[b]);
        room_vis[((uint16_t)(wall_y[b] + 1) << 5) | gap_x[b]] = TS_LUNAR_INT_DOOR_NS;
    }
    if (level_open[0] != 0xFF) {
        door_ns(level_open[0], 0);
        room_vis[32 | level_open[0]] = TS_LUNAR_INT_DOOR_NS;
    }
    if (level_open[2] != 0xFF) door_ns(level_open[2], h - 1);
    if (level_open[1] != 0xFF) door_side(w - 1, level_open[1], TS_LUNAR_INT_DOOR_E);
    if (level_open[3] != 0xFF) door_side(0, level_open[3], TS_LUNAR_INT_DOOR_W);
    for (uint8_t k = 0; k < level_conts; k++) level_draw_container(k);
}
