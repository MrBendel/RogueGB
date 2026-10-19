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
#define PACK_START 4   // backpack slots a new character has ...
#define PACK_STEP 4    // ... each upgrade from Marro adds ...
#define PACK_MAX 12    // ... up to
// Ammo: a reserve per gun type (pistol, shotgun, rifle, sniper; the sword needs
// none), limited on an expedition and filled when one starts. A reload draws from
// it; drops and chests add to it. Each backpack upgrade carries half again as much.
#define AMMO_TYPES 4
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
// Its stats against the gun held (with the skills): damage and fire rate into
// l1, magazine and reload into l2 (each up to 40 characters and a 0).
void gun_card_lines(const Gun *g, char *l1, char *l2) BANKED;
// What Marro asks for it (credits): more for a higher level, double a rarity step.
uint16_t gun_price(const Gun *g) BANKED;
// The first free loadout slot (LOADOUT: none), the first free backpack slot (GUN_NONE: none).
uint8_t loadout_free(void) BANKED;
uint8_t pack_free(void) BANKED;
// The first free loadout slot (LOADOUT: none); the first free backpack slot (GUN_NONE: none).
uint8_t loadout_free(void) BANKED;
uint8_t pack_free(void) BANKED;
// Into the backpack's first free slot: 1, or 0 if it's full.
uint8_t pack_add(const Gun *g) BANKED;
// What the next backpack upgrade costs (0: it's as big as it gets).
uint16_t pack_upgrade_price(void) BANKED;

extern uint8_t melee_fallback;  // every gun dry: the sword in hand until a switch or ammo
uint8_t ammo_limited(void) BANKED;          // on an expedition with a file
uint8_t ammo_cap(uint8_t type) BANKED;      // the most rounds of a type carried
void ammo_fill(void) BANKED;                // every reserve full (a new expedition)
uint8_t ammo_left(void) BANKED;             // the held gun's reserve (255: unlimited)
uint8_t ammo_take(uint8_t need) BANKED;     // a reload: rounds out of the reserve (up to need)
uint8_t ammo_give(uint8_t type, uint8_t n) BANKED;  // a pickup: rounds added (0: full)
void ammo_dry(void) BANKED;                 // the held gun is out: the next with rounds, or the sword

#endif
