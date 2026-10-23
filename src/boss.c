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
#include "oam.h"
#include "scrapback.h"
#include "cragback.h"
#include "globfather.h"
#include "flynt.h"

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
// BOOM & BEWM (FAITHFUL SOUTHERN SHELF WALKTHROUGH ENCOUNTER)
// -------------------------------------------------------------

// Big Bertha (Mounted Heavy Artillery Emplacement)
#define BERTHA_AIM 0       // 35 frames: tracks player, barrel flashes, armored (guard=1)
#define BERTHA_BARRAGE 1   // 22 frames: fires 3 fast lethal rockets (2.5 px/f) at player's position
#define BERTHA_RELOAD 2    // 60 frames: vents steam, shield down (guard=0), PUNISH WINDOW!

static uint8_t bertha_state, bertha_timer, bertha_shots_left, bertha_shot_cooldown;
static uint8_t boom_phase, boom_dead, bewm_dead; // 0 = mounted in Bertha (20 HP), 1 = on foot (12 HP)

// Bewm (The Fast Jetpack Flusher)
#define BEWM_ORBIT 0         // 40 frames: swoops with thrusters, altitude z=14
#define BEWM_FLUSH_GRENADE 1 // flies to player's cover position and drops a grenade!
#define BEWM_SHOTGUN_RUN 2   // swoops down close (z=4) and fires a twin pellet spread
#define BEWM_STUNNED 3       // 35 frames: stunned / venting on ground (z=0), PUNISH WINDOW!

static uint8_t bewm_slot, bewm_hp, bewm_state, bewm_timer;
static int16_t bewm_x16, bewm_y16;   // 8.8 position
static int16_t bewm_vx, bewm_vy;     // 8.8 velocity with easing
static uint8_t bewm_z;               // altitude (0 to 18)
static uint8_t bewm_orbit_angle;

// Cover fins, grenades, and Psycho adds
static uint8_t grenade_active, grenade_timer, grenade_x, grenade_y;
static uint8_t psycho_slot, psycho_timer;
static const char *duo_status_msg;
static uint8_t shown_bewm_hp;

// Code-native pixel art, prepared once with display off. 0 is transparent.
// 0: Closed Warden, 1: exposed Warden, 2: Claimjaw, 3: relay.
// 4: Big Bertha (Aim Down), 5: Boom on Foot, 6: Bewm (Jetpack), 7: Big Bertha (Aim Angled Left).
static const char machine_art[8][16][17] = {
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
    {"0000333333330000","0033222222223300","0322111111112230","3321133333311233",
     "3221332222331223","3333333333333333","0000032112300000","0000032112300000",
     "0000032112300000","0000032112300000","0000032112300000","0000332112330000",
     "0000311221130000","0000330000330000","0000033333300000","0000000000000000"},
    {"0000333333330000","0033222222223300","0322333333332230","0323111111113230",
     "0323111111113230","0333333333333330","0032222222223000","3332222222223330",
     "3113222222223113","3113333333333113","3330322222230333","0000322222230000",
     "0000322002230000","0003322002233000","0003333003333000","0000000000000000"},
    {"0000003333000000","0000332222330000","0003221111223000","0003221111223000",
     "0000332222330000","0033333333333300","0322332222332230","0321332222331230",
     "0333333333333330","0033032222303300","0031303223031300","0003003223003000",
     "0000003223000000","0000032002300000","0000033003300000","0000000000000000"},
    {"0000333333330000","0033222222223300","0322111111112230","3321133333311233",
     "3221332222331223","3333333333333333","0000321123000000","0003211230000000",
     "0032112300000000","0321123000000000","3211230000000000","3211233000000000",
     "3112211300000000","3300003300000000","0333333000000000","0000000000000000"}
};

static void load_art(void) {
    uint8_t tiles[64];
    for (uint8_t f = 0; f < 8; f++) {
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

static void art_flip(uint8_t i, uint8_t frame, uint8_t pal, uint8_t flip_x) {
    uint8_t base = MACHINE_VRAM + (frame << 2);
    if (flip_x) {
        actor_t0[i] = base + 2;
        actor_t1[i] = base;
        actor_p0[i] = actor_p1[i] = pal | 0x20; // S_FLIPX
    } else {
        actor_t0[i] = base;
        actor_t1[i] = base + 2;
        actor_p0[i] = actor_p1[i] = pal;
    }
}

static void update_bertha_aim(void) {
    if (boss_slot == 0xFF) return;
    uint8_t pal = actor_guard[boss_slot] ? SPR_PAL_SHOOTER : SPR_PAL_SHOT;
    if (player_x + 8 < 68) {
        art_flip(boss_slot, 7, pal, 0); // Aiming Angled Down-Left
    } else if (player_x + 8 > 92) {
        art_flip(boss_slot, 7, pal, 1); // Aiming Angled Down-Right
    } else {
        art_flip(boss_slot, 4, pal, 0); // Aiming Straight Down
    }
}

static void guard(uint8_t on) {
    if (boss_slot == 0xFF) return;
    actor_guard[boss_slot] = on;
    if (boss_kind == BOSS_WARDEN) art(boss_slot, on ? 0 : 1, on ? SPR_PAL_WEAPON : SPR_PAL_SHOT);
    else if (boss_kind == BOSS_DUO && boom_phase == 0) update_bertha_aim();
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
    room_vis_from_map();
    room_randomize_floor();
}

static uint8_t machine(uint8_t x, uint8_t y, uint8_t hp, uint8_t frame) {
    uint8_t i = actor_spawn(ACTOR_PEASHOOTER, x, y);
    if (i == 0xFF) return i;
    actor_scripted[i] = 1;
    actor_hp[i] = hp;
    art(i, frame, frame == 2 ? SPR_PAL_SLIME : SPR_PAL_SHOOTER);
    return i;
}

// True 360-degree vector aim directly at the player using 16-bit integer math
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
    if (x < 16) x = 16; else if (x > 144) x = 144;
    if (y < 20) y = 20; else if (y > 108) y = 108;
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

void boss_start(uint8_t kind) BANKED {
    if (kind == BOSS_GLOBFATHER) {
        globfather_start();shown_hp=shown_state=shown_relays=shown_phase=0xFF;boss_status();return;
    }
    if (kind == BOSS_CRAGBACK) {
        cragback_start();shown_hp=shown_state=shown_relays=shown_phase=0xFF;boss_status();return;
    }
    if (kind == BOSS_SCRAPBACK) {
        scrapback_start();
        shown_hp = shown_state = shown_relays = shown_phase = 0xFF;
        boss_status();return;
    }
    if (kind == BOSS_GUNSHIP) {
        gunship_start();
        shown_hp = shown_state = shown_relays = shown_phase = 0xFF;
        boss_status();
        return;
    }
    if (kind == BOSS_FLYNT) {
        flynt_start();
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
        // Place the 2 solid cover fins from the walkthrough
        room_map[((uint16_t)4 << 5) | 3] = BLK_SOLID;
        room_map[((uint16_t)4 << 5) | 6] = BLK_SOLID;

        boom_dead = bewm_dead = 0;
        enemy_bullet_damage = BULLET_DAMAGE;
        enemy_bullet_cause = CAUSE_PEA;

        boss_hp_max = 20;
        boss_slot = machine(72, 20, 20, 4);  // Big Bertha Emplacement (Frame 4)
        bewm_slot = machine(32, 40, 12, 6);  // Bewm (Frame 6)
        boss_hp = 20;
        bewm_hp = 12;

        boom_phase = 0;
        bertha_state = BERTHA_AIM;
        bertha_timer = 35;
        bertha_shots_left = 0;
        bertha_shot_cooldown = 0;

        bewm_state = BEWM_ORBIT;
        bewm_timer = 40;
        bewm_x16 = 32 << 8;
        bewm_y16 = 40 << 8;
        bewm_vx = bewm_vy = 0;
        bewm_z = 14;
        bewm_orbit_angle = 0;

        grenade_active = 0;
        grenade_timer = 0;
        psycho_slot = 0xFF;
        psycho_timer = 120; // First Psycho add arrives after 4 seconds

        guard(1); // Frontal armor protects Bertha
        duo_status_msg = "TAKE COVER!       ";
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
    shown_hp = shown_state = shown_relays = shown_phase = shown_bewm_hp = 0xFF;
    if (kind == BOSS_WARDEN) { boss_status(); return; }
    bkg_text(0, 0, "                    ");
    bkg_text(0, 1, "                    ");
    bkg_text(1, 0, kind == BOSS_CLAIMJAW ? "CLAIMJAW" : kind == BOSS_DUO ? "BOOM & BEWM" : "WARDEN");
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
// FAITHFUL BOOM & BEWM COMBAT LOOP
// -------------------------------------------------------------
static void update_duo(void) {
    if (boss_state == BOSS_WON) return;

    // 1. Track Bertha & Boom HP
    if (boom_phase == 0) {
        if (boss_slot != 0xFF && actor_type[boss_slot] && actor_hp[boss_slot] > 0) {
            boss_hp = actor_hp[boss_slot];
        } else {
            // Big Bertha Destroyed -> Flush Boom out on Foot!
            boom_phase = 1;
            boss_hp = 12;
            boss_slot = machine(72, 48, 12, 5); // Cleanly re-spawn Boom into actor table
            guard(0);
            clear_bullets();
            camera_shake(SHAKE_BIG);
            bertha_timer = 40;
            duo_status_msg = "BOOM ON FOOT!     ";
        }
    } else {
        if (!boom_dead) {
            if (boss_slot != 0xFF && actor_type[boss_slot] && actor_hp[boss_slot] > 0) {
                boss_hp = actor_hp[boss_slot];
            } else {
                boss_hp = 0;
                boom_dead = 1;
                boss_slot = 0xFF;
            }
        }
    }

    // 2. Track Bewm HP
    if (!bewm_dead) {
        if (bewm_slot != 0xFF && actor_type[bewm_slot] && actor_hp[bewm_slot] > 0) {
            bewm_hp = actor_hp[bewm_slot];
        } else {
            bewm_hp = 0;
            bewm_dead = 1;
            bewm_slot = 0xFF;
        }
    }

    // 3. Victory: both defeated
    if (boom_dead && bewm_dead) {
        boss_state = BOSS_WON;
        boss_reward = 1;
        actors_clear();
        camera_shake(SHAKE_SMALL);
        duo_status_msg = "BOOM & BEWM DOWN! ";
        return;
    }

    // ---------------------------------------------------------
    // 1. BIG BERTHA (Mounted Emplacement / Boom on Foot)
    // ---------------------------------------------------------
    if (!boom_dead && boss_slot != 0xFF && actor_type[boss_slot]) {
        if (boom_phase == 0) {
            // Big Bertha Mounted Artillery
            switch (bertha_state) {
            case BERTHA_AIM:
                update_bertha_aim();
                actor_flash[boss_slot] = bertha_timer & 4;
                if (bertha_timer) bertha_timer--;
                if (!bertha_timer) {
                    bertha_state = BERTHA_BARRAGE;
                    bertha_shots_left = 3;
                    bertha_shot_cooldown = 1;
                    duo_status_msg = "MISSILES FIRED!   ";
                }
                break;
            case BERTHA_BARRAGE:
                actor_flash[boss_slot] = 1;
                if (!--bertha_shot_cooldown) {
                    uint16_t bx = actor_x[boss_slot] + 4;
                    if (player_x + 8 < 68) bx = actor_x[boss_slot] + 1;
                    else if (player_x + 8 > 92) bx = actor_x[boss_slot] + 7;
                    fire_aimed_bullet(bx, actor_y[boss_slot] + 12, 5);
                    camera_shake(SHAKE_SMALL);
                    bertha_shots_left--;
                    bertha_shot_cooldown = 7;
                    if (!bertha_shots_left) {
                        bertha_state = BERTHA_RELOAD;
                        bertha_timer = 60; // 1 full second reload window!
                        guard(0); // SHIELD DROPS! AUTO-AIM LOCKS ON!
                        art_flip(boss_slot, 4, SPR_PAL_SHOT, 0);
                        duo_status_msg = "RELOADING - FIRE! ";
                    }
                }
                break;
            case BERTHA_RELOAD:
                art_flip(boss_slot, 4, SPR_PAL_SHOT, 0);
                actor_flash[boss_slot] = (bertha_timer < 20) && (bertha_timer & 4);
                if (bertha_timer) bertha_timer--;
                if (!bertha_timer) {
                    bertha_state = BERTHA_AIM;
                    bertha_timer = 35;
                    guard(1); // Armor back up
                    update_bertha_aim();
                    duo_status_msg = "TAKE COVER!       ";
                }
                break;
            }
        } else {
            // Boom on Foot: aggressive assault rifle skirmisher
            if (bertha_timer) bertha_timer--;
            int8_t mx = (player_x > actor_x[boss_slot] + 4) ? 1 : (player_x < actor_x[boss_slot] - 4) ? -1 : 0;
            int8_t my = (player_y > actor_y[boss_slot] + 4) ? 1 : (player_y < actor_y[boss_slot] - 4) ? -1 : 0;
            uint16_t nx = actor_x[boss_slot] + mx * 2;
            uint16_t ny = actor_y[boss_slot] + my * 2;
            if (enemy_box_free(nx, ny)) {
                actor_x[boss_slot] = nx;
                actor_y[boss_slot] = ny;
            }
            actor_flash[boss_slot] = (bertha_timer < 15) && (bertha_timer & 4);
            if (!bertha_timer) {
                fire_aimed_bullet(actor_x[boss_slot] + 4, actor_y[boss_slot] + 4, 3);
                fire_aimed_bullet(actor_x[boss_slot] + 2, actor_y[boss_slot] + 2, 3);
                bertha_timer = 40;
            }
        }
    }

    // ---------------------------------------------------------
    // 2. BEWM (The Fast Jetpack Flusher)
    // ---------------------------------------------------------
    if (!bewm_dead && bewm_slot != 0xFF && actor_type[bewm_slot]) {
        switch (bewm_state) {
        case BEWM_ORBIT: {
            bewm_orbit_angle += 2;
            static const int8_t cos16[16] = {48, 44, 34, 18, 0, -18, -34, -44, -48, -44, -34, -18, 0, 18, 34, 44};
            static const int8_t sin16[16] = {0, 18, 34, 44, 48, 44, 34, 18, 0, -18, -34, -44, -48, -44, -34, -18};
            uint8_t a = (bewm_orbit_angle >> 3) & 15;
            int16_t tx = (player_x + 8) + cos16[a];
            int16_t ty = (player_y + 8) + sin16[a];
            if (tx < 24) tx = 24; else if (tx > 136) tx = 136;
            if (ty < 24) ty = 24; else if (ty > 104) ty = 104;

            int16_t cur_x = bewm_x16 >> 8, cur_y = bewm_y16 >> 8;
            int16_t desired_vx = (tx - cur_x) * 8;
            int16_t desired_vy = (ty - cur_y) * 8;
            if (desired_vx > 320) desired_vx = 320; else if (desired_vx < -320) desired_vx = -320;
            if (desired_vy > 320) desired_vy = 320; else if (desired_vy < -320) desired_vy = -320;

            bewm_vx += (desired_vx - bewm_vx) >> 3;
            bewm_vy += (desired_vy - bewm_vy) >> 3;
            bewm_x16 += bewm_vx;
            bewm_y16 += bewm_vy;

            bewm_z = 14 + (((bewm_orbit_angle >> 2) & 7) < 4 ? ((bewm_orbit_angle >> 2) & 3) : (3 - ((bewm_orbit_angle >> 2) & 3)));

            if (bewm_timer) bewm_timer--;
            if (!bewm_timer) {
                // If player is hiding behind a cover fin, flush them with a grenade!
                if (player_y >= 64 && (player_x < 56 || player_x > 88)) {
                    bewm_state = BEWM_FLUSH_GRENADE;
                    bewm_timer = 24;
                    grenade_active = 1;
                    grenade_timer = 32;
                    grenade_x = player_x;
                    grenade_y = player_y;
                    duo_status_msg = "BEWM: GRENADE OUT!";
                } else {
                    bewm_state = BEWM_SHOTGUN_RUN;
                    bewm_timer = 20;
                    bewm_z = 6; // Swoops low
                    duo_status_msg = "BEWM: SHOTGUN DIVE!";
                }
            }
            break;
        }
        case BEWM_FLUSH_GRENADE:
            actor_flash[bewm_slot] = bewm_timer & 4;
            bewm_x16 += bewm_vx;
            bewm_y16 += bewm_vy;
            if (bewm_timer) bewm_timer--;
            if (!bewm_timer) {
                bewm_state = BEWM_ORBIT;
                bewm_timer = 45;
            }
            break;
        case BEWM_SHOTGUN_RUN:
            bewm_z = 6;
            bewm_x16 += bewm_vx;
            bewm_y16 += bewm_vy;
            if (bewm_timer) bewm_timer--;
            if (!bewm_timer) {
                fire_aimed_bullet(bewm_x16 >> 8, bewm_y16 >> 8, 3);
                fire_aimed_bullet((bewm_x16 >> 8) + 4, bewm_y16 >> 8, 3);
                bewm_state = BEWM_ORBIT;
                bewm_timer = 45;
                bewm_z = 14;
            }
            break;
        case BEWM_STUNNED:
            bewm_z = 0;
            actor_flash[bewm_slot] = bewm_timer & 4;
            if (bewm_timer) bewm_timer--;
            if (!bewm_timer) {
                bewm_state = BEWM_ORBIT;
                bewm_timer = 50;
                bewm_z = 14;
            }
            break;
        }

        int16_t by = (bewm_y16 >> 8) - bewm_z;
        if (by < 16) by = 16;
        actor_x[bewm_slot] = bewm_x16 >> 8;
        actor_y[bewm_slot] = (uint16_t)by;
    }

    // ---------------------------------------------------------
    // 3. GRENADE HAZARD UPDATE
    // ---------------------------------------------------------
    if (grenade_active) {
        if (grenade_timer) grenade_timer--;
        if (!grenade_timer) {
            grenade_active = 0;
            camera_shake(SHAKE_SMALL);
            if (player_hittable()) {
                int16_t gdx = (int16_t)(player_x + PLAYER_HB_X - grenade_x);
                int16_t gdy = (int16_t)(player_y + PLAYER_HB_Y - grenade_y);
                if (gdx > -18 && gdx < 18 && gdy > -18 && gdy < 18) {
                    hurt_cause = 10;
                    player_hurt(12, grenade_x + 8, grenade_y + 8);
                }
            }
            fire_direct_bullet(grenade_x + 4, grenade_y + 4, 256, 256);
            fire_direct_bullet(grenade_x + 4, grenade_y + 4, -256, 256);
            fire_direct_bullet(grenade_x + 4, grenade_y + 4, 256, -256);
            fire_direct_bullet(grenade_x + 4, grenade_y + 4, -256, -256);
        }
    }

    // ---------------------------------------------------------
    // 4. PSYCHO ADD REINFORCEMENTS (Fodder & Second Wind)
    // ---------------------------------------------------------
    if (psycho_timer) psycho_timer--;
    if (!psycho_timer) {
        if (psycho_slot == 0xFF || !actor_type[psycho_slot]) {
            psycho_slot = actor_spawn(ACTOR_SLIME, (player_x > 80 ? 24 : 128), 104);
            if (psycho_slot != 0xFF) {
                actor_hp[psycho_slot] = 3; // Dies quickly in 1-2 hits
                actor_timer[psycho_slot] = 30;
            }
        }
        psycho_timer = 150; // Every 5 seconds
    }
}

void boss_update(void) BANKED {
    if (!boss_kind || boss_state == BOSS_WON) return;

    if (boss_kind == BOSS_GLOBFATHER) { globfather_update(); return; }
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
    if (boss_kind == BOSS_SCRAPBACK) { scrapback_update(); return; }
    if (boss_kind == BOSS_CRAGBACK) { cragback_update(); return; }
    if (boss_kind == BOSS_FLYNT) { flynt_update(); return; }
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

// Draw custom boss layers (Bewm shadow, grenade reticle)
// Uses OAM_PROP (slots 11..14), leaving player (3..4), guns (1..2, 5..6), shadow (7..8), reticle (9..10) completely intact!
void duo_draw(void) BANKED {
    for (uint8_t i = OAM_PROP; i < OAM_FIRST_ACTOR; i++) shadow_OAM[i].y = 0;
    if (boss_kind != BOSS_DUO || boss_state == BOSS_WON) return;

    // 1. Bewm Ground Shadow (Slots 11, 12)
    if (!bewm_dead && bewm_slot != 0xFF && bewm_hp > 0 && bewm_z > 2) {
        int16_t sx = (bewm_x16 >> 8) + 8 - cam_x;
        int16_t sy = (bewm_y16 >> 8) + 12 + 16 - cam_y;
        if (sy > 0 && sy < 160 && sx > -8 && sx < 168) {
            shadow_OAM[OAM_PROP].x = sx;         shadow_OAM[OAM_PROP].y = sy; shadow_OAM[OAM_PROP].tile = SHADOW_VRAM; shadow_OAM[OAM_PROP].prop = SPR_PAL_UI;
            shadow_OAM[OAM_PROP + 1].x = sx + 8; shadow_OAM[OAM_PROP + 1].y = sy; shadow_OAM[OAM_PROP + 1].tile = SHADOW_VRAM + 1; shadow_OAM[OAM_PROP + 1].prop = SPR_PAL_UI;
        }
    }

    // 2. Grenade Ground Reticle (Slots 13, 14)
    if (grenade_active && grenade_timer > 0) {
        if (grenade_timer & 4) {
            int16_t mx = grenade_x + 8 - cam_x;
            int16_t my = grenade_y + 16 - cam_y;
            if (my > 0 && my < 160 && mx > -8 && mx < 168) {
                shadow_OAM[OAM_PROP + 2].x = mx;     shadow_OAM[OAM_PROP + 2].y = my; shadow_OAM[OAM_PROP + 2].tile = MACHINE_VRAM + 12; shadow_OAM[OAM_PROP + 2].prop = SPR_PAL_BULLET;
                shadow_OAM[OAM_PROP + 3].x = mx + 8; shadow_OAM[OAM_PROP + 3].y = my; shadow_OAM[OAM_PROP + 3].tile = MACHINE_VRAM + 14; shadow_OAM[OAM_PROP + 3].prop = SPR_PAL_BULLET;
            }
        }
    }
}

// Two text rows occupy the arena's top wall, not the combat area.
void boss_status(void) BANKED {
    if (boss_kind == BOSS_FLYNT) { flynt_status(); return; }
    if (boss_kind == BOSS_WARDEN) { warden_status(); return; }

    if (boss_kind == BOSS_DUO) {
        if (boss_hp != shown_hp || bewm_hp != shown_bewm_hp) {
            uint8_t *row = gfx_strip(0, 0, 0, 20);
            const char *p1 = boom_phase ? "BOOM:  " : "BERTHA:";
            for (uint8_t i = 0; i < 7; i++) row[i] = FONT_TILES + p1[i] - ' ';
            row[7] = boss_hp ? FONT_TILES + ('0' - ' ') + boss_hp / 10 : FONT_TILES + ('-' - ' ');
            row[8] = boss_hp ? FONT_TILES + ('0' - ' ') + boss_hp % 10 : FONT_TILES + ('-' - ' ');
            row[9] = FONT_TILES;
            const char *p2 = "BEWM:";
            for (uint8_t i = 0; i < 5; i++) row[10 + i] = FONT_TILES + p2[i] - ' ';
            row[15] = bewm_hp ? FONT_TILES + ('0' - ' ') + bewm_hp / 10 : FONT_TILES + ('-' - ' ');
            row[16] = bewm_hp ? FONT_TILES + ('0' - ' ') + bewm_hp % 10 : FONT_TILES + ('-' - ' ');
            row[17] = row[18] = row[19] = FONT_TILES;
            shown_hp = boss_hp;
            shown_bewm_hp = bewm_hp;
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
    if (boss_kind == BOSS_SCRAPBACK && boss_state != BOSS_WON) {
        s = boss_state == BOSS_RECOVER ? "SHELL OPEN - FIRE!" :
            boss_state == BOSS_WARN ? "CHARGE INCOMING!" :
            boss_state == BOSS_CHARGE ? "DODGE SIDEWAYS!" : "BAIT A WALL CRASH";
    }
    if (boss_kind == BOSS_CRAGBACK && boss_state != BOSS_WON) {
        s = boss_state == BOSS_REINFORCE ? "HOPPING - MOVE!" : boss_state == BOSS_WARN ? "ROCK INCOMING!" : boss_state == BOSS_CHARGE ? "DODGE THE ROCK!" :
            boss_state == BOSS_RELAYS ? "SLAM - GET BACK!" : boss_state == BOSS_RECOVER ? "DIGGING - FIRE!" : "KEEP YOUR DISTANCE";
    }
    if (boss_kind == BOSS_GLOBFATHER && boss_state != BOSS_WON) {
        s=boss_state==BOSS_WARN?"HOP INCOMING!":boss_state==BOSS_CHARGE?"JUMP THE SPLASH!":
          boss_phase==3?"RESTING - FIRE!":boss_phase==2?"SPLIT - FIRE!":"FINISH THE SLIMES!";
    }
    uint8_t *row = gfx_strip(1, 1, 0, 18);
    uint8_t n = 0;
    for (; n < 18 && s[n]; n++) row[n] = FONT_TILES + s[n] - ' ';
    for (; n < 18; n++) row[n] = FONT_TILES;
}
