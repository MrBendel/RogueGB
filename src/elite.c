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
#include "state.h"
#include "gfx.h"

extern uint8_t pickups_map;  // (pickups.c: a descent map or the town, where loot can lie)

#define ELITE_LUCK 12  // its gun: as lucky as a guarded building's locker (pickups.c)
#define CALM 24        // think steps (48 frames) unhurt before a shield recharges ...
#define RECHARGE 4     // ... then 1 health every this many

uint8_t elite_top[ACTOR_MAX];   // a shielded one's full health
uint8_t elite_calm[ACTOR_MAX];  // think steps until its shield's next point

// (Copied into RAM: hud_say runs in its own bank, where this one's text isn't.)
static void say(const char *s) {
    char t[8];
    for (uint8_t k = 0; k < 8; k++) t[k] = s[k];
    hud_say(t);
}

void elite_make(uint8_t i) BANKED {
    uint8_t m = 1 + (uint8_t)(rand() & 0x7F) % ELITE_KINDS;
    actor_elite[i] = m;
    actor_hp[i] = elite_top[i] = (actor_hp[i] << 1) + 2;
    elite_calm[i] = 0;
    actor_cache_frame(i);  // (its colours)
    say(m == ELITE_SWIFT ? "SWIFT! " : m == ELITE_BOMBER ? "BOMBER!" : "SHIELD!");
}

// Hurt, its shield waits CALM steps, then recharges a point at a time up to full,
// shimmering as it does; shot to half or below, it breaks for good.
void elite_shield(uint8_t i) BANKED {
    uint8_t hp = actor_hp[i], top = elite_top[i];
    // (The shimmer: actor.c's colours, or the UI's every other 4 frames; a new
    // frame (actor_cache_frame) puts them back.)
    uint8_t pal = !elite_calm[i] || elite_calm[i] > RECHARGE || hp >= top || ((uint8_t)frame_count & 4) ? SPR_PAL_BULLET : SPR_PAL_UI;
    actor_p0[i] = (actor_p0[i] & ~7) | pal;
    actor_p1[i] = (actor_p1[i] & ~7) | pal;
    if (hp <= top >> 1) {
        actor_elite[i] = ELITE_BROKEN;
        say("BROKEN!");
    } else if (actor_hurt[i]) {
        elite_calm[i] = CALM;
    } else if (elite_calm[i]) {
        elite_calm[i]--;
    } else if (hp < top) {
        actor_hp[i] = hp + 1;
        elite_calm[i] = RECHARGE;
    }
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
