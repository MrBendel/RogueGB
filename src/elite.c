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
#include "player.h"
#include "camera.h"
#include "room.h"
#include "health.h"

extern uint8_t pickups_map;  // (pickups.c: a descent map or the town, where loot can lie)

#define ELITE_LUCK 12  // its gun: as lucky as a guarded building's locker (pickups.c)
#define CALM 24        // think steps (48 frames) unhurt before a shield recharges ...
#define RECHARGE 4     // ... then 1 health every this many

#define DRIP 12        // think steps (24 frames) between an acid one's puddles
#define PUDDLE_T 120   // frames a puddle lasts (outlasting 4 drips: a trail of 4 behind it)
#define ACID_DAMAGE 1

uint8_t elite_top[ACTOR_MAX];   // an elite's full health
uint8_t elite_calm[ACTOR_MAX];  // think steps until its shield's next point

// (Copied into RAM: hud_say runs in its own bank, where this one's text isn't.)
static void say(const char *s) {
    char t[8];
    for (uint8_t k = 0; k < 8; k++) t[k] = s[k];
    hud_say(t);
}

uint8_t elite_living;
char elite_name[8];

// Its name: two pieces, Borderlands style (RUSTJAW, NULLBOB ...), 7 letters to fit
// the HUD.
static const char name_a[8][5] = {"RUST", "VOLT", "SCAB", "GRIM", "NULL", "DUST", "BOLT", "SLAG"};
static const char name_b[8][4] = {"JAW", "BOB", "GUT", "REX", "MAW", "VEX", "TOE", "HAM"};

void elite_make(uint8_t i) BANKED {
    uint8_t m = 1 + (rand() & (ELITE_KINDS - 1));
    uint8_t r = rand();
    for (uint8_t k = 0; k < 4; k++) elite_name[k] = name_a[r & 7][k];
    for (uint8_t k = 0; k < 3; k++) elite_name[4 + k] = name_b[(r >> 3) & 7][k];
    elite_name[7] = 0;
    elite_living++;
    actor_elite[i] = m;
    actor_hp[i] = elite_top[i] = (actor_hp[i] << 1) + 2;
    elite_calm[i] = 0;
    actor_cache_frame(i);  // (its colours)
    say(m == ELITE_SWIFT ? "SWIFT! " : m == ELITE_BOMBER ? "BOMBER!" : m == ELITE_SHIELDED ? "SHIELD!" : "ACID!  ");
    hud_say_next(elite_name);
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

// --- Acid puddles ---
uint8_t acid_puddles;
uint16_t puddle_x[ACID_MAX], puddle_y[ACID_MAX];  // the 8x16 sprite's top-left, room pixels
uint8_t puddle_life[ACID_MAX];                     // frames left; 0: none
uint8_t puddle_next;                               // the slot the next one takes (the oldest)

// A puddle in the bottom 5 rows of an 8x16 sprite, bubbling (the hot palette:
// 1 the shine, 2 the acid, 3 its rim).
static const char puddle_art[2][6][9] = {
 {"00000000","00333300","03211230","32122223","03222230","00333300"},
 {"00001000","00333300","03222130","32221123","03222230","00333300"}};

void elite_init(void) BANKED {
    uint8_t data[32];
    for (uint8_t f = 0; f < 2; f++) {
        for (uint8_t k = 0; k < 32; k++) data[k] = 0;
        for (uint8_t y = 0; y < 6; y++) {
            uint8_t lo = 0, hi = 0;
            for (uint8_t x = 0; x < 8; x++) {
                uint8_t c = puddle_art[f][y][x] - '0';
                lo = (lo << 1) | (c & 1);
                hi = (hi << 1) | (c >> 1);
            }
            data[(10 + y) * 2] = lo;
            data[(10 + y) * 2 + 1] = hi;
        }
        set_sprite_data(ACID_VRAM + f * 2, 2, data);
    }
    elite_clear();
}

void elite_clear(void) BANKED {
    for (uint8_t k = 0; k < ACID_MAX; k++) puddle_life[k] = 0;
    acid_puddles = puddle_next = elite_living = 0;
}

// Every DRIP think steps, a puddle where it stands (over the oldest, when all 4 are down).
void elite_drip(uint8_t i) BANKED {
    if (elite_calm[i]) {
        elite_calm[i]--;
        return;
    }
    elite_calm[i] = DRIP;
    uint8_t k = puddle_next;
    puddle_next = (k + 1) & (ACID_MAX - 1);
    if (!puddle_life[k]) acid_puddles++;
    puddle_x[k] = actor_x[i] + 4;
    puddle_y[k] = actor_y[i];
    puddle_life[k] = PUDDLE_T;
}

uint8_t elite_puddles(uint8_t oam) BANKED {
    uint16_t px = player_x + PLAYER_HB_X, py = player_y + PLAYER_HB_Y;
    uint8_t touch = player_z == 0 && player_hittable();  // (jump them)
    uint8_t tile = ACID_VRAM + (((uint8_t)frame_count >> 3) & 2);
    for (uint8_t k = 0; k < ACID_MAX; k++) {
        if (!puddle_life[k]) continue;
        if (!--puddle_life[k]) {
            acid_puddles--;
            continue;
        }
        uint16_t x = puddle_x[k], y = puddle_y[k];
        // Its box: the acid's 6 x 4 px at the sprite's rows 11-14.
        if (touch && (uint16_t)(x + 1 + 6 - px - 1) < (uint16_t)(6 + PLAYER_HB_W - 1) &&
            (uint16_t)(y + 11 + 4 - py - 1) < (uint16_t)(4 + PLAYER_HB_H - 1)) {
            hurt_cause = CAUSE_ACID_POOL;
            player_hurt(ACID_DAMAGE, x + 4, y + 13);
            touch = 0;
        }
        uint16_t sx = x - cam_x + 8, sy = y - cam_y + 16;
        if (sx >= 168 || sy >= VIEW_H + 16 + 16) continue;
        set_sprite_tile(oam, tile);
        set_sprite_prop(oam, SPR_PAL_BULLET);
        move_sprite(oam++, (uint8_t)sx, (uint8_t)sy);
    }
    return oam;
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
    if (elite_living) elite_living--;
}
