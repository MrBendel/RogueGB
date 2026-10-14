#pragma bank 255
// Trial enemy families: compact silhouettes and 30 Hz state machines.
#include "enemy_family.h"
#include "actor_internal.h"
#include "player.h"
#include "state.h"
#include "aim.h"
#include "gfx.h"
#include "run.h"

#define FAMILY_VRAM 96
// Two 16x16 poses per family occupy tiles 96..127, replacing chest/boss art
// only inside these trials. Body silhouettes remain recognizable without color.
static const char art[5][16][17] = {
 {"0000033333000000","0000332223300000","0000321212300000","0000332223300000",
  "0000033233000000","0033332223330000","0322222222233000","0323332233323000",
  "0330032230032300","0000032230003300","0000033330000000","0000032230000000",
  "0000032033000000","0000332003300000","0000323002300000","0000333003300000"},
 {"0000333333000000","0000321123000000","0000332233000000","0033332233333000",
  "0332222222223300","0322333333222300","0322311113222300","0332333333223300",
  "0033322223333000","0000322223000000","0000333333000000","0000323323000000",
  "0000320032300000","0000320032300000","0003330033330000","0003330033330000"},
 {"0000033333000000","0000332223300000","0000321112300000","0000333333300000",
  "0000032223000000","0003333333330000","0032221122223000","0032333333323000",
  "0032232223223330","0003322223331130","0000333333033330","0000323323000000",
  "0000320032300000","0000320032300000","0003330033330000","0003330033330000"},
 {"0000003333000000","0000033223300000","0033333333333300","0032222222222300",
  "0032333333332300","0032311111132300","0032312222132300","0032312222132300",
  "0032311111132300","0032333333332300","0032222222222300","0033333333333300",
  "0003220000223000","0003330000333000","0000000000000000","0000000000000000"},
 {"0000000000000000","0000000000000000","0000000000300300","0000000000333330",
  "0000033333321230","0333332222222233","0322223332221113","0332222222233333",
  "0032222222233000","0033332233330000","0032032032030000","0032032032030000",
  "0333033033030000","0000000000000000","0000000000000000","0000000000000000"}
};
static const uint8_t hp[5] = {3, 7, 5, 8, 4};
static const int8_t dx8[8] = {0,1,1,1,0,-1,-1,-1};
static const int8_t dy8[8] = {-1,-1,0,1,1,1,0,-1};

void enemy_family_load(void) BANKED {
    uint8_t tiles[64];
    for (uint8_t k = 0; k < 4; k++) {
        // The hound-only trial replaces the unused Husk art, keeping the same
        // 32-tile budget. Faction Mix continues to use the four original bodies.
        uint8_t source = enemy_trial_kind == 8 && k == 0 ? 4 : k;
        for (uint8_t pose = 0; pose < 2; pose++) {
            for (uint8_t col = 0; col < 2; col++) {
                for (uint8_t y = 0; y < 16; y++) {
                    uint8_t lo = 0, hi = 0;
                    for (uint8_t x = 0; x < 8; x++) {
                        uint8_t xx = (col << 3) + x;
                        uint8_t yy = pose && k < 3 && y >= 12 ? 27 - y : y;
                        uint8_t c = art[source][yy][xx] - '0';
                        if (pose && k == 3 && y >= 5 && y <= 8 && xx >= 6 && xx <= 9) c = 1;
                        lo = (lo << 1) | (c & 1); hi = (hi << 1) | (c >> 1);
                    }
                    uint8_t at = (col << 5) + (y << 1);
                    tiles[at] = lo; tiles[at + 1] = hi;
                }
            }
            set_sprite_data(FAMILY_VRAM + (k << 3) + (pose << 2), 4, tiles);
        }
    }
}

static void draw(uint8_t i, uint8_t family) {
    uint8_t pose = family == FAMILY_BASTION ? !actor_guard[i] : ((uint8_t)frame_count >> 4) & 1;
    uint8_t art_slot = family == FAMILY_HOUND ? 0 : family - FAMILY_HUSK;
    actor_t0[i] = FAMILY_VRAM + (art_slot << 3) + (pose << 2);
    actor_t1[i] = actor_t0[i] + 2;
    actor_p0[i] = actor_p1[i] = family == FAMILY_HUSK ? SPR_PAL_SLIME : SPR_PAL_SHOOTER;
}

static void guarded(uint8_t i, uint8_t on) {
    actor_guard[i] = on;
    if (on) {
        aim_score[i] = AIM_NONE;
        if (aim_target == i) aim_target = AIM_NONE;
    }
}

void enemy_family_spawn(uint8_t family, uint8_t x, uint8_t y) BANKED {
    // Reuse the ordinary collision/projectile actor; actor_scripted identifies
    // the trial family and bypasses peashooter AI and frame-cache updates.
    uint8_t i = actor_spawn(ACTOR_PEASHOOTER, x, y);
    if (i == 0xFF) return;
    actor_scripted[i] = family;
    actor_hp[i] = hp[family - FAMILY_HUSK];
    actor_state[i] = 0; actor_timer[i] = 60;
    if (family == FAMILY_BASTION) { guarded(i, 1); actor_timer[i] = 90; }
    if (family == FAMILY_HOUND) actor_timer[i] = 60 + (i << 4);
    draw(i, family);
}

static uint8_t move(uint8_t i, int8_t dx, int8_t dy) {
    uint16_t x = actor_x[i] + dx, y = actor_y[i] + dy;
    if (!enemy_box_free(x, y)) return 0;
    actor_x[i] = x; actor_y[i] = y;
    return 1;
}

static void lock_direction(uint8_t i) {
    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    actor_face[i] = ax > ay ? (dx > 0 ? 2 : 6) : (dy > 0 ? 4 : 0);
}

void enemy_family_update(void) BANKED {
    for (uint8_t i = (uint8_t)frame_count & 1; i < ACTOR_MAX; i += 2) {
        uint8_t family = actor_scripted[i];
        if (!actor_type[i] || !actor_hp[i] || family < FAMILY_HUSK) continue;
        uint8_t t = actor_timer[i] = actor_timer[i] > 2 ? actor_timer[i] - 2 : 0;
        actor_flash[i] = 0;
        if (family == FAMILY_HUSK) {
            // One pixel every four frames, cardinal steps: swarm pressure.
            if (!((uint8_t)frame_count & 2)) {
                lock_direction(i);
                uint8_t d = actor_face[i];
                if (!move(i, dx8[d], dy8[d])) {
                    int8_t sx = player_x > actor_x[i] ? 1 : -1;
                    int8_t sy = player_y > actor_y[i] ? 1 : -1;
                    move(i, dx8[d] ? 0 : sx, dy8[d] ? 0 : sy);
                }
            }
        } else if (family == FAMILY_RAGEBOUND) {
            if (actor_state[i] == 0 && !t) {
                lock_direction(i); actor_state[i] = 1; actor_timer[i] = 36;
            } else if (actor_state[i] == 1) {
                actor_flash[i] = (t & 4) != 0;
                if (!t) { actor_state[i] = 2; actor_timer[i] = 40; }
            } else if (actor_state[i] == 2) {
                uint8_t d = actor_face[i];
                if (!t || !move(i, dx8[d] * 3, dy8[d] * 3)) {
                    actor_state[i] = 0; actor_timer[i] = 70;
                }
            }
        } else if (family == FAMILY_CONTRACTOR) {
            if (actor_state[i] == 0) {
                // Reposition during reload: retreat from close players, otherwise strafe.
                if (!(t & 6)) {
                    lock_direction(i);
                    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
                    uint8_t d = actor_face[i];
                    if (dx > -40 && dx < 40 && dy > -40 && dy < 40) move(i, -dx8[d], -dy8[d]);
                    else if (!move(i, actor_try[i] ? -1 : 1, 0)) actor_try[i] ^= 1;
                }
                if (!t) { lock_direction(i); actor_state[i] = 1; actor_timer[i] = 30; }
            } else if (actor_state[i] == 1) {
                actor_flash[i] = (t & 4) != 0;
                if (!t) { actor_state[i] = 2; actor_timer[i] = 24; actor_try[i] = 0; }
            } else {
                if (t == 22 || t == 12 || t == 2) {
                    uint8_t d = actor_face[i]; fire_bullet(i, dx8[d], dy8[d]);
                }
                if (!t) { actor_state[i] = 0; actor_timer[i] = 80; }
            }
        } else if (family == FAMILY_HOUND) {
            if (actor_state[i] == 0) {
                lock_direction(i);
                uint8_t d = actor_face[i];
                if (t && !(t & 2)) move(i, dx8[d], dy8[d]);
                if (!t) {
                    int16_t dx = (int16_t)(player_x - actor_x[i]), dy = (int16_t)(player_y - actor_y[i]);
                    int8_t sx = dx > 8 ? 1 : dx < -8 ? -1 : 0;
                    int8_t sy = dy > 8 ? 1 : dy < -8 ? -1 : 0;
                    if (sx || sy) {
                        for (uint8_t dir = 0; dir < 8; dir++)
                            if (dx8[dir] == sx && dy8[dir] == sy) actor_face[i] = dir;
                    }
                    actor_state[i] = 1; actor_timer[i] = 24;
                }
            } else if (actor_state[i] == 1) {
                actor_flash[i] = (t & 4) != 0;
                if (!t) { actor_state[i] = 2; actor_timer[i] = 24; }
            } else if (actor_state[i] == 2) {
                uint8_t d = actor_face[i];
                if (!t || !move(i, dx8[d] * 2, dy8[d] * 2)) {
                    actor_state[i] = 3; actor_timer[i] = 42;
                }
            } else if (!t) {
                actor_state[i] = 0; actor_timer[i] = 60;
            }
        } else if (family == FAMILY_BASTION) {
            if (actor_state[i] == 0) {
                actor_flash[i] = t <= 24 && (t & 4);
                if (t == 24) lock_direction(i);
                if (!t) {
                    guarded(i, 0);
                    uint8_t d = actor_face[i];
                    fire_bullet(i, dx8[d], dy8[d]);
                    fire_bullet(i, dx8[(d + 1) & 7], dy8[(d + 1) & 7]);
                    fire_bullet(i, dx8[(d + 7) & 7], dy8[(d + 7) & 7]);
                    actor_state[i] = 1; actor_timer[i] = 72;
                }
            } else if (!t) {
                guarded(i, 1); actor_state[i] = 0; actor_timer[i] = 90;
            }
        }
        draw(i, family);
    }
}
