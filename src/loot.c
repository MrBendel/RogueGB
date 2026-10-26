#pragma bank 255
// Loot: guns as items (loot.h). Banked: rolls happen when loot drops, the rest on
// a weapon switch.
#include <gb/gb.h>
#include <rand.h>
#include <string.h>
#include "loot.h"
#include "save.h"
#include "weapon.h"
#include "skills.h"
#include "hud.h"
#include "run.h"

#define MIN_INTERVAL 3   // the fastest a gun fires (frames between shots)
#define MIN_RELOAD 12    // ... and reloads

Gun gun_held;
uint8_t gun_mods;
uint8_t slot_ammo[LOADOUT];
uint16_t guns_rolled;
uint8_t melee_fallback;

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
    melee_fallback = 0;
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
            if (s != profile.gun_sel || melee_fallback) loadout_select(s);
            return;
        }
    }
}

// --- describing a gun (the loot card, the shop) ---

static char *put(char *p, const char *s) {
    while (*s) *p++ = *s++;
    return p;
}

static char *put_num(char *p, int16_t n, uint8_t sign) {
    if (n < 0) *p++ = '-', n = -n;
    else if (sign) *p++ = '+';
    char d[5];
    uint8_t k = 0;
    do {
        d[k++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (k) *p++ = d[--k];
    return p;
}

// " (+n)" when they differ.
static char *put_diff(char *p, int16_t d) {
    if (!d) return p;
    p = put(p, " (");
    p = put_num(p, d, 1);
    return put(p, ")");
}

// Shots a second, from frames between them.
static int16_t rate(const WeaponStats *w) {
    uint8_t f = w->mode == FIRE_SEMI && w->tap_interval ? w->tap_interval : w->fire_interval;
    return f ? 60 / f : 0;
}

void gun_card_lines(const Gun *g, char *l1, char *l2) BANKED {
    WeaponStats n = weapon_stats[g->type];
    gun_apply(&n, g);
    if (skill_fx) skills_weapon(&n);
    const WeaponStats *h = &wpn;
    char *p;
    // Damage (x pellets), and shots a second; the magazine and reload.
    p = put(l1, "DMG ");
    p = put_num(p, n.damage, 0);
    if (n.pellets > 1) *p++ = 'x', p = put_num(p, n.pellets, 0);
    p = put_diff(p, (int16_t)n.damage * n.pellets - (int16_t)h->damage * h->pellets);
    if (n.mode != FIRE_MELEE) {
        p = put(p, "   RATE ");
        p = put_num(p, rate(&n), 0);
        p = put_diff(p, rate(&n) - rate(h));
    }
    *p = 0;
    if (n.mag) {
        p = put(l2, "MAG ");
        p = put_num(p, n.mag, 0);
        p = put_diff(p, (int16_t)n.mag - h->mag);
        p = put(p, "   RELOAD ");
        p = put_num(p, n.reload / 6 / 10, 0);
        *p++ = '.';
        p = put_num(p, n.reload / 6 % 10, 0);
        *p++ = 's';
        *p = 0;
    } else {
        p = put(l2, "A blade: no magazine.");
        *p = 0;
    }
}

uint8_t loadout_free(void) BANKED {
    uint8_t s = 0;
    while (s < LOADOUT && profile.guns[s].type != GUN_NONE) s++;
    return s;
}

uint8_t pack_free(void) BANKED {
    for (uint8_t s = 0; s < profile.pack_size; s++) if (profile.pack[s].type == GUN_NONE) return s;
    return GUN_NONE;
}

uint8_t pack_add(const Gun *g) BANKED {
    uint8_t s = pack_free();
    if (s == GUN_NONE) return 0;
    profile.pack[s] = *g;
    return 1;
}

uint16_t pack_upgrade_price(void) BANKED {
    return profile.pack_size >= PACK_MAX ? 0 : profile.pack_size == PACK_START ? 300 : 800;
}

// --- ammo ---

static const uint8_t ammo_base[AMMO_TYPES] = {72, 24, 120, 12};

uint8_t ammo_limited(void) BANKED {
    return save_slot != SAVE_NONE && play_mode == MODE_DESCENT;
}

uint8_t ammo_cap(uint8_t type) BANKED {
    uint8_t b = ammo_base[type], tier = (profile.pack_size - PACK_START) / PACK_STEP;
    return b + (b >> 1) * tier;
}

void ammo_fill(void) BANKED {
    for (uint8_t t = 0; t < AMMO_TYPES; t++) profile.ammo[t] = ammo_cap(t);
}

uint8_t ammo_full(void) BANKED {
    for (uint8_t t = 0; t < AMMO_TYPES; t++)
        if (profile.ammo[t] < ammo_cap(t)) return 0;
    return 1;
}

uint8_t ammo_left(void) BANKED {
    if (!ammo_limited() || weapon_cur >= AMMO_TYPES) return 255;
    return profile.ammo[weapon_cur];
}

uint8_t ammo_take(uint8_t need) BANKED {
    if (!ammo_limited() || weapon_cur >= AMMO_TYPES) return need;
    uint8_t r = profile.ammo[weapon_cur];
    if (need > r) need = r;
    profile.ammo[weapon_cur] = r - need;
    return need;
}

// The first loadout slot after the held one whose gun can fire (rounds in it or
// in reserve, or a sword); LOADOUT: none.
static uint8_t armed_slot(void) {
    for (uint8_t i = 1; i <= LOADOUT; i++) {
        uint8_t s = (profile.gun_sel + i) % LOADOUT, t = profile.guns[s].type;
        if (t == GUN_NONE) continue;
        if (t >= AMMO_TYPES || slot_ammo[s] || profile.ammo[t]) return s;
    }
    return LOADOUT;
}

uint8_t ammo_give(uint8_t type, uint8_t n) BANKED {
    uint8_t r = profile.ammo[type], cap = ammo_cap(type);
    if (r >= cap) return 0;
    if (n > cap - r) n = cap - r;
    profile.ammo[type] = r + n;
    // On the sword because everything was dry: back to a gun that can use it.
    if (melee_fallback) {
        for (uint8_t s = 0; s < LOADOUT; s++) {
            if (profile.guns[s].type != type) continue;
            loadout_select(s);
            break;
        }
    }
    return n;
}

void ammo_dry(void) BANKED {
    char t[8] = "NO AMMO";
    hud_say(t);
    if (gun_mods) slot_ammo[profile.gun_sel] = weapon_ammo[weapon_cur];
    uint8_t s = armed_slot();
    if (s < LOADOUT) {
        loadout_select(s);
        return;
    }
    // Nothing loaded anywhere: the sword (no gun's parts on it).
    gun_mods = 0;
    melee_fallback = 1;
    weapon_select(WEAPON_SWORD);
}

uint16_t gun_price(const Gun *g) BANKED {
    return (uint16_t)(15 + 5 * g->level) << g->rarity;
}

void gun_name(char *dst, const Gun *g) BANKED {
    const char *a = prefixes[g->prefix], *b = type_names[g->type];
    while (*a) *dst++ = *a++;
    while (*b) *dst++ = *b++;
    *dst = 0;
}
