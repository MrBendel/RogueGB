#pragma bank 255
// Elites (elite.h). The camp that wakes one names it on the HUD; actor.c draws it
// in SPR_PAL_BULLET, runs a swift one's think step twice, and calls elite_fell
// when it's down.
#include <rand.h>
#include "elite.h"
#include "actor_internal.h"
#include "hud.h"
#include "loot.h"
#include "pickups.h"
#include "progress.h"
#include "save.h"
#include "descent.h"

extern uint8_t pickups_map;  // (pickups.c: a descent map or the town, where loot can lie)

#define ELITE_LUCK 12  // its gun: as lucky as a guarded building's locker (pickups.c)

void elite_make(uint8_t i) BANKED {
    uint8_t m = 1 + (rand() & 1);
    actor_elite[i] = m;
    actor_hp[i] = (actor_hp[i] << 1) + 2;
    actor_cache_frame(i);  // (its colours)
    // (Copied into RAM: hud_say runs in its own bank, where this one's text isn't.)
    char t[8] = "SWIFT! ";
    if (m == ELITE_BOMBER) t[0] = 'B', t[1] = 'O', t[2] = 'M', t[3] = 'B', t[4] = 'E', t[5] = 'R', t[6] = '!';
    hud_say(t);
}

void elite_fell(uint8_t i) BANKED {
    if (actor_elite[i] == ELITE_BOMBER) {
        static const int8_t dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        static const int8_t dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
        for (uint8_t k = 0; k < 8; k++) fire_bullet(i, dx[k], dy[k]);
    }
    xp_pending += XP_ELITE;
    Gun g;
    loot_roll(&g, profile.level, (layer_depth - 1) * 4 + ELITE_LUCK);
    if (pickups_map) pickups_drop_gun(actor_x[i], actor_y[i] + 4, &g);
    actor_elite[i] = ELITE_NONE;
}
