#pragma bank 255
// Generated expedition layers: see level.h. Banked: runs when a layer starts
// and when a room is entered, never per frame.
#include <gb/gb.h>
#include "level.h"
#include "room.h"
#include "gfx.h"
#include "chunks.h"

uint8_t layer_type[LAYER_CELLS];
uint8_t layer_doors[LAYER_CELLS];
uint8_t layer_cw[LAYER_CELLS], layer_ch[LAYER_CELLS];
uint8_t layer_cleared[LAYER_CELLS];
uint16_t layer_seed[LAYER_CELLS];
uint8_t layer_start, layer_exit, layer_rooms;
uint8_t level_room;
uint8_t level_slots, level_items;
uint16_t level_slot_x[LEVEL_SLOTS], level_slot_y[LEVEL_SLOTS];
uint16_t level_item_x[LEVEL_ITEMS], level_item_y[LEVEL_ITEMS];
uint8_t level_carved;

// The generator's own random numbers (xorshift), so layouts depend only on the
// seed, never on gameplay's rand().
uint16_t level_rng;

static uint8_t rnd(void) {
    uint16_t x = level_rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    level_rng = x;
    return (uint8_t)x ^ (uint8_t)(x >> 8);
}

// 0..n-1 (0 if n is 0), without a division: draw under the next power of two.
static uint8_t below(uint8_t n) {
    uint8_t m = 1, r;
    if (n < 2) return 0;
    while (m < n - 1) m = (m << 1) | 1;
    do r = rnd() & m; while (r >= n);
    return r;
}

// --- Layer layout ---

static const int8_t step_x[4] = {0, 1, 0, -1};  // N, E, S, W
static const int8_t step_y[4] = {-1, 0, 1, 0};
static const uint8_t door_bit[4] = {DOOR_N, DOOR_E, DOOR_S, DOOR_W};
static const uint8_t door_opposite[4] = {DOOR_S, DOOR_W, DOOR_N, DOOR_E};

static void link(uint8_t a, uint8_t d) {
    layer_doors[a] |= door_bit[d];
    layer_doors[(uint8_t)(a + step_x[d] + step_y[d] * LAYER_W)] |= door_opposite[d];
}

// Room sizes in chunks (w, h) for combat rooms: 1.5-2 screens mostly, a big
// 2x2-screen room now and then.
static const uint8_t combat_w[6] = {3, 4, 2, 3, 4, 4};
static const uint8_t combat_h[6] = {2, 2, 3, 3, 3, 4};

static void size_room(uint8_t i) {
    uint8_t t = layer_type[i], w = 2, h = 2;
    if (t == ROOM_COMBAT) {
        uint8_t k = below(6);
        w = combat_w[k];
        h = combat_h[k];
    } else if (t == ROOM_CONNECTOR) {
        uint8_t k = below(3);  // a square, or long and thin
        if (k == 1) w = 4;
        else if (k == 2) h = 4;
    }
    layer_cw[i] = w;
    layer_ch[i] = h;
}

// A branch room off `from` through side d, if that slot is free (not off the exit).
static void add_branch(uint8_t from, uint8_t d) {
    if (!layer_type[from] || from == layer_exit) return;
    int8_t bx = (int8_t)(from % LAYER_W) + step_x[d], by = (int8_t)(from / LAYER_W) + step_y[d];
    if (bx < 0 || bx >= LAYER_W || by < 0 || by >= LAYER_H) return;
    uint8_t to = (uint8_t)by * LAYER_W + (uint8_t)bx;
    if (layer_type[to]) return;
    layer_type[to] = below(2) ? ROOM_CACHE : ROOM_COMBAT;
    link(from, d);
    layer_rooms++;
}

void level_generate(uint16_t seed) BANKED {
    level_rng = seed ? seed : 0xACE1;
    for (uint8_t i = 0; i < LAYER_CELLS; i++) {
        layer_type[i] = ROOM_NONE;
        layer_doors[i] = 0;
        layer_cleared[i] = 0;
        layer_cw[i] = layer_ch[i] = 0;
    }
    // The main path: start in the top row, wander left and right, step down,
    // and leave from the bottom row. At most 10 rooms long.
    uint8_t x = below(LAYER_W), y = 0, cell = x, len = 1, dir = below(2) ? 1 : 3;
    layer_start = cell;
    layer_type[cell] = ROOM_COMBAT;
    while (1) {
        uint8_t go_down = below(3) == 0 || len >= 9;
        int8_t nx = (int8_t)x + step_x[dir];
        if (!go_down && (nx < 0 || nx >= LAYER_W || layer_type[(uint8_t)(cell + step_x[dir])])) {
            dir ^= 2;  // turn around: E <-> W
            nx = (int8_t)x + step_x[dir];
            if (nx < 0 || nx >= LAYER_W || layer_type[(uint8_t)(cell + step_x[dir])]) go_down = 1;
        }
        if (go_down) {
            if (y == LAYER_H - 1) break;
            link(cell, 2);
            y++;
            cell += LAYER_W;
            dir = below(2) ? 1 : 3;
        } else {
            link(cell, dir);
            x = (uint8_t)nx;
            cell = y * LAYER_W + x;
        }
        layer_type[cell] = below(4) ? ROOM_COMBAT : ROOM_CONNECTOR;
        len++;
    }
    layer_exit = cell;
    layer_type[layer_start] = ROOM_START;
    layer_type[layer_exit] = ROOM_EXIT;
    layer_rooms = len;
    // Branches: off any room but the exit, until there are 8-12 rooms.
    uint8_t target = 8 + below(5);
    // Random tries first; then, so a short path still makes 8, every room and
    // side in turn from a random starting point.
    for (uint8_t tries = 0; tries < 60 && layer_rooms < target; tries++) add_branch(below(LAYER_CELLS), below(4));
    uint8_t from = below(LAYER_CELLS);
    for (uint8_t k = 0; k < LAYER_CELLS * 4 && layer_rooms < 8; k++) add_branch((from + (k >> 2)) & (LAYER_CELLS - 1), k & 3);
    for (uint8_t i = 0; i < LAYER_CELLS; i++) {
        if (!layer_type[i]) continue;
        size_room(i);
        layer_seed[i] = ((uint16_t)rnd() << 8) | rnd() | 1;
    }
}

// --- Building a room ---

static const uint8_t type_tag[6] = {0, TAG_START, TAG_COMBAT, TAG_CONNECTOR, TAG_CACHE, TAG_EXIT};
uint8_t chunk_tag_ram[CHUNK_COUNT], chunk_weight_ram[CHUNK_COUNT];
uint8_t chunk_tables_loaded;

// A chunk for this tag, by weight.
static uint8_t pick_chunk(uint8_t tag) {
    uint8_t total = 0;
    for (uint8_t k = 0; k < CHUNK_COUNT; k++)
        if (chunk_tag_ram[k] & tag) total += chunk_weight_ram[k];
    uint8_t r = below(total);
    for (uint8_t k = 0; k < CHUNK_COUNT; k++) {
        if (!(chunk_tag_ram[k] & tag)) continue;
        if (r < chunk_weight_ram[k]) return k;
        r -= chunk_weight_ram[k];
    }
    return 0;
}

#define MAP(x, y) room_map[((uint16_t)(y) << 5) | (x)]
#define VISITED 0x80

static uint8_t walkable(uint8_t b) {
    return b == BLK_FLOOR;
}

// Mark every floor block reachable from (x, y) with VISITED: a breadth-first
// fill through a queue of map indices (each block is visited once). The room's
// outer ring is wall except at doors, and outside the room the map is wall, so
// the fill can't leave it; indices off the top are skipped.
#define ROOM_CELLS_MAX (4 * CHUNK_W * 4 * CHUNK_H)
uint16_t fill_queue[ROOM_CELLS_MAX];

static void flood(uint8_t x, uint8_t y) {
    uint16_t k = ((uint16_t)y << 5) | x, head = 0, tail = 0;
    if (room_map[k] != BLK_FLOOR) return;
    room_map[k] |= VISITED;
    fill_queue[tail++] = k;
    while (head != tail) {
        k = fill_queue[head++];
        uint16_t n[4] = {k - 1, k + 1, k - 32, k + 32};
        for (uint8_t d = 0; d < 4; d++) {
            uint16_t m = n[d];
            if (m >= ROOM_MAX_W * ROOM_MAX_H || room_map[m] != BLK_FLOOR) continue;
            room_map[m] |= VISITED;
            fill_queue[tail++] = m;
        }
    }
}

static void unvisit(void) {
    for (uint8_t j = 0; j < room_h; j++)
        for (uint8_t i = 0; i < room_w; i++) MAP(i, j) &= ~VISITED;
}

// A floor line from (x0, y0) to (x1, y1): across, then down or up.
static void carve(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    while (x0 != x1) {
        MAP(x0, y0) = BLK_FLOOR;
        x0 += x0 < x1 ? 1 : -1;
    }
    while (y0 != y1) {
        MAP(x0, y0) = BLK_FLOOR;
        y0 += y0 < y1 ? 1 : -1;
    }
    MAP(x1, y1) = BLK_FLOOR;
}

void level_build_room(uint8_t i) BANKED {
    if (!layer_type[i]) return;  // no room in that slot
    if (!chunk_tables_loaded) {
        banked_memcpy(chunk_tag_ram, chunk_tags, CHUNK_COUNT, BANK(chunks));
        banked_memcpy(chunk_weight_ram, chunk_weight, CHUNK_COUNT, BANK(chunks));
        chunk_tables_loaded = 1;
    }
    level_rng = layer_seed[i];
    uint8_t w = layer_cw[i] * CHUNK_W, h = layer_ch[i] * CHUNK_H, tag = type_tag[layer_type[i]];
    room_w = w;
    room_h = h;
    room_px_w = (uint16_t)w * BLOCK_SIZE;
    room_px_h = (uint16_t)h * BLOCK_SIZE;
    for (uint16_t k = 0; k < sizeof(room_map); k++) room_map[k] = BLK_SOLID;
    level_slots = level_items = 0;

    // Chunks, each picked by weight for the room's type and flipped at random.
    uint8_t cells[CHUNK_CELLS];
    for (uint8_t cy = 0; cy < layer_ch[i]; cy++) {
        for (uint8_t cx = 0; cx < layer_cw[i]; cx++) {
            uint8_t k = pick_chunk(tag), flip = rnd();
            banked_memcpy(cells, chunk_cells + (uint16_t)k * CHUNK_CELLS, CHUNK_CELLS, BANK(chunks));
            for (uint8_t r = 0; r < CHUNK_H; r++) {
                uint8_t sr = flip & 2 ? CHUNK_H - 1 - r : r;
                for (uint8_t c = 0; c < CHUNK_W; c++) {
                    uint8_t sc = flip & 1 ? CHUNK_W - 1 - c : c;
                    uint8_t v = cells[(sr << 2) + sr + sc], b = BLK_FLOOR;
                    uint8_t x = cx * CHUNK_W + c, y = cy * CHUNK_H + r;
                    switch (v) {
                    case CELL_WALL: b = BLK_SOLID; break;
                    case CELL_PIT: b = BLK_PIT; break;
                    case CELL_RAISED: b = BLK_RAISED; break;
                    case CELL_MAYBE_WALL: if (rnd() & 1) b = BLK_SOLID; break;
                    case CELL_MAYBE_PIT: if (rnd() & 1) b = BLK_PIT; break;
                    case CELL_MAYBE_RAISED: if (rnd() & 1) b = BLK_RAISED; break;
                    case CELL_ENEMY:
                        if (level_slots < LEVEL_SLOTS) {
                            level_slot_x[level_slots] = (uint16_t)x * BLOCK_SIZE;
                            level_slot_y[level_slots++] = (uint16_t)y * BLOCK_SIZE;
                        }
                        break;
                    case CELL_ITEM:
                        if (level_items < LEVEL_ITEMS) {
                            level_item_x[level_items] = (uint16_t)x * BLOCK_SIZE;
                            level_item_y[level_items++] = (uint16_t)y * BLOCK_SIZE;
                        }
                        break;
                    }
                    MAP(x, y) = b;
                }
            }
        }
    }
    // The outer ring is wall; doors are 2-block gaps in the middle of a side,
    // with the 2 blocks inside them clear.
    for (uint8_t x = 0; x < w; x++) MAP(x, 0) = MAP(x, h - 1) = BLK_SOLID;
    for (uint8_t y = 0; y < h; y++) MAP(0, y) = MAP(w - 1, y) = BLK_SOLID;
    uint8_t mx = (w >> 1) - 1, my = (h >> 1) - 1, d = layer_doors[i];
    // The inner end of each door (where paths join), per side N, E, S, W.
    uint8_t in_x[4] = {mx, w - 3, mx, 2}, in_y[4] = {2, my, h - 3, my};
    for (uint8_t s = 0; s < 4; s++) {
        if (!(d & door_bit[s])) continue;
        for (uint8_t k = 0; k < 3; k++) {  // the gap and two blocks in, 2 wide
            if (s == 0) MAP(mx, k) = MAP(mx + 1, k) = BLK_FLOOR;
            else if (s == 2) MAP(mx, h - 1 - k) = MAP(mx + 1, h - 1 - k) = BLK_FLOOR;
            else if (s == 1) MAP(w - 1 - k, my) = MAP(w - 1 - k, my + 1) = BLK_FLOOR;
            else MAP(k, my) = MAP(k, my + 1) = BLK_FLOOR;
        }
    }
    // Every door must reach every other: flood from the first; if any door
    // isn't reached, carve each door's path to the room's middle.
    level_carved = 0;
    uint8_t first = 0xFF;
    for (uint8_t s = 0; s < 4; s++) if (d & door_bit[s]) { first = s; break; }
    if (first != 0xFF) {
        flood(in_x[first], in_y[first]);
        uint8_t ok = 1;
        for (uint8_t s = 0; s < 4; s++)
            if ((d & door_bit[s]) && !(MAP(in_x[s], in_y[s]) & VISITED)) ok = 0;
        unvisit();
        if (!ok) {
            level_carved = 1;
            for (uint8_t s = 0; s < 4; s++)
                if (d & door_bit[s]) carve(in_x[s], in_y[s], mx, my);
        }
    }
}
