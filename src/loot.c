#pragma bank 255
// Loot: guns as items (loot.h). Banked: rolls happen when loot drops, the rest on
// a weapon switch.
#include <gb/gb.h>
#include <rand.h>
#include <string.h>
#include "loot.h"
#include "save.h"
#include "weapon.h"

#define MIN_INTERVAL 3   // the fastest a gun fires (frames between shots)
#define MIN_RELOAD 12    // ... and reloads

Gun gun_held;
uint8_t gun_mods;
uint8_t slot_ammo[LOADOUT];
uint16_t guns_rolled;

// Rarity by a roll out of 100: common 60, uncommon 25, rare 10, epic 4, legendary 1
// (luck moves the roll up: deeper areas find better guns).
static const uint8_t rarity_floor[RARITIES] = {0, 60, 85, 95, 99};
static const char *const type_names[WEAPON_COUNT] = {"Pistol", "Shotgun", "Rifle", "Sniper", "Sword"};
static const char *const prefixes[] = {"", "Heavy ", "Rapid ", "Drum ", "Quick "};

void gun_plain(Gun *g, uint8_t type) BANKED {
    memset(g, 0, sizeof(Gun));
    g->type = type;
    g->level = 1;
}

void loot_roll(Gun *g, uint8_t level, uint8_t luck) BANKED {
    gun_plain(g, (uint8_t)(rand() % WEAPON_COUNT));
    g->level = level ? level : 1;
    uint8_t r = rand() % 100;
    r = r + luck > 99 ? 99 : r + luck;
    uint8_t rarity = 0;
    while (rarity + 1 < RARITIES && r >= rarity_floor[rarity + 1]) rarity++;
    g->rarity = rarity;
    // Bonus points: one a rarity step, one every third level; each goes to a
    // stat at random (a sword has no magazine or fire rate: its go to damage).
    uint8_t points = rarity + level / 3;
    uint8_t best = 0, best_n = 0;
    uint8_t got[4] = {0, 0, 0, 0};
    while (points--) {
        uint8_t s = rand() & 3;
        if (g->type == WEAPON_SWORD) s = 0;
        got[s]++;
    }
    g->dmg = got[0];
    g->rate = got[1];
    g->mag = got[2] * 2;
    g->reload = got[3];
    for (uint8_t s = 0; s < 4; s++) if (got[s] > best_n) best_n = got[s], best = s + 1;
    g->prefix = best;
    guns_rolled++;
}

void gun_apply(WeaponStats *w, const Gun *g) BANKED {
    w->damage += g->dmg + (g->level - 1) / 4;  // a little more damage every 4 levels
    if (w->mode != FIRE_MELEE) {
        uint8_t off = g->rate;
        w->fire_interval = w->fire_interval > MIN_INTERVAL + off ? w->fire_interval - off : MIN_INTERVAL;
        if (w->tap_interval) w->tap_interval = w->tap_interval > MIN_INTERVAL + off ? w->tap_interval - off : MIN_INTERVAL;
    }
    if (w->mag) w->mag += g->mag;
    uint8_t roff = g->reload << 2;
    if (w->reload) w->reload = w->reload > MIN_RELOAD + roff ? w->reload - roff : MIN_RELOAD;
}

void loadout_select(uint8_t slot) BANKED {
    if (profile.guns[slot].type == GUN_NONE) return;
    // Each slot keeps its own magazine (two shotguns don't share one).
    if (gun_mods) slot_ammo[profile.gun_sel] = weapon_ammo[weapon_cur];
    profile.gun_sel = slot;
    gun_held = profile.guns[slot];
    gun_mods = 1;
    weapon_select(gun_held.type);
    weapon_ammo[weapon_cur] = slot_ammo[slot] < wpn.mag ? slot_ammo[slot] : wpn.mag;
}

void loadout_next(void) BANKED {
    for (uint8_t i = 1; i <= LOADOUT; i++) {
        uint8_t s = (profile.gun_sel + i) % LOADOUT;
        if (profile.guns[s].type != GUN_NONE) {
            if (s != profile.gun_sel) loadout_select(s);
            return;
        }
    }
}

void gun_name(char *dst, const Gun *g) BANKED {
    const char *a = prefixes[g->prefix], *b = type_names[g->type];
    while (*a) *dst++ = *a++;
    while (*b) *dst++ = *b++;
    *dst = 0;
}
