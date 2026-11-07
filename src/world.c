#pragma bank 255
// The world table (world.h): one row per world. A new world adds a row, its
// tilesets (content/tilesets) and its case in world_load_tiles; the descent, the
// level generator's plan and the miniboss flow read it from here.
// Its bosses' state shares one stretch of RAM with every other boss's (room.h
// BOSSES_ADDR), so a world's fights cost no RAM beyond the biggest one.
#include <rand.h>
#include <gb/cgb.h>
#include "world.h"
#include "level.h"
#include "actor.h"
#include "chunks.h"
#include "gfx.h"
#include "tiles_lunar_ext.h"
#include "tiles_lunar_int.h"

static const World worlds[WORLD_COUNT] = {
    // LunarBase: from the first area every kind but the medic and the stalker,
    // spiders the most; from the second, stalkers and the medic drone join; from
    // the third, stalkers, bulwarks and medics the most. (A map draws 2 or 3 kinds
    // from it, repeats weighting the draw; its camps take those in turn: world_squad.) The Claimjaw, Scrapback, Cragback. Its maintenance drones,
    // spiders, stalkers and patrol hounds slate blue with a cyan eye; its turrets,
    // mercs and bulwarks in the corporation's yellow, a red visor; its bullets and
    // elites hot magenta.
    {CHUNK_THEME_LUNAR,
     {{ACTOR_SPIDER, ACTOR_PEASHOOTER, ACTOR_SLIME, ACTOR_HOUND, ACTOR_SPIDER, ACTOR_DUELIST, ACTOR_BULWARK, ACTOR_TANK},
      {ACTOR_SPIDER, ACTOR_DUELIST, ACTOR_STALKER, ACTOR_HOUND, ACTOR_BULWARK, ACTOR_SPIDER, ACTOR_MEDIC, ACTOR_PEASHOOTER},
      {ACTOR_MEDIC, ACTOR_BULWARK, ACTOR_STALKER, ACTOR_DUELIST, ACTOR_SPIDER, ACTOR_TANK, ACTOR_STALKER, ACTOR_HOUND}},
     {0, 1, 2},
     {RGB(10, 30, 31), RGB(11, 15, 26), RGB(2, 3, 10)},
     {RGB(31, 8, 6), RGB(28, 22, 4), RGB(5, 3, 9)},
     {RGB(31, 26, 31), RGB(31, 4, 16), RGB(6, 0, 4)}},
};

uint8_t world_id;

void world_enter(uint8_t id) BANKED {
    world_id = id;
    area_theme = worlds[id].theme;
    // (Play's palettes are in: play_enter set them just before.)
    for (uint8_t k = 0; k < 3; k++) {
        pal_base_obj[(SPR_PAL_SLIME << 2) + 1 + k] = worlds[id].creature_pal[k];
        pal_base_obj[(SPR_PAL_SHOOTER << 2) + 1 + k] = worlds[id].machine_pal[k];
        pal_base_obj[(SPR_PAL_BULLET << 2) + 1 + k] = worlds[id].hot_pal[k];
    }
}

void world_load_tiles(uint8_t indoor) BANKED {
    switch (world_id) {
    default:  // WORLD_LUNAR
        if (indoor) tiles_lunar_int_load();
        else tiles_lunar_ext_load();
        break;
    }
}

uint8_t world_roster_kind(uint8_t tier, uint8_t j) BANKED {
    return worlds[world_id].roster[tier][j];
}

uint8_t world_miniboss_kind(uint8_t i) BANKED {
    return worlds[world_id].minibosses[i];
}

uint8_t room_kinds[ROOM_KINDS], room_kind_n;
uint16_t room_seed;

void world_room_clear(uint16_t seed) BANKED {
    room_kind_n = 0;
    room_seed = seed | 1;  // (xorshift: never 0)
}

// The map's kinds: 2 (3 from the second area) different ones from the depth's roster.
static void room_pick(uint8_t depth) {
    uint8_t tier = depth >= WORLD_TIERS ? WORLD_TIERS - 1 : depth ? depth - 1 : 0;
    uint8_t want = depth >= 2 ? 3 : 2;
    const uint8_t *roster = worlds[world_id].roster[tier];
    for (uint8_t tries = 0; tries < 32 && room_kind_n < want; tries++) {
        room_seed ^= room_seed << 7;  // (xorshift16: the map's own draw, not rand's)
        room_seed ^= room_seed >> 9;
        room_seed ^= room_seed << 8;
        uint8_t k = roster[(room_seed >> 8) & (WORLD_ROSTER - 1)], dup = 0;
        for (uint8_t i = 0; i < room_kind_n; i++) dup |= room_kinds[i] == k;
        if (!dup) room_kinds[room_kind_n++] = k;
    }
}

uint8_t world_enemy(uint8_t depth) BANKED {
    return world_squad(depth, rand());
}

uint8_t world_squad(uint8_t depth, uint8_t j) BANKED {
    if (!room_kind_n) room_pick(depth);
    return room_kinds[j % room_kind_n];
}

uint8_t world_miniboss(uint8_t depth) BANKED {
    return worlds[world_id].minibosses[(uint8_t)((depth - 1) % WORLD_MINIBOSSES)];
}
