#pragma bank 255
// Actor setup, banked: loading enemy graphics (once per scene) and spawning. The
// per-frame work is in actor.c (bank 0).
#include <gb/gb.h>
#include <string.h>
#include "actor_internal.h"

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

    for (uint8_t i = 0; i < ACTOR_MAX; i++) actor_type[i] = ACTOR_NONE;
    for (uint8_t i = 0; i < BULLET_MAX; i++) bullet_alive[i] = 0;
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

