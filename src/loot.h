// Loot: guns as items (docs/design/progression.md). A gun is a base type (the
// five in weapon.h) at a level and a rarity, with rolled bonuses on top of the
// type's stats: damage, fire rate, magazine, reload. A file carries up to
// LOADOUT guns (SELECT cycles them); they're saved with it and kept through death.
// Rarer and deeper guns roll more bonus points.
#ifndef LOOT_H
#define LOOT_H

#include <gb/gb.h>
#include <stdint.h>
#include "weapon.h"

#define LOADOUT 4
#define GUN_NONE 0xFF  // an empty loadout slot (its type)

#define RARITY_COMMON 0     // white
#define RARITY_UNCOMMON 1   // green
#define RARITY_RARE 2       // blue
#define RARITY_EPIC 3       // purple
#define RARITY_LEGENDARY 4  // orange
#define RARITIES 5

// Name prefixes, by the gun's biggest bonus.
#define PREFIX_NONE 0
#define PREFIX_HEAVY 1   // damage
#define PREFIX_RAPID 2   // fire rate
#define PREFIX_DRUM 3    // magazine
#define PREFIX_QUICK 4   // reload

typedef struct {
    uint8_t type;    // WEAPON_*, or GUN_NONE
    uint8_t rarity;  // RARITY_*
    uint8_t level;
    uint8_t dmg;     // + damage per hit
    uint8_t rate;    // frames off the fire interval
    uint8_t mag;     // + rounds
    uint8_t reload;  // frames off the reload (x4)
    uint8_t prefix;  // PREFIX_*
    uint8_t spare;
} Gun;

// --- watchable state ---
extern Gun gun_held;         // the loadout's gun being held (weapon_select puts it on top)
extern uint8_t gun_mods;     // gun_held applies (a file's loadout; not the lab's plain guns)
extern uint8_t slot_ammo[LOADOUT];
extern uint16_t guns_rolled;

void gun_plain(Gun *g, uint8_t type) BANKED;             // a common gun, no bonuses, level 1
void loot_roll(Gun *g, uint8_t level, uint8_t luck) BANKED;  // a random gun (luck: 0..; deeper is luckier)
void gun_apply(WeaponStats *w, const Gun *g) BANKED;     // a gun's bonuses on its type's stats
void loadout_select(uint8_t slot) BANKED;                // hold the file's gun in this slot
void loadout_next(void) BANKED;                          // SELECT: the next gun in the loadout
// "Rapid Shotgun" etc. into dst (at most 20 characters and a 0).
void gun_name(char *dst, const Gun *g) BANKED;

#endif
