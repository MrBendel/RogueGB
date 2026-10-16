#pragma bank 255
// Small-screen encounter experiments. Reuse the collision/projectile pools;
// scripted actors get their own state machine and compact machine sprites.
#include "boss.h"
#include "actor_internal.h"
#include "aim.h"
#include "room.h"
#include "player.h"
#include "health.h"
#include "gfx.h"
#include "camera.h"
#include "gunship.h"
#include "warden.h"

#define MACHINE_VRAM 96
#define SHADOW_VRAM 8

uint8_t boss_kind, boss_slot, boss_state, boss_timer, boss_phase;
uint8_t boss_hp, boss_hp_max, boss_relays, boss_reward;
uint16_t boss_attacks, boss_cycles;
int8_t boss_dx, boss_dy;
uint8_t warden_burst_left, warden_burst_timer, warden_shotgun;
static uint8_t relay_slots[2], relay_clock;
static uint8_t shown_hp, shown_state, shown_relays, shown_phase;
static const int8_t dx8[8] = {0,1,1,1,0,-1,-1,-1};
static const int8_t dy8[8] = {-1,-1,0,1,1,1,0,-1};

// -------------------------------------------------------------
// SPROCKET & BLAST: DYNAMIC MODERN BOSS ENGINE
// -------------------------------------------------------------

// Blast (Jetpack Flyer)
#define BLAST_HOVER 0
#define BLAST_SWOOP_WARN 1
#define BLAST_SWOOP_DIVE 2
#define BLAST_SLAM_RECOVER 3
#define BLAST_BURST_ATTACK 4

static uint8_t blast_slot, blast_hp, blast_state, blast_timer;
static int16_t blast_x16, blast_y16;   // 8.8 fixed-point position
static int16_t blast_vx, blast_vy;     // 8.8 velocity with easing
static uint8_t blast_z;                // altitude (0 = ground, 12-16 = hover, 24 = dive prep)
static uint8_t blast_orbit_angle;      // orbital trajectory angle
static uint8_t blast_burst_left, blast_burst_timer;
static int16_t blast_dive_vx, blast_dive_vy;

// Sprocket (Mobile Tank Cannon / On-Foot Skirmisher)
#define SPROCKET_DRIVE 0
#define SPROCKET_MORTAR_WARN 1
#define SPROCKET_RAM_WARN 2
#define SPROCKET_RAM 3
#define SPROCKET_RECOVER 4

static uint8_t sprocket_state, sprocket_timer, sprocket_phase;
static int16_t sprocket_x16, sprocket_y16;
static int16_t sprocket_vx;
static uint8_t mortar_active, mortar_timer, mortar_x, mortar_y;
static uint8_t duo_attack_cycle, shown_blast_hp;
static const char *duo_status_msg;

// Code-native pixel art, prepared once with display off. 0 is transparent.
// 0: Closed Warden, 1: exposed Warden, 2: Claimjaw, 3: relay.
// 4: Sprocket Cannon, 5: Sprocket on Foot, 6: Blast (Jetpack).
static const char machine_art[7][16][17] = {
    {"0000333333330000","0033333333333300","0332222222222330","0323333333333230",
     "3323111111113233","3223133333313223","3223132222313223","3223132222313223",
     "3223133333313223","3323111111113233","0323333333333230","0332222222222330",
     "0033333333333300","0032200000022300","0033300000033300","0000000000000000"},
    {"0000333333330000","0033222222223300","0332000000002330","0320033333300230",
     "3320331111330233","3220312222130223","3200312112130023","3200312112130023",
     "3220312222130223","3320331111330233","0320033333300230","0332000000002330",
     "0033222222223300","0032200000022300","0033300000033300","0000000000000000"},
    {"0000000000000000","0330000000000330","0323333333333230","0322222222222230",
     "0333313333133330","0322312222132230","3322333333332233","3222222222222223",
     "3233333333333323","3210310310310123","3310310310310133","0333333333333330",
     "0322300000032230","0333300000033330","0000000000000000","0000000000000000"},
    {"0000003333000000","0000033113300000","0000331221330000","0000321221230000",
     "0000321221230000","0000331221330000","0000033113300000","0000003333000000",
     "0000003223000000","0000033223300000","0000032222300000","0000332222330000",
     "0000323333230000","0000333333330000","0000000000000000","0000000000000000"},
    {"0000333333330000","0033222332223300","0322112332112230","0321112332111230",
     "3321112332111233","3222222332222223","3333333333333333","0322222332222230",
     "0323322332233230","0323322332233230","0333333333333330","3322222222222233",
     "3222222222222223","3333333333333333","0330033003300330","0000000000000000"},
    {"0000333333330000","0033222222223300","0322333333332230","0323111111113230",
     "0323111111113230","0333333333333330","0032222222223000","3332222222223330",
     "3113222222223113","3113333333333113","3330322222230333","0000322222230000",
     "0000322002230000","0003322002233000","0003333003333000","0000000000000000"},
    {"0000003333000000","0000332222330000","0003221111223000","0003221111223000",
     "0000332222330000","0033333333333300","0322332222332230","0321332222331230",
     "0333333333333330","0033032222303300","0031303223031300","0003003223003000",
     "0000003223000000","0000032002300000","0000033003300000","0000000000000000"}
};

static void load_art(void) {
    uint8_t tiles[64];
    for (uint8_t f = 0; f < 7; f++) {
        for (uint8_t col = 0; col < 2; col++) {
            for (uint8_t y = 0; y < 16; y++) {
                uint8_t lo = 0, hi = 0;
                for (uint8_t x = 0; x < 8; x++) {
                    uint8_t c = machine_art[f][y][(col << 3) + x] - '0';
                    lo = (lo << 1) | (c & 1);
                    hi = (hi << 1) | (c >> 1);
                }
                uint8_t at = (col << 5) + (y << 1);
                tiles[at] = lo; tiles[at + 1] = hi;
            }
        }
        set_sprite_data(MACHINE_VRAM + (f << 2), 4, tiles);
    }
}

static void art(uint8_t i, uint8_t frame, uint8_t pal) {
    actor_t0[i] = MACHINE_VRAM + (frame << 2);
    actor_t1[i] = actor_t0[i] + 2;
    actor_p0[i] = actor_p1[i] = pal;
}

static void guard(uint8_t on) {
    actor_guard[boss_slot] = on;
    if (boss_kind == BOSS_WARDEN) art(boss_slot, on ? 0 : 1, on ? SPR_PAL_WEAPON : SPR_PAL_SHOT);
    if (on) {
        aim_score[boss_slot] = AIM_NONE;
        if (aim_target == boss_slot) aim_target = AIM_NONE;
    }
}

static void clear_bullets(void) {
    for (uint8_t i = 0; i < BULLET_MAX; i++) bullets[i].alive = 0;
    bullet_count = bullets_active = 0;
}

void boss_room(void) BANKED {
    room_load_tiles();
    room_w = 10; room_h = 8;
    room_px_w = 160; room_px_h = 128;
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID;
    for (uint8_t y = 1; y < 7; y++)
        for (uint8_t x = 1; x < 9; x++) room_map[((uint16_t)y << 5) | x] = BLK_FLOOR;
}

static uint8_t machine(uint8_t x, uint8_t y, uint8_t hp, uint8_t frame) {
    uint8_t i = actor_spawn(ACTOR_PEASHOOTER, x, y);
    if (i == 0xFF) return i;
    actor_scripted[i] = 1;
    actor_hp[i] = hp;
    art(i, frame, frame == 2 ? SPR_PAL_SLIME : SPR_PAL_SHOOTER);
    return i;
}

// True 360-degree vector aim directly at the player using 16-bit math
static void aim_at_player(uint16_t src_x, uint16_t src_y, uint8_t mult, int16_t *out_vx, int16_t *out_vy) {
    int16_t dx = (int16_t)((player_x + 8) - src_x);
    int16_t dy = (int16_t)((player_y + 8) - src_y);
    uint16_t ax = dx < 0 ? -dx : dx;
    uint16_t ay = dy < 0 ? -dy : dy;
    uint16_t dist = (ax > ay) ? (ax + (ay >> 1)) : (ay + (ax >> 1));
    if (dist < 1) dist = 1;
    uint16_t rx = (ax << 7) / dist;
    uint16_t ry = (ay << 7) / dist;
    int16_t vx = (int16_t)(rx * mult);
    int16_t vy = (int16_t)(ry * mult);
    *out_vx = dx < 0 ? -vx : vx;
    *out_vy = dy < 0 ? -vy : vy;
}

static void fire_aimed_bullet(uint16_t x, uint16_t y, uint8_t mult) {
    if (bullet_count == BULLET_MAX) return;
    Bullet *b = bullets;
    for (uint8_t k = 0; k < BULLET_MAX; k++, b++) {
        if (b->alive) continue;
        b->alive = 1;
        b->x = x;
        b->y = y;
        b->xs = b->ys = 0;
        int16_t vx, vy;
        aim_at_player(x + 4, y + 4, mult, &vx, &vy);
        bullet_set_velocity(b, vx, vy);
        bullet_count++;
        bullets_fired++;
        return;
    }
}

static void fire_direct_bullet(uint16_t x, uint16_t y, int16_t vx, int16_t vy) {
    if (bullet_count == BULLET_MAX) return;
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

void boss_start(uint8_t kind) BANKED {
    if (kind == BOSS_GUNSHIP) {
        gunship_start();
        shown_hp = shown_state = shown_relays = shown_phase = 0xFF;
        boss_status();
        return;
    }
    boss_kind = kind;
    boss_phase = boss_reward = 0;
    boss_attacks = boss_cycles = 0;
    warden_burst_left = warden_burst_timer = 0;
    boss_dx = 0; boss_dy = 1;
    enemy_hp_bonus = shooter_spread = 0;
    bullet_speed = 256; slime_speed = 160; slime_rest = 40;
    load_art();
    if (kind == BOSS_WARDEN) warden_start();

    if (kind == BOSS_DUO) {
        // Place 2 tactical cover pillars
        room_map[((uint16_t)3 << 5) | 3] = BLK_SOLID;
        room_map[((uint16_t)3 << 5) | 6] = BLK_SOLID;

        boss_hp_max = 24;
        boss_slot = machine(72, 28, 24, 4);  // Sprocket Cannon (Frame 4)
        blast_slot = machine(32, 48, 16, 6); // Blast (Frame 6)
        boss_hp = 24;
        blast_hp = 16;
        sprocket_phase = 0;
        sprocket_state = SPROCKET_DRIVE;
        sprocket_timer = 50;
        sprocket_x16 = 72 << 8;
        sprocket_y16 = 28 << 8;
        sprocket_vx = 192; // 0.75 px/f patrol

        mortar_active = 0;
        mortar_timer = 0;

        blast_state = BLAST_HOVER;
        blast_timer = 45;
        blast_x16 = 32 << 8;
        blast_y16 = 48 << 8;
        blast_vx = blast_vy = 0;
        blast_z = 14;
        blast_orbit_angle = 0;
        blast_burst_left = 0;

        duo_attack_cycle = 0;
        guard(1); // Frontal gun mantle
        duo_status_msg = "SPROCKET & BLAST  ";
    } else {
        boss_hp_max = kind == BOSS_CLAIMJAW ? 18 : 48;
        boss_slot = machine(kind == BOSS_WARDEN ? 104 : 72, kind == BOSS_WARDEN ? 160 : 32, boss_hp_max, kind == BOSS_CLAIMJAW ? 2 : 0);
        boss_hp = boss_hp_max;
        boss_relays = kind == BOSS_WARDEN ? 2 : 0;
        if (boss_relays) {
            relay_slots[0] = machine(32, 176, 5, 3);
            relay_slots[1] = machine(176, 176, 5, 3);
        }
        boss_state = boss_relays ? BOSS_RELAYS : BOSS_RECOVER;
        boss_timer = 90; relay_clock = 100;
        guard(boss_relays != 0);
    }

    health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
    shown_hp = shown_state = shown_relays = shown_phase = shown_blast_hp = 0xFF;
    if (kind == BOSS_WARDEN) { boss_status(); return; }
    bkg_text(0, 0, "                    ");
    bkg_text(0, 1, "                    ");
    bkg_text(1, 0, kind == BOSS_CLAIMJAW ? "CLAIMJAW" : kind == BOSS_DUO ? "SPROCKET & BLAST" : "WARDEN");
    boss_status();
}

static void lock_direction(uint8_t i) {
    int16_t dx = (int16_t)(player_x - actor_x[i]);
    int16_t dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    boss_dx = ax > ay ? (dx < 0 ? -1 : 1) : 0;
    boss_dy = ax > ay ? 0 : (dy < 0 ? -1 : 1);
}

static void shotgun(void) {
    static const int16_t spread[7] = {-224,-144,-72,0,72,144,224};
    uint8_t pellet = 0;
    for (uint8_t k = 0; k < BULLET_MAX && pellet < 7; k++) {
        Bullet *b = &bullets[k];
        if (b->alive) continue;
        int16_t side = spread[pellet++];
        int16_t vx = boss_dx ? (boss_dx > 0 ? 384 : -384) : side;
        int16_t vy = boss_dy ? (boss_dy > 0 ? 384 : -384) : side;
        b->alive = 1; b->xs = b->ys = 0;
        b->x = actor_x[boss_slot] + 4; b->y = actor_y[boss_slot];
        bullet_set_velocity(b, vx, vy);
        bullet_count++; bullets_fired++;
    }
    boss_attacks++;
}

static void ring(void) {
    static const int16_t vx8[8] = {0,181,256,181,0,-181,-256,-181};
    static const int16_t vy8[8] = {-256,-181,0,181,256,181,0,-181};
    uint8_t d = 0;
    for (uint8_t k = 0; k < BULLET_MAX && d < 8; k++) {
        Bullet *b = &bullets[k];
        if (b->alive) continue;
        int16_t vx = vx8[d], vy = vy8[d];
        d++; b->alive = 1; b->xs = b->ys = 0;
        b->x = actor_x[boss_slot] + 4; b->y = actor_y[boss_slot];
        bullet_set_velocity(b, vx, vy);
        bullet_count++; bullets_fired++;
    }
    boss_attacks++;
}

static void recover(uint8_t frames) {
    boss_state = BOSS_RECOVER;
    boss_timer = frames;
    guard(0);
    boss_cycles++;
}

// -------------------------------------------------------------
// DYNAMIC DUO COMBAT LOOP
// -------------------------------------------------------------
static void update_duo(void) {
    if (boss_state == BOSS_WON) return;

    // Track HP
    if (sprocket_phase == 0) {
        boss_hp = actor_type[boss_slot] ? actor_hp[boss_slot] : 0;
        if (!boss_hp) {
            // Cannon Totaled -> Sprocket Ejection!
            sprocket_phase = 1;
            boss_hp = 12;
            actor_hp[boss_slot] = 12;
            art(boss_slot, 5, SPR_PAL_SHOOTER); // Frame 5: on foot
            guard(0);
            clear_bullets();
            camera_shake(SHAKE_SMALL);
            sprocket_timer = 40;
            duo_status_msg = "CANNON WRECKED! EJECT!";
        }
    } else {
        boss_hp = actor_type[boss_slot] ? actor_hp[boss_slot] : 0;
    }

    blast_hp = actor_type[blast_slot] ? actor_hp[blast_slot] : 0;

    // Win condition: both down
    if (!boss_hp && !blast_hp) {
        boss_state = BOSS_WON;
        boss_reward = 1;
        actors_clear();
        camera_shake(SHAKE_SMALL);
        duo_status_msg = "DUO DEFEATED! +1  ";
        return;
    }

    // ---------------------------------------------------------
    // 1. SPROCKET UPDATE
    // ---------------------------------------------------------
    if (boss_hp > 0) {
        if (sprocket_phase == 0) {
            // Phase 1: Heavy Mobile Tank
            switch (sprocket_state) {
            case SPROCKET_DRIVE: {
                // Active Tank Tread Repositioning
                int16_t target_x = player_x;
                if (target_x < 24) target_x = 24;
                else if (target_x > 120) target_x = 120;
                int16_t cur_x = sprocket_x16 >> 8;

                if (cur_x < target_x - 6) sprocket_vx = 220;
                else if (cur_x > target_x + 6) sprocket_vx = -220;
                else sprocket_vx = 0;

                sprocket_x16 += sprocket_vx;
                actor_x[boss_slot] = sprocket_x16 >> 8;
                actor_y[boss_slot] = sprocket_y16 >> 8;

                if (sprocket_timer) sprocket_timer--;
                if (!sprocket_timer) {
                    if ((duo_attack_cycle++ & 1) == 0) {
                        // Launch Floor Mortar AoE
                        mortar_active = 1;
                        mortar_timer = 36;
                        mortar_x = player_x;
                        mortar_y = player_y;
                        sprocket_state = SPROCKET_MORTAR_WARN;
                        sprocket_timer = 40;
                        duo_status_msg = "MORTAR TARGETED!  ";
                    } else {
                        // Ram / Bulldozer Rush
                        sprocket_state = SPROCKET_RAM_WARN;
                        sprocket_timer = 32;
                        guard(1);
                        duo_status_msg = "CANNON CHARGING!  ";
                    }
                }
                break;
            }
            case SPROCKET_MORTAR_WARN:
                actor_flash[boss_slot] = sprocket_timer & 4;
                if (sprocket_timer) sprocket_timer--;
                if (!sprocket_timer) {
                    // Direct cannon shell fired accurately at player
                    fire_aimed_bullet(actor_x[boss_slot] + 4, actor_y[boss_slot] + 6, 3);
                    sprocket_state = SPROCKET_RECOVER;
                    sprocket_timer = 50;
                    guard(0); // Damage window!
                    duo_status_msg = "CORE VENTING!     ";
                }
                break;
            case SPROCKET_RAM_WARN:
                actor_flash[boss_slot] = sprocket_timer & 4;
                if (sprocket_timer) sprocket_timer--;
                if (!sprocket_timer) {
                    sprocket_state = SPROCKET_RAM;
                    sprocket_timer = 24;
                }
                break;
            case SPROCKET_RAM: {
                // High-speed rush downward (2.5 px/f)
                sprocket_y16 += 640;
                actor_y[boss_slot] = sprocket_y16 >> 8;
                if (sprocket_timer) sprocket_timer--;
                uint16_t sy = actor_y[boss_slot];
                if (!sprocket_timer || sy >= 80) {
                    camera_shake(SHAKE_SMALL);
                    sprocket_state = SPROCKET_RECOVER;
                    sprocket_timer = 60;
                    guard(0); // Big punish window!
                    duo_status_msg = "CANNON STUNNED - FIRE!";
                }
                break;
            }
            case SPROCKET_RECOVER:
                if (sprocket_timer) sprocket_timer--;
                if (!sprocket_timer) {
                    // Drive back up to base line
                    if ((sprocket_y16 >> 8) > 28) {
                        sprocket_y16 -= 384;
                        actor_y[boss_slot] = sprocket_y16 >> 8;
                    } else {
                        sprocket_y16 = 28 << 8;
                        actor_y[boss_slot] = 28;
                        sprocket_state = SPROCKET_DRIVE;
                        sprocket_timer = 55;
                        guard(1);
                        duo_status_msg = "SPROCKET & BLAST  ";
                    }
                }
                break;
            }
        } else {
            // Phase 2: Sprocket on Foot (Aggressive Skirmisher with Twin Shotgun)
            if (sprocket_timer) sprocket_timer--;
            int8_t mx = (player_x > actor_x[boss_slot] + 4) ? 1 : (player_x < actor_x[boss_slot] - 4) ? -1 : 0;
            int8_t my = (player_y > actor_y[boss_slot] + 4) ? 1 : (player_y < actor_y[boss_slot] - 4) ? -1 : 0;
            uint16_t nx = actor_x[boss_slot] + mx * 2;
            uint16_t ny = actor_y[boss_slot] + my * 2;
            if (enemy_box_free(nx, ny)) {
                actor_x[boss_slot] = nx;
                actor_y[boss_slot] = ny;
            }
            actor_flash[boss_slot] = (sprocket_timer < 20) && (sprocket_timer & 4);
            if (!sprocket_timer) {
                // Direct aimed shotgun spread
                fire_aimed_bullet(actor_x[boss_slot] + 4, actor_y[boss_slot] + 4, 3);
                fire_aimed_bullet(actor_x[boss_slot] + 2, actor_y[boss_slot] + 2, 3);
                fire_aimed_bullet(actor_x[boss_slot] + 6, actor_y[boss_slot] + 6, 3);
                sprocket_timer = 50;
            }
        }
    }

    // ---------------------------------------------------------
    // 2. MORTAR HAZARD UPDATE
    // ---------------------------------------------------------
    if (mortar_active) {
        if (mortar_timer) mortar_timer--;
        if (!mortar_timer) {
            mortar_active = 0;
            camera_shake(SHAKE_SMALL);
            // AoE damage check
            if (player_hittable()) {
                int16_t mdx = (int16_t)(player_x + PLAYER_HB_X - mortar_x);
                int16_t mdy = (int16_t)(player_y + PLAYER_HB_Y - mortar_y);
                if (mdx > -18 && mdx < 18 && mdy > -18 && mdy < 18) {
                    hurt_cause = 10;
                    player_hurt(12, mortar_x + 8, mortar_y + 8);
                }
            }
            // Scatters 4 shrapnel shells
            fire_direct_bullet(mortar_x + 4, mortar_y + 4, 256, 256);
            fire_direct_bullet(mortar_x + 4, mortar_y + 4, -256, 256);
            fire_direct_bullet(mortar_x + 4, mortar_y + 4, 256, -256);
            fire_direct_bullet(mortar_x + 4, mortar_y + 4, -256, -256);
        }
    }

    // ---------------------------------------------------------
    // 3. BLAST (JETPACK FLYER) UPDATE
    // ---------------------------------------------------------
    if (blast_hp > 0) {
        switch (blast_state) {
        case BLAST_HOVER: {
            blast_orbit_angle += 2;
            static const int8_t cos16[16] = {48, 44, 34, 18, 0, -18, -34, -44, -48, -44, -34, -18, 0, 18, 34, 44};
            static const int8_t sin16[16] = {0, 18, 34, 44, 48, 44, 34, 18, 0, -18, -34, -44, -48, -44, -34, -18};
            uint8_t a = (blast_orbit_angle >> 3) & 15;
            int16_t tx = (player_x + 8) + cos16[a];
            int16_t ty = (player_y + 8) + sin16[a];
            if (tx < 24) tx = 24; else if (tx > 136) tx = 136;
            if (ty < 24) ty = 24; else if (ty > 104) ty = 104;

            int16_t cur_x = blast_x16 >> 8, cur_y = blast_y16 >> 8;
            int16_t desired_vx = (tx - cur_x) * 8;
            int16_t desired_vy = (ty - cur_y) * 8;
            if (desired_vx > 300) desired_vx = 300; else if (desired_vx < -300) desired_vx = -300;
            if (desired_vy > 300) desired_vy = 300; else if (desired_vy < -300) desired_vy = -300;

            // Easing / momentum
            blast_vx += (desired_vx - blast_vx) >> 3;
            blast_vy += (desired_vy - blast_vy) >> 3;
            blast_x16 += blast_vx;
            blast_y16 += blast_vy;

            // Sinusoidal hover bobbing
            blast_z = 12 + (((blast_orbit_angle >> 2) & 7) < 4 ? ((blast_orbit_angle >> 2) & 3) : (3 - ((blast_orbit_angle >> 2) & 3)));

            if (blast_timer) blast_timer--;
            if (!blast_timer) {
                if ((duo_attack_cycle & 2) == 0) {
                    // Strafe burst
                    blast_state = BLAST_BURST_ATTACK;
                    blast_burst_left = sprocket_phase ? 4 : 3;
                    blast_burst_timer = 1;
                    duo_status_msg = "BLAST: STRAFE BURST!";
                } else {
                    // Prep dive-bomb
                    blast_state = BLAST_SWOOP_WARN;
                    blast_timer = 30;
                    duo_status_msg = "JETPACK DIVE INCOMING!";
                }
            }
            break;
        }
        case BLAST_BURST_ATTACK:
            blast_x16 += blast_vx;
            blast_y16 += blast_vy;
            if (!--blast_burst_timer) {
                fire_aimed_bullet(blast_x16 >> 8, (blast_y16 >> 8) - blast_z, 3);
                blast_burst_left--;
                blast_burst_timer = 10;
                if (!blast_burst_left) {
                    blast_state = BLAST_HOVER;
                    blast_timer = sprocket_phase ? 35 : 55;
                }
            }
            break;
        case BLAST_SWOOP_WARN:
            blast_z = 22; // High altitude
            actor_flash[blast_slot] = blast_timer & 4;
            if (blast_timer) blast_timer--;
            if (!blast_timer) {
                int16_t dx = (player_x + 8) - (blast_x16 >> 8);
                int16_t dy = (player_y + 8) - (blast_y16 >> 8);
                uint16_t ax = dx < 0 ? -dx : dx;
                uint16_t ay = dy < 0 ? -dy : dy;
                uint16_t dist = (ax > ay) ? (ax + (ay >> 1)) : (ay + (ax >> 1));
                if (dist < 1) dist = 1;
                uint16_t rx = (ax << 7) / dist;
                uint16_t ry = (ay << 7) / dist;
                int16_t vx = (int16_t)(rx * 7); // 7 * 128 = 896 = 3.5 px/f plunge!
                int16_t vy = (int16_t)(ry * 7);
                blast_dive_vx = dx < 0 ? -vx : vx;
                blast_dive_vy = dy < 0 ? -vy : vy;
                blast_state = BLAST_SWOOP_DIVE;
                blast_timer = 16;
                duo_status_msg = "LOOK OUT BELOW!   ";
            }
            break;
        case BLAST_SWOOP_DIVE:
            blast_z = (blast_timer * 22) / 16; // Rapidly drops to 0
            blast_x16 += blast_dive_vx;
            blast_y16 += blast_dive_vy;
            if (blast_timer) blast_timer--;
            if (!blast_timer) {
                // Ground slam!
                blast_z = 0;
                camera_shake(SHAKE_SMALL);
                if (player_hittable()) {
                    int16_t bx = blast_x16 >> 8, by = blast_y16 >> 8;
                    int16_t pdx = (player_x + PLAYER_HB_X) - bx;
                    int16_t pdy = (player_y + PLAYER_HB_Y) - by;
                    if (pdx > -16 && pdx < 16 && pdy > -16 && pdy < 16) {
                        hurt_cause = 10;
                        player_hurt(10, bx + 8, by + 8);
                    }
                }
                // Shockwave spark burst
                fire_direct_bullet(blast_x16 >> 8, blast_y16 >> 8, 256, 0);
                fire_direct_bullet(blast_x16 >> 8, blast_y16 >> 8, -256, 0);
                fire_direct_bullet(blast_x16 >> 8, blast_y16 >> 8, 0, 256);
                fire_direct_bullet(blast_x16 >> 8, blast_y16 >> 8, 0, -256);

                blast_state = BLAST_SLAM_RECOVER;
                blast_timer = 45; // Stunned on ground!
                actor_guard[blast_slot] = 0; // Punish window!
                duo_status_msg = "BLAST GROUNDED - FIRE!";
            }
            break;
        case BLAST_SLAM_RECOVER:
            blast_z = 0;
            actor_flash[blast_slot] = (blast_timer < 20) && (blast_timer & 4);
            if (blast_timer) blast_timer--;
            if (!blast_timer) {
                blast_state = BLAST_HOVER;
                blast_timer = sprocket_phase ? 40 : 65;
                blast_z = 12;
            }
            break;
        }

        // Apply sprite positions
        actor_x[blast_slot] = blast_x16 >> 8;
        actor_y[blast_slot] = (blast_y16 >> 8) - blast_z;
    }
}

void boss_update(void) BANKED {
    if (!boss_kind || boss_state == BOSS_WON) return;

    if (boss_kind == BOSS_DUO) {
        update_duo();
        return;
    }

    boss_hp = actor_type[boss_slot] ? actor_hp[boss_slot] : 0;
    if (!boss_hp) {
        warden_burst_left = warden_burst_timer = 0;
        boss_state = BOSS_WON;
        boss_reward = 1;
        if (actor_type[boss_slot]) enemies_killed++;
        uint8_t previous_oam = oam_end_last;
        actors_clear();
        oam_end_last = previous_oam;
        camera_shake(SHAKE_SMALL);
        return;
    }
    if (boss_kind == BOSS_GUNSHIP) { gunship_update(); return; }
    if (boss_kind == BOSS_WARDEN) warden_update();
    actor_flash[boss_slot] = boss_state == BOSS_WARN && (boss_timer & 4);
    if (boss_kind == BOSS_WARDEN) art(boss_slot, actor_guard[boss_slot] ? 0 : 1, actor_guard[boss_slot] ? SPR_PAL_WEAPON : SPR_PAL_SHOT);
    if (boss_state == BOSS_RELAYS) {
        boss_relays = 0;
        for (uint8_t n = 0; n < 2; n++) {
            uint8_t i = relay_slots[n];
            if (i == 0xFF || !actor_type[i] || !actor_hp[i]) continue;
            boss_relays++;
            actor_flash[i] = relay_clock < 30 && (relay_clock & 4);
        }
        if (!boss_relays) {
            clear_bullets();
            recover(100);
        } else if (!--relay_clock) {
            for (uint8_t n = 0; n < 2; n++) {
                uint8_t i = relay_slots[n];
                if (i == 0xFF || !actor_type[i] || !actor_hp[i]) continue;
                lock_direction(i);
                fire_bullet(i, boss_dx, boss_dy);
            }
            relay_clock = 100;
        }
        return;
    }
    if (boss_kind == BOSS_WARDEN) {
        if (boss_state == BOSS_RETREAT) {
            if (warden_chase()) recover(100);
            return;
        }
        if ((warden_floor == 0 && boss_hp <= 32) || (warden_floor == 1 && boss_hp <= 16)) {
            warden_floor++;
            if (boss_hp <= 24) boss_phase = 1;
            warden_burst_left = warden_burst_timer = 0;
            clear_bullets(); guard(1); boss_state = BOSS_RETREAT;
            return;
        }
        if (boss_hp <= 24) boss_phase = 1;
        if (!warden_near()) {
            warden_burst_left = warden_burst_timer = 0;
            clear_bullets(); guard(1); boss_state = BOSS_RECOVER; boss_timer = 72;
            return;
        }
        if (actor_guard[boss_slot] && boss_state == BOSS_RECOVER) guard(0);
    }
    if (boss_timer) boss_timer--;
    switch (boss_state) {
    case BOSS_RECOVER:
        if (boss_kind == BOSS_WARDEN && warden_burst_left && !--warden_burst_timer) {
            if (warden_shotgun) shotgun(); else ring();
            warden_burst_left--; warden_burst_timer = 18;
        }
        if (!boss_timer) {
            lock_direction(boss_slot);
            boss_state = BOSS_WARN;
            boss_timer = 42;
            guard(1);
        }
        break;
    case BOSS_WARN:
        if (boss_kind == BOSS_WARDEN) {
            uint8_t target = (boss_cycles & 1) ? 104 : 40;
            if (actor_x[boss_slot] < target) actor_x[boss_slot] += 2;
            else if (actor_x[boss_slot] > target) actor_x[boss_slot] -= 2;
        }
        if (!boss_timer) {
            if (boss_kind == BOSS_CLAIMJAW) {
                boss_state = BOSS_CHARGE;
                boss_timer = 44;
                boss_attacks++;
            } else {
                warden_shotgun = boss_cycles & 1;
                if (warden_shotgun) shotgun(); else ring();
                warden_burst_left = boss_phase ? 2 : 1;
                warden_burst_timer = 18;
                recover(boss_phase ? 54 : 72);
            }
        }
        break;
    case BOSS_CHARGE: {
        uint16_t x = actor_x[boss_slot] + boss_dx * 2;
        uint16_t y = actor_y[boss_slot] + boss_dy * 2;
        if (!boss_timer || !enemy_box_free(x, y)) {
            recover(100);
            camera_shake(SHAKE_SMALL);
        } else {
            actor_x[boss_slot] = x;
            actor_y[boss_slot] = y;
        }
        break;
    }
    }
}

// Draw custom boss layers (shadows, mortars, reticles)
void duo_draw(void) BANKED {
    for (uint8_t i = 4; i < 10; i++) shadow_OAM[i].y = 0;
    if (boss_kind != BOSS_DUO || boss_state == BOSS_WON) return;

    // 1. Blast Ground Shadow (Slots 4, 5)
    if (blast_hp > 0 && blast_z > 2) {
        int16_t sx = (blast_x16 >> 8) + 8 - cam_x;
        int16_t sy = (blast_y16 >> 8) + 12 + 16 - cam_y;
        if (sy > 0 && sy < 160 && sx > -8 && sx < 168) {
            shadow_OAM[4].x = sx;     shadow_OAM[4].y = sy; shadow_OAM[4].tile = SHADOW_VRAM; shadow_OAM[4].prop = SPR_PAL_UI;
            shadow_OAM[5].x = sx + 8; shadow_OAM[5].y = sy; shadow_OAM[5].tile = SHADOW_VRAM + 1; shadow_OAM[5].prop = SPR_PAL_UI;
        }
    }

    // 2. Mortar Ground Reticle (Slots 6, 7)
    if (mortar_active && mortar_timer > 0) {
        if (mortar_timer & 4) {
            int16_t mx = mortar_x + 8 - cam_x;
            int16_t my = mortar_y + 16 - cam_y;
            if (my > 0 && my < 160 && mx > -8 && mx < 168) {
                shadow_OAM[6].x = mx;     shadow_OAM[6].y = my; shadow_OAM[6].tile = MACHINE_VRAM + 12; shadow_OAM[6].prop = SPR_PAL_BULLET;
                shadow_OAM[7].x = mx + 8; shadow_OAM[7].y = my; shadow_OAM[7].tile = MACHINE_VRAM + 14; shadow_OAM[7].prop = SPR_PAL_BULLET;
            }
        }
    }
}

// Two text rows occupy the arena's top wall, not the combat area. HP + mechanic
// stay visible throughout the fight; only changed values are queued to VRAM.
void boss_status(void) BANKED {
    if (boss_kind == BOSS_WARDEN) { warden_status(); return; }

    if (boss_kind == BOSS_DUO) {
        if (boss_hp != shown_hp || blast_hp != shown_blast_hp) {
            uint8_t *row = gfx_strip(0, 0, 0, 20);
            const char *p1 = "SPROCKET:";
            for (uint8_t i = 0; i < 9; i++) row[i] = FONT_TILES + p1[i] - ' ';
            row[9] = boss_hp ? FONT_TILES + ('0' - ' ') + boss_hp / 10 : FONT_TILES + ('-' - ' ');
            row[10] = boss_hp ? FONT_TILES + ('0' - ' ') + boss_hp % 10 : FONT_TILES + ('-' - ' ');
            row[11] = FONT_TILES;
            const char *p2 = "BLAST:";
            for (uint8_t i = 0; i < 6; i++) row[12 + i] = FONT_TILES + p2[i] - ' ';
            row[18] = blast_hp ? FONT_TILES + ('0' - ' ') + blast_hp / 10 : FONT_TILES + ('-' - ' ');
            row[19] = blast_hp ? FONT_TILES + ('0' - ' ') + blast_hp % 10 : FONT_TILES + ('-' - ' ');
            shown_hp = boss_hp;
            shown_blast_hp = blast_hp;
        }
        if (duo_status_msg) {
            uint8_t *row = gfx_strip(1, 1, 0, 18);
            uint8_t n = 0;
            for (; n < 18 && duo_status_msg[n]; n++) row[n] = FONT_TILES + duo_status_msg[n] - ' ';
            for (; n < 18; n++) row[n] = FONT_TILES;
        }
        return;
    }

    if (boss_hp != shown_hp) {
        uint8_t *row = gfx_strip(12, 0, 0, 5);
        row[0] = FONT_TILES + ('H' - ' ');
        row[1] = FONT_TILES + ('P' - ' ');
        row[2] = FONT_TILES;
        row[3] = FONT_TILES + ('0' - ' ') + boss_hp / 10;
        row[4] = FONT_TILES + ('0' - ' ') + boss_hp % 10;
        shown_hp = boss_hp;
    }
    if (boss_state == shown_state && boss_relays == shown_relays && boss_phase == shown_phase) return;
    shown_state = boss_state; shown_relays = boss_relays; shown_phase = boss_phase;
    const char *s = "CORE OPEN         ";
    if (boss_state == BOSS_CRUISE) s = "ARMORED / BOMBS";
    else if (boss_state == BOSS_LASER_WARN) s = "ARMORED: LASER >>>";
    else if (boss_state == BOSS_LASER) s = "CORE OPEN - FIRE!";
    else if (boss_state == BOSS_RELAYS) s = boss_relays == 2 ? "BREAK 2 RELAYS    " : "BREAK 1 RELAY     ";
    else if (boss_state == BOSS_WARN) {
        if (boss_kind == BOSS_WARDEN) s = (boss_cycles & 1) ? "SHOTGUN - MOVE!" : "RINGS - FIND A GAP";
        else s = boss_dx > 0 ? "CHARGE RIGHT      " : boss_dx < 0 ? "CHARGE LEFT       " :
                 boss_dy > 0 ? "CHARGE DOWN       " : "CHARGE UP         ";
    } else if (boss_state == BOSS_CHARGE) s = "DODGE!            ";
    else if (boss_state == BOSS_REINFORCE) s = "BACKUP INCOMING   ";
    else if (boss_state == BOSS_WON) s = "CLEAR! SALVAGE +1  ";
    else if (boss_kind == BOSS_GUNSHIP) s = "CORE OPEN - FIRE!";
    else if (boss_kind == BOSS_CLAIMJAW) s = "VENT OPEN - FIRE! ";
    else if (boss_kind == BOSS_WARDEN) s = "CORE OPEN - FIRE!";
    uint8_t *row = gfx_strip(1, 1, 0, 18);
    uint8_t n = 0;
    for (; n < 18 && s[n]; n++) row[n] = FONT_TILES + s[n] - ' ';
    for (; n < 18; n++) row[n] = FONT_TILES;
}
