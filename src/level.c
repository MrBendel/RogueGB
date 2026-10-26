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

// Field map sizes in chunks (w, h): 2x3 to 3x3 screens (at most 24 blocks tall:
// room.h ROOM_MAX_H).
static const uint8_t field_w[5] = {6, 6, 4, 5, 5};
static const uint8_t field_h[5] = {4, 6, 6, 4, 6};

void level_generate(uint16_t seed) BANKED {
    level_rng = seed ? seed : 0xACE1;
    for (uint8_t i = 0; i < AREA_CELLS; i++) {
        area_type[i] = MAP_NONE;
        area_links[i] = 0;
        area_camps_cleared[i] = 0;
        area_chests_opened[i] = area_conts_opened[i] = 0;
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
            area_cw[i] = 2, area_ch[i] = 3;  // the entrance room: two rooms deep
        } else {
            area_cw[i] = area_ch[i] = 4;
        }
        area_seed[i] = ((uint16_t)rnd() << 8) | rnd() | 1;
    }
}

// --- Building a map ---

// The chunk tables' tags and weights (the editor patches them in ROM), copied for
// each build into the top of room_vis: its looks aren't drawn until the build's
// end, and the flood fill's ring only uses its first 512 bytes. No RAM of their own.
uint8_t chunk_count_ram;
#define chunk_tag_tab (room_vis + 512)
#define chunk_weight_tab (room_vis + 512 + CHUNK_MAX)
// The chunk picked for each chunk cell of the last map built (cy * 8 + cx);
// watchable, for the chunk editor's preview and survey.
uint8_t level_picks[CHUNK_ROWS * 8];
// The area's chunk theme (content/chunks/*.json "theme"): only its chunks are picked.
uint8_t area_theme = CHUNK_THEME_LUNAR;

// A chunk for this role tag, by weight.
static uint8_t pick_chunk(uint8_t tag) {
    uint8_t total = 0;
    for (uint8_t k = 0; k < chunk_count_ram; k++)
        if (chunk_tag_tab[k] & tag) total += chunk_weight_tab[k];
    uint8_t r = below(total);
    for (uint8_t k = 0; k < chunk_count_ram; k++) {
        if (!(chunk_tag_tab[k] & tag)) continue;
        if (r < chunk_weight_tab[k]) return k;
        r -= chunk_weight_tab[k];
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
// The ring lives in room_vis (the first 512 of its 768 bytes): the fill runs while a map
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
uint8_t roles[CHUNK_ROWS * 8];  // [cy * 8 + cx]

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

// Solid blocks tidied before the look: a spur one block wide sticking out of a
// building (or a pair of lone blocks) would be drawn as a crate stuck to its side,
// so it goes; a lone block (a prop) right beside a building goes too. Only floor is
// added, so nothing gets cut off.
static uint8_t solid_at(uint8_t x, uint8_t y) {
    return MAP(x, y) == BLK_SOLID;
}

static void tidy(uint8_t w, uint8_t h) {
    for (uint8_t pass = 0; pass < 4; pass++) {
        uint8_t changed = 0;
        for (uint8_t y = 1; y + 1 < h; y++)
            for (uint8_t x = 1; x + 1 < w; x++) {
                if (!solid_at(x, y)) continue;
                uint8_t n = solid_at(x, y - 1) + solid_at(x + 1, y) + solid_at(x, y + 1) + solid_at(x - 1, y);
                uint8_t near = n;
                if (!n) near = solid_at(x - 1, y - 1) + solid_at(x + 1, y - 1) + solid_at(x - 1, y + 1) + solid_at(x + 1, y + 1);
                if (n == 1 || (!n && near)) {
                    MAP(x, y) = BLK_FLOOR;
                    changed = 1;
                }
            }
        if (!changed) break;
    }
}

// An exterior map's look: the theme's rules, then the set pieces and pipes over them.
static void look_exterior(void);

// Set pieces (content/tilesets/lunar_ext.json "prefab" pieces, cut from the pack's
// example: the hab module, the solar station, the tank, racks...). Each building
// the chunks left (a solid rectangle clear of the rim) is packed with them: the
// biggest that fits along its front, then smaller ones in what's left, and any
// block still unclaimed becomes ground. A set piece only turns solid blocks into
// ground, so it can't cut a way off. They're stamped over the theme's look
// (level_look_exterior), from this list, each time it's drawn.
// The lists are on level_build_map's stack (work RAM is full): set pieces' x, y
// and index; pipe runs as x | y << 5 | (length - 1) << 10 | down << 14
// | bend << 15 (a bent pipe is two runs: across, its last block the elbow, then
// down or up from the elbow, with no coupling at that end).
// An exit map's shaft: 2x2 pit blocks in its middle (descent.c: falling in is the
// way down). Its blocks are kept ground while set pieces are planned.
#define SHAFT_X(w) (((w) >> 1) - 1)
#define SHAFT_Y(h) (((h) >> 1) - 1)

#define PF_MAX 40
#define PIPE_MAX 12
uint8_t pf_count, pipe_count;
uint8_t level_doors, door_bx[DOORS_MAX], door_by[DOORS_MAX];
uint8_t level_conts, cont_bx[CONT_MAX], cont_by[CONT_MAX], cont_kind[CONT_MAX];
uint8_t area_conts_opened[AREA_CELLS];
uint8_t level_vends, vend_bx[VENDS_MAX], vend_by[VENDS_MAX];
static const uint8_t cont_w[3] = {1, 1, 2}, cont_h[3] = {2, 1, 2};
#define PF_HAB 0  // (the first set piece: the hab module, the one with a door)
#define HAB_DOOR_X 1  // its door's column, and its step's row
#define HAB_STEP_Y 4
static uint8_t *pf_x, *pf_y, *pf_id;
static uint16_t *pipes;
static const uint8_t pf_size[] = TILES_LUNAR_EXT_PF_SIZES;
#define CLAIMED 0x40  // (while planning: a block a set piece has taken)
#define PF_BIG 4  // the first four (hab, solar, tank, module) go on open ground; the rest beside them


#define PIPE_DOWN 0x4000
#define PIPE_BEND 0x8000
#define PIPE_Y(p) (((p) >> 5) & 31)

// A pipe: solid blocks, a coupling at each end where it meets a set piece. A bent
// one's across part ends in the elbow, turning toward its other part (`other`: the
// next word for the across part, the one before for the up or down part).
static void draw_pipe(uint16_t p, uint16_t other) {
    uint8_t x = p & 31, y = PIPE_Y(p), n = ((p >> 10) & 15) + 1, down = !!(p & PIPE_DOWN), bend = !!(p & PIPE_BEND);
    uint8_t below = bend && (down ? y > PIPE_Y(other) : PIPE_Y(other) > y);  // (the up or down part below its elbow)
    for (uint8_t i = 0; i < n; i++) {
        uint16_t k = down ? ((uint16_t)(y + i) << 5) | x : ((uint16_t)y << 5) | (x + i);
        uint8_t first = !i, last = i + 1 == n;
        room_map[k] = BLK_SOLID;
        if (down) {
            if (bend) first &= !below, last &= below;  // (no coupling at the elbow end)
            room_vis[k] = first ? TS_LUNAR_EXT_PIPE_V_T : last ? TS_LUNAR_EXT_PIPE_V_B : TS_LUNAR_EXT_PIPE_V;
        } else if (bend && last) {
            room_vis[k] = below ? TS_LUNAR_EXT_PIPE_BEND_D : TS_LUNAR_EXT_PIPE_BEND_U;
        } else {
            room_vis[k] = first ? TS_LUNAR_EXT_PIPE_H_L : last ? TS_LUNAR_EXT_PIPE_H_R : TS_LUNAR_EXT_PIPE_H;
            if (first && last) room_vis[k] = TS_LUNAR_EXT_PIPE_H_LR;
        }
    }
}

static void stamp_set_pieces(void) {
    for (uint8_t k = 0; k < pf_count; k++) tiles_lunar_ext_stamp(pf_id[k], pf_x[k], pf_y[k]);
    for (uint8_t k = 0; k < pipe_count; k++) {
        uint16_t p = pipes[k];
        draw_pipe(p, p & PIPE_DOWN ? (k ? pipes[k - 1] : 0) : (k + 1 < pipe_count ? pipes[k + 1] : 0));
    }
}

// Are the w x h blocks at x, y all unclaimed blocks of the building?
static uint8_t pf_room(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t x1, uint8_t y1) {
    if (x + w > x1 || y + h > y1) return 0;
    for (uint8_t j = 0; j < h; j++)
        for (uint8_t i = 0; i < w; i++)
            if (MAP(x + i, y + j) != BLK_SOLID) return 0;
    return 1;
}

static void add_door(uint8_t pf, uint8_t x, uint8_t y) {
    if (pf != PF_HAB || level_doors >= DOORS_MAX) return;
    door_bx[level_doors] = x + HAB_DOOR_X;
    door_by[level_doors++] = y + HAB_STEP_Y;
}

static void pf_place(uint8_t pf, uint8_t x, uint8_t y) {
    if (pf_count >= PF_MAX) return;
    add_door(pf, x, y);
    pf_x[pf_count] = x;
    pf_id[pf_count] = pf;
    pf_y[pf_count++] = y;
    for (uint8_t j = 0; j < pf_size[pf * 2 + 1]; j++)
        for (uint8_t i = 0; i < pf_size[pf * 2]; i++) MAP(x + i, y + j) = BLK_SOLID | CLAIMED;
}

// Open ground for a set piece: all floor, none of it a camp's slot, a chest or a
// point that must stay reachable.
static uint8_t open_ground(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t j = 0; j < h; j++)
        for (uint8_t i = 0; i < w; i++)
            if (MAP(x + i, y + j) != BLK_FLOOR) return 0;
    // (The middle: the shaft and two blocks either side of it, where you arrive
    // beside it back from its miniboss.)
    if ((uint8_t)(SHAFT_X(room_w) + 3 - x) < w + 5 && (uint8_t)(SHAFT_Y(room_h) + 1 - y) < h + 1) return 0;
    for (uint8_t k = 0; k < musts; k++)
        if ((uint8_t)(must_x[k] - x) < w && (uint8_t)(must_y[k] - y) < h) return 0;  // (left or above wraps big)
    for (uint8_t c = 0; c < level_camps; c++)
        for (uint8_t s = 0; s < camp_slots[c]; s++)
            if ((uint8_t)((camp_slot_x[c][s] >> 4) - x) < w && (uint8_t)((camp_slot_y[c][s] >> 4) - y) < h) return 0;
    for (uint8_t c = 0; c < level_chests; c++)
        if ((uint8_t)((chest_x[c] >> 4) - x) < w && (uint8_t)((chest_y[c] >> 4) - y) < h) return 0;
    return 1;
}

static uint8_t still_connected(void);

// Stamp a set piece on open ground and keep it if everything that must connect
// still does; otherwise put the ground back.
static uint8_t try_place(uint8_t pf, uint8_t x, uint8_t y) {
    if (pf_count >= PF_MAX) return 0;
    tiles_lunar_ext_stamp(pf, x, y);
    uint8_t w = pf_size[pf * 2], h = pf_size[pf * 2 + 1];
    if (!still_connected()) {
        for (uint8_t j = 0; j < h; j++)
            for (uint8_t i = 0; i < w; i++) MAP(x + i, y + j) = BLK_FLOOR;
        return 0;
    }
    pf_x[pf_count] = x;
    pf_id[pf_count] = pf;
    pf_y[pf_count++] = y;
    add_door(pf, x, y);
    return 1;
}

// Everything that must connect still does (a flood from the first such point).
static uint8_t still_connected(void) {
    flood(must_x[0], must_y[0]);
    uint8_t ok = 1;
    for (uint8_t k = 1; k < musts; k++)
        if (!(MAP(must_x[k], must_y[k]) & VISITED)) ok = 0;
    unvisit();
    return ok;
}

static uint16_t pipe_word(uint8_t x, uint8_t y, uint8_t n, uint16_t flags) {
    return x | ((uint16_t)y << 5) | ((uint16_t)(n - 1) << 10) | flags;
}

static void clear_pipe(uint16_t p) {
    uint8_t x = p & 31, y = PIPE_Y(p), n = ((p >> 10) & 15) + 1;
    for (uint8_t i = 0; i < n; i++)
        if (p & PIPE_DOWN) MAP(x, y + i) = BLK_FLOOR;
        else MAP(x + i, y) = BLK_FLOOR;
}

// Nothing solid alongside a run (another pipe, a set piece's side): pipes don't
// bunch up or hug what they pass. (Its ends meet its two set pieces.)
static uint8_t clear_beside(uint8_t x, uint8_t y, uint8_t n, uint8_t down) {
    for (uint8_t i = 0; i < n; i++)
        if (down ? MAP(x - 1, y + i) == BLK_SOLID || MAP(x + 1, y + i) == BLK_SOLID
                 : MAP(x + i, y - 1) == BLK_SOLID || MAP(x + i, y + 1) == BLK_SOLID)
            return 0;
    return 1;
}

// A pipe across open ground, kept only if everything still connects.
static void try_pipe(uint8_t x, uint8_t y, uint8_t n, uint8_t down) {
    if (pipe_count >= PIPE_MAX || !n || n > 16 || !open_ground(x, y, down ? 1 : n, down ? n : 1)) return;
    if (!clear_beside(x, y, n, down)) return;
    uint16_t p = pipe_word(x, y, n, down ? PIPE_DOWN : 0);
    draw_pipe(p, 0);
    if (still_connected()) pipes[pipe_count++] = p;
    else clear_pipe(p);
}

// A bent pipe: across from x0 to its elbow at x1 (on row y), then vn blocks (1 or
// more: the elbow's direction is read from them) down or up from vy.
static void try_bent_pipe(uint8_t x0, uint8_t y, uint8_t x1, uint8_t vy, uint8_t vn) {
    uint8_t n = x1 - x0 + 1;
    if (pipe_count + 2 > PIPE_MAX || n > 16 || !vn || vn > 16 || !open_ground(x0, y, n, 1) || !open_ground(x1, vy, 1, vn)) return;
    if (!clear_beside(x0, y, n, 0) || !clear_beside(x1, vy, vn, 1)) return;
    uint16_t across = pipe_word(x0, y, n, PIPE_BEND), turn = pipe_word(x1, vy, vn, PIPE_DOWN | PIPE_BEND);
    draw_pipe(across, turn);
    draw_pipe(turn, across);
    if (!still_connected()) {
        clear_pipe(across);
        clear_pipe(turn);
        return;
    }
    pipes[pipe_count++] = across;
    pipes[pipe_count++] = turn;
}

// The lowest solid row of column x between rows y and y + h, or 0xFF.
static uint8_t solid_row(uint8_t x, uint8_t y, uint8_t h) {
    for (uint8_t r = y + h; r-- > y;)
        if (MAP(x, r) == BLK_SOLID) return r;
    return 0xFF;
}

// Pipes between neighbouring set pieces (up to 8 blocks apart): across from one's
// side to the next one's, on the lowest row where both are solid; or down from one
// to one below it, on the first column where both are; or, for two offset both
// ways, across from one's side and bent down onto the other's top or up into its
// underside.
static void run_pipes(void) {
    uint8_t links[PF_MAX];  // pipes per set piece: two at most
    for (uint8_t k = 0; k < pf_count; k++) links[k] = 0;
    pipe_count = 0;
    for (uint8_t a = 0; a < pf_count; a++)
        for (uint8_t b = 0; b < pf_count; b++) {
            if (a == b || links[a] >= 2 || links[b] >= 2) continue;
            uint8_t had = pipe_count;  // (one pipe between two set pieces: the first that fits)
            uint8_t ax = pf_x[a], ay = pf_y[a], aw = pf_size[pf_id[a] * 2], ah = pf_size[pf_id[a] * 2 + 1];
            uint8_t bx = pf_x[b], by = pf_y[b], bw = pf_size[pf_id[b] * 2], bh = pf_size[pf_id[b] * 2 + 1];
            if (bx > ax + aw && bx - ax - aw <= 8) {
                uint8_t top = ay > by ? ay : by, bottom = ay + ah < by + bh ? ay + ah : by + bh;
                for (uint8_t y = bottom; y-- > top;)
                    if (MAP(ax + aw - 1, y) == BLK_SOLID && MAP(bx, y) == BLK_SOLID) {
                        try_pipe(ax + aw, y, bx - ax - aw, 0);
                        break;
                    }
            }
            if (pipe_count == had && by > ay + ah && by - ay - ah <= 8) {
                uint8_t left = ax > bx ? ax : bx, right = ax + aw < bx + bw ? ax + aw : bx + bw;
                for (uint8_t x = left; x < right; x++)
                    if (MAP(x, ay + ah - 1) == BLK_SOLID && MAP(x, by) == BLK_SOLID) {
                        try_pipe(x, ay + ah, by - ay - ah, 1);
                        break;
                    }
            }
            // Bent: b to the right of a's side, wholly below or above that row.
            if (pipe_count == had && bx >= ax + aw && bx + bw - ax - aw <= 9) {
                uint8_t r = solid_row(ax + aw - 1, ay, ah);
                for (uint8_t c = bx > ax + aw ? bx : ax + aw + 1; r != 0xFF && c < bx + bw; c++) {
                    if (by > r + 1 && MAP(c, by) == BLK_SOLID && by - r - 1 <= 8) {
                        try_bent_pipe(ax + aw, r, c, r + 1, by - r - 1);  // down onto b's top
                        break;
                    }
                    uint8_t rb = solid_row(c, by, bh);
                    if (by + bh < r && rb == by + bh - 1 && r - rb - 1 <= 8) {
                        try_bent_pipe(ax + aw, r, c, rb + 1, r - rb - 1);  // up into b's underside
                        break;
                    }
                }
            }
            if (pipe_count != had) links[a]++, links[b]++;
        }
}

// How many of a set piece the map has already: each one halves its chance of
// being picked again, so a map isn't all tanks.
static uint8_t uses(uint8_t pf) {
    uint8_t n = 0;
    for (uint8_t k = 0; k < pf_count; k++) n += pf_id[k] == pf;
    return n;
}

static void pack_building(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // The set piece that fits best (by its size, halved for each one already on the
    // map), on the front (the bottom row), anywhere along it.
    uint8_t best = 0xFF, score = 0;
    for (uint8_t pf = 0; pf < TILES_LUNAR_EXT_PREFABS; pf++) {
        uint8_t w = pf_size[pf * 2], h = pf_size[pf * 2 + 1], s = ((uint8_t)(w * h * 4) >> uses(pf)) + 1;  // (+ 1: something always fits)
        if (w > x1 - x0 || h > y1 - y0 || s <= score) continue;
        best = pf;
        score = s;
    }
    if (best == 0xFF) return;  // too small for any: the theme draws it
    uint8_t w = pf_size[best * 2], h = pf_size[best * 2 + 1];
    pf_place(best, x0 + below(x1 - x0 - w + 1), y1 - h);
    // The rest: the first set piece (from a random one on) that fits at each free block.
    for (uint8_t y = y0; y < y1; y++)
        for (uint8_t x = x0; x < x1; x++) {
            if (MAP(x, y) != BLK_SOLID) continue;
            uint8_t from = below(TILES_LUNAR_EXT_PREFABS);
            for (uint8_t n = 0; n < TILES_LUNAR_EXT_PREFABS; n++) {
                uint8_t pf = from + n < TILES_LUNAR_EXT_PREFABS ? from + n : from + n - TILES_LUNAR_EXT_PREFABS;
                if (!pf_room(x, y, pf_size[pf * 2], pf_size[pf * 2 + 1], x1, y1)) continue;
                pf_place(pf, x, y);
                break;
            }
        }
    for (uint8_t y = y0; y < y1; y++)
        for (uint8_t x = x0; x < x1; x++)
            if (MAP(x, y) == BLK_SOLID) MAP(x, y) = BLK_FLOOR;
}

// Containers against the set pieces: lockers and a chest beside their fronts (the
// bottom row, either side), a loot box now and then; each kept only if
// everything still connects. Solid (cover).
static void place_containers(uint8_t w, uint8_t h) {
    static const uint8_t want[3] = {3, 1, 1};
    level_conts = 0;
    for (uint8_t kind = 0; kind < 3; kind++) {
        uint8_t n = want[kind];
        if (kind == CONT_BOX && below(2)) n = 0;  // (one map in two)
        for (uint8_t tries = 0; n && tries < 16 && level_conts < CONT_MAX && pf_count; tries++) {
            uint8_t p = below(pf_count), cw = cont_w[kind], ch = cont_h[kind];
            uint8_t pw = pf_size[pf_id[p] * 2], ph = pf_size[pf_id[p] * 2 + 1];
            uint8_t x = below(2) ? pf_x[p] + pw : pf_x[p] - cw, y = pf_y[p] + ph - ch;
            if (pf_id[p] == PF_HAB) y--;  // (a hab's bottom row is its step: beside its front)
            if (x < 2 || x + cw + 2 > w || y < 2 || y + ch + 2 > h || !open_ground(x, y, cw, ch)) continue;
            for (uint8_t j = 0; j < ch; j++)
                for (uint8_t i = 0; i < cw; i++) MAP(x + i, y + j) = BLK_SOLID;
            if (!still_connected()) {
                for (uint8_t j = 0; j < ch; j++)
                    for (uint8_t i = 0; i < cw; i++) MAP(x + i, y + j) = BLK_FLOOR;
                continue;
            }
            cont_bx[level_conts] = x;
            cont_by[level_conts] = y;
            cont_kind[level_conts++] = kind;
            n--;
        }
    }
}

// The vendor post (an exit map): its two machines side by side, 4 to 7 blocks
// left or right of the shaft (descent.c starts the miniboss within 3), on a row
// near it; every such spot is tried (from a random one on), first with a block of
// open ground around, then without; kept only if everything still connects.
static uint8_t try_vendors(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t margin) {
    if (x < 2 || x + 4 > w || y < 2 || y + 4 > h) return 0;
    if (!open_ground(x - margin, y - margin, 2 + 2 * margin, 2 + 2 * margin)) return 0;
    for (uint8_t j = 0; j < 2; j++) MAP(x, y + j) = MAP(x + 1, y + j) = BLK_SOLID;
    if (!still_connected()) {
        for (uint8_t j = 0; j < 2; j++) MAP(x, y + j) = MAP(x + 1, y + j) = BLK_FLOOR;
        return 0;
    }
    vend_bx[0] = x, vend_bx[1] = x + 1;
    vend_by[0] = vend_by[1] = y;
    level_vends = 2;
    return 1;
}

static void place_vendors(uint8_t w, uint8_t h) {
    level_vends = 0;
    uint8_t from = below(40);
    for (uint8_t margin = 1; margin != 0xFF; margin--)
        for (uint8_t n = 0; n < 40; n++) {
            uint8_t c = from + n < 40 ? from + n : from + n - 40;  // 2 sides x 4 offsets x 5 rows
            uint8_t off = 4 + ((c >> 1) & 3), y = SHAFT_Y(h) - 2 + c / 8;
            uint8_t x = c & 1 ? SHAFT_X(w) + 2 + off : SHAFT_X(w) - 1 - off;
            if (try_vendors(x, y, w, h, margin)) return;
        }
}

static void plan_set_pieces(uint8_t w, uint8_t h, uint8_t shaft) {
    pf_count = pipe_count = level_doors = 0;
    if (shaft)
        for (uint8_t b = 0; b < 4; b++) MAP(SHAFT_X(w) + (b & 1), SHAFT_Y(h) + (b >> 1)) = BLK_FLOOR;
    // Raised platforms are buildings outdoors: with no stairs up to them they'd only
    // be walls that look like buildings (decided 2026-10-24). Both stop everything,
    // so nothing that connected stops connecting.
    for (uint8_t y = 1; y + 1 < h; y++)
        for (uint8_t x = 1; x + 1 < w; x++)
            if (MAP(x, y) == BLK_RAISED) MAP(x, y) = BLK_SOLID;
    // Lone blocks (the theme's crates) thinned to about one in three: props come
    // in groups now, beside the set pieces.
    for (uint8_t y = 1; y + 1 < h; y++)
        for (uint8_t x = 1; x + 1 < w; x++)
            if (MAP(x, y) == BLK_SOLID && MAP(x - 1, y) != BLK_SOLID && MAP(x + 1, y) != BLK_SOLID
                && MAP(x, y - 1) != BLK_SOLID && MAP(x, y + 1) != BLK_SOLID && (uint8_t)(x * 7 + y * 5) % 3)
                MAP(x, y) = BLK_FLOOR;
    // Every building, whatever its shape, in rectangles: from each unclaimed block
    // (left to right, top down) the solid run along its row, as deep as it stays
    // full, packed with set pieces. The rim isn't a building.
    for (uint8_t y = 1; y + 1 < h; y++)
        for (uint8_t x = 1; x + 1 < w; x++) {
            if (MAP(x, y) != BLK_SOLID) continue;
            uint8_t x1 = x, y1 = y + 1;
            while (x1 + 1 < w && MAP(x1, y) == BLK_SOLID) x1++;
            while (y1 + 1 < h && pf_room(x, y1, x1 - x, 1, x1, y1 + 1)) y1++;
            if ((x1 - x) * (y1 - y) >= 2) pack_building(x, y, x1, y1);
        }
    // A lone block left against a set piece (an L's corner) is ground; one in the
    // open stays the theme's (a crate). Then the claimed blocks back to plain solid
    // (the stamps set their types).
    for (uint8_t y = 1; y + 1 < h; y++)
        for (uint8_t x = 1; x + 1 < w; x++)
            if (MAP(x, y) == BLK_SOLID && ((MAP(x - 1, y) | MAP(x + 1, y) | MAP(x, y - 1) | MAP(x, y + 1)) & CLAIMED))
                MAP(x, y) = BLK_FLOOR;
    for (uint8_t y = 0; y < h; y++)
        for (uint8_t x = 0; x < w; x++) MAP(x, y) &= ~CLAIMED;
    stamp_set_pieces();
    // An exit map's vendor post first: its spot near the middle.
    level_vends = 0;
    if (shaft) place_vendors(w, h);
    // Then on open ground, for the density of the pack's example: a big set piece
    // now and then (about one per 120 blocks of map), each with small ones beside
    // its front.
    uint8_t want = (uint8_t)(((uint16_t)w * h) / 120);
    for (uint8_t tries = 0; want && tries < 24; tries++) {
        uint8_t big = below(PF_BIG);
        if (uses(big) > uses((big + 1) % PF_BIG)) big = (big + 1) % PF_BIG;  // (the less used of two)
        uint8_t pw = pf_size[big * 2], ph = pf_size[big * 2 + 1];
        if (pw + 4 > w || ph + 4 > h) continue;
        uint8_t x = 2 + below(w - pw - 3), y = 2 + below(h - ph - 3);
        if (!open_ground(x - 1, y - 1, pw + 2, ph + 2) || !try_place(big, x, y)) continue;
        want--;
        for (uint8_t side = 0; side < 2; side++) {
            uint8_t small = PF_BIG + below(TILES_LUNAR_EXT_PREFABS - PF_BIG);
            uint8_t sw = pf_size[small * 2], sh = pf_size[small * 2 + 1];
            uint8_t sx = side ? x + pw : x - sw, sy = y + ph - sh;
            if (sx >= 2 && sx + sw + 2 <= w && sy >= 2 && open_ground(sx, sy, sw, sh)) try_place(small, sx, sy);
        }
    }
    run_pipes();
    place_containers(w, h);
    if (shaft)
        for (uint8_t b = 0; b < 4; b++) MAP(SHAFT_X(w) + (b & 1), SHAFT_Y(h) + (b >> 1)) = BLK_PIT;
}

// The entrance room is made by hand (content/tilesets/lunar_int.json, its map
// "entrance": level.h) with its outer doors closed; this opens the ones the area's
// links ask for, where the generator put them.
static void open_door(uint16_t k, uint8_t step, uint8_t door, uint8_t before, uint8_t after) {
    room_map[k] = BLK_FLOOR;
    room_vis[k] = door;
    room_vis[k - step] = before;  // the jambs, along the wall
    room_vis[k + step] = after;
}

static void entrance_room(void) {
    uint8_t bank = BANK(tiles_lunar_int);
    for (uint8_t y = 0; y < TS_LUNAR_INT_MAP_ENTRANCE_H; y++) {
        uint16_t k = (uint16_t)y << 5, from = (uint16_t)y * TS_LUNAR_INT_MAP_ENTRANCE_W;
        banked_memcpy(room_map + k, tiles_lunar_int_entrance_type + from, TS_LUNAR_INT_MAP_ENTRANCE_W, bank);
        banked_memcpy(room_vis + k, tiles_lunar_int_entrance_vis + from, TS_LUNAR_INT_MAP_ENTRANCE_W, bank);
    }
    if (level_open[0] != 0xFF) {  // north: through the wall's face below it too
        open_door(level_open[0], 1, TS_LUNAR_INT_DOOR_NS, TS_LUNAR_INT_JAMB_NS_L, TS_LUNAR_INT_JAMB_NS_R);
        for (uint8_t y = 1; y <= 2; y++) {
            room_map[((uint16_t)y << 5) + level_open[0]] = BLK_FLOOR;
            room_vis[((uint16_t)y << 5) + level_open[0]] = TS_LUNAR_INT_DOOR_NS;
        }
    }
    if (level_open[1] != 0xFF)
        open_door(((uint16_t)level_open[1] << 5) | (room_w - 1), 32, TS_LUNAR_INT_DOOR_E, TS_LUNAR_INT_JAMB_E_UP, TS_LUNAR_INT_JAMB_E_DOWN);
    if (level_open[2] != 0xFF)
        open_door(((uint16_t)(room_h - 1) << 5) | level_open[2], 1, TS_LUNAR_INT_DOOR_NS, TS_LUNAR_INT_JAMB_NS_L, TS_LUNAR_INT_JAMB_NS_R);
    if (level_open[3] != 0xFF)
        open_door((uint16_t)level_open[3] << 5, 32, TS_LUNAR_INT_DOOR_W, TS_LUNAR_INT_JAMB_W_UP, TS_LUNAR_INT_JAMB_W_DOWN);
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
    banked_memcpy(&chunk_count_ram, &chunk_count, 1, BANK(chunks));
    banked_memcpy(chunk_tag_tab, chunk_tags, chunk_count_ram, BANK(chunks));
    banked_memcpy(chunk_weight_tab, chunk_weight, chunk_count_ram, BANK(chunks));
    for (uint8_t k = 0; k < chunk_count_ram; k++) {  // other themes' chunks weigh nothing here
        uint8_t t;
        banked_memcpy(&t, &chunk_theme[k], 1, BANK(chunks));
        if (t != area_theme) chunk_weight_tab[k] = 0;
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
        uint8_t at = entrance ? (s ? (len - 1) >> 1 : ENTRANCE_DOOR_N) : 3 + below(len - 6 - OPENING_W + 1);
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
    tidy(w, h);
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
    if (area_type[i] == MAP_START) {
        pf_count = pipe_count = level_doors = level_conts = level_vends = 0;
        entrance_room();
    } else {
        uint8_t xs[PF_MAX], ys[PF_MAX], ids[PF_MAX];
        uint16_t runs[PIPE_MAX];
        pf_x = xs, pf_y = ys, pf_id = ids, pipes = runs;
        plan_set_pieces(w, h, area_type[i] == MAP_EXIT);
        look_exterior();
    }
}

static void look_exterior(void) {
    tiles_lunar_ext_apply();
    stamp_set_pieces();
    for (uint8_t k = 0; k < level_conts; k++) level_draw_container(k);
    for (uint8_t k = 0; k < level_vends; k++) {  // (ammo, then health)
        uint8_t v = k ? TS_LUNAR_EXT_VEND_HEALTH : TS_LUNAR_EXT_VEND_AMMO;
        room_vis[((uint16_t)vend_by[k] << 5) | vend_bx[k]] = v;
        room_vis[((uint16_t)(vend_by[k] + 1) << 5) | vend_bx[k]] = v + 1;
    }
}

// A container's blocks: closed, or open (its bit in the map's area_conts_opened).
// The pieces are in kind order, each closed then open (lunar_ext.json cont_*).
void level_draw_container(uint8_t k) BANKED {
    static const uint8_t first[3] = {TS_LUNAR_EXT_CONT_LOCKER, TS_LUNAR_EXT_CONT_CHEST, TS_LUNAR_EXT_CONT_BOX};
    uint8_t kind = cont_kind[k], cw = cont_w[kind], ch = cont_h[kind];
    uint8_t v = first[kind] + (area_conts_opened[level_map] & (1 << k) ? cw * ch : 0);
    for (uint8_t j = 0; j < ch; j++)
        for (uint8_t i = 0; i < cw; i++) room_vis[((uint16_t)(cont_by[k] + j) << 5) | (cont_bx[k] + i)] = v++;
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
            } else if (!x) {
                t = BLK_SOLID;
                v = plates ? TS_LUNAR_INT_WALL_W_PLATES : TS_LUNAR_INT_WALL_W;
            } else if (x == IN_W - 1) {
                t = BLK_SOLID;
                v = plates ? TS_LUNAR_INT_WALL_E_PLATES : TS_LUNAR_INT_WALL_E;
            } else if (y == 2 && !plates) {
                v = TS_LUNAR_INT_FLOOR_SHADOW;  // (the top wall's shadow)
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
    // The way out: the south door, its jambs either side.
    in_put(IN_DOOR, IN_H - 1, TS_LUNAR_INT_DOOR_NS, BLK_FLOOR);
    in_put(IN_DOOR - 1, IN_H - 1, TS_LUNAR_INT_JAMB_NS_L, BLK_SOLID);
    in_put(IN_DOOR + 1, IN_H - 1, TS_LUNAR_INT_JAMB_NS_R, BLK_SOLID);
    // The chest, on one side, above the couches.
    level_chests = 1;
    chest_x[0] = (uint16_t)(below(2) ? 1 : IN_W - 2) * BLOCK_SIZE;
    chest_y[0] = 3 * BLOCK_SIZE;
    chest_camp[0] = 0xFF;  // (a cache: a little luckier)
}

// --- A miniboss's arena ---
// The combat trials' room (boss.c boss_room: 10x8, a rim, open ground: the fights
// are tuned to it) drawn in the world's exterior art instead of the test tiles.
void level_build_arena(void) BANKED {
    tiles_lunar_ext_load();
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
