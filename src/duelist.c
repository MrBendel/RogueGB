#pragma bank 255
// The duelist (actor.h ACTOR_DUELIST): the descent's merc (docs/design/runs.md,
// "Enemies: a roster by role"). It keeps to mid range, sidestepping across your
// line of fire, then stops, aims (flashing: the warning), fires a burst of three
// at where you were, and stands reloading after it: the moment to hit it. Shot
// while it strafes, it dashes aside once. Any number can run at once, like the
// hound; the trials' Mercenary (mercenary.c) is the one-at-a-time version.
#include <gb/gb.h>
#include <rand.h>
#include "duelist.h"
#include "actor_internal.h"
#include "player.h"
#include "state.h"

#define STRAFE 0
#define AIM 1
#define BURST 2
#define RELOAD 3
#define DODGE 4
#define NEAR 44        // px: closer than this, it backs off ...
#define FAR 80         // ... farther, it closes in; between, it sidesteps
#define REACH 112      // px: it opens fire from no farther than this
#define AIM_T 20       // frames of warning
#define BURST_T 18     // frames firing: a shot at 16, 8 and 0 left
#define RELOAD_T 40    // frames standing after a burst
#define DODGE_T 8      // frames dashing aside, 2 px a frame
// actor_vx, actor_vy: the burst's direction, locked as it aims. actor_try: bit 0
// the way it sidesteps, bit 1 set once it has dodged this strafe.

uint8_t duelist_frames[DUELIST_FRAMES * 4];

// Standing (aiming) and a step, facing right (3 outline, 2 armour, 1 the visor and
// the rifle's barrel: the shooters' palette, world.c).
static const char art[2][16][17] = {
 {"0000000000000000","0000003333000000","0000032222300000","0000322221130000",
  "0000322211113000","0000322222230000","0000033223300000","0000322222233330",
  "0003222223311113","0003222222333330","0000322222300000","0000033333000000",
  "0000032003200000","0000032003200000","0000333003330000","0000000000000000"},
 {"0000000000000000","0000003333000000","0000032222300000","0000322221130000",
  "0000322211113000","0000322222230000","0000033223300000","0000322222233330",
  "0003222223311113","0003222222333330","0000322222300000","0000033333000000",
  "0000320000320000","0003200000032000","0033300000033300","0000000000000000"}};

// Its 4 columns: tiles v, v + 2, v + 4 and v2 (sprite VRAM has no 8 in a row
// free: actor_internal.h).
void duelist_init(uint8_t v, uint8_t v2) BANKED {
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
            uint8_t col = f * 2 + half;
            set_sprite_data(col == 3 ? v2 : v + col * 2, 2, data);
        }
    // Frames (columns from v, 2 tiles each; flags): standing, a step; flipped.
    uint8_t c3 = (uint8_t)(v2 - v) >> 1;
    duelist_frames[0] = 0;  duelist_frames[1] = 0;       duelist_frames[2] = 1;  duelist_frames[3] = 0;
    duelist_frames[4] = 2;  duelist_frames[5] = 0;       duelist_frames[6] = c3; duelist_frames[7] = 0;
    duelist_frames[8] = 1;  duelist_frames[9] = S_FLIPX; duelist_frames[10] = 0; duelist_frames[11] = S_FLIPX;
    duelist_frames[12] = c3; duelist_frames[13] = S_FLIPX; duelist_frames[14] = 2; duelist_frames[15] = S_FLIPX;
}

// One axis at a time, so it slides along walls; 0 if it couldn't move at all.
static uint8_t step(uint8_t i, int8_t dx, int8_t dy) {
    uint8_t ok = 0;
    if (dx && enemy_box_free(actor_x[i] + dx, actor_y[i])) actor_x[i] += dx, ok = 1;
    if (dy && enemy_box_free(actor_x[i], actor_y[i] + dy)) actor_y[i] += dy, ok = 1;
    return ok;
}

static int8_t sign(int16_t d) {
    return d > 4 ? 1 : d < -4 ? -1 : 0;
}

void duelist_update(uint8_t i) BANKED {
    uint8_t t = actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t state = actor_state[i], moving = 0;
    int8_t way = actor_try[i] & 1 ? -1 : 1;
    // Across the line between you: its way by actor_try.
    int8_t cx = ax > ay ? 0 : -sign(dy) * way, cy = ax > ay ? sign(dx) * way : 0;
    if (!cx && !cy) cy = way;
    if (state == STRAFE) {
        uint16_t d = ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);  // (octagonal)
        if (actor_hurt[i] && !(actor_try[i] & 2)) {
            // Shot: a dash aside, once a strafe.
            actor_try[i] |= 2;
            actor_state[i] = DODGE;
            actor_timer[i] = DODGE_T;
        } else {
            int8_t mx = cx, my = cy;
            if (d > FAR) mx = sign(dx), my = sign(dy);
            else if (d < NEAR) mx = -sign(dx), my = -sign(dy);
            // 1 px a frame (a think step is 2 frames), the same diagonally.
            if (!step(i, mx * 2, my * 2)) actor_try[i] ^= 1;  // blocked: the other way
            moving = 1;
            if (!t) {
                if (d < REACH) {
                    // The warning; the burst is aimed now, in 8 directions
                    // (diagonal when the axes are within 2:1), at where you are.
                    actor_state[i] = AIM;
                    actor_timer[i] = AIM_T;
                    actor_vx[i] = (ax << 1) > ay ? (dx > 0 ? 1 : -1) : 0;
                    actor_vy[i] = (ay << 1) > ax ? (dy > 0 ? 1 : -1) : 0;
                } else {
                    actor_timer[i] = 20;
                }
            }
        }
        actor_face[i] = dx < 0;
    } else if (state == AIM) {
        if (!t) {
            actor_state[i] = BURST;
            actor_timer[i] = BURST_T;
        }
    } else if (state == BURST) {
        if (!(t & 7)) fire_bullet(i, (int8_t)actor_vx[i], (int8_t)actor_vy[i]);
        if (!t) {
            actor_state[i] = RELOAD;
            actor_timer[i] = RELOAD_T;
        }
    } else if (state == RELOAD) {
        if (!t) {
            actor_state[i] = STRAFE;
            actor_timer[i] = 50 + (rand() & 31);
            actor_try[i] = (actor_try[i] ^ 1) & 1;  // the other way, and a dodge again
        }
    } else {  // DODGE
        moving = 1;
        if (!step(i, cx * 4, cy * 4) || !t) {
            actor_state[i] = STRAFE;
            actor_timer[i] = 16;  // (it aims again soon)
        }
    }
    actor_flash[i] = actor_state[i] == AIM && (t & 4);
    // Stepping while it moves; standing to aim, fire and reload.
    uint8_t f = moving ? ((uint8_t)frame_count >> 3) & 1 : 0;
    actor_frame[i] = f + (actor_face[i] ? 2 : 0);
}
