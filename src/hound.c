#pragma bank 255
// The hound (actor.h ACTOR_HOUND): the descent's flanker (docs/design/runs.md,
// "Enemies: a roster by role"). It closes to mid range and circles you, then
// crouches (flashing: the warning), lunges at where you were, and rests after
// it: the moment to hit it. Any number can run at once, like the slime and the
// peashooter. Banked: actors_update calls it on each hound's think step.
// The silhouette is the Acid Wolf trial's running stride (acid_wolf.c).
#include <gb/gb.h>
#include <rand.h>
#include "hound.h"
#include "actor_internal.h"
#include "player.h"
#include "state.h"

#define PROWL 0
#define CROUCH 1
#define LUNGE 2
#define REST 3
#define NEAR 28        // px: closer than this, it backs off ...
#define FAR 52         // ... farther, it closes in; between, it circles
#define REACH 72       // px: it lunges from no farther than this
#define CROUCH_T 16    // frames of warning
#define LUNGE_T 16     // frames lunging, 3 px a frame (2 on a diagonal)
#define REST_T 30      // frames standing after a lunge
// actor_try: bit 0 the way it circles; actor_face: left (1) or right (0).

uint8_t hound_frames[HOUND_FRAMES * 4];

// Running stride A and B, facing right (3 outline, 2 body, 1 the eye and throat:
// the enemies' palette, actor.c).
static const char art[2][16][17] = {
 {"0000000000000000","0000000030030000","0000000032330000","0000000032233000",
  "0300000332133300","0323333222231233","0032222222233330","0003222222232300",
  "0003222222233000","0000322222330000","0000332333230000","0000323000323000",
  "0003230000032300","0003300000033300","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000030030000","0000000032330000",
  "0000000032233000","0333000332133300","0032333222231233","0003222222233330",
  "0003222222232300","0000322222333000","0000332333230000","0003230000032300",
  "0032300000003230","0033300000003330","0000000000000000","0000000000000000"}};

void hound_init(uint8_t v) BANKED {
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
            set_sprite_data(v + f * 4 + half * 2, 2, data);
        }
    // Frames (columns of 2 tiles, flags): stride A, B facing right; A, B flipped.
    static const uint8_t frames[HOUND_FRAMES * 4] = {
        0, 0, 1, 0,  2, 0, 3, 0,
        1, S_FLIPX, 0, S_FLIPX,  3, S_FLIPX, 2, S_FLIPX,
    };
    for (uint8_t i = 0; i < sizeof(frames); i++) hound_frames[i] = frames[i];
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

void hound_update(uint8_t i) BANKED {
    uint8_t t = actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint8_t state = actor_state[i], moving = 0;
    if (state == PROWL) {
        uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        uint16_t d = ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);  // (octagonal)
        int8_t sx = sign(dx), sy = sign(dy), mx, my;
        if (d > FAR) mx = sx, my = sy;
        else if (d < NEAR) mx = -sx, my = -sy;
        else {
            // Around you: across the line between you, its way by actor_try.
            int8_t way = actor_try[i] & 1 ? -1 : 1;
            if (ax > ay) mx = 0, my = sx * way;
            else mx = -sy * way, my = 0;
            if (!mx && !my) my = way;
        }
        // 1.5 px a frame (a think step is 2 frames), a little less diagonally.
        int8_t s = mx && my ? 2 : 3;
        if (!step(i, mx * s, my * s)) actor_try[i] ^= 1;  // blocked: circle the other way
        moving = 1;
        actor_face[i] = dx < 0;
        if (!t) {
            if (d < REACH) {
                // The warning; the lunge is aimed now, at where you are.
                actor_state[i] = CROUCH;
                actor_timer[i] = CROUCH_T;
                actor_vx[i] = sx;
                actor_vy[i] = sy;
            } else {
                actor_timer[i] = 20;
            }
        }
    } else if (state == CROUCH) {
        if (!t) {
            actor_state[i] = LUNGE;
            actor_timer[i] = LUNGE_T;
        }
    } else if (state == LUNGE) {
        int8_t sx = (int8_t)actor_vx[i], sy = (int8_t)actor_vy[i], s = sx && sy ? 4 : 6;
        moving = 1;
        if (sx) actor_face[i] = sx < 0;
        if (!step(i, sx * s, sy * s) || !t) {
            actor_state[i] = REST;
            actor_timer[i] = REST_T;
        }
    } else if (!t) {
        actor_state[i] = PROWL;
        actor_timer[i] = 40 + (rand() & 31);
        actor_try[i] ^= 1;  // the other way round next time
    }
    actor_flash[i] = actor_state[i] == CROUCH && (t & 4);
    // Running strides while it moves; crouched, stride B; resting, stride A.
    uint8_t f = actor_state[i] == CROUCH ? 1 : moving ? ((uint8_t)frame_count >> 3) & 1 : 0;
    actor_frame[i] = f + (actor_face[i] ? 2 : 0);
}
