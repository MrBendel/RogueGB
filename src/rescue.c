#pragma bank 255
// Rescue jobs (rescue.h). An area's captive is held in one of its field maps (from
// the area's seed), beside its first camp; they're a talker (talk.h TALK_PERSON),
// so A beside them comes here. Nothing of this is saved but the job: leave, die
// or go up to the miniboss's arena mid-rescue and they're back where they were held.
// Banked: per frame only while someone follows you or you hold a camp.
#include <gb/gb.h>
#include <rand.h>
#include "rescue.h"
#include "mission.h"
#include "talk.h"
#include "descent.h"
#include "level.h"
#include "room.h"
#include "player.h"
#include "camera.h"
#include "actor.h"
#include "world.h"
#include "save.h"
#include "hud.h"
#include "dialog.h"
#include "dialogue.h"
#include "gfx.h"
#include "sfx.h"

extern uint16_t area_seed_base;  // descent.c
extern uint8_t bullet_count;     // actor.c

uint8_t rescue_slot = 0xFF;
uint8_t rescue_map = 0xFF;
uint8_t rescue_state;
uint16_t rescue_x, rescue_y;
uint8_t rescue_hp;
uint16_t rescue_timer;
uint8_t rescue_hurt;   // frames a follower can't be hurt again (and flashes)
uint8_t rescue_spawn;  // RS_HOLD: frames to the next enemy
uint8_t rescue_tick;   // ... and to the next second (the time on the HUD)

// Each one's lines: freed (they head home), following, holding, and safe.
static const uint8_t say_freed[PEOPLE] = {DLG_FREED_TESS, DLG_FREED_OKAFOR, DLG_FREED_NELL, DLG_FREED_PIP};
static const uint8_t say_follow[PEOPLE] = {DLG_FOLLOW_TESS, DLG_FOLLOW_OKAFOR, DLG_FOLLOW_NELL, DLG_FOLLOW_PIP};
static const uint8_t say_hold[PEOPLE] = {DLG_HOLD_TESS, DLG_HOLD_OKAFOR, DLG_HOLD_NELL, DLG_HOLD_PIP};

static uint8_t who(void) {
    return RESCUE_WHO(profile.mis_target[rescue_slot]);
}

void rescue_area(void) BANKED {
    rescue_slot = rescue_map = 0xFF;
    rescue_state = RS_NONE;
    for (uint8_t s = 0; s < MISSION_SLOTS; s++)
        if (profile.mis_kind[s] == MIS_RESCUE && profile.mis_need[s] == layer_depth && !profile.mis_have[s]) {
            rescue_slot = s;
            break;
        }
    if (rescue_slot == 0xFF) return;
    // One of the area's field maps (never its entrance or its exit), the same each time.
    uint8_t fields = 0;
    for (uint8_t m = 0; m < AREA_CELLS; m++) fields += area_type[m] == MAP_FIELD;
    if (!fields) return;
    uint8_t k = (uint8_t)((area_seed_base >> 3) + layer_depth * 5) % fields;
    for (uint8_t m = 0; m < AREA_CELLS; m++)
        if (area_type[m] == MAP_FIELD && !k--) {
            rescue_map = m;
            break;
        }
}

// Job done: they're safe (or on their way home). The board pays when it's turned in.
static void rescued(void) {
    mission_rescued(rescue_slot);
    dlg_start(say_freed[who()]);
    rescue_slot = 0xFF;
    rescue_state = RS_NONE;
}

// The captive's talker (the last added on their map).
static void captive_gone(void) {
    for (uint8_t i = 0; i < talk_count; i++)
        if (talk_script[i] == TALK_DO_FREE) talk_remove(i);
}

// Where they're held: the block of their camp's last enemy slot (or the map's
// middle without a camp), or the nearest floor to it.
static void hold_block(uint8_t *bx, uint8_t *by) {
    uint16_t x = (uint16_t)(room_w >> 1) << 4, y = (uint16_t)(room_h >> 1) << 4;
    if (level_camps) {
        x = camp_slot_x[0][camp_slots[0] - 1] + 8;
        y = camp_slot_y[0][camp_slots[0] - 1] + 8;
    }
    *bx = x >> 4;
    *by = y >> 4;
    for (int8_t r = 0; r < 3; r++)
        for (int8_t dy = -r; dy <= r; dy++)
            for (int8_t dx = -r; dx <= r; dx++) {
                uint8_t cx = *bx + dx, cy = *by + dy;
                if (room_map[((uint16_t)cy << 5) | cx] == BLK_FLOOR) {
                    *bx = cx;
                    *by = cy;
                    return;
                }
            }
}

void rescue_enter(uint8_t map) BANKED {
    if (rescue_slot == 0xFF) return;
    if (rescue_state == RS_FOLLOW) {
        if (map == area_start) {  // out: safe
            rescued();
            return;
        }
        talk_person_load(who());
        rescue_x = player_x;
        rescue_y = player_y;
        return;
    }
    rescue_state = RS_NONE;  // (a hold left unfinished starts again)
    if (map != rescue_map) return;
    uint8_t bx, by;
    hold_block(&bx, &by);
    talk_add(bx, by, TALK_PERSON + who(), TALK_DO_FREE);
    talk_init();
    rescue_state = RS_HELD;
}

void rescue_free(void) BANKED {
    if (rescue_state == RS_HOLD) {
        char h[8] = "HOLD ON";
        hud_say(h);
        return;
    }
    if (level_camps && camp_state[0] != CAMP_CLEARED) {
        char g[8] = "GUARDED";
        hud_say(g);
        return;
    }
    uint8_t how = RESCUE_HOW(profile.mis_target[rescue_slot]), p = who();
    rescue_x = (uint16_t)talk_bx[talk_near] << 4;
    rescue_y = (uint16_t)talk_by[talk_near] << 4;
    if (how == RESCUE_ESCORT) {
        captive_gone();
        rescue_state = RS_FOLLOW;
        rescue_hp = ESCORT_HP;
        rescue_hurt = 0;
        dlg_start(say_follow[p]);
    } else if (how == RESCUE_HOLD) {
        rescue_state = RS_HOLD;
        rescue_timer = HOLD_T + 1;
        rescue_tick = 1;  // (the time shows at once)
        rescue_spawn = HOLD_SPAWN >> 1;
        dlg_start(say_hold[p]);
    } else {
        captive_gone();
        rescued();
    }
}

// RS_FOLLOW: a step toward the player (not into walls), and what hits them.
static void follow(void) {
    int16_t dx = (int16_t)(player_x - rescue_x), dy = (int16_t)(player_y + 4 - rescue_y);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax + ay > 160) {  // stuck behind something: catch up
        rescue_x = player_x;
        rescue_y = player_y;
    } else if (ax + ay > 20) {
        uint8_t v = ax + ay > 48 ? 2 : 1;
        if (ax > 4) {
            uint16_t nx = dx < 0 ? rescue_x - v : rescue_x + v;
            if (room_block_at(nx + (dx < 0 ? 3 : 12), rescue_y + 12) == BLK_FLOOR) rescue_x = nx;
        }
        if (ay > 4) {
            uint16_t ny = dy < 0 ? rescue_y - v : rescue_y + v;
            if (room_block_at(rescue_x + 8, ny + (dy < 0 ? 8 : 15)) == BLK_FLOOR) rescue_y = ny;
        }
    }
    if (rescue_hurt) {
        rescue_hurt--;
        return;
    }
    // Enemy bullets (the middle of their 8x16 sprite in the follower's body) ...
    uint8_t hit = 0;
    for (uint8_t i = 0; i < BULLET_MAX; i++) {
        Bullet *b = &bullets[i];
        if (!b->alive) continue;
        if ((uint16_t)(b->x + 4 - rescue_x - 2) < 12 && (uint16_t)(b->y + 8 - rescue_y - 4) < 12) {
            b->alive = 0;
            bullet_count--;
            hit = 1;
            break;
        }
    }
    // ... and enemies' touch.
    if (!hit)
        for (uint8_t i = 0; i < ACTOR_MAX; i++)
            if (actor_type[i] && actor_hp[i] && (uint16_t)(actor_x[i] - rescue_x + 10) < 20 && (uint16_t)(actor_y[i] - rescue_y + 10) < 20) {
                hit = 1;
                break;
            }
    if (!hit) return;
    rescue_hurt = 40;
    sfx_play(SFX_HURT);
    if (--rescue_hp) return;
    // Down: dragged back to where they were held (the job stays yours).
    char t[8] = "DOWN!  ";
    hud_say(t);
    rescue_state = RS_NONE;
}

// RS_HOLD: the clock runs while you're near; enemies come at the camp.
static void hold(void) {
    uint16_t cx = rescue_x, cy = rescue_y;  // (where they're held)
    int16_t dx = (int16_t)(player_x - cx), dy = (int16_t)(player_y - cy);
    if (dx > -HOLD_NEAR && dx < HOLD_NEAR && dy > -HOLD_NEAR && dy < HOLD_NEAR && !--rescue_tick) {
        // A second held: "HOLD 12", the seconds left.
        rescue_tick = 60;
        if (!--rescue_timer) {
            captive_gone();
            rescued();
            return;
        }
        char t[8] = "HOLD   ";
        uint8_t left = (uint8_t)rescue_timer;
        if (left >= 10) t[5] = '0' + (left >= 20 ? 2 : 1), left -= left >= 20 ? 20 : 10;
        t[6] = '0' + left;
        hud_say(t);
    }
    if (--rescue_spawn) return;
    rescue_spawn = HOLD_SPAWN;
    if (actor_count >= 2 + (layer_depth >> 1)) return;
    // From a few blocks off, on open floor: around them, from a random direction.
    static const int8_t ring_x[8] = {6, 4, 0, -4, -6, -4, 0, 4}, ring_y[8] = {0, 4, 6, 4, 0, -4, -6, -4};
    uint8_t d = rand() & 7;
    for (uint8_t k = 0; k < 16; k++, d = (d + 3) & 7) {
        uint8_t far = k < 8 ? 16 : 10;  // (px a step: 6 x 16 = 96, then 60)
        uint16_t x = cx + ring_x[d] * far, y = cy + ring_y[d] * far;
        if (x >= room_px_w - 32 || y >= room_px_h - 32 || x < 16 || y < 16) continue;
        if (room_block_at(x + 2, y + 4) != BLK_FLOOR || room_block_at(x + 13, y + 15) != BLK_FLOOR) continue;
        actor_spawn(world_enemy(layer_depth), x, y);
        return;
    }
}

void rescue_update(void) BANKED {
    if (rescue_state == RS_FOLLOW) follow();
    else if (rescue_state == RS_HOLD) hold();
}

uint8_t rescue_draw(uint8_t oam) BANKED {
    if (rescue_state != RS_FOLLOW) return oam;
    return talk_person_draw(oam, who(), rescue_x, rescue_y, rescue_hurt & 4 ? SPR_PAL_FLASH : SPR_PAL_PLAYER);
}
