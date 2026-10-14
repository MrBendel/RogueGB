#pragma bank 255
// Small-screen encounter experiments. Reuse the collision/projectile pools;
// scripted actors get their own state machine and four compact machine sprites.
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

// Code-native pixel art, prepared once with display off. 0 is transparent.
// Closed Warden, exposed Warden, Claimjaw, relay. Each takes two 8x16 sprites.
static const char machine_art[4][16][17] = {
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
     "0000323333230000","0000333333330000","0000000000000000","0000000000000000"}
};

static void load_art(void) {
    uint8_t tiles[64];
    for (uint8_t f = 0; f < 4; f++) {
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
    boss_hp_max = kind == BOSS_CLAIMJAW ? 18 : 48;
    boss_slot = machine(72, 32, boss_hp_max, kind == BOSS_CLAIMJAW ? 2 : 0);
    boss_hp = boss_hp_max;
    boss_relays = kind == BOSS_WARDEN ? 2 : 0;
    if (boss_relays) {
        relay_slots[0] = machine(32, 56, 5, 3);
        relay_slots[1] = machine(112, 56, 5, 3);
    }
    boss_state = boss_relays ? BOSS_RELAYS : BOSS_RECOVER;
    boss_timer = 90; relay_clock = 100;
    guard(boss_relays != 0);
    // Identical starting resources for both trials, independent of lab tuning.
    health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
    shown_hp = shown_state = shown_relays = shown_phase = 0xFF;
    bkg_text(0, 0, "                    ");
    bkg_text(0, 1, "                    ");
    bkg_text(1, 0, kind == BOSS_CLAIMJAW ? "CLAIMJAW" : "WARDEN");
    boss_status();
}

static void lock_direction(uint8_t i) {
    int16_t dx = (int16_t)(player_x - actor_x[i]);
    int16_t dy = (int16_t)(player_y - actor_y[i]);
    uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    // Cardinal charges: the player can read the axis and step aside.
    boss_dx = ax > ay ? (dx < 0 ? -1 : 1) : 0;
    boss_dy = ax > ay ? 0 : (dy < 0 ? -1 : 1);
}

// Seven distinct trajectories across a cone, not duplicated cardinal shots.
static void shotgun(void) {
    static const int16_t spread[7] = {-224,-144,-72,0,72,144,224};
    uint8_t pellet=0;
    for (uint8_t k=0;k<BULLET_MAX && pellet<7;k++) {
        Bullet *b=&bullets[k];
        if (b->alive) continue;
        int16_t side=spread[pellet++];
        int16_t vx=boss_dx ? (boss_dx>0?384:-384) : side;
        int16_t vy=boss_dy ? (boss_dy>0?384:-384) : side;
        b->alive=1; b->xs=b->ys=0;
        b->x=actor_x[boss_slot]+4; b->y=actor_y[boss_slot];
        bullet_set_velocity(b,vx,vy);
        bullet_count++; bullets_fired++;
    }
    boss_attacks++;
}

static void ring(void) {
    for (uint8_t d=0; d<8; d++) fire_bullet(boss_slot, dx8[d], dy8[d]);
    boss_attacks++;
}

static void recover(uint8_t frames) {
    boss_state = BOSS_RECOVER;
    boss_timer = frames;
    guard(0);
    boss_cycles++;
}

static void reinforcements(void) {
    // Two spawn spots selected from four corners, away from the player.
    static const uint8_t xs[4] = {24,120,24,120};
    static const uint8_t ys[4] = {24,24,88,88};
    uint8_t left = 2;
    for (uint8_t n = 0; n < 4 && left; n++) {
        int16_t dx = (int16_t)(xs[n] - player_x), dy = (int16_t)(ys[n] - player_y);
        if (dx > -40 && dx < 40 && dy > -40 && dy < 40) continue;
        uint8_t i = actor_spawn(ACTOR_SLIME, xs[n], ys[n]);
        if (i == 0xFF) break;
        actor_timer[i] = 60; // grace period to read the new targets
        left--;
    }
}

void boss_update(void) BANKED {
    if (!boss_kind || boss_state == BOSS_WON) return;
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
    if (boss_kind == BOSS_WARDEN && !boss_phase && boss_hp <= boss_hp_max / 2) {
        boss_phase = 1;
        warden_burst_left = warden_burst_timer = 0;
        boss_state = BOSS_REINFORCE;
        boss_timer = 60;
        clear_bullets();
        guard(1);
        return;
    }
    if (boss_timer) boss_timer--;
    switch (boss_state) {
    case BOSS_RECOVER:
        if (boss_kind == BOSS_WARDEN && warden_burst_left && !--warden_burst_timer) {
            if (warden_shotgun) shotgun(); else ring();
            warden_burst_left--; warden_burst_timer=18;
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
                warden_shotgun=boss_cycles & 1;
                if (warden_shotgun) shotgun(); else ring();
                warden_burst_left=boss_phase?2:1;
                warden_burst_timer=18;
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
    case BOSS_REINFORCE:
        if (!boss_timer) { reinforcements(); recover(100); }
        break;
    }
}

// Two text rows occupy the arena's top wall, not the combat area. HP + mechanic
// stay visible throughout the fight; only changed values are queued to VRAM.
void boss_status(void) BANKED {
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
