#pragma bank 255
#include "flynt.h"
#include <gb/cgb.h>
#include <string.h>
#include "boss.h"
#include "actor_internal.h"
#include "aim.h"
#include "room.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"
#include "oam.h"
#include "sfx.h"

#define MACHINE_VRAM 96
#define SHADOW_VRAM 8
#define VENT_TILE_BASE 144u

#define VENT_COUNT 4
uint8_t flynt_state, flynt_timer, flynt_phase;
uint8_t flynt_vent_cycle, flynt_vent_timer;
uint8_t flynt_minion_timer, flynt_z;
uint8_t vent_stage[VENT_COUNT];   // 0: cool, 1: steam warn, 2: flame
uint16_t flynt_cleaves, flynt_slams, flynt_stuns;

static uint8_t flynt_hp, flynt_hp_max;
static uint8_t flynt_shown_hp, flynt_shown_state;
static int16_t flynt_x16, flynt_y16;
static int16_t flynt_target_x, flynt_target_y;
static uint8_t flynt_minion_slot;
static uint8_t flynt_cloak_used_24, flynt_cloak_used_12;
static const char *flynt_status_msg;

// 4 furnace grates in the four quadrants of the arena floor
static const uint8_t vent_tx[VENT_COUNT] = {4, 14, 4, 14};
static const uint8_t vent_ty[VENT_COUNT] = {4, 4, 10, 10};
static uint8_t vent_painted[VENT_COUNT]; // stage currently in VRAM
static uint8_t vent_cursor;

// 16x16 metasprite pixel art for Captain Flynt:
// Frame 0: Stalk / Idle with pirate tricorn, furnace pauldrons, flame cleaver
// Frame 1: Cleave Windup (harpoon cleaver raised overhead)
// Frame 2: Flame Cloak (blazing fiery shield aura)
// Frame 3: Overheated / Venting kneel (exhaust steam, open core)
static const char flynt_art[4][16][17] = {
    // Frame 0: Stalk / Harpoon Ready
    {"0000333333330000",
     "0033333333333300",
     "3332222222223333",
     "0003322222233000",
     "0003112222130000",
     "0033332222333000",
     "0331133333311300",
     "3311322222231130",
     "3333322222233330",
     "0003321111233000",
     "0033321111233300",
     "0033222222223300",
     "0033220000223300",
     "0033300000033300",
     "0333000000003330",
     "0000000000000000"},

    // Frame 1: Cleave Windup / Aim
    {"0000000033330000",
     "0000000331133000",
     "0000003311113300",
     "0033333333333300",
     "3332222222223333",
     "0003322222233000",
     "0003112222130000",
     "0033332222333000",
     "0331133333311300",
     "3311322222231130",
     "3333321111233330",
     "0033321111233300",
     "0033222222223300",
     "0033220000223300",
     "0033300000033300",
     "0000000000000000"},

    // Frame 2: Flame Cloak / Overheat Shield
    {"0001003333001000",
     "0011033113301100",
     "0110331111330110",
     "1103322222233011",
     "0133112222113310",
     "0331112222111330",
     "1311133333311131",
     "3111321111231113",
     "3111321111231113",
     "1311133333311131",
     "0331112222111330",
     "0133112222113310",
     "1103322222233011",
     "0110331111330110",
     "0011033113301100",
     "0001003333001000"},

    // Frame 3: Overheated / Venting Kneel
    {"0000000000000000",
     "0000333333000000",
     "0003322223300000",
     "0033222222330000",
     "0033112211330000",
     "0003333333300000",
     "0000100001000000",
     "0001100001100000",
     "0033113311330000",
     "0332222222233000",
     "3322111111223300",
     "3332111111233300",
     "0333222222333000",
     "0033333333330000",
     "0003330033300000",
     "0000000000000000"}
};

extern uint16_t pal_bg[PAL_COUNT * 4];
extern volatile uint8_t pal_dirty;

// CGB background palette for rising floor spikes (Palette 5)
static const uint16_t spike_bg_palette[4] = {
    RGB8(232, 220, 192), // Floor stone base
    RGB8(250, 250, 250), // Gleaming steel spike blade
    RGB8(208, 32, 48),   // Crimson blood
    RGB8(36, 28, 36)     // Deep hole / shadow
};

// Warning flash palette (intense glowing fiery red/amber danger)
static const uint16_t spike_warn_palette[4] = {
    RGB8(255, 110, 60),  // Glowing fiery warning orange/red stone!
    RGB8(255, 245, 170), // Intense white-hot tip highlight!
    RGB8(255, 30, 20),   // Burning danger red
    RGB8(80, 15, 10)     // Deep pit / warning dark rim
};

// Pre-baked 2BPP tile graphics for floor spike traps (4 stages x 4 tiles = 64 bytes per stage):
// Stage 0: Retracted holes in stone plate with dried blood
// Stage 1: Warning stage: silver tips peeking up inside holes
// Stage 2: Extending stage: sharp triangular spikes rising halfway
// Stage 3: Fully extended sharp blood-tipped spikes (active hazard)
static const uint8_t spike_tiles_var0[4][64] = {
    // Stage 0
    {
        0x80, 0x80, 0x39, 0x39, 0x39, 0x39, 0x10, 0x10, 0x00, 0x00, 0x39, 0x79, 0x39, 0xF9, 0x10, 0x50,
        0x01, 0x01, 0xCC, 0xCE, 0xC8, 0xCE, 0x80, 0x82, 0x00, 0x00, 0xCE, 0xCE, 0xCE, 0xCE, 0x82, 0x82,
        0x00, 0x00, 0x00, 0x00, 0x39, 0x39, 0x39, 0x39, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x00, 0x00, 0xCC, 0xCE, 0xC8, 0xCE, 0x80, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 1
    {
        0x80, 0x80, 0x39, 0x29, 0x39, 0x29, 0x10, 0x10, 0x00, 0x00, 0x39, 0x49, 0x39, 0xC9, 0x10, 0x50,
        0x01, 0x01, 0xCA, 0x4C, 0xC8, 0x4E, 0x80, 0x82, 0x00, 0x00, 0xCE, 0x4A, 0xCE, 0x4A, 0x82, 0x82,
        0x00, 0x00, 0x00, 0x00, 0x39, 0x29, 0x39, 0x29, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x00, 0x00, 0xCA, 0x4C, 0xC8, 0x4E, 0x80, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 2
    {
        0x90, 0x80, 0x39, 0x00, 0x7F, 0x46, 0x39, 0x39, 0x00, 0x10, 0x09, 0x70, 0x1F, 0xE6, 0x39, 0x39,
        0x81, 0x03, 0xC0, 0x06, 0xEA, 0x2C, 0xCE, 0xCE, 0x82, 0x00, 0xC6, 0x00, 0xEE, 0x28, 0xCE, 0xCE,
        0x00, 0x00, 0x10, 0x00, 0x39, 0x00, 0x7F, 0x46, 0x39, 0x39, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x80, 0x02, 0xC0, 0x06, 0xEA, 0x2C, 0xCE, 0xCE, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 3
    {
        0x90, 0x80, 0x39, 0x00, 0x39, 0x00, 0x7F, 0x46, 0x00, 0x10, 0x01, 0x38, 0x19, 0x60, 0x3F, 0xC6,
        0x81, 0x03, 0xC0, 0x06, 0xC2, 0x04, 0xEA, 0x2C, 0x82, 0x00, 0xC6, 0x00, 0xC6, 0x00, 0xEE, 0x28,
        0x39, 0x39, 0x10, 0x00, 0x39, 0x00, 0x39, 0x00, 0x7F, 0x46, 0x7F, 0x46, 0x39, 0x39, 0x80, 0x80,
        0xCE, 0xCE, 0x80, 0x02, 0xC0, 0x06, 0xC2, 0x04, 0xEA, 0x2C, 0xEE, 0x28, 0xCE, 0xCE, 0x01, 0x01,
    },
};

static const uint8_t spike_tiles_var1[4][64] = {
    // Stage 0
    {
        0x80, 0x80, 0x39, 0x79, 0x39, 0xF9, 0x10, 0x50, 0x00, 0x00, 0x39, 0x39, 0x39, 0x39, 0x10, 0x10,
        0x01, 0x01, 0xCE, 0xCE, 0xCE, 0xCE, 0x82, 0x82, 0x00, 0x00, 0xCC, 0xCE, 0xC8, 0xCE, 0x80, 0x82,
        0x00, 0x00, 0x00, 0x00, 0x39, 0x79, 0x39, 0xF9, 0x10, 0x50, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x00, 0x00, 0xCE, 0xCE, 0xCE, 0xCE, 0x82, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 1
    {
        0x80, 0x80, 0x39, 0x49, 0x39, 0xC9, 0x10, 0x50, 0x00, 0x00, 0x39, 0x29, 0x39, 0x29, 0x10, 0x10,
        0x01, 0x01, 0xCE, 0x4A, 0xCE, 0x4A, 0x82, 0x82, 0x00, 0x00, 0xCA, 0x4C, 0xC8, 0x4E, 0x80, 0x82,
        0x00, 0x00, 0x00, 0x00, 0x39, 0x49, 0x39, 0xC9, 0x10, 0x50, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x00, 0x00, 0xCE, 0x4A, 0xCE, 0x4A, 0x82, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 2
    {
        0x80, 0x90, 0x09, 0x70, 0x1F, 0xE6, 0x39, 0x39, 0x10, 0x00, 0x39, 0x00, 0x7F, 0x46, 0x39, 0x39,
        0x83, 0x01, 0xC6, 0x00, 0xEE, 0x28, 0xCE, 0xCE, 0x80, 0x02, 0xC0, 0x06, 0xEA, 0x2C, 0xCE, 0xCE,
        0x00, 0x00, 0x00, 0x10, 0x09, 0x70, 0x1F, 0xE6, 0x39, 0x39, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,
        0x00, 0x00, 0x82, 0x00, 0xC6, 0x00, 0xEE, 0x28, 0xCE, 0xCE, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    },
    // Stage 3
    {
        0x80, 0x90, 0x01, 0x38, 0x19, 0x60, 0x3F, 0xC6, 0x10, 0x00, 0x39, 0x00, 0x39, 0x00, 0x7F, 0x46,
        0x83, 0x01, 0xC6, 0x00, 0xC6, 0x00, 0xEE, 0x28, 0x80, 0x02, 0xC0, 0x06, 0xC2, 0x04, 0xEA, 0x2C,
        0x39, 0x39, 0x00, 0x10, 0x01, 0x38, 0x19, 0x60, 0x3F, 0xC6, 0x7F, 0x46, 0x39, 0x39, 0x80, 0x80,
        0xCE, 0xCE, 0x82, 0x00, 0xC6, 0x00, 0xC6, 0x00, 0xEE, 0x28, 0xEE, 0x28, 0xCE, 0xCE, 0x01, 0x01,
    },
};

static void set_floor_palette(const uint16_t *c) {
    memcpy(&pal_base_bg[5 << 2], c, 8);
    memcpy(&pal_bg[5 << 2], c, 8);
    pal_dirty = 1;
}

static void flynt_set_spike_stage(uint8_t s) {
    if (s > 3) s = 3;
    set_bkg_data((uint8_t)VENT_TILE_BASE, 4, spike_tiles_var0[s]);
    set_bkg_data((uint8_t)(VENT_TILE_BASE + 4u), 4, spike_tiles_var1[s]);
}

static void load_flynt_art(void) {
    uint8_t tiles[64];
    // 1. Sprite art: 4 frames for Captain Flynt
    for (uint8_t f = 0; f < 4; f++) {
        for (uint8_t col = 0; col < 2; col++) {
            for (uint8_t y = 0; y < 16; y++) {
                uint8_t lo = 0, hi = 0;
                for (uint8_t x = 0; x < 8; x++) {
                    uint8_t c = flynt_art[f][y][(col << 3) + x] - '0';
                    lo = (lo << 1) | (c & 1);
                    hi = (hi << 1) | (c >> 1);
                }
                uint8_t at = (col << 5) + (y << 1);
                tiles[at] = lo; tiles[at + 1] = hi;
            }
        }
        set_sprite_data(MACHINE_VRAM + (f << 2), 4, tiles);
    }

    // 2. Load initial retracted stage 0 for spike floor
    flynt_set_spike_stage(0);

    // Set background palette 5 to steel spike & blood colors
    set_floor_palette(spike_bg_palette);
    gfx_set_fade(0);
}

static void flynt_set_art(uint8_t frame, uint8_t pal) {
    actor_t0[boss_slot] = MACHINE_VRAM + (frame << 2);
    actor_t1[boss_slot] = actor_t0[boss_slot] + 2;
    actor_p0[boss_slot] = actor_p1[boss_slot] = pal;
}

static void fire_bullet_direct(uint16_t x, uint16_t y, int16_t vx, int16_t vy) {
    if (bullet_count == BULLET_MAX) return;
    if (x < 16) x = 16; else if (x > 144) x = 144;
    if (y < 20) y = 20; else if (y > 108) y = 108;
    Bullet *b = bullets;
    for (uint8_t k = 0; k < BULLET_MAX; k++, b++) {
        if (b->alive) continue;
        b->alive = 1;
        b->x = x;
        b->y = y;
        b->xs = b->ys = 0;
        bullet_set_velocity(b, vx, vy);
        bullet_count++;
        bullets_fired++;
        return;
    }
}

// True vector aim towards target position
static void aim_vector(uint16_t sx, uint16_t sy, uint16_t tx, uint16_t ty, uint8_t speed_mult, int16_t *out_vx, int16_t *out_vy) {
    int16_t dx = (int16_t)(tx - sx);
    int16_t dy = (int16_t)(ty - sy);
    uint16_t ax = dx < 0 ? -dx : dx;
    uint16_t ay = dy < 0 ? -dy : dy;
    uint16_t dist = (ax > ay) ? (ax + (ay >> 1)) : (ay + (ax >> 1));
    if (dist < 1) dist = 1;
    uint16_t rx = (ax << 7) / dist;
    uint16_t ry = (ay << 7) / dist;
    int16_t vx = (int16_t)(rx * speed_mult);
    int16_t vy = (int16_t)(ry * speed_mult);
    *out_vx = dx < 0 ? -vx : vx;
    *out_vy = dy < 0 ? -vy : vy;
}

void flynt_room(void) BANKED {
    boss_room();
    // 1. In VRAM Bank 1, map all arena floor cells (tx: 2..17, ty: 2..13) to Palette 5
    VBK_REG = 1;
    fill_bkg_rect(2, 2, 16, 12, 5);
    VBK_REG = 0;

    // 2. Map all floor visual blocks in room_tile_lut to the two spike trap variants at VENT_TILE_BASE:
    // Variant 0 (tiles 144..147): blocks 0, 8, 10
    // Variant 1 (tiles 148..151): blocks 7, 9, 11
    for (uint8_t v = 0; v < VIS_BLOCKS; v++) {
        if (v == 0 || v == 8 || v == 10) {
            room_tile_lut[(v << 2) + 0] = (uint8_t)(VENT_TILE_BASE + 0u);
            room_tile_lut[(v << 2) + 1] = (uint8_t)(VENT_TILE_BASE + 1u);
            room_tile_lut[(v << 2) + 2] = (uint8_t)(VENT_TILE_BASE + 2u);
            room_tile_lut[(v << 2) + 3] = (uint8_t)(VENT_TILE_BASE + 3u);
        } else if (v == 7 || v == 9 || v == 11) {
            room_tile_lut[(v << 2) + 0] = (uint8_t)(VENT_TILE_BASE + 4u);
            room_tile_lut[(v << 2) + 1] = (uint8_t)(VENT_TILE_BASE + 5u);
            room_tile_lut[(v << 2) + 2] = (uint8_t)(VENT_TILE_BASE + 6u);
            room_tile_lut[(v << 2) + 3] = (uint8_t)(VENT_TILE_BASE + 7u);
        }
    }
}

void flynt_start(void) BANKED {
    boss_kind = BOSS_FLYNT;
    boss_phase = flynt_phase = 0;
    boss_reward = 0;
    flynt_cleaves = flynt_slams = flynt_stuns = 0;
    flynt_cloak_used_24 = flynt_cloak_used_12 = 0;
    flynt_hp = flynt_hp_max = 36;
    boss_hp = flynt_hp;
    boss_hp_max = flynt_hp_max;

    flynt_minion_slot = 0xFF;
    flynt_minion_timer = 180;
    flynt_z = 0;

    // Spawn Flynt in upper arena
    boss_slot = actor_spawn(ACTOR_PEASHOOTER, 72, 36);
    if (boss_slot != 0xFF) {
        actor_scripted[boss_slot] = 1;
        actor_hp[boss_slot] = flynt_hp;
        actor_guard[boss_slot] = 0;
        flynt_x16 = (int16_t)72 << 8;
        flynt_y16 = (int16_t)36 << 8;
    }

    flynt_state = FLYNT_STALK;
    flynt_timer = 45;
    flynt_status_msg = "STALK";

    flynt_vent_cycle = 0;
    flynt_vent_timer = 90;
    for (uint8_t i = 0; i < VENT_COUNT; i++) {
        vent_stage[i] = 0;
        vent_painted[i] = 0;
    }
    vent_cursor = 0;

    load_flynt_art();
    flynt_set_art(0, SPR_PAL_SHOOTER);
    flynt_shown_hp = flynt_shown_state = 0xFF;
}

// Redraw / strip sync hook
static void update_vent_graphics(void) {
    // Floor is mapped across all cells in room_tile_lut and VRAM bank 1;
    // stage changes are applied instantaneously to VRAM tiles at VENT_TILE_BASE.
}

// Check player collision with floor spikes
static void update_vent_hazards(void) {
    if (!player_hittable() || player_z > 0) return; // JUMPING OVER SPIKES IS SAFE!
    if (vent_stage[0] < 2 && flynt_vent_cycle != 2) return;

    uint16_t px = player_x + PLAYER_HB_X;
    uint16_t py = player_y + PLAYER_HB_Y;

    // Entire arena floor area (x: 16..144, y: 16..112)
    if (px + PLAYER_HB_W > 16 && px < 144 &&
        py + PLAYER_HB_H > 16 && py < 112) {
        hurt_cause = 15;
        player_hurt(8, player_x + 8, player_y + 8);
        camera_shake(SHAKE_SMALL);
    }
}

static void update_furnace_cycle(void) {
    if (flynt_vent_timer) flynt_vent_timer--;
    if (!flynt_vent_timer) {
        flynt_vent_cycle = (flynt_vent_cycle + 1) % 3;
        switch (flynt_vent_cycle) {
        case 0: // All spikes retracted (safe window)
            for (uint8_t i = 0; i < VENT_COUNT; i++) vent_stage[i] = 0;
            flynt_set_spike_stage(0);
            set_floor_palette(spike_bg_palette);
            flynt_vent_timer = (flynt_phase >= 2) ? 60 : 90;
            break;

        case 1: // Warning telegraph: Palette flashes & tips emerge!
            for (uint8_t i = 0; i < VENT_COUNT; i++) vent_stage[i] = 1;
            flynt_set_spike_stage(1); // Tips poke up inside holes!
            flynt_vent_timer = 36;
            sfx_play(SFX_ENEMY_SHOT);
            break;

        case 2: // Spikes burst out across the whole floor!
            for (uint8_t i = 0; i < VENT_COUNT; i++) vent_stage[i] = 2;
            flynt_set_spike_stage(2); // Spikes thrust halfway up!
            set_floor_palette(spike_bg_palette);
            flynt_vent_timer = 24; // Spikes active for 24 frames (~0.4s) - jump duration is 28f!
            camera_shake(SHAKE_SMALL);
            sfx_play(SFX_SWORD);
            break;
        }
    } else if (flynt_vent_cycle == 1) {
        // Warning telegraph: Flash floor palette every 4 frames!
        if ((flynt_vent_timer & 4) != 0) {
            set_floor_palette(spike_warn_palette); // Intense burning danger flash!
        } else {
            set_floor_palette(spike_bg_palette);
        }
    } else if (flynt_vent_cycle == 2) {
        // At frame 18 of eruption (6 frames in), snap to full razor spikes!
        if (flynt_vent_timer == 18) {
            for (uint8_t i = 0; i < VENT_COUNT; i++) vent_stage[i] = 3;
            flynt_set_spike_stage(3);
        }
    }

    update_vent_hazards();
    update_vent_graphics();
}

static void trigger_cleave(void) {
    flynt_cleaves++;
    int16_t vx, vy;
    aim_vector((flynt_x16 >> 8) + 8, (flynt_y16 >> 8) + 8, player_x + 8, player_y + 8, 3, &vx, &vy);

    // Center projectile directly at player
    fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, vx, vy);

    // Flanking angled projectiles (+/- 25 degrees)
    // vx' = (7*vx - 3*vy)/8, vy' = (7*vy + 3*vx)/8
    int16_t vx1 = (vx * 7 - vy * 3) >> 3;
    int16_t vy1 = (vy * 7 + vx * 3) >> 3;
    fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, vx1, vy1);

    int16_t vx2 = (vx * 7 + vy * 3) >> 3;
    int16_t vy2 = (vy * 7 - vx * 3) >> 3;
    fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, vx2, vy2);

    // In Phase 3 (Desperation): add 2 wider flame pellets!
    if (flynt_phase >= 2) {
        int16_t vx3 = (vx * 5 - vy * 5) >> 3;
        int16_t vy3 = (vy * 5 + vx * 5) >> 3;
        fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, vx3, vy3);

        int16_t vx4 = (vx * 5 + vy * 5) >> 3;
        int16_t vy4 = (vy * 5 - vx * 5) >> 3;
        fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, vx4, vy4);
    }

    camera_shake(SHAKE_SMALL);
    sfx_play(SFX_SWORD);
}

static void trigger_slam(void) {
    flynt_slams++;
    camera_shake(SHAKE_MEDIUM);
    sfx_play(SFX_SHIELD_BREAK);

    // If player is grounded close to landing point, deal heavy shockwave damage
    if (player_hittable() && player_z == 0) {
        int16_t dx = (int16_t)(player_x + 8 - (flynt_x16 >> 8) - 8);
        int16_t dy = (int16_t)(player_y + 8 - (flynt_y16 >> 8) - 8);
        if (dx > -24 && dx < 24 && dy > -24 && dy < 24) {
            hurt_cause = 15;
            player_hurt(12, (flynt_x16 >> 8) + 8, (flynt_y16 >> 8) + 8);
        }
    }

    // Emit 6 radial shockwave fire bullets outward
    static const int16_t slam_vx[6] = {384, 192, -192, -384, -192, 192};
    static const int16_t slam_vy[6] = {0, 332, 332, 0, -332, -332};
    for (uint8_t k = 0; k < 6; k++) {
        fire_bullet_direct((flynt_x16 >> 8) + 4, (flynt_y16 >> 8) + 4, slam_vx[k], slam_vy[k]);
    }
}

void flynt_update(void) BANKED {
    if (boss_state == BOSS_WON || boss_slot == 0xFF || !actor_type[boss_slot]) return;

    flynt_hp = actor_hp[boss_slot];
    boss_hp = flynt_hp;

    // Check defeat
    if (flynt_hp == 0) {
        boss_state = BOSS_WON;
        boss_reward = 1;
        enemies_killed++;
        for (uint8_t i = 0; i < VENT_COUNT; i++) vent_stage[i] = 0;
        update_vent_graphics();
        actors_clear();
        camera_shake(SHAKE_BIG);
        sfx_play(SFX_DEATH);
        return;
    }

    // -------------------------------------------------------------
    // PHASE TRANSITION TRIGGERS (Flame Shield / Cloak)
    // -------------------------------------------------------------
    if (flynt_hp <= 24 && !flynt_cloak_used_24) {
        flynt_cloak_used_24 = 1;
        flynt_phase = 1;
        flynt_state = FLYNT_CLOAK;
        flynt_timer = 90;
        actor_guard[boss_slot] = 1; // DEFLECTS SHOTS!
        flynt_set_art(2, SPR_PAL_SHOT); // Glowing fire shield
        flynt_status_msg = "FLAME SHIELD ON!";
        camera_shake(SHAKE_MEDIUM);
        sfx_play(SFX_SHIELD_BREAK);
    } else if (flynt_hp <= 12 && !flynt_cloak_used_12) {
        flynt_cloak_used_12 = 1;
        flynt_phase = 2;
        flynt_state = FLYNT_CLOAK;
        flynt_timer = 90;
        actor_guard[boss_slot] = 1;
        flynt_set_art(2, SPR_PAL_SHOT);
        flynt_status_msg = "FLAME SHIELD ON!";
        camera_shake(SHAKE_MEDIUM);
        sfx_play(SFX_SHIELD_BREAK);
    }

    // -------------------------------------------------------------
    // FURNACE HAZARDS & MINION REINFORCEMENTS
    // -------------------------------------------------------------
    update_furnace_cycle();

    // Minion psycho add in Phase 2/3 (Second Wind fodder)
    if (flynt_phase >= 2) {
        if (flynt_minion_timer) flynt_minion_timer--;
        if (!flynt_minion_timer) {
            if (flynt_minion_slot == 0xFF || !actor_type[flynt_minion_slot]) {
                flynt_minion_slot = actor_spawn(ACTOR_SLIME, (player_x > 80 ? 24 : 128), 32);
                if (flynt_minion_slot != 0xFF) {
                    actor_hp[flynt_minion_slot] = 3;
                    actor_timer[flynt_minion_slot] = 20;
                }
            }
            flynt_minion_timer = 180;
        }
    }

    // -------------------------------------------------------------
    // FLYNT STATE MACHINE
    // -------------------------------------------------------------
    if (flynt_timer) flynt_timer--;

    switch (flynt_state) {
    case FLYNT_STALK: {
        flynt_set_art(0, SPR_PAL_SHOOTER);
        actor_guard[boss_slot] = 0;
        flynt_status_msg = "STALK";

        // Step smoothly toward player, keeping ~48px combat spacing
        int16_t dx = (int16_t)(player_x - (flynt_x16 >> 8));
        int16_t dy = (int16_t)(player_y - (flynt_y16 >> 8));
        int16_t vx = 0, vy = 0;
        if (dx > 4) vx = 192; else if (dx < -4) vx = -192;
        if (dy > 4) vy = 160; else if (dy < -4) vy = -160;

        uint16_t nx = (flynt_x16 + vx) >> 8;
        uint16_t ny = (flynt_y16 + vy) >> 8;
        if (nx >= 20 && nx <= 124) flynt_x16 += vx;
        if (ny >= 24 && ny <= 96) flynt_y16 += vy;

        if (!flynt_timer) {
            flynt_state = FLYNT_AIM;
            flynt_timer = (flynt_phase >= 2) ? 24 : 32;
            flynt_set_art(1, SPR_PAL_SHOOTER);
        }
        break;
    }

    case FLYNT_AIM:
        actor_flash[boss_slot] = flynt_timer & 4;
        flynt_status_msg = "AIMING CLEAVER!";
        if (!flynt_timer) {
            flynt_state = FLYNT_CLEAVE;
            flynt_timer = 16;
            trigger_cleave();
        }
        break;

    case FLYNT_CLEAVE:
        flynt_set_art(1, SPR_PAL_SHOT);
        if (!flynt_timer) {
            flynt_state = FLYNT_RECOVER;
            flynt_timer = (flynt_phase >= 2) ? 28 : 42;
            flynt_set_art(0, SPR_PAL_WEAPON);
        }
        break;

    case FLYNT_RECOVER:
        flynt_status_msg = "VULNERABLE - FIRE!";
        if (!flynt_timer) {
            flynt_state = FLYNT_STALK;
            flynt_timer = (flynt_phase >= 2) ? 35 : 48;
        }
        break;

    case FLYNT_CLOAK: { // Heat Shield / Overheat mode
        actor_guard[boss_slot] = 1; // Immune to frontal shots
        flynt_status_msg = "FLAME SHIELD ON!";
        flynt_set_art(2, (flynt_timer & 4) ? SPR_PAL_SHOT : SPR_PAL_WEAPON);

        // Periodically perform Leap Slam during cloak
        if (flynt_timer == 60) {
            flynt_state = FLYNT_LEAP;
            flynt_target_x = player_x;
            flynt_target_y = player_y;
            if (flynt_target_x < 24) flynt_target_x = 24; else if (flynt_target_x > 120) flynt_target_x = 120;
            if (flynt_target_y < 28) flynt_target_y = 28; else if (flynt_target_y > 96) flynt_target_y = 96;
            flynt_timer = 30;
            flynt_z = 0;
            sfx_play(SFX_JUMP);
        } else if (!flynt_timer) {
            // Overheat Burnout! Steam vents blow, drops to one knee!
            flynt_state = FLYNT_STUNNED;
            flynt_stuns++;
            flynt_timer = 75; // Punish window!
            actor_guard[boss_slot] = 0; // Guard drops!
            flynt_set_art(3, SPR_PAL_WEAPON);
            flynt_status_msg = "OVERHEATED - FIRE!";
            camera_shake(SHAKE_SMALL);
            sfx_play(SFX_HURT);
        }
        break;
    }

    case FLYNT_LEAP: { // High leap into air
        actor_guard[boss_slot] = 1;
        flynt_status_msg = "LEAP SLAM!";
        // Parabolic altitude arc
        if (flynt_timer > 15) flynt_z = (30 - flynt_timer);
        else flynt_z = flynt_timer;

        // Move towards target landing spot
        int16_t dx = (int16_t)(flynt_target_x - (flynt_x16 >> 8));
        int16_t dy = (int16_t)(flynt_target_y - (flynt_y16 >> 8));
        flynt_x16 += dx * 16;
        flynt_y16 += dy * 16;

        if (!flynt_timer) {
            flynt_z = 0;
            flynt_state = FLYNT_SLAM;
            flynt_timer = 18;
            trigger_slam();
        }
        break;
    }

    case FLYNT_SLAM:
        flynt_set_art(2, SPR_PAL_SHOT);
        if (!flynt_timer) {
            // Resume flame cloak
            flynt_state = FLYNT_CLOAK;
            flynt_timer = 35;
        }
        break;

    case FLYNT_STUNNED:
        actor_guard[boss_slot] = 0; // VULNERABLE PUNISH WINDOW
        flynt_set_art(3, (flynt_timer & 4) ? SPR_PAL_WEAPON : SPR_PAL_FLASH);
        flynt_status_msg = "OVERHEATED - FIRE!";
        if (!flynt_timer) {
            // Stun ends, stand back up
            flynt_state = FLYNT_STALK;
            flynt_timer = 40;
            flynt_set_art(0, SPR_PAL_SHOOTER);
        }
        break;
    }

    // Apply Flynt position to actor slot
    int16_t draw_y = (flynt_y16 >> 8) - flynt_z;
    if (draw_y < 16) draw_y = 16;
    actor_x[boss_slot] = flynt_x16 >> 8;
    actor_y[boss_slot] = (uint16_t)draw_y;
}

// Draw Flynt's leap shadow using OAM_PROP (slots 11, 12)
void flynt_draw(void) BANKED {
    for (uint8_t i = OAM_PROP; i < OAM_FIRST_ACTOR; i++) shadow_OAM[i].y = 0;
    if (boss_kind != BOSS_FLYNT || boss_state == BOSS_WON) return;

    if (flynt_z > 2 && boss_slot != 0xFF) {
        int16_t sx = (flynt_x16 >> 8) + 8 - cam_x;
        int16_t sy = (flynt_y16 >> 8) + 12 + 16 - cam_y;
        if (sy > 0 && sy < 160 && sx > -8 && sx < 168) {
            shadow_OAM[OAM_PROP].x = sx;         shadow_OAM[OAM_PROP].y = sy;
            shadow_OAM[OAM_PROP].tile = SHADOW_VRAM; shadow_OAM[OAM_PROP].prop = SPR_PAL_UI;
            shadow_OAM[OAM_PROP + 1].x = sx + 8; shadow_OAM[OAM_PROP + 1].y = sy;
            shadow_OAM[OAM_PROP + 1].tile = SHADOW_VRAM + 1; shadow_OAM[OAM_PROP + 1].prop = SPR_PAL_UI;
        }
    }
}

void flynt_status(void) BANKED {
    if (boss_kind != BOSS_FLYNT) return;

    // Header Row 0: BOSS HP
    if (flynt_hp != flynt_shown_hp) {
        uint8_t *row = gfx_strip(0, 0, 0, 20);
        const char *title = "FLYNT: ";
        for (uint8_t i = 0; i < 7; i++) row[i] = FONT_TILES + title[i] - ' ';
        row[7] = flynt_hp ? FONT_TILES + ('0' - ' ') + flynt_hp / 10 : FONT_TILES + ('-' - ' ');
        row[8] = flynt_hp ? FONT_TILES + ('0' - ' ') + flynt_hp % 10 : FONT_TILES + ('-' - ' ');
        for (uint8_t i = 9; i < 20; i++) row[i] = FONT_TILES;
        flynt_shown_hp = flynt_hp;
    }

    // Header Row 1: STATUS CUES
    if (flynt_state != flynt_shown_state || boss_state == BOSS_WON) {
        const char *msg = boss_state == BOSS_WON ? "CLEAR! SALVAGE +1  " : flynt_status_msg;
        uint8_t *row = gfx_strip(1, 1, 0, 18);
        uint8_t n = 0;
        for (; n < 18 && msg[n]; n++) row[n] = FONT_TILES + msg[n] - ' ';
        for (; n < 18; n++) row[n] = FONT_TILES;
        flynt_shown_state = flynt_state;
    }
}
