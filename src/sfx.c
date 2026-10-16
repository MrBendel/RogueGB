#pragma bank 255
// Placeholder sound effects: see sfx.h. Banked (a short call a frame).
//
// Register reminders (Pan Docs, "Audio"): a square channel's period x gives
// 131072 / (2048 - x) Hz. NRx2 is the envelope: start volume (bits 7-4), up (bit
// 3), pace (bits 2-0, 0 = hold). NR10 is channel 1's sweep: pace (6-4), down
// (bit 3), step (2-0). NR43 is the noise: clock shift (7-4, higher = lower),
// 7-bit "metallic" mode (bit 3), divider (2-0). Writing bit 7 of NRx4 restarts
// the channel.
#include <gb/gb.h>
#include "sfx.h"
#include "aim.h"
#include "actor.h"
#include "player.h"
#include "health.h"
#include "weapon.h"
#include "chest.h"

// A step: how many frames before the next one (for the last step: how long the
// sound holds its channel against lower priorities), then the channel's registers:
// sweep (channel 1 only), duty/length, envelope, period low, period high (or for
// noise: length, envelope, the noise setting, 0).
typedef struct {
    uint8_t frames;
    uint8_t sweep, duty, env, lo, hi;
} Step;

#define P(x) ((x) & 0xFF), (((x) >> 8) & 7)  // a period, low and high bits
#define DUTY12 0x00
#define DUTY25 0x40
#define DUTY50 0x80

// --- the sounds ---
// Gunshots: a square blip swept down fast ("pew"), each gun its own pitch and duty.
static const Step pistol[] = {{4, 0x1C, DUTY50, 0xA2, P(1880)}};
static const Step rifle[] = {{3, 0x1B, DUTY25, 0x91, P(1930)}};
static const Step shotgun[] = {{10, 0, 0, 0xF4, 0x51, 0}};  // a low blast of noise
static const Step sniper[] = {{10, 0, 0, 0xF3, 0x33, 0}};   // a sharp crack
static const Step crit[] = {{12, 0, 0, 0xF5, 0x22, 0}};
static const Step sword[] = {{2, 0, 0, 0x71, 0x13, 0}, {3, 0, 0, 0x51, 0x22, 0}};  // a swish
static const Step hit[] = {{2, 0, 0, 0x91, 0x41, 0}};
static const Step kill[] = {{4, 0, 0, 0xC3, 0x55, 0}, {4, 0, 0, 0x93, 0x66, 0}};
static const Step hurt[] = {{15, 0x2F, DUTY12, 0xF3, P(1600)}};  // a harsh drop
static const Step shield_hit[] = {{3, 0, DUTY50, 0x92, P(1985)}, {1, 0, DUTY50, 0x61, P(1990)}};  // a ping
static const Step shield_break[] = {{6, 0, 0, 0xF5, 0x29, 0}, {4, 0, 0, 0xA4, 0x3A, 0}};  // glass
static const Step shield_full[] = {{4, 0, DUTY25, 0x61, P(1899)}, {6, 0, DUTY25, 0x72, P(1923)}};
static const Step reload[] = {{3, 0, 0, 0x61, 0x10, 0}};  // a click
static const Step reloaded[] = {{3, 0, DUTY12, 0x81, P(1750)}, {3, 0, DUTY12, 0x91, P(1840)}};  // clack-clack
static const Step chest[] = {  // an arpeggio up: C E G C
    {5, 0, DUTY50, 0xA2, P(1798)}, {5, 0, DUTY50, 0xA2, P(1849)}, {5, 0, DUTY50, 0xA2, P(1881)},
    {10, 0, DUTY50, 0xB3, P(1923)},
};
static const Step pickup[] = {{4, 0, DUTY50, 0xB1, P(1899)}, {8, 0, DUTY50, 0xC2, P(1985)}};
static const Step jump[] = {{4, 0x14, DUTY50, 0x51, P(1600)}};  // a small rise
static const Step dash[] = {{4, 0, 0, 0x52, 0x23, 0}};
static const Step enemy_shot[] = {{3, 0x15, DUTY25, 0x51, P(1500)}};
static const Step death[] = {{20, 0x77, DUTY12, 0xF7, P(1700)}};  // a long fall
static const Step start[] = {{5, 0, DUTY50, 0xA2, P(1849)}, {10, 0, DUTY50, 0xB3, P(1923)}};
static const Step deflect[] = {{4, 0, DUTY25, 0xB1, P(1960)}};
static const Step text[] = {{1, 0, DUTY12, 0x41, P(1985)}};  // a soft tick
static const Step relay[] = {{5, 0, 0, 0x72, 0x14, 0}, {3, 0, 0, 0x41, 0x05, 0}, {6, 0, 0, 0x62, 0x24, 0}};  // static
static const Step level[] = {  // C E G C E, the last one held
    {5, 0, DUTY50, 0xB2, P(1798)}, {5, 0, DUTY50, 0xB2, P(1849)}, {5, 0, DUTY50, 0xB2, P(1881)},
    {5, 0, DUTY50, 0xC2, P(1923)}, {20, 0, DUTY50, 0xD6, P(1949)},
};
static const Step chime[] = {  // G, E, C: polite, and a little too pleased with itself
    {9, 0, DUTY50, 0x93, P(1881)}, {9, 0, DUTY50, 0x93, P(1849)}, {18, 0, DUTY50, 0xA5, P(1798)},
};

// Each sound's channel (0 = channel 1, 1 = channel 2, 2 = channel 4), priority,
// step count and steps, as separate arrays: plain byte indexing (an array of
// structs would multiply, a slow routine on this CPU).
#define N(steps) (sizeof(steps) / sizeof(Step))
static const uint8_t snd_ch[SFX_COUNT] = {0, 2, 0, 2, 2, 2, 2, 2, 0, 1, 2, 1, 2, 1, 1, 1, 0, 2, 1, 0, 1, 1, 1, 2, 1, 1};
static const uint8_t snd_prio[SFX_COUNT] = {2, 3, 2, 3, 4, 2, 1, 3, 5, 4, 5, 3, 2, 2, 4, 4, 1, 1, 1, 6, 4, 3, 1, 4, 4, 5};
static const uint8_t snd_n[SFX_COUNT] = {
    N(pistol), N(shotgun), N(rifle), N(sniper), N(crit), N(sword), N(hit), N(kill), N(hurt), N(shield_hit),
    N(shield_break), N(shield_full), N(reload), N(reloaded), N(chest), N(pickup), N(jump), N(dash),
    N(enemy_shot), N(death), N(start), N(deflect), N(text), N(relay), N(chime), N(level),
};
static const Step *const snd_steps[SFX_COUNT] = {
    pistol, shotgun, rifle, sniper, crit, sword, hit, kill, hurt, shield_hit, shield_break, shield_full,
    reload, reloaded, chest, pickup, jump, dash, enemy_shot, death, start, deflect, text, relay, chime, level,
};

uint8_t sfx_last = 0xFF;
uint16_t sfx_played;
// Per channel (0: channel 1, 1: channel 2, 2: channel 4), as plain globals (the
// compiler's array indexing is slow): the step playing, steps after it, frames
// left in it, the sound's priority (0 = free).
const Step *step0, *step1, *step2;
uint8_t more0, more1, more2;
uint8_t left0, left1, left2;
uint8_t prio0, prio1, prio2;
uint8_t sfx_busy;  // channels with a sound being stepped (a bit each)

#define WRITE0(s)     NR10_REG = (s)->sweep; NR11_REG = (s)->duty; NR12_REG = (s)->env; NR13_REG = (s)->lo;     NR14_REG = 0x80 | (s)->hi  /* restart, no length limit */
#define WRITE1(s)     NR21_REG = (s)->duty; NR22_REG = (s)->env; NR23_REG = (s)->lo; NR24_REG = 0x80 | (s)->hi
#define WRITE2(s)     NR41_REG = (s)->duty; NR42_REG = (s)->env; NR43_REG = (s)->lo; NR44_REG = 0x80

void sfx_init(void) BANKED {
    NR52_REG = 0x80;  // sound on
    NR50_REG = 0x77;  // full volume, both sides
    NR51_REG = 0xFF;  // every channel to both sides
    sfx_busy = 0;
    prio0 = prio1 = prio2 = 0;
}

#define START(n, st, prio, count)     if (prio##n > (prio)) return;     step##n = (st); more##n = (count) - 1; left##n = (st)->frames; prio##n = (prio);     sfx_busy |= 1 << n;     WRITE##n(st)

void sfx_play(uint8_t id) BANKED {
    const Step *st = snd_steps[id];
    uint8_t prio = snd_prio[id], n = snd_n[id];
    switch (snd_ch[id]) {
    case 0: START(0, st, prio, n); break;
    case 1: START(1, st, prio, n); break;
    default: START(2, st, prio, n); break;
    }
    sfx_last = id;
    sfx_played++;
}

// One channel's frame: count down its step; then the next step, or free the
// channel (the hardware envelope fades what's left).
#define STEP(n)     if ((sfx_busy & (1 << n)) && !--left##n) {         if (!more##n) {             sfx_busy &= ~(1 << n);             prio##n = 0;         } else {             more##n--;             step##n++;             left##n = step##n->frames;             WRITE##n(step##n);         }     }

void sfx_update(void) BANKED {
    if (!sfx_busy) return;
    STEP(0)
    STEP(1)
    STEP(2)
}

// --- What the game did since last frame, from its counters ---
// Only the low byte of each counter is kept: a counter can't move 256 in a
// frame, and 8-bit compares are cheap (this runs every frame). A copy is only
// updated when it changed, so a quiet frame is just the compares.
uint8_t seen_fired, seen_hit, seen_melee, seen_kills, seen_crits, seen_jumps, seen_dashes, seen_bullets;
uint8_t seen_player_hits, seen_shield_hits, seen_reloads, seen_deflects;
uint8_t seen_reload_t, seen_chest, seen_shield, seen_broke, seen_swing;

extern uint16_t bullets_fired, deflects, melee_hits;

void sfx_watch_reset(void) BANKED {
    seen_fired = (uint8_t)shots_fired;
    seen_hit = (uint8_t)shots_hit;
    seen_melee = (uint8_t)melee_hits;
    seen_kills = (uint8_t)enemies_killed;
    seen_crits = (uint8_t)crits;
    seen_jumps = (uint8_t)player_jumps;
    seen_dashes = (uint8_t)player_dashes;
    seen_bullets = (uint8_t)bullets_fired;
    seen_player_hits = (uint8_t)player_hits;
    seen_shield_hits = (uint8_t)shield_hits;
    seen_reloads = (uint8_t)reloads;
    seen_deflects = (uint8_t)deflects;
    seen_reload_t = reload_t;
    seen_chest = chest_state;
    seen_shield = shield_hp;
    seen_broke = shield_broke;
    seen_swing = swing_t;
}

// Did a counter move? (and note it)
#define MOVED(counter, seen) ((uint8_t)(counter) != seen && ((seen = (uint8_t)(counter)), 1))

void sfx_watch(void) BANKED {
    // Lowest priority first, so on a busy frame the most important one is what
    // ends up playing on each channel.
    if (MOVED(bullets_fired, seen_bullets)) sfx_play(SFX_ENEMY_SHOT);
    if (MOVED(player_jumps, seen_jumps)) sfx_play(SFX_JUMP);
    if (MOVED(player_dashes, seen_dashes)) sfx_play(SFX_DASH);
    if (MOVED(shots_hit, seen_hit) | MOVED(melee_hits, seen_melee)) sfx_play(SFX_HIT);
    if (swing_t != seen_swing) {
        if (swing_t == 1) sfx_play(SFX_SWORD);
        seen_swing = swing_t;
    }
    uint8_t crit = MOVED(crits, seen_crits);
    if (MOVED(shots_fired, seen_fired) && !crit) {
        static const uint8_t gun[] = {SFX_PISTOL, SFX_SHOTGUN, SFX_RIFLE, SFX_SNIPER, SFX_SWORD};
        sfx_play(gun[weapon_cur]);
    }
    if (MOVED(reloads, seen_reloads)) sfx_play(SFX_RELOAD);
    if (reload_t != seen_reload_t) {
        if (!reload_t && wpn.mag && weapon_ammo[weapon_cur] == wpn.mag) sfx_play(SFX_RELOADED);
        seen_reload_t = reload_t;
    }
    if (MOVED(deflects, seen_deflects)) sfx_play(SFX_DEFLECT);
    if (MOVED(enemies_killed, seen_kills)) sfx_play(SFX_KILL);
    uint8_t shield_hit = MOVED(shield_hits, seen_shield_hits);
    if (shield_hp != seen_shield) {
        if (shield_hp == shield_cap && seen_shield < shield_cap && !shield_hit) sfx_play(SFX_SHIELD_FULL);
        seen_shield = shield_hp;
    }
    if (crit) sfx_play(SFX_CRIT);
    if (shield_hit && !shield_broke) sfx_play(SFX_SHIELD_HIT);
    if (chest_state != seen_chest) {
        if (chest_state == CHEST_OPEN) sfx_play(SFX_CHEST);
        else if (chest_state == CHEST_TAKEN) sfx_play(SFX_PICKUP);
        seen_chest = chest_state;
    }
    if (MOVED(player_hits, seen_player_hits)) sfx_play(SFX_HURT);
    if (shield_broke != seen_broke) {
        if (shield_broke > seen_broke) sfx_play(SFX_SHIELD_BREAK);
        seen_broke = shield_broke;
    }
}
