// Health and the shield layer (issue #10, ROADMAP "Health and shields").
// Borderlands-style: the shield takes damage first and the overflow spills into
// health; it recharges after a delay with no hits. You start a run without one.
// Health only regenerates with a perk (health_regen, 0 = none for now).
#ifndef HEALTH_H
#define HEALTH_H

#include <gb/gb.h>
#include <stdint.h>

#define HEALTH_START 60  // max health on a first run (tunable: it'll grow with levels)

// The first shield (from the chest): capacity, recharge delay and rate.
#define SHIELD_START_CAP 40
#define SHIELD_START_DELAY 150  // frames without a hit before it recharges
#define SHIELD_START_RATE 2     // frames per point recharged (0 counts as 1)
// A file's shield, bought in town (profile.has_shield: its tier, 0 for none):
// 25, 40, 55, 70 capacity.
#define SHIELD_TIERS 4
#define SHIELD_TIER_CAP(t) (10 + 15 * (t))

// What hurt the player (set by whoever calls player_hurt / health_damage just
// before): the first two match the actor types (actor.h), so a touch passes its type.
#define CAUSE_NONE 0
#define CAUSE_SLIME 1
#define CAUSE_PEASHOOTER 2
#define CAUSE_PEA 3
#define CAUSE_PIT 4
#define CAUSE_HOUND 21  // (run_over.c names them all)
#define CAUSE_DUELIST 22
#define CAUSE_TANK 23
#define CAUSE_MEDIC 24
#define CAUSE_ACID_POOL 25
extern uint8_t hurt_cause;   // the hit being dealt
extern uint8_t death_cause;  // the hit that ended the run (CAUSE_NONE: the run was ended from the pause panel)

// health_damage flags
#define DMG_DIRECT 1  // skips the shield (falling into a pit)

// --- watchable state ---
extern uint8_t player_hp, player_hp_max;
extern uint8_t shield_hp, shield_cap;  // shield_cap 0 = no shield
extern uint8_t shield_delay, shield_rate;
extern uint8_t shield_wait;    // frames left before the shield recharges
extern uint8_t health_regen;   // frames per health point regenerated (0 = none)
extern uint16_t player_hits;   // hits that reached health
extern uint16_t shield_hits;   // hits the shield took all of
// Effects for the HUD (hud.c), counting down: the shield took a hit / just broke.
extern uint8_t shield_flash, shield_broke;

// A new run: full health, no shield.
void health_start(void) BANKED;
// Equip a shield (capacity, delay, rate); it starts full.
void health_give_shield(uint8_t cap, uint8_t delay, uint8_t rate) BANKED;
// Take dmg. Returns 1 if any of it reached health (the caller knocks the player
// back), 0 if the shield took it all.
uint8_t health_damage(uint8_t dmg, uint8_t flags) BANKED;
// Per frame: shield recharge, health regen, effect timers.
void health_update(void) BANKED;

#endif
