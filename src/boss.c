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

// Trials replace chest graphics (the chest is not initialized here). Remain below
// tile 128, where sprite tiles would overlap the background's signed tile area.
#define MACHINE_VRAM 96
uint8_t boss_kind, boss_slot, boss_state, boss_timer, boss_phase;
uint8_t boss_hp, boss_hp_max, boss_relays, boss_reward;
uint16_t boss_attacks, boss_cycles;
int8_t boss_dx, boss_dy;
uint8_t warden_burst_left, warden_burst_timer, warden_shotgun;
static uint8_t relay_slots[2], relay_clock;
static uint8_t shown_hp, shown_state, shown_relays, shown_phase;
static const int8_t dx8[8] = {0,1,1,1,0,-1,-1,-1};
static const int8_t dy8[8] = {-1,-1,0,1,1,1,0,-1};

// Duo encounter state (Sprocket & Blast)
#define BLAST_IDLE 0
#define BLAST_WARN_DASH 1
#define BLAST_DASH 2
#define BLAST_WARN_SHOT 3
#define BLAST_RECOVER 4

static uint8_t blast_slot, blast_hp, blast_state, blast_timer;
static int8_t blast_dx, blast_dy;
static uint8_t sprocket_phase, sprocket_sweep_left, sprocket_sweep_base_d, sprocket_sweep_timer;
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

static uint8_t aim_8way(uint8_t i, int8_t *out_dx, int8_t *out_dy) {
    int16_t dx = (int16_t)(player_x - actor_x[i]);
    int16_t dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int8_t sx = (ax << 1) > ay ? (dx > 0 ? 1 : -1) : 0;
    int8_t sy = (ay << 1) > ax ? (dy > 0 ? 1 : -1) : 0;
    *out_dx = sx;
    *out_dy = sy;
    for (uint8_t d = 0; d < 8; d++) {
        if (dx8[d] == sx && dy8[d] == sy) return d;
    }
    return 0;
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
        // Place tactical cover pillars
        room_map[((uint16_t)3 << 5) | 3] = BLK_SOLID;
        room_map[((uint16_t)3 << 5) | 6] = BLK_SOLID;

        boss_hp_max = 20;
        boss_slot = machine(104, 28, 20, 4); // Sprocket Cannon (Frame 4)
        blast_slot = machine(32, 40, 14, 6); // Blast (Frame 6)
        boss_hp = 20;
        blast_hp = 14;
        sprocket_phase = 0;
        sprocket_sweep_left = 0;
        duo_attack_cycle = 0;
        blast_state = BLAST_IDLE;
        blast_timer = 40;
        boss_timer = 70;
        boss_state = BOSS_RECOVER;
        guard(1); // Frontal gun shield
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

    // Identical starting resources for all trials, independent of lab tuning.
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

// Seven distinct trajectories across a cone, not duplicated cardinal shots.
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

static void update_duo(void) {
    if (boss_state == BOSS_WON) return;

    // Track Sprocket HP
    if (sprocket_phase == 0) {
        boss_hp = actor_type[boss_slot] ? actor_hp[boss_slot] : 0;
        if (!boss_hp) {
            // Ejection! Cannon destroyed -> Sprocket leaps out on foot
            sprocket_phase = 1;
            boss_hp = 10;
            actor_hp[boss_slot] = 10;
            art(boss_slot, 5, SPR_PAL_SHOOTER); // Frame 5: on foot
            guard(0);
            clear_bullets();
            camera_shake(SHAKE_SMALL);
            boss_timer = 50;
            duo_status_msg = "CANNON DOWN! EJECT!";
        }
    } else {
        boss_hp = actor_type[boss_slot] ? actor_hp[boss_slot] : 0;
    }

    // Track Blast HP
    blast_hp = actor_type[blast_slot] ? actor_hp[blast_slot] : 0;

    // Win condition: both Sprocket and Blast are defeated
    if (!boss_hp && !blast_hp) {
        boss_state = BOSS_WON;
        boss_reward = 1;
        actors_clear();
        camera_shake(SHAKE_SMALL);
        duo_status_msg = "DUO DEFEATED! +1  ";
        return;
    }

    // --- SPROCKET UPDATE ---
    if (boss_hp > 0) {
        if (sprocket_phase == 0) {
            // Phase 1: Heavy Cannon
            if (sprocket_sweep_left > 0) {
                if (!--sprocket_sweep_timer) {
                    uint8_t d = (sprocket_sweep_base_d - 2 + (5 - sprocket_sweep_left)) & 7;
                    fire_bullet(boss_slot, dx8[d], dy8[d]);
                    sprocket_sweep_left--;
                    sprocket_sweep_timer = 6;
                    if (!sprocket_sweep_left) {
                        boss_timer = 60;
                        guard(0); // damage window after sweep
                        duo_status_msg = "VENT OPEN - FIRE! ";
                    }
                }
            } else if (boss_timer) {
                boss_timer--;
                actor_flash[boss_slot] = (boss_timer < 30) && (boss_timer & 4);
                if (!boss_timer) {
                    int16_t cdx = (int16_t)(player_x - actor_x[boss_slot]);
                    int16_t cdy = (int16_t)(player_y - actor_y[boss_slot]);
                    if (cdx > -28 && cdx < 28 && cdy > -28 && cdy < 28) {
                        // Point-blank repulsor pulse
                        fire_bullet(boss_slot, 0, 1);
                        fire_bullet(boss_slot, 0, -1);
                        fire_bullet(boss_slot, 1, 0);
                        fire_bullet(boss_slot, -1, 0);
                        camera_shake(SHAKE_SMALL);
                        boss_timer = 50;
                        guard(0);
                        duo_status_msg = "REPULSOR PULSE!   ";
                    } else if (duo_attack_cycle++ & 1) {
                        // 3-way Mortar Spread
                        int8_t sx, sy;
                        uint8_t d = aim_8way(boss_slot, &sx, &sy);
                        fire_bullet(boss_slot, dx8[d], dy8[d]);
                        fire_bullet(boss_slot, dx8[(d + 1) & 7], dy8[(d + 1) & 7]);
                        fire_bullet(boss_slot, dx8[(d + 7) & 7], dy8[(d + 7) & 7]);
                        camera_shake(SHAKE_SMALL);
                        boss_timer = 70;
                        guard(0); // damage window!
                        duo_status_msg = "VENT OPEN - FIRE! ";
                    } else {
                        // Start Sweeping Fan Barrage
                        int8_t sx, sy;
                        sprocket_sweep_base_d = aim_8way(boss_slot, &sx, &sy);
                        sprocket_sweep_left = 5;
                        sprocket_sweep_timer = 1;
                        guard(1);
                        duo_status_msg = "SWEEP BARRAGE!   ";
                    }
                } else if (boss_timer == 35) {
                    guard(1); // Shield comes up before firing
                    duo_status_msg = (duo_attack_cycle & 1) ? "MORTAR INCOMING! " : "SWEEP INCOMING!  ";
                }
            }
        } else {
            // Phase 2: Sprocket on Foot (Aggressive Shotgun Skirmisher)
            if (boss_timer) boss_timer--;
            int8_t mx = (player_x > actor_x[boss_slot] + 4) ? 1 : (player_x < actor_x[boss_slot] - 4) ? -1 : 0;
            int8_t my = (player_y > actor_y[boss_slot] + 4) ? 1 : (player_y < actor_y[boss_slot] - 4) ? -1 : 0;
            uint16_t nx = actor_x[boss_slot] + mx;
            uint16_t ny = actor_y[boss_slot] + my;
            if (enemy_box_free(nx, ny)) {
                actor_x[boss_slot] = nx;
                actor_y[boss_slot] = ny;
            }
            actor_flash[boss_slot] = (boss_timer < 20) && (boss_timer & 4);
            if (!boss_timer) {
                int8_t sx, sy;
                uint8_t d = aim_8way(boss_slot, &sx, &sy);
                fire_bullet(boss_slot, dx8[d], dy8[d]);
                fire_bullet(boss_slot, dx8[(d + 1) & 7], dy8[(d + 1) & 7]);
                fire_bullet(boss_slot, dx8[(d + 7) & 7], dy8[(d + 7) & 7]);
                boss_timer = 65;
            }
        }
    }

    // --- BLAST UPDATE (Jetpack Skirmisher) ---
    if (blast_hp > 0) {
        if (blast_timer) blast_timer--;
        switch (blast_state) {
        case BLAST_IDLE: {
            int16_t bdx = (int16_t)(player_x - actor_x[blast_slot]);
            int16_t bdy = (int16_t)(player_y - actor_y[blast_slot]);
            uint16_t dist = (bdx < 0 ? -bdx : bdx) + (bdy < 0 ? -bdy : bdy);
            int8_t mx = 0, my = 0;
            if (dist > 54) {
                mx = bdx > 0 ? 1 : -1;
                my = bdy > 0 ? 1 : -1;
            } else if (dist < 30) {
                mx = bdx > 0 ? -1 : 1;
                my = bdy > 0 ? -1 : 1;
            } else {
                mx = bdy > 0 ? -1 : 1;
                my = bdx > 0 ? 1 : -1;
            }
            uint16_t bx = actor_x[blast_slot] + mx;
            uint16_t by = actor_y[blast_slot] + my;
            if (enemy_box_free(bx, by)) {
                actor_x[blast_slot] = bx;
                actor_y[blast_slot] = by;
            }

            if (!blast_timer) {
                if ((duo_attack_cycle & 2) == 0) {
                    aim_8way(blast_slot, &blast_dx, &blast_dy);
                    blast_state = BLAST_WARN_DASH;
                    blast_timer = 24;
                    duo_status_msg = "JETPACK DIVE!     ";
                } else {
                    blast_state = BLAST_WARN_SHOT;
                    blast_timer = 20;
                    duo_status_msg = "GRENADE OUT!      ";
                }
            }
            break;
        }
        case BLAST_WARN_DASH:
            actor_flash[blast_slot] = blast_timer & 4;
            if (!blast_timer) {
                blast_state = BLAST_DASH;
                blast_timer = 20;
            }
            break;
        case BLAST_DASH: {
            uint16_t dnx = actor_x[blast_slot] + blast_dx * 2;
            uint16_t dny = actor_y[blast_slot] + blast_dy * 2;
            if (!blast_timer || !enemy_box_free(dnx, dny)) {
                blast_state = BLAST_RECOVER;
                blast_timer = 30;
                camera_shake(SHAKE_SMALL);
            } else {
                actor_x[blast_slot] = dnx;
                actor_y[blast_slot] = dny;
            }
            break;
        }
        case BLAST_WARN_SHOT:
            actor_flash[blast_slot] = blast_timer & 4;
            if (!blast_timer) {
                int8_t sx, sy;
                aim_8way(blast_slot, &sx, &sy);
                fire_bullet(blast_slot, sx, sy);
                blast_state = BLAST_RECOVER;
                blast_timer = sprocket_phase ? 25 : 45;
            }
            break;
        case BLAST_RECOVER:
            if (!blast_timer) {
                blast_state = BLAST_IDLE;
                blast_timer = sprocket_phase ? 40 : 80;
            }
            break;
        }
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
        boss_reward = 1; // one trial salvage token; no campaign inventory yet
        if (actor_type[boss_slot]) enemies_killed++; // melee killed it after the last reap
        uint8_t previous_oam = oam_end_last;
        actors_clear(); // victory removes remaining projectiles and adds
        // Unlike a room transition the display stays on. Let actors_update hide
        // every sprite emitted by the previous frame instead of leaving ghosts.
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
            recover(100); // clear transition and a generous first damage window
        } else if (!--relay_clock) {
            for (uint8_t n = 0; n < 2; n++) {
                uint8_t i = relay_slots[n];
                if (i == 0xFF || !actor_type[i] || !actor_hp[i]) continue;
                // Aim relays independently; boss lock is unused during this phase.
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
