#pragma bank 255
// Three camp kinds made from the enemy trials (docs/design/enemies.md), as actor
// kinds a squad can hold any number of (actor.h ACTOR_SPIDER, _BULWARK, _STALKER):
// - The wireweaver spider: a fast swarmer. Scuttles in, sidles, flashes, lunges.
// - The bulwark: a shield trooper. Walks at you behind a shield that stops shots
//   from in front; lowers it to fire a burst, or bashes you up close. Flank it.
// - The rift stalker: a shimmer your shots pass through while it circles to your
//   side. It flashes, dashes at you, and stands exposed after. Dodge, then punish.
// Their art is in the Color's second VRAM bank, after the guns' (SQUAD_VRAM).
#include <gb/gb.h>
#include <rand.h>
#include "squad.h"
#include "actor_internal.h"
#include "player.h"
#include "aim.h"
#include "state.h"
#include "wireweaver_art.inc"
#include "bulwark_art.inc"
#include "rift_stalker_art.inc"

uint8_t spider_frames[4 * 2], bulwark_frames[4 * 4], stalker_frames[4 * 2];
uint16_t squad_blocks;  // shots a bulwark's shield stopped (watchable)

// A think step covers two frames (actor_internal.h AI_STEP).
static uint8_t tick(uint8_t i) {
    return actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
}

static int8_t sign(int16_t d) {
    return d > 4 ? 1 : d < -4 ? -1 : 0;
}

static uint8_t step(uint8_t i, int8_t dx, int8_t dy) {
    uint8_t ok = 0;
    if (dx && enemy_box_free(actor_x[i] + dx, actor_y[i])) actor_x[i] += dx, ok = 1;
    if (dy && enemy_box_free(actor_x[i], actor_y[i] + dy)) actor_y[i] += dy, ok = 1;
    return ok;
}

// Along its velocity (two frames of it); 0 when it hit something.
static uint8_t fly(uint8_t i) {
    int16_t fx = (int16_t)actor_xs[i] + (actor_vx[i] << 1), fy = (int16_t)actor_ys[i] + (actor_vy[i] << 1);
    uint16_t x = actor_x[i] + (fx >> 8), y = actor_y[i] + (fy >> 8);
    if (!enemy_box_free(x, y)) return 0;
    actor_x[i] = x, actor_y[i] = y;
    actor_xs[i] = (uint8_t)fx, actor_ys[i] = (uint8_t)fy;
    return 1;
}

// A charge at you, `k` (1/256 px a frame) per pixel away, from no farther than 56.
static void aim_charge(uint8_t i, uint8_t k) {
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    while (dx > 56 || dx < -56 || dy > 56 || dy < -56) dx >>= 1, dy >>= 1;
    if (!dx && !dy) dy = 24;
    actor_vx[i] = dx * k, actor_vy[i] = dy * k;
    actor_xs[i] = actor_ys[i] = 0;
}

void squad_init(void) BANKED {
    VBK_REG = 1;
    set_sprite_data(SPIDER_VRAM, 8, weaver_art);
    set_sprite_data(BULWARK_VRAM, 8, bulwark_art);     // (its right-facing walk; left is flipped)
    set_sprite_data(STALKER_VRAM, 4, stalker_art + 2 * 64);  // (one frame of its shimmer)
    VBK_REG = 0;
    static const uint8_t sf[] = {0, S_BANK, 1, S_BANK,  2, S_BANK, 3, S_BANK};
    static const uint8_t bf[] = {0, S_BANK, 1, S_BANK,  2, S_BANK, 3, S_BANK,
                                 1, S_BANK | S_FLIPX, 0, S_BANK | S_FLIPX,  3, S_BANK | S_FLIPX, 2, S_BANK | S_FLIPX};
    static const uint8_t tf[] = {0, S_BANK, 1, S_BANK,  1, S_BANK | S_FLIPX, 0, S_BANK | S_FLIPX};
    for (uint8_t k = 0; k < sizeof(sf); k++) spider_frames[k] = sf[k], stalker_frames[k] = tf[k];
    for (uint8_t k = 0; k < sizeof(bf); k++) bulwark_frames[k] = bf[k];
    squad_blocks = 0;
}

void squad_spawn(uint8_t i) BANKED {
    uint8_t type = actor_type[i];
    actor_state[i] = 0;
    actor_try[i] = i & 1;
    if (type == ACTOR_SPIDER) actor_hp[i] = SPIDER_HP, actor_timer[i] = 24 + (i << 3);
    else if (type == ACTOR_BULWARK) actor_hp[i] = BULWARK_HP, actor_timer[i] = 50 + (i << 3), actor_try[i] = player_x < actor_x[i];  // (facing you)
    else actor_hp[i] = STALKER_HP, actor_timer[i] = 40 + (i << 3), actor_guard[i] = 1;
}

// ---- The spider ----
#define S_SCUTTLE 0
#define S_WARN 1
#define S_LUNGE 2
#define S_REST 3

static void spider(uint8_t i) {
    uint8_t t = tick(i), s = actor_state[i];
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    if (s == S_SCUTTLE) {
        // In fast, then sideways across you (1.5 px a frame), turning back at walls.
        uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        int8_t mx = sign(dx), my = sign(dy);
        if (ax < 40 && ay < 40) {
            int8_t side = actor_try[i] ? 1 : -1;
            if (ay > ax) mx = side, my = ay > 24 ? my : 0;
            else my = side, mx = ax > 24 ? mx : 0;
        }
        if (!step(i, mx * 3, my * 3)) actor_try[i] ^= 1;
        if (!t) {
            if (ax < 48 && ay < 48) {
                actor_state[i] = S_WARN, actor_timer[i] = 16;
                aim_charge(i, 28);
            } else actor_timer[i] = 12;
        }
    } else if (s == S_WARN) {
        if (!t) actor_state[i] = S_LUNGE, actor_timer[i] = 12;
    } else if (s == S_LUNGE) {
        if (!fly(i) || !t) actor_state[i] = S_REST, actor_timer[i] = 30;
    } else if (!t) {
        actor_state[i] = S_SCUTTLE, actor_timer[i] = 30 + (rand() & 15);
        actor_try[i] ^= 1;
    }
    actor_flash[i] = actor_state[i] == S_WARN && (t & 4);
    actor_frame[i] = ((uint8_t)frame_count >> 2) & 1;
}

// ---- The bulwark ----
#define B_ADVANCE 0
#define B_AIM 1
#define B_BURST 2
#define B_REST 3
#define B_WIND 4
#define B_BASH 5
// actor_try: 0 facing right, 1 left. The shield is up while it advances or winds up.

static uint8_t shield_up(uint8_t i) {
    return actor_state[i] == B_ADVANCE || actor_state[i] == B_WIND;
}

// Is (x, y) in front of its shield (within 45 degrees of where it faces)?
static uint8_t in_front(uint8_t i, int16_t dx, int16_t dy) {
    int16_t along = actor_try[i] ? -dx : dx;
    if (dy < 0) dy = -dy;
    return along > 0 && along >= dy;
}

uint8_t squad_blocks_melee(uint8_t i, uint16_t x, uint16_t y) BANKED {
    return shield_up(i) && in_front(i, (int16_t)(x - actor_x[i]), (int16_t)(y - actor_y[i]));
}

// Shots coming at the shield's face stop on it; from behind or the side, they hit.
static void block(uint8_t i) {
    Bullet *s = shots;
    for (uint8_t n = 0; n < SHOT_MAX; n++, s++) {
        if (!s->alive) continue;
        int16_t dx = (int16_t)(s->x - actor_x[i]) - 4, dy = (int16_t)(s->y - actor_y[i]);
        int16_t along = actor_try[i] ? -dx : dx, v = actor_try[i] ? -s->vx : s->vx;
        // Coming at it (v < 0), and at its face before the next think step.
        if (along < 0 || along > 48 || v >= 0 || along + (v >> 7) > 12 || dy < -14 || dy > 14) continue;
        s->alive = 0;
        shot_count--;
        squad_blocks++;
        actor_flash[i] = 1;
    }
}

static void bulwark(uint8_t i) {
    uint8_t t = tick(i), s = actor_state[i];
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t moving = 0;
    if (s == B_ADVANCE) {
        // Turns to you slowly (every so often), and walks at you at half a pixel a frame.
        if (!(t & 15)) actor_try[i] = dx < 0;
        if ((uint8_t)frame_count & 2) moving = step(i, sign(dx), sign(dy));
        if (!t) {
            actor_try[i] = dx < 0;
            if (ax < 28 && ay < 28) actor_state[i] = B_WIND, actor_timer[i] = 22, aim_charge(i, 24);
            else if (ax < 112 && ay < 96) {
                actor_state[i] = B_AIM, actor_timer[i] = 28;
                actor_vx[i] = (ax << 1) > ay ? (dx > 0 ? 1 : -1) : 0;
                actor_vy[i] = (ay << 1) > ax ? (dy > 0 ? 1 : -1) : 0;
            } else actor_timer[i] = 30;
        }
    } else if (s == B_AIM) {
        if (!t) actor_state[i] = B_BURST, actor_timer[i] = 24;
    } else if (s == B_BURST) {
        if (!(t & 7)) fire_bullet(i, (int8_t)actor_vx[i], (int8_t)actor_vy[i]);
        if (!t) actor_state[i] = B_REST, actor_timer[i] = 40;
    } else if (s == B_WIND) {
        if (!t) actor_state[i] = B_BASH, actor_timer[i] = 12;
    } else if (s == B_BASH) {
        moving = 1;
        if (!fly(i) || !t) actor_state[i] = B_REST, actor_timer[i] = 40;
    } else if (!t) {
        actor_state[i] = B_ADVANCE, actor_timer[i] = 54 + (rand() & 15);
    }
    actor_flash[i] = 0;
    if (shield_up(i)) block(i);
    s = actor_state[i];
    if (s == B_AIM || s == B_WIND) actor_flash[i] = (t & 4) != 0;
    actor_frame[i] = (actor_try[i] << 1) | (moving ? ((uint8_t)frame_count >> 3) & 1 : 0);
}

// ---- The rift stalker ----
#define T_STALK 0
#define T_WARN 1
#define T_DASH 2
#define T_RECOVER 3
// actor_try: the side of you it's circling to.

static void stalker(uint8_t i) {
    uint8_t t = tick(i), s = actor_state[i];
    if (s == T_STALK) {
        int16_t tx = (int16_t)player_x + (actor_try[i] ? 32 : -32), ty = (int16_t)player_y - 20;
        step(i, sign(tx - (int16_t)actor_x[i]) << 1, sign(ty - (int16_t)actor_y[i]) << 1);
        if (!t) actor_state[i] = T_WARN, actor_timer[i] = 26, aim_charge(i, 20);
    } else if (s == T_WARN) {
        if (!t) actor_state[i] = T_DASH, actor_timer[i] = 16;
    } else if (s == T_DASH) {
        if (!fly(i) || !t) actor_state[i] = T_RECOVER, actor_timer[i] = 48;
    } else if (!t) {
        actor_state[i] = T_STALK, actor_timer[i] = 44 + (rand() & 15);
        actor_try[i] ^= 1;
    }
    s = actor_state[i];
    // Shots pass through the shimmer (it flickers); solid from the warning on.
    actor_guard[i] = s == T_STALK;
    actor_flash[i] = s == T_WARN && (t & 4);
    actor_frame[i] = s == T_STALK ? ((uint8_t)frame_count >> 1) & 1 : ((uint8_t)frame_count >> 4) & 1;
}

void squad_update(uint8_t i) BANKED {
    uint8_t type = actor_type[i];
    if (type == ACTOR_SPIDER) spider(i);
    else if (type == ACTOR_BULWARK) bulwark(i);
    else stalker(i);
}
