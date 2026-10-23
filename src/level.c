#pragma bank 255
// Generated areas: see level.h. Banked: runs when an area starts and when a map
// is entered, never per frame.
#include <gb/gb.h>
#include "level.h"
#include "room.h"
#include "tiles_lunar_ext.h"
#include "tiles_lunar_int.h"
#include "gfx.h"
#include "chunks.h"

uint8_t area_type[AREA_CELLS];
uint8_t area_links[AREA_CELLS];
uint8_t area_cw[AREA_CELLS], area_ch[AREA_CELLS];
uint8_t area_camps_cleared[AREA_CELLS];
uint8_t area_chests_opened[AREA_CELLS];
uint16_t area_seed[AREA_CELLS];
uint8_t area_start, area_exit, area_maps;
uint8_t level_map;
uint8_t level_open[4];
uint8_t level_camps;
uint16_t camp_x[LEVEL_CAMPS], camp_y[LEVEL_CAMPS];
uint8_t camp_slots[LEVEL_CAMPS];
uint16_t camp_slot_x[LEVEL_CAMPS][CAMP_SLOTS], camp_slot_y[LEVEL_CAMPS][CAMP_SLOTS];
uint8_t level_chests;
uint16_t chest_x[LEVEL_CHESTS], chest_y[LEVEL_CHESTS];
uint8_t chest_camp[LEVEL_CHESTS];
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

// 0..n-1 (0 if n is 0 or 1), without a division: draw under the next power of two.
static uint8_t below(uint8_t n) {
    uint8_t m = 1, r;
    if (n < 2) return 0;
    while (m < n - 1) m = (m << 1) | 1;
    do r = rnd() & m; while (r >= n);
    return r;
}

// --- Area layout ---

static const int8_t step_x[4] = {0, 1, 0, -1};  // N, E, S, W
static const int8_t step_y[4] = {-1, 0, 1, 0};
static const uint8_t edge_bit[4] = {EDGE_N, EDGE_E, EDGE_S, EDGE_W};

// The neighbouring slot through side d, or 0xFF off the grid.
static uint8_t neighbour(uint8_t i, uint8_t d) {
    int8_t x = (int8_t)(i % AREA_W) + step_x[d], y = (int8_t)(i / AREA_W) + step_y[d];
    if (x < 0 || x >= AREA_W || y < 0 || y >= AREA_H) return 0xFF;
    return (uint8_t)y * AREA_W + (uint8_t)x;
}

static void link(uint8_t a, uint8_t d) {
    area_links[a] |= edge_bit[d];
    area_links[neighbour(a, d)] |= edge_bit[(d + 2) & 3];
}

// Field map sizes in chunks (w, h): 3x2 to 3x4 screens.
static const uint8_t field_w[5] = {6, 6, 4, 6, 5};
static const uint8_t field_h[5] = {4, 6, 6, 8, 6};

void level_generate(uint16_t seed) BANKED {
    level_rng = seed ? seed : 0xACE1;
    for (uint8_t i = 0; i < AREA_CELLS; i++) {
        area_type[i] = MAP_NONE;
        area_links[i] = 0;
        area_camps_cleared[i] = 0;
        area_chests_opened[i] = 0;
        area_cw[i] = area_ch[i] = 0;
    }
    // The start is the entrance room, on the bottom row: south of it is the way
    // back to town (off the grid), and doors on 2 or 3 of its other sides lead in
    // (docs/design/runs.md). Then grow: link a new map onto a random existing one
    // until there are 6-9. Every map is reachable from the start.
    uint8_t target = 6 + below(4);
    area_start = (AREA_H - 1) * AREA_W + below(AREA_W);
    area_type[area_start] = MAP_FIELD;
    area_maps = 1;
    {
        uint8_t sides[3], n = 0;
        for (uint8_t d = 0; d < 4; d++)
            if (d != 2 && neighbour(area_start, d) != 0xFF) sides[n++] = d;  // N, E, W on the grid
        uint8_t doors = n == 3 ? 2 + below(2) : n;
        while (doors--) {
            uint8_t k = below(n), d = sides[k];
            sides[k] = sides[--n];
            uint8_t to = neighbour(area_start, d);
            area_type[to] = MAP_FIELD;
            link(area_start, d);
            area_maps++;
        }
    }
    for (uint16_t tries = 0; tries < 400 && area_maps < target; tries++) {
        uint8_t from = below(AREA_CELLS), d = below(4);
        if (!area_type[from]) continue;
        uint8_t to = neighbour(from, d);
        if (to == 0xFF || area_type[to]) continue;
        area_type[to] = MAP_FIELD;
        link(from, d);
        area_maps++;
    }
    // A link or two between maps that touch but aren't linked: loops.
    uint8_t loops = 1 + below(2);
    for (uint8_t tries = 0; tries < 40 && loops; tries++) {
        uint8_t from = below(AREA_CELLS), d = below(4);
        if (!area_type[from] || (area_links[from] & edge_bit[d])) continue;
        uint8_t to = neighbour(from, d);
        if (to == 0xFF || !area_type[to]) continue;
        link(from, d);
        loops--;
    }
    // The way onward: the map farthest from the start (breadth first).
    uint8_t dist[AREA_CELLS], queue[AREA_CELLS], head = 0, tail = 0;
    for (uint8_t i = 0; i < AREA_CELLS; i++) dist[i] = 0xFF;
    dist[area_start] = 0;
    queue[tail++] = area_start;
    area_exit = area_start;
    while (head != tail) {
        uint8_t i = queue[head++];
        if (dist[i] > dist[area_exit]) area_exit = i;
        for (uint8_t d = 0; d < 4; d++) {
            if (!(area_links[i] & edge_bit[d])) continue;
            uint8_t j = neighbour(i, d);
            if (dist[j] != 0xFF) continue;
            dist[j] = dist[i] + 1;
            queue[tail++] = j;
        }
    }
    area_type[area_start] = MAP_START;
    area_type[area_exit] = MAP_EXIT;
    for (uint8_t i = 0; i < AREA_CELLS; i++) {
        if (!area_type[i]) continue;
        if (area_type[i] == MAP_FIELD) {
            uint8_t k = below(5);
            area_cw[i] = field_w[k];
            area_ch[i] = field_h[k];
        } else if (area_type[i] == MAP_START) {
            area_cw[i] = area_ch[i] = 2;  // the entrance room: one screen
        } else {
            area_cw[i] = area_ch[i] = 4;
        }
        area_seed[i] = ((uint16_t)rnd() << 8) | rnd() | 1;
    }
}

// --- Building a map ---

uint8_t chunk_tag_ram[CHUNK_MAX], chunk_weight_ram[CHUNK_MAX];
uint8_t chunk_count_ram;
// The chunk picked for each chunk cell of the last map built (cy * 8 + cx);
// watchable, for the chunk editor's preview and survey.
uint8_t level_picks[64];
uint8_t chunk_tables_loaded;
// The area's chunk theme (content/chunks/*.json "theme"): only its chunks are picked.
uint8_t area_theme = CHUNK_THEME_LUNAR;

// A chunk for this role tag, by weight.
static uint8_t pick_chunk(uint8_t tag) {
    uint8_t total = 0;
    for (uint8_t k = 0; k < chunk_count_ram; k++)
        if (chunk_tag_ram[k] & tag) total += chunk_weight_ram[k];
    uint8_t r = below(total);
    for (uint8_t k = 0; k < chunk_count_ram; k++) {
        if (!(chunk_tag_ram[k] & tag)) continue;
        if (r < chunk_weight_ram[k]) return k;
        r -= chunk_weight_ram[k];
    }
    return 0;
}

#define MAP(x, y) room_map[((uint16_t)(y) << 5) | (x)]
#define VISITED 0x80

// Mark every floor block reachable from (x, y) with VISITED: a breadth-first
// fill through a ring of map indices (each block is visited once; the frontier
// of a fill on a map up to 30x32 blocks stays far under 256). The map's edges
// are wall except at openings, and outside the map is wall, so the fill can't
// leave it; indices off the top are skipped.
// The ring lives in room_vis (512 of its 1024 bytes): the fill runs while a map
// is built, before its looks are filled in (room_vis_from_map, last), with the
// display off; work RAM below the maps has no room for it.
#define fill_queue ((uint16_t *)ROOM_VIS_ADDR)

static void flood(uint8_t x, uint8_t y) {
    uint16_t k = ((uint16_t)y << 5) | x;
    uint8_t head = 0, tail = 0;  // wrap around the ring
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

// Each chunk cell's role in the map being built.
#define ROLE_OPEN 0
#define ROLE_ROCK 1
#define ROLE_CAMP 2
#define ROLE_CACHE 3
#define ROLE_MARK 4  // start or exit: the map's middle
uint8_t roles[8 * 8];  // [cy * 8 + cx]

static const uint8_t role_tag[5] = {TAG_OPEN, TAG_ROCK, TAG_CAMP, TAG_CACHE, TAG_START};

// Points every build must connect: openings, camps, chests, the middle (blocks).
#define MUST_MAX 12
uint8_t must_x[MUST_MAX], must_y[MUST_MAX], musts;

static void must(uint8_t x, uint8_t y) {
    if (musts < MUST_MAX) {
        must_x[musts] = x;
        must_y[musts++] = y;
    }
}

// Lay out the chunk roles: a rock cluster or two, camps (fields only), maybe a
// cache along an edge, and the start/exit mark in the middle.
static void plan_roles(uint8_t i, uint8_t cw, uint8_t ch) {
    for (uint8_t k = 0; k < sizeof(roles); k++) roles[k] = ROLE_OPEN;
    if (area_type[i] == MAP_START) {  // the entrance room: plain floor all over
        for (uint8_t y = 0; y < ch; y++)
            for (uint8_t x = 0; x < cw; x++) roles[y * 8 + x] = ROLE_MARK;
        return;
    }
    if (area_type[i] != MAP_FIELD) {
        roles[(ch >> 1) * 8 + (cw >> 1)] = ROLE_MARK;
        return;
    }
    // Rock: 1-2 clusters of 2-4 cells, wandering from a random interior cell.
    uint8_t clusters = 1 + below(2);
    while (clusters--) {
        uint8_t x = 1 + below(cw - 2), y = 1 + below(ch - 2), n = 2 + below(3);
        while (n--) {
            roles[y * 8 + x] = ROLE_ROCK;
            uint8_t d = below(4);
            int8_t nx = (int8_t)x + step_x[d], ny = (int8_t)y + step_y[d];
            if (nx >= 1 && nx < cw - 1 && ny >= 1 && ny < ch - 1) x = (uint8_t)nx, y = (uint8_t)ny;
        }
    }
    // Camps: 1-2, on open cells, at least 3 cells apart.
    uint8_t want = 1 + below(2), placed = 0, px = 0xFF, py = 0xFF;
    for (uint8_t tries = 0; tries < 20 && placed < want; tries++) {
        uint8_t x = below(cw), y = below(ch);
        if (roles[y * 8 + x] != ROLE_OPEN) continue;
        if (px != 0xFF && (uint8_t)(x - px + 2) < 5 && (uint8_t)(y - py + 2) < 5) continue;
        roles[y * 8 + x] = ROLE_CAMP;
        px = x;
        py = y;
        placed++;
    }
    // A cache in about a third of fields, along the top or bottom edge.
    if (below(3) == 0) {
        for (uint8_t tries = 0; tries < 10; tries++) {
            uint8_t x = below(cw), y = below(2) ? 0 : ch - 1;
            if (roles[y * 8 + x] != ROLE_OPEN) continue;
            roles[y * 8 + x] = ROLE_CACHE;
            break;
        }
    }
}

void level_build_map(uint8_t i) BANKED {
    if (!area_type[i]) return;  // no map in that slot
    if (!chunk_tables_loaded) {
        banked_memcpy(&chunk_count_ram, &chunk_count, 1, BANK(chunks));
        banked_memcpy(chunk_tag_ram, chunk_tags, chunk_count_ram, BANK(chunks));
        banked_memcpy(chunk_weight_ram, chunk_weight, chunk_count_ram, BANK(chunks));
        // Other themes' chunks weigh nothing here.
        for (uint8_t k = 0; k < chunk_count_ram; k++) {
            uint8_t t;
            banked_memcpy(&t, &chunk_theme[k], 1, BANK(chunks));
            if (t != area_theme) chunk_weight_ram[k] = 0;
        }
        chunk_tables_loaded = 1;
    }
    level_rng = area_seed[i];
    uint8_t cw = area_cw[i], ch = area_ch[i];
    uint8_t w = cw * CHUNK_W, h = ch * CHUNK_H;
    room_w = w;
    room_h = h;
    room_px_w = (uint16_t)w * BLOCK_SIZE;
    room_px_h = (uint16_t)h * BLOCK_SIZE;
    for (uint16_t k = 0; k < sizeof(room_map); k++) room_map[k] = BLK_SOLID;
    level_camps = level_chests = 0;
    musts = 0;
    plan_roles(i, cw, ch);

    // Chunks for each cell's role, picked by weight and flipped at random.
    uint8_t cells[CHUNK_CELLS];
    for (uint8_t cy = 0; cy < ch; cy++) {
        for (uint8_t cx = 0; cx < cw; cx++) {
            uint8_t role = roles[cy * 8 + cx];
            uint8_t tag = role == ROLE_MARK && area_type[i] == MAP_EXIT ? TAG_EXIT : role_tag[role];
            uint8_t k = pick_chunk(tag), flip = rnd();
            level_picks[cy * 8 + cx] = k;
            uint8_t camp = 0xFF;
            if (role == ROLE_CAMP && level_camps < LEVEL_CAMPS) {
                camp = level_camps++;
                camp_slots[camp] = 0;
                camp_x[camp] = (uint16_t)cx * (CHUNK_W * BLOCK_SIZE) + (CHUNK_W * BLOCK_SIZE / 2);
                camp_y[camp] = (uint16_t)cy * (CHUNK_H * BLOCK_SIZE) + (CHUNK_H * BLOCK_SIZE / 2);
                must(cx * CHUNK_W + 2, cy * CHUNK_H + 2);
            }
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
                        if (camp != 0xFF && camp_slots[camp] < CAMP_SLOTS) {
                            uint8_t s = camp_slots[camp]++;
                            camp_slot_x[camp][s] = (uint16_t)x * BLOCK_SIZE;
                            camp_slot_y[camp][s] = (uint16_t)y * BLOCK_SIZE;
                        }
                        break;
                    case CELL_ITEM:
                        if (level_chests < LEVEL_CHESTS && (camp != 0xFF || role == ROLE_CACHE)) {
                            chest_x[level_chests] = (uint16_t)x * BLOCK_SIZE;
                            chest_y[level_chests] = (uint16_t)y * BLOCK_SIZE;
                            chest_camp[level_chests++] = camp;
                            must(x, y);
                        }
                        break;
                    }
                    MAP(x, y) = b;
                }
            }
        }
    }
    // Edges are wall, except an opening (OPENING_W wide, 3 deep) onto each
    // linked map, somewhere along that side.
    for (uint8_t x = 0; x < w; x++) MAP(x, 0) = MAP(x, h - 1) = BLK_SOLID;
    for (uint8_t y = 0; y < h; y++) MAP(0, y) = MAP(w - 1, y) = BLK_SOLID;
    // The entrance room's doors sit in the middle of its walls, and the south one
    // (no map behind it) is the way back to town.
    uint8_t links = area_links[i], entrance = area_type[i] == MAP_START;
    if (entrance) links |= EDGE_S;
    for (uint8_t s = 0; s < 4; s++) {
        level_open[s] = 0xFF;
        if (!(links & edge_bit[s])) continue;
        uint8_t len = s & 1 ? h : w;  // E and W run down the side
        // (The entrance room's are doorways, one block wide; elsewhere OPENING_W.)
        uint8_t wide = entrance ? 1 : OPENING_W;
        uint8_t at = entrance ? (len - 1) >> 1 : 3 + below(len - 6 - OPENING_W + 1);
        level_open[s] = at;
        for (uint8_t a = 0; a < wide; a++) {
            for (uint8_t d = 0; d < 3; d++) {
                if (s == 0) MAP(at + a, d) = BLK_FLOOR;
                else if (s == 2) MAP(at + a, h - 1 - d) = BLK_FLOOR;
                else if (s == 1) MAP(w - 1 - d, at + a) = BLK_FLOOR;
                else MAP(d, at + a) = BLK_FLOOR;
            }
        }
        uint8_t mid = at + (wide >> 1);
        if (s == 0) must(mid, 2);
        else if (s == 2) must(mid, h - 3);
        else if (s == 1) must(w - 3, mid);
        else must(2, mid);
    }
    must(w >> 1, h >> 1);  // the middle (the start or exit mark)
    // Everything that must connect does: fill from the first point; carve from
    // any point the fill didn't reach back to the first.
    level_carved = 0;
    MAP(must_x[0], must_y[0]) = BLK_FLOOR;
    flood(must_x[0], must_y[0]);
    uint8_t missed[MUST_MAX];
    for (uint8_t k = 1; k < musts; k++) missed[k] = !(MAP(must_x[k], must_y[k]) & VISITED);
    unvisit();
    for (uint8_t k = 1; k < musts; k++) {
        if (!missed[k]) continue;
        level_carved = 1;
        carve(must_x[k], must_y[k], must_x[0], must_y[0]);
    }
    // The look: the theme's rules (content/tilesets/): the entrance room is indoors.
    if (area_type[i] == MAP_START) tiles_lunar_int_apply();
    else tiles_lunar_ext_apply();
}
