#pragma bank 255
// The world table (world.h): one row per world. A new world adds a row, its
// tilesets (content/tilesets) and its case in world_load_tiles; the descent, the
// level generator's plan and the miniboss flow read it from here.
// Its bosses' state shares one stretch of RAM with every other boss's (room.h
// BOSSES_ADDR), so a world's fights cost no RAM beyond the biggest one.
#include <rand.h>
#include "world.h"
#include "level.h"
#include "actor.h"
#include "chunks.h"
#include "tiles_lunar_ext.h"
#include "tiles_lunar_int.h"

static const World worlds[WORLD_COUNT] = {
    // LunarBase: slimes, a peashooter in four; from the second area, a hound in
    // place of a slime (docs/design/runs.md: flankers join in area 2). The
    // Claimjaw, Scrapback, Cragback.
    {CHUNK_THEME_LUNAR,
     {{ACTOR_PEASHOOTER, ACTOR_SLIME, ACTOR_SLIME, ACTOR_SLIME},
      {ACTOR_PEASHOOTER, ACTOR_SLIME, ACTOR_SLIME, ACTOR_HOUND}},
     {0, 1, 2}},
};

uint8_t world_id;

void world_enter(uint8_t id) BANKED {
    world_id = id;
    area_theme = worlds[id].theme;
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
