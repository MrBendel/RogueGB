#pragma bank 255
// The descent: generated areas (level.c) explored map by map. Walking out
// through an opening fades to the linked map, arriving at its matching opening;
// health, ammo and the rest carry over. Enemies live in camps that wake as the
// player comes near; a cleared camp stays cleared for the expedition. The exit
// map has a shaft in its middle: dropping in leads to a new area, deeper.
// Banked: a few compares a frame, the rest only on a map change.
#include <gb/gb.h>
#include <gb/metasprites.h>
#include "descent.h"
#include "level.h"
#include "room.h"
#include "world.h"
#include "player.h"
#include "camera.h"
#include "actor.h"
#include "gfx.h"
#include "hud.h"
#include "save.h"
#include "pickups.h"
#include "talk.h"
#include "dialogue.h"
#include "progress.h"
#include "run.h"
#include "weapon.h"
#include "loot.h"
#include "health.h"
#include "sfx.h"
#include <string.h>

uint8_t layer_depth;      // 1 for the first area
uint16_t maps_entered;    // map changes this run (watchable)
uint16_t area_seed_base;  // the run's seed; each area's is derived from it and the depth
uint16_t saved_seconds;   // run_seconds at the last save
// Per camp of the current map: asleep, awake, or cleared; and its enemies' actor slots.
uint8_t camp_state[LEVEL_CAMPS];
uint8_t camp_bit0;  // the map's first camp bit in area_camps_cleared: 0, or a building's (level.h INSIDE_CAMP_BIT + door)
uint8_t camp_actors[LEVEL_CAMPS];

extern uint8_t actor_count;  // enemies alive (actor.c)

#define WAKE_X 112  // px from a camp's middle at which it wakes (about a screen)
#define WAKE_Y 96

// (The shaft, 2x2 pit blocks in an exit map's middle, is dug by level.c as the map
// is built; player.c lets a fall there through instead of hurting.)

static void start_area(void) {
    level_generate(area_seed_base + (uint16_t)layer_depth * 7919);
}

void descent_start(uint16_t seed) BANKED {
    world_enter(WORLD_LUNAR);  // (the only world so far: the town's lift will choose)
    layer_depth = 1;
    ammo_fill();  // a new expedition: full reserves
    maps_entered = 0;
    saved_seconds = 0;
    area_seed_base = seed;
    start_area();
    descent_enter(area_start, EDGE_S);  // in through the entrance room's south door
}

// The expedition saved in the profile (save.h): its area rebuilt from the seed,
// its cleared camps put back, and the player at an opening of its map (or the
// middle of the start map).
void descent_resume(void) BANKED {
    world_enter(profile.exp_world < WORLD_COUNT ? profile.exp_world : WORLD_LUNAR);
    layer_depth = profile.exp_depth;
    maps_entered = 0;
    saved_seconds = 0;
    area_seed_base = profile.exp_seed;
    start_area();
    memcpy(area_camps_cleared, profile.exp_camps, sizeof(area_camps_cleared));
    memcpy(area_chests_opened, profile.exp_chests, sizeof(area_chests_opened));
    memcpy(area_conts_opened, profile.exp_conts, sizeof(area_conts_opened));
    memcpy(area_inside_opened, profile.exp_inside, sizeof(area_inside_opened));
    uint8_t map = profile.exp_map, links = area_links[map], side = EDGE_S;  // (the entrance: its south door)
    if (map != area_start) side = links & EDGE_N ? EDGE_N : links & EDGE_E ? EDGE_E : links & EDGE_S ? EDGE_S : EDGE_W;
    // Back from its miniboss (or a continue after): beside the shaft, open now.
    if (area_type[map] == MAP_EXIT && (area_chests_opened[map] & AREA_BOSS_BEATEN)) side = DESCENT_NO_DOOR;
    descent_enter(map, side);
}

static void autosave(void);

void descent_save(void) BANKED {
    autosave();
}

// The area's miniboss: the world's, in turn with depth (play.c: MODE_MINIBOSS's kinds).
uint8_t descent_miniboss(void) BANKED {
    return world_miniboss(layer_depth);
}

// Each shield tier's price in town (credits).
static const uint16_t shield_prices[SHIELD_TIERS] = {100, 250, 500, 900};

// A vending machine: the vendor post's (level.h), its price by depth, for every
// reserve full or full health (nothing charged if you're full already); or the
// town's, beside Marro: a random gun at your level for twice a common one's
// price (as lucky as his stock), dropped in front of the machine with its card;
// or the shield, then each tier up. Paid in credits.
void descent_vend(uint8_t what) BANKED {
    uint16_t price;
    char t[8] = "-    CR";
    if (what == TALK_DO_VEND_GUN) {
        price = (uint16_t)(15 + 5 * profile.level) << 1;
    } else if (what == TALK_DO_VEND_SHIELD) {
        if (profile.has_shield >= SHIELD_TIERS) {
            hud_say("MAX    ");
            return;
        }
        price = shield_prices[profile.has_shield];
    } else {
        uint8_t ammo = what == TALK_DO_VEND_AMMO;
        price = (uint16_t)(ammo ? 25 : 40) * layer_depth;
        if (ammo ? ammo_full() : player_hp == player_hp_max) {
            hud_say("FULL   ");
            return;
        }
    }
    if (profile.credits < price) {
        hud_say("NO CR  ");
        return;
    }
    if (what == TALK_DO_VEND_GUN) {
        Gun g;
        loot_roll(&g, profile.level, profile.best_depth * 3);
        // Below the machine (talk_near's block): out of its reach, so A takes the gun.
        if (!pickups_drop_gun((uint16_t)talk_bx[talk_near] << 4, ((uint16_t)talk_by[talk_near] + 2) << 4, &g)) {
            hud_say("NO ROOM");  // (three on the floor already)
            return;
        }
    } else if (what == TALK_DO_VEND_SHIELD) {
        profile.has_shield++;
        health_give_shield(SHIELD_TIER_CAP(profile.has_shield), shield_delay, SHIELD_START_RATE);
    } else if (what == TALK_DO_VEND_AMMO) {
        ammo_fill();
    } else {
        player_hp = player_hp_max;
    }
    profile.credits -= price;
    for (int8_t k = 3; k >= 1; k--) {  // "-nnn CR"
        t[k] = '0' + price % 10;
        price /= 10;
        if (!price) break;
    }
    hud_say(t);
    sfx_play(SFX_PICKUP);
    save_write();
}

// Save at a map's entrance (save.h: nothing without a slot).
static void autosave(void) {
    if (save_slot == SAVE_NONE) return;
    save_add_time(run_seconds - saved_seconds);
    saved_seconds = run_seconds;
    profile.exp_active = 1;
    profile.exp_seed = area_seed_base;
    profile.exp_depth = layer_depth;
    profile.exp_world = world_id;
    profile.exp_map = level_map;
    memcpy(profile.exp_camps, area_camps_cleared, sizeof(profile.exp_camps));
    memcpy(profile.exp_chests, area_chests_opened, sizeof(profile.exp_chests));
    memcpy(profile.exp_conts, area_conts_opened, sizeof(profile.exp_conts));
    memcpy(profile.exp_inside, area_inside_opened, sizeof(profile.exp_inside));
    if (layer_depth > profile.best_depth) profile.best_depth = layer_depth;
    save_write();
}

// Where the player stands on arriving through the opening on `side` of the new
// map: just inside it.
static void place_player(uint8_t side) {
    uint8_t s = side == EDGE_N ? 0 : side == EDGE_E ? 1 : side == EDGE_S ? 2 : 3;
    // The opening's middle, less half the player (an entrance room's doorways are one block).
    uint16_t at = (uint16_t)(level_open[s] + (level_map == area_start ? 0 : 1)) * BLOCK_SIZE;
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
    level_inside = 0xFF;
    chest_bit0 = camp_bit0 = 0;
    // Each map's tileset (the display is off): the entrance room is indoors.
    world_load_tiles(map == area_start);
    level_build_map(map);  // (an exit map's shaft included)
    pit_shaft = area_type[map] == MAP_EXIT;
    pickups_load_map(area_chests_opened[map]);  // (after the shaft: chests make their blocks solid)
    // An area's entrance room has the comms device on its upper back wall: the briefing
    // (the way back to town is its south door: descent_door).
    talk_clear();
    if (map == area_start) {
        for (uint8_t b = 0; b < 2; b++)  // (on the wall: from below)
            talk_add(COMMS_X + b, COMMS_Y + 1, TALK_COMMS, DLG_AREA_BRIEFING);
        talk_init();
    }
    // An exit map's vendor post: its machines (drawn by the map) talk from below.
    for (uint8_t k = 0; k < level_vends; k++)
        talk_add(vend_bx[k], vend_by[k] + 1, TALK_COMMS, k ? TALK_DO_VEND_HEALTH : TALK_DO_VEND_AMMO);
    if (level_vends) talk_init();
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

// Wake camp c: its enemies at its slots, from the world's roster, up to
// 2 + depth of them.
static void wake(uint8_t c) {
    uint8_t n = 2 + layer_depth;
    camp_state[c] = CAMP_AWAKE;
    for (uint8_t s = 0; s < camp_slots[c] && n; s++) {
        uint16_t x = camp_slot_x[c][s], y = camp_slot_y[c][s];
        if (room_block_at(x + 8, y + 8) != BLK_FLOOR) continue;
        uint8_t a = actor_spawn(world_enemy(layer_depth), x, y);
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
                area_camps_cleared[level_map] |= 1 << (c + camp_bit0);
                char t[8] = "CLEAR! ";
                hud_say(t);
                xp_gain(XP_CAMP + 5 * layer_depth);
            }
        }
    }
}

// In through building door k of the current map: its interior (the interior
// tileset, from a seed of the area, the map and the door), the player just inside
// its south door. Its chest is bit 3 + k of the map's.
static void enter_building(uint8_t k) {
    level_inside = k;
    world_load_tiles(1);
    level_build_interior(area_seed_base ^ ((uint16_t)layer_depth * 7919) ^ ((uint16_t)level_map << 8) ^ (k * 97 + 1));
    chest_bit0 = 3 + k;
    camp_bit0 = INSIDE_CAMP_BIT + k;
    // Its guard (if it has one), asleep unless cleared, before the chest that waits for it.
    for (uint8_t c = 0; c < LEVEL_CAMPS; c++) {
        camp_state[c] = c < level_camps && !(area_camps_cleared[level_map] & (1 << (c + camp_bit0))) ? CAMP_ASLEEP : CAMP_CLEARED;
        camp_actors[c] = 0;
    }
    pickups_load_map(area_chests_opened[level_map] >> chest_bit0);
    talk_clear();
    player_x = (uint16_t)level_open[2] * BLOCK_SIZE;
    player_y = room_px_h - 3 * BLOCK_SIZE + 4;
    player_xs = player_ys = 0;
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

uint8_t level_inside = 0xFF;

uint8_t descent_door(void) BANKED {
    if (shaft_taken) return DESCENT_SHAFT;
    // An opening is in the outer ring of blocks; stepping into it leaves.
    int16_t cx = (int16_t)player_x + 8, fy = (int16_t)player_y + 12;
    if (level_inside != 0xFF) return fy > (int16_t)room_px_h - BLOCK_SIZE ? DESCENT_OUTSIDE : 0;
    // The exit map's shaft is guarded (docs/design/runs.md: a miniboss every area):
    // within 3 blocks of it, the fight; once won, the way down is open. (A file's
    // descent: the arena and the way back go through the save.)
    if (area_type[level_map] == MAP_EXIT && save_slot != SAVE_NONE && !(area_chests_opened[level_map] & AREA_BOSS_BEATEN)) {
        int16_t dx = cx - (int16_t)((room_w >> 1) << 4), dy = fy - (int16_t)((room_h >> 1) << 4);
        if (dx > -48 && dx < 48 && dy > -48 && dy < 48) return DESCENT_ARENA;
    }
    // A building's door: up its step, into the door (solid: the player stops with
    // the top of their box at the step's top).
    for (uint8_t k = 0; k < level_doors; k++)
        if ((uint8_t)(cx >> 4) == door_bx[k] && player_y + PLAYER_HB_Y <= (uint16_t)door_by[k] * BLOCK_SIZE + 1
            && player_y + PLAYER_HB_Y + PLAYER_HB_H > (uint16_t)door_by[k] * BLOCK_SIZE)
            return DESCENT_INSIDE | k;
    uint8_t d = 0;
    if (cx < BLOCK_SIZE) d = EDGE_W;
    else if (cx > (int16_t)room_px_w - BLOCK_SIZE) d = EDGE_E;
    else if (fy < BLOCK_SIZE) d = EDGE_N;
    else if (fy > (int16_t)room_px_h - BLOCK_SIZE) d = EDGE_S;
    if (d == EDGE_S && level_map == area_start) return DESCENT_TOWN;  // the entrance's way out
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
        descent_enter(area_start, EDGE_S);
        player_fall_timer = 0;
        player_z = 0;
        char t[8] = "DEPTH  ";
        t[6] = '0' + (layer_depth > 9 ? 9 : layer_depth);
        hud_say(t);
        xp_gain(XP_SHAFT * layer_depth);
    } else if (d & DESCENT_INSIDE) {
        enter_building(d & 3);
    } else if (d == DESCENT_OUTSIDE) {
        // Back out onto the building's step: its map rebuilt (the same from its seed).
        uint8_t k = level_inside;
        level_inside = 0xFF;
        chest_bit0 = 0;
        descent_enter(level_map, EDGE_S);
        player_x = (uint16_t)door_bx[k] * BLOCK_SIZE;
        player_y = (uint16_t)door_by[k] * BLOCK_SIZE + 4;
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
