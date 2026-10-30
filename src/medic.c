#pragma bank 255
// The medic drone (actor.h ACTOR_MEDIC): the descent's support (docs/design/runs.md,
// "Enemies: a roster by role"). It never shoots. It hangs back from you, drifting
// to whichever of its squad is hurt, and every few seconds glows (flashing: the
// warning) and patches up everyone near it. Kill it first, or the fight drags.
#include <gb/gb.h>
#include "medic.h"
#include "actor_internal.h"
#include "player.h"
#include "state.h"
#include "hound.h"
#include "duelist.h"
#include "tank.h"
#include "elite.h"

#define HOVER 0
#define GLOW 1
#define NEAR 48        // px: closer than this, it backs off from you ...
#define FAR 88         // ... farther (with nobody hurt), it comes closer
#define REACH 48       // px: its heal reaches squadmates this near
#define HEAL 3         // health a pulse gives back
#define GLOW_T 16      // frames of warning
#define PULSE_T 90     // frames between pulses
#define NOBODY 0xFF
// actor_try: the squadmate it's drifting to, or NOBODY.

uint8_t medic_frames[MEDIC_FRAMES * 4];
extern uint8_t elite_top[ACTOR_MAX];  // (elite.c: an elite's full health)

// A boxy drone with a red cross, its rotor arms out (3 outline, 2 the casing,
// 1 the cross: the machines' palette, world.c). Hovering high, then low.
static const char art[2][16][17] = {
 {"0000000000000000","0000000000000000","3330000000000333","0033333333333300",
  "0000322222230000","0003222112223000","0003221111223000","0003222112223000",
  "0003222222223000","0000333333330000","0000003003000000","0000000000000000",
  "0000000000000000","0000000000000000","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000000000000","3330000000000333",
  "0033333333333300","0000322222230000","0003222112223000","0003221111223000",
  "0003222112223000","0003222222223000","0000333333330000","0000003003000000",
  "0000000000000000","0000000000000000","0000000000000000","0000000000000000"}};

void medic_init(uint8_t v) BANKED {
    uint8_t data[32];
    for (uint8_t f = 0; f < 2; f++)
        for (uint8_t half = 0; half < 2; half++) {
            for (uint8_t y = 0; y < 16; y++) {
                uint8_t lo = 0, hi = 0;
                for (uint8_t x = 0; x < 8; x++) {
                    uint8_t c = art[f][y][half * 8 + x] - '0';
                    lo = (lo << 1) | (c & 1);
                    hi = (hi << 1) | (c >> 1);
                }
                data[y * 2] = lo;
                data[y * 2 + 1] = hi;
            }
            set_sprite_data(v + (f * 2 + half) * 2, 2, data);
        }
    static const uint8_t frames[MEDIC_FRAMES * 4] = {0, 0, 1, 0,  2, 0, 3, 0};
    for (uint8_t k = 0; k < sizeof(frames); k++) medic_frames[k] = frames[k];
}

// A squadmate's full health: its kind's (with the depth's bonus), or an elite's.
static uint8_t full_hp(uint8_t j) {
    static const uint8_t base[ACTOR_TYPE_COUNT] = {0, SLIME_HP, SHOOTER_HP, HOUND_HP, DUELIST_HP, TANK_HP, MEDIC_HP};
    return actor_elite[j] ? elite_top[j] : base[actor_type[j]] + enemy_hp_bonus;
}

static uint8_t near(uint8_t i, uint8_t j, uint8_t r) {
    uint16_t dx = actor_x[j] - actor_x[i] + r, dy = actor_y[j] - actor_y[i] + r;
    return dx < (uint16_t)(r << 1) && dy < (uint16_t)(r << 1);
}

static int8_t sign(int16_t d) {
    return d > 4 ? 1 : d < -4 ? -1 : 0;
}

void medic_update(uint8_t i) BANKED {
    uint8_t t = actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint16_t d = ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);  // (octagonal)
    if (actor_state[i] == HOVER) {
        // Now and then, pick the most hurt squadmate to drift to.
        if (!(t & 15)) {
            uint8_t best = NOBODY, gap = 0;
            for (uint8_t j = 0; j < ACTOR_MAX; j++) {
                if (j == i || !actor_type[j] || actor_scripted[j] || !actor_hp[j]) continue;
                uint8_t full = full_hp(j);
                if (actor_hp[j] < full && full - actor_hp[j] > gap) gap = full - actor_hp[j], best = j;
            }
            actor_try[i] = best;
        }
        int8_t mx = 0, my = 0;
        uint8_t j = actor_try[i];
        if (d < NEAR) {
            mx = -sign(dx), my = -sign(dy);  // away from you first
        } else if (j != NOBODY && actor_type[j]) {
            if (!near(i, j, 16)) mx = sign((int16_t)(actor_x[j] - actor_x[i])), my = sign((int16_t)(actor_y[j] - actor_y[i]));
        } else if (d > FAR) {
            mx = sign(dx), my = sign(dy);
        }
        // 1 px a think step, one axis at a time (sliding along walls).
        if (mx && enemy_box_free(actor_x[i] + mx, actor_y[i])) actor_x[i] += mx;
        if (my && enemy_box_free(actor_x[i], actor_y[i] + my)) actor_y[i] += my;
        if (!t) {
            actor_state[i] = GLOW;
            actor_timer[i] = GLOW_T;
        }
    } else if (!t) {  // GLOW: the pulse
        for (uint8_t j = 0; j < ACTOR_MAX; j++) {
            if (!actor_type[j] || actor_scripted[j] || !actor_hp[j] || !near(i, j, REACH)) continue;
            uint8_t full = full_hp(j), hp = actor_hp[j] + HEAL;
            if (actor_hp[j] < full) actor_hp[j] = hp < full ? hp : full;
        }
        actor_state[i] = HOVER;
        actor_timer[i] = PULSE_T;
    }
    actor_flash[i] = actor_state[i] == GLOW && (t & 4);
    actor_frame[i] = ((uint8_t)frame_count >> 4) & 1;
}
