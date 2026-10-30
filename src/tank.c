#pragma bank 255
// The loader (actor.h ACTOR_TANK): the descent's tank (docs/design/runs.md,
// "Enemies: a roster by role"). An armoured cargo loader that rolls slowly at you
// and soaks damage; shots don't push it. Close in, it stops and flashes, then
// slams the deck: a ring of 8 peas. It stands recovering after: the moment to
// pour it on. Keep your distance and it's a slow target; get greedy and it slams.
#include <gb/gb.h>
#include "tank.h"
#include "actor_internal.h"
#include "player.h"
#include "state.h"

#define ROLL 0
#define WINDUP 1
#define RECOVER 2
#define NEAR 40        // px: closer than this, it slams (when ready)
#define WINDUP_T 24    // frames of warning
#define RECOVER_T 44   // frames standing after a slam
#define READY_T 30     // frames rolling before it can slam again

uint8_t tank_frames[TANK_FRAMES * 4];

// Treads still and rolling, facing right (3 outline, 2 armour, 1 the visor and
// the tread lights: the creatures' palette, world.c).
static const char art[2][16][17] = {
 {"0000000000000000","0000000000000000","0000000000000000","0000000000000000",
  "0003333333333000","0032222222222300","0032211111122300","0032222222222333",
  "0032222222222323","0033333333333323","0322222222222333","0322222222222300",
  "0333333333333330","3121212121212123","0333333333333330","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000000000000","0000000000000000",
  "0003333333333000","0032222222222300","0032211111122300","0032222222222333",
  "0032222222222323","0033333333333323","0322222222222333","0322222222222300",
  "0333333333333330","3212121212121213","0333333333333330","0000000000000000"}};

void tank_init(uint8_t v) BANKED {
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
    // Frames (columns from v, 2 tiles each; flags): still, rolling; flipped.
    static const uint8_t frames[TANK_FRAMES * 4] = {
        0, 0, 1, 0,  2, 0, 3, 0,  1, S_FLIPX, 0, S_FLIPX,  3, S_FLIPX, 2, S_FLIPX};
    for (uint8_t k = 0; k < sizeof(frames); k++) tank_frames[k] = frames[k];
}

static int8_t sign(int16_t d) {
    return d > 2 ? 1 : d < -2 ? -1 : 0;
}

void tank_update(uint8_t i) BANKED {
    uint8_t t = actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t state = actor_state[i], moving = 0;
    if (state == ROLL) {
        if (!t && ax < NEAR && ay < NEAR) {
            actor_state[i] = WINDUP;
            actor_timer[i] = WINDUP_T;
        } else {
            // Half a px a frame (1 a think step), one axis at a time so it slides
            // along walls.
            int8_t mx = sign(dx), my = sign(dy);
            if (mx && enemy_box_free(actor_x[i] + mx, actor_y[i])) actor_x[i] += mx;
            if (my && enemy_box_free(actor_x[i], actor_y[i] + my)) actor_y[i] += my;
            moving = 1;
            if (mx) actor_face[i] = mx < 0;
        }
    } else if (state == WINDUP) {
        if (!t) {
            static const int8_t rx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            static const int8_t ry[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
            for (uint8_t k = 0; k < 8; k++) fire_bullet(i, rx[k], ry[k]);
            actor_state[i] = RECOVER;
            actor_timer[i] = RECOVER_T;
        }
    } else if (!t) {  // RECOVER
        actor_state[i] = ROLL;
        actor_timer[i] = READY_T;
    }
    actor_flash[i] = actor_state[i] == WINDUP && (t & 4);
    uint8_t f = moving ? ((uint8_t)frame_count >> 3) & 1 : 0;
    actor_frame[i] = f + (actor_face[i] ? 2 : 0);
}
