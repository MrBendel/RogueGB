#pragma bank 255
// The descent: generated areas (level.c) explored map by map. Walking out
// through an opening fades to the linked map, arriving at its matching opening;
// health, ammo and the rest carry over. Enemies live in camps that wake as the
// player comes near; a cleared camp stays cleared for the expedition. The exit
// map has a shaft in its middle: dropping in leads to a new area, deeper.
// Banked: a few compares a frame, the rest only on a map change.
#include <gb/gb.h>
#include <rand.h>
#include <gb/metasprites.h>
#include "descent.h"
#include "level.h"
#include "room.h"
#include "tiles_lunar_ext.h"
#include "player.h"
#include "camera.h"
#include "actor.h"
#include "gfx.h"
#include "hud.h"
#include "save.h"
#include "pickups.h"
#include "talk.h"
#include "progress.h"
#include "run.h"
#include "weapon.h"
#include <string.h>

uint8_t layer_depth;      // 1 for the first area
uint16_t maps_entered;    // map changes this run (watchable)
uint16_t area_seed_base;  // the run's seed; each area's is derived from it and the depth
uint16_t saved_seconds;   // run_seconds at the last save
// Per camp of the current map: asleep, awake, or cleared; and its enemies' actor slots.
uint8_t camp_state[LEVEL_CAMPS];
uint8_t camp_actors[LEVEL_CAMPS];

extern uint8_t actor_count;  // enemies alive (actor.c)

#define WAKE_X 112  // px from a camp's middle at which it wakes (about a screen)
#define WAKE_Y 96

// The shaft: 2x2 pit blocks in the exit map's middle (player.c lets a fall there
// through instead of hurting).
static void dig_shaft(void) {
    uint8_t x = (room_w >> 1) - 1, y = (room_h >> 1) - 1;
    uint16_t k = ((uint16_t)y << 5) | x;
    room_map[k] = room_map[k + 1] = room_map[k + 32] = room_map[k + 33] = BLK_PIT;
}

static void start_area(void) {
    level_generate(area_seed_base + (uint16_t)layer_depth * 7919);
}

void descent_start(uint16_t seed) BANKED {
    layer_depth = 1;
    ammo_fill();  // a new expedition: full reserves
    maps_entered = 0;
    saved_seconds = 0;
    area_seed_base = seed;
    start_area();
    descent_enter(area_start, DESCENT_NO_DOOR);
}

// The expedition saved in the profile (save.h): its area rebuilt from the seed,
// its cleared camps put back, and the player at an opening of its map (or the
// middle of the start map).
void descent_resume(void) BANKED {
    layer_depth = profile.exp_depth;
    maps_entered = 0;
    saved_seconds = 0;
    area_seed_base = profile.exp_seed;
    start_area();
    memcpy(area_camps_cleared, profile.exp_camps, sizeof(area_camps_cleared));
    memcpy(area_chests_opened, profile.exp_chests, sizeof(area_chests_opened));
    uint8_t map = profile.exp_map, links = area_links[map], side = DESCENT_NO_DOOR;
    if (map != area_start) side = links & EDGE_N ? EDGE_N : links & EDGE_E ? EDGE_E : links & EDGE_S ? EDGE_S : EDGE_W;
    descent_enter(map, side);
}

// Save at a map's entrance (save.h: nothing without a slot).
static void autosave(void) {
    if (save_slot == SAVE_NONE) return;
    save_add_time(run_seconds - saved_seconds);
    saved_seconds = run_seconds;
    profile.exp_active = 1;
    profile.exp_seed = area_seed_base;
    profile.exp_depth = layer_depth;
    profile.exp_map = level_map;
    memcpy(profile.exp_camps, area_camps_cleared, sizeof(profile.exp_camps));
    memcpy(profile.exp_chests, area_chests_opened, sizeof(profile.exp_chests));
    if (layer_depth > profile.best_depth) profile.best_depth = layer_depth;
    save_write();
}

// Where the player stands on arriving through the opening on `side` of the new
// map: just inside it.
static void place_player(uint8_t side) {
    uint8_t s = side == EDGE_N ? 0 : side == EDGE_E ? 1 : side == EDGE_S ? 2 : 3;
    uint16_t at = (uint16_t)(level_open[s] + 1) * BLOCK_SIZE;  // the opening's middle, less half the player
    switch (side) {
    case EDGE_N: player_x = at; player_y = 2 * BLOCK_SIZE - 8; break;
    case EDGE_S: player_x = at; player_y = room_px_h - 3 * BLOCK_SIZE + 4; break;
    case EDGE_W: player_x = 2 * BLOCK_SIZE; player_y = at - 4; break;
    case EDGE_E: player_x = room_px_w - 3 * BLOCK_SIZE; player_y = at - 4; break;
    default:  // the middle (beside the shaft in an exit map)
        player_x = (uint16_t)((room_w >> 1) - 1) * BLOCK_SIZE - 24;
        player_y = (uint16_t)((room_h >> 1) - 1) * BLOCK_SIZE;
        break;
    }
    player_xs = player_ys = 0;
}

void descent_enter(uint8_t map, uint8_t side) BANKED {
    level_map = map;
    level_build_map(map);
    if (area_type[map] == MAP_EXIT) {
        dig_shaft();
        tiles_lunar_ext_apply();
    }
    pit_shaft = area_type[map] == MAP_EXIT;
    pickups_load_map(area_chests_opened[map]);  // (after the shaft: chests make their blocks solid)
    // An area's start map has the lift back up to town (a file's only).
    talk_clear();
    if (map == area_start && save_slot != SAVE_NONE) {
        talk_add(room_w >> 1, room_h >> 1, TALK_LIFT, TALK_DO_UP);
        talk_init();
    }
    enemy_hp_bonus = layer_depth - 1;           // deeper enemies take more to bring down
    shaft_taken = 0;
    place_player(side);
    maps_entered++;
    for (uint8_t c = 0; c < LEVEL_CAMPS; c++) {
        camp_state[c] = area_camps_cleared[map] & (1 << c) ? CAMP_CLEARED : CAMP_ASLEEP;
        camp_actors[c] = 0;
    }
    if (area_type[map] == MAP_EXIT) {
        char t[8] = "SHAFT  ";
        hud_say(t);
    }
    autosave();
}

// Wake camp c: its enemies at its slots, a third of them peashooters, up to
// 2 + depth of them.
static void wake(uint8_t c) {
    uint8_t n = 2 + layer_depth;
    camp_state[c] = CAMP_AWAKE;
    for (uint8_t s = 0; s < camp_slots[c] && n; s++) {
        uint16_t x = camp_slot_x[c][s], y = camp_slot_y[c][s];
        if (room_block_at(x + 8, y + 8) != BLK_FLOOR) continue;
        uint8_t a = actor_spawn((rand() & 3) == 0 ? ACTOR_PEASHOOTER : ACTOR_SLIME, x, y);
        if (a >= ACTOR_MAX) break;
        for (uint8_t o = 0; o < LEVEL_CAMPS; o++) camp_actors[o] &= ~(1 << a);  // a reused slot
        camp_actors[c] |= 1 << a;
        n--;
    }
}

void descent_update(void) BANKED {
    for (uint8_t c = 0; c < level_camps; c++) {
        if (camp_state[c] == CAMP_ASLEEP) {
            int16_t dx = (int16_t)(player_x + 8 - camp_x[c]), dy = (int16_t)(player_y + 12 - camp_y[c]);
            if (dx > -WAKE_X && dx < WAKE_X && dy > -WAKE_Y && dy < WAKE_Y) wake(c);
        } else if (camp_state[c] == CAMP_AWAKE) {
            uint8_t alive = 0;
            for (uint8_t a = 0; a < ACTOR_MAX; a++)
                if ((camp_actors[c] & (1 << a)) && actor_type[a] != ACTOR_NONE) alive = 1;
            if (!alive) {
                camp_state[c] = CAMP_CLEARED;
                area_camps_cleared[level_map] |= 1 << c;
                char t[8] = "CLEAR! ";
                hud_say(t);
                xp_gain(XP_CAMP + 5 * layer_depth);
            }
        }
    }
}

// The linked map through side d, and the side of it the player comes in by.
static uint8_t through(uint8_t d, uint8_t *side) {
    switch (d) {
    case EDGE_N: *side = EDGE_S; return level_map - AREA_W;
    case EDGE_S: *side = EDGE_N; return level_map + AREA_W;
    case EDGE_E: *side = EDGE_W; return level_map + 1;
    default: *side = EDGE_E; return level_map - 1;
    }
}

uint8_t descent_door(void) BANKED {
    if (shaft_taken) return DESCENT_SHAFT;
    // An opening is in the outer ring of blocks; stepping into it leaves.
    int16_t cx = (int16_t)player_x + 8, fy = (int16_t)player_y + 12;
    uint8_t d = 0;
    if (cx < BLOCK_SIZE) d = EDGE_W;
    else if (cx > (int16_t)room_px_w - BLOCK_SIZE) d = EDGE_E;
    else if (fy < BLOCK_SIZE) d = EDGE_N;
    else if (fy > (int16_t)room_px_h - BLOCK_SIZE) d = EDGE_S;
    if (!(area_links[level_map] & d)) return 0;
    return d;
}

void descent_change_room(uint8_t d) BANKED {
    fade_out();
    DISPLAY_OFF;
    gfx_sprites(0);
    actors_clear();
    if (d == DESCENT_SHAFT) {
        // Down the shaft: a new area, one deeper, from its start map.
        layer_depth++;
        start_area();
        descent_enter(area_start, DESCENT_NO_DOOR);
        player_fall_timer = 0;
        player_z = 0;
        char t[8] = "DEPTH  ";
        t[6] = '0' + (layer_depth > 9 ? 9 : layer_depth);
        hud_say(t);
        xp_gain(XP_SHAFT * layer_depth);
    } else {
        uint8_t side, next = through(d, &side);
        descent_enter(next, side);
    }
    camera_init(player_x + 8, player_y + 8);
    // Every sprite still sits where it was on the old map's screen (the gun,
    // the muzzle flash, the reticle, enemies): clear them all, then draw just
    // the player. The rest come back as the next frame draws them.
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
    player_draw();
    DISPLAY_ON;
    gfx_sprites(1);
    fade_in();
}
