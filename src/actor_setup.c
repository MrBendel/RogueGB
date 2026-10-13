#pragma bank 255
// Actor setup, banked: loading enemy graphics (once per scene) and spawning. The
// per-frame work is in actor.c (bank 0).
#include <gb/gb.h>
#include <string.h>
#include "actor_internal.h"
#include "player.h"
#include "gfx.h"
#include "aim.h"

void actors_init(void) BANKED {
    // Load each type's columns (2 tiles each) into its own block of sprite VRAM,
    // and its frame table into RAM. The graphics are in switchable banks: the
    // banked_* helpers (gfx.c, bank 0) switch to them and back.
    uint8_t v = ENEMY_VRAM;
    type_vram[ACTOR_SLIME] = v;
    banked_sprite_data(v, SPR_SLIME_COLUMN_COUNT * 2, spr_slime_tiles, BANK(spr_slime));
    banked_memcpy(slime_frames, spr_slime_frames, sizeof(slime_frames), BANK(spr_slime));
    v += SPR_SLIME_COLUMN_COUNT * 2;
    type_vram[ACTOR_PEASHOOTER] = v;
    banked_sprite_data(v, SPR_PEASHOOTER_COLUMN_COUNT * 2, spr_peashooter_tiles, BANK(spr_peashooter));
    banked_memcpy(shooter_frames, spr_peashooter_frames, sizeof(shooter_frames), BANK(spr_peashooter));
    v += SPR_PEASHOOTER_COLUMN_COUNT * 2;
    bullet_vram = v;
    banked_sprite_data(v, SPR_PEA_COLUMN_COUNT * 2, spr_pea_tiles, BANK(spr_pea));
    banked_memcpy(pea_frames, spr_pea_frames, sizeof(pea_frames), BANK(spr_pea));
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
            actor_hp[i] = SLIME_HP + enemy_hp_bonus;
            actor_timer[i] = slime_rest + (i << 3);
        } else {
            actor_hp[i] = SHOOTER_HP + enemy_hp_bonus;
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

// --- Rarely-run actor actions, banked to keep bank 0 for the per-frame work ---

// Knock enemy i back by (dx, dy) px: all the way if it lands on plain floor, else
// half way, else not at all (so it can't be pushed into a wall or a pit).
void actor_push(uint8_t i, int8_t dx, int8_t dy) BANKED {
    uint16_t x = actor_x[i] + dx, y = actor_y[i] + dy;
    if (!enemy_box_free(x, y)) {
        x = actor_x[i] + (dx >> 1);
        y = actor_y[i] + (dy >> 1);
        if (!enemy_box_free(x, y)) return;
    }
    actor_x[i] = x;
    actor_y[i] = y;
}

// A peashooter's shot (every 1.5 s or so): an enemy bullet from shooter i in
// 8-way direction (sx, sy). The diagonal speed is a multiply, so it's cached.
static uint16_t bullet_diag_for;
static int16_t bullet_diag;
void fire_bullet(uint8_t i, int8_t sx, int8_t sy) BANKED {
    if (bullet_count == BULLET_MAX) return;  // full: don't scan for a free slot
    Bullet *b = bullets;
    for (uint8_t k = 0; k < BULLET_MAX; k++, b++) {
        if (b->alive) continue;
        b->alive = 1;
        // The bullet's 8x16 sprite, centered on the shooter.
        b->x = actor_x[i] + 4;
        b->y = actor_y[i];
        b->xs = b->ys = 0;
        int16_t vx, vy;
        if (bullet_diag_for != bullet_speed) bullet_diag = diag_scale(bullet_diag_for = bullet_speed);
        set_velocity(sx, sy, bullet_speed, bullet_diag, &vx, &vy);
        bullet_set_velocity(b, vx, vy);
        bullets_fired++;
        bullet_count++;
        return;
    }
}

// The 8 directions, clockwise from up, to turn a shot 45 degrees either way.
static const int8_t dir_x[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t dir_y[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

void fire_volley(uint8_t i, int8_t sx, int8_t sy) BANKED {
    fire_bullet(i, sx, sy);
    if (!shooter_spread) return;
    uint8_t k = 0;
    while (k < 7 && (dir_x[k] != sx || dir_y[k] != sy)) k++;
    uint8_t l = (k - 1) & 7, r = (k + 1) & 7;
    fire_bullet(i, dir_x[l], dir_y[l]);
    fire_bullet(i, dir_x[r], dir_y[r]);
}
