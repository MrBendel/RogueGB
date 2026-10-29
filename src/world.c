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
    // LunarBase: slimes, a peashooter in four; from the second area, a hound and
    // a duelist in place of two slimes (docs/design/runs.md: flankers and duelists
    // join in area 2). The Claimjaw, Scrapback, Cragback. Its maintenance drones
    // and patrol hounds slate blue with a cyan eye; its turrets and mercs in the
    // corporation's yellow, a red visor.
    {CHUNK_THEME_LUNAR,
     {{ACTOR_PEASHOOTER, ACTOR_SLIME, ACTOR_SLIME, ACTOR_SLIME},
      {ACTOR_PEASHOOTER, ACTOR_SLIME, ACTOR_HOUND, ACTOR_DUELIST}},
     {0, 1, 2},
     {RGB(10, 30, 31), RGB(11, 15, 26), RGB(2, 3, 10)},
     {RGB(31, 8, 6), RGB(28, 22, 4), RGB(5, 3, 9)}},
};

uint8_t world_id;

void world_enter(uint8_t id) BANKED {
    world_id = id;
    area_theme = worlds[id].theme;
    // (Play's palettes are in: play_enter set them just before.)
    for (uint8_t k = 0; k < 3; k++) {
        pal_base_obj[(SPR_PAL_SLIME << 2) + 1 + k] = worlds[id].creature_pal[k];
        pal_base_obj[(SPR_PAL_SHOOTER << 2) + 1 + k] = worlds[id].machine_pal[k];
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

uint8_t world_enemy(uint8_t depth) BANKED {
    uint8_t tier = depth >= WORLD_TIERS ? WORLD_TIERS - 1 : depth ? depth - 1 : 0;
    return worlds[world_id].roster[tier][rand() & (WORLD_ROSTER - 1)];
}

uint8_t world_miniboss(uint8_t depth) BANKED {
    return worlds[world_id].minibosses[(uint8_t)((depth - 1) % WORLD_MINIBOSSES)];
}
