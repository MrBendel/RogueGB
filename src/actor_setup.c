#pragma bank 255
// Actor setup, banked: loading enemy graphics (once per scene) and spawning. The
// per-frame work is in actor.c (bank 0).
#include <gb/gb.h>
#include <string.h>
#include "actor_internal.h"
#include "aim.h"

void actors_init(void) BANKED {
    // Load each type's columns (2 tiles each) into its own block of sprite VRAM,
    // and its frame table into RAM. The graphics are in switchable banks, and this
    // may be called from banked code, so restore the caller's bank afterwards.
    uint8_t saved = CURRENT_BANK;
    uint8_t v = ENEMY_VRAM;
    type_vram[ACTOR_SLIME] = v;
    SWITCH_ROM(BANK(spr_slime));
    set_sprite_data(v, SPR_SLIME_COLUMN_COUNT * 2, spr_slime_tiles);
    memcpy(slime_frames, spr_slime_frames, sizeof(slime_frames));
    v += SPR_SLIME_COLUMN_COUNT * 2;
    type_vram[ACTOR_PEASHOOTER] = v;
    SWITCH_ROM(BANK(spr_peashooter));
    set_sprite_data(v, SPR_PEASHOOTER_COLUMN_COUNT * 2, spr_peashooter_tiles);
    memcpy(shooter_frames, spr_peashooter_frames, sizeof(shooter_frames));
    v += SPR_PEASHOOTER_COLUMN_COUNT * 2;
    bullet_vram = v;
    SWITCH_ROM(BANK(spr_pea));
    set_sprite_data(v, SPR_PEA_COLUMN_COUNT * 2, spr_pea_tiles);
    memcpy(pea_frames, spr_pea_frames, sizeof(pea_frames));
    SWITCH_ROM(saved);
    aim_init(v + SPR_PEA_COLUMN_COUNT * 2);

    for (uint8_t i = 0; i < ACTOR_MAX; i++) actor_type[i] = ACTOR_NONE;
    for (uint8_t i = 0; i < BULLET_MAX; i++) bullets[i].alive = 0;
    actors_active = bullets_active = 0;
    actor_count = bullet_count = 0;
    bullets_fired = 0;
    oam_end_last = 0;
}


uint8_t actor_spawn(uint8_t type, uint16_t x, uint16_t y) BANKED {
    for (uint8_t i = 0; i < ACTOR_MAX; i++) {
        if (actor_type[i] != ACTOR_NONE) continue;
        actor_type[i] = type;
        actor_x[i] = x;
        actor_y[i] = y;
        actor_xs[i] = actor_ys[i] = 0;
        actor_vx[i] = actor_vy[i] = 0;
        actor_face[i] = FACE_DOWN;
        actor_try[i] = 0;
        actor_hurt[i] = 0;
        aim_score[i] = AIM_NONE;  // a new enemy in this slot: not scored yet
        actor_frame[i] = 0;
        actor_flash[i] = 0;
        actor_state[i] = STATE_REST;
        actor_count++;
        // Stagger timers by slot so a wave doesn't move in lockstep.
        if (type == ACTOR_SLIME) {
            actor_hp[i] = SLIME_HP;
            actor_timer[i] = SLIME_REST + (i << 3);
        } else {
            actor_hp[i] = SHOOTER_HP;
            actor_timer[i] = shooter_interval + (i << 4);
        }
        actor_cache_frame(i);
        return i;
    }
    return 0xFF;
}


// Debug/test stress: every enemy slot filled around the player (alternating
// slimes and peashooters), and every bullet slot with a slow bullet drifting
// through the view. Positions may overlap walls; that's fine for a load test.
void actors_debug_stress(uint16_t px, uint16_t py) BANKED {
    static const int8_t ox[ACTOR_MAX] = {-56, -24, 24, 56, -56, -24, 24, 56};
    static const int8_t oy[ACTOR_MAX] = {-40, -48, -48, -40, 32, 40, 40, 32};
    for (uint8_t i = 0; i < ACTOR_MAX; i++) {
        if (actor_type[i] != ACTOR_NONE) continue;
        actor_spawn((i & 1) ? ACTOR_SLIME : ACTOR_PEASHOOTER, px + ox[i], py + oy[i]);
    }
    Bullet *bl = bullets;
    uint8_t col = 0;
    uint16_t gx = px - 64, gy = py - 48;
    for (uint8_t b = 0; b < BULLET_MAX; b++, bl++) {
        // A 5x4 grid spread over the view, drifting slowly left or right.
        uint16_t x = gx, y = gy;
        gx += 32;
        if (++col == 5) {
            col = 0;
            gx = px - 64;
            gy += 24;
        }
        if (bl->alive) continue;
        bl->alive = 1;
        bullet_count++;
        bl->x = x;
        bl->y = y;
        bl->xs = bl->ys = 0;
        bl->vx = (b & 1) ? 64 : -64;
        bl->vx_sign = (b & 1) ? 0 : -1;
        bl->vy = 0;
        bl->vy_sign = 0;
    }
}
