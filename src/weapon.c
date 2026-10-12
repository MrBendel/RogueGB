#pragma bank 255
// Weapons: presets, art loading and switching (see weapon.h). Banked: it runs at
// scene start and when switching weapons.
#include <gb/gb.h>
#include <string.h>
#include "weapon.h"
#include "gfx.h"
#include "spr_pistol.h"
#include "spr_shotgun.h"
#include "spr_rifle.h"
#include "spr_sniper.h"
#include "spr_sword.h"
#include "spr_sword_smear.h"
#include "weapon_pistol.h"
#include "weapon_shotgun.h"
#include "weapon_rifle.h"
#include "weapon_sniper.h"
#include "weapon_sword.h"

uint8_t weapon_cur;
uint8_t rifle_mode = RIFLE_AUTO;
WeaponStats wpn;
WeaponStats weapon_stats[WEAPON_COUNT];
uint8_t gun_vram, gun_blank;
uint8_t gun_frames[16 * 4];
int8_t gun_place[7][16];
uint8_t smear_vram, smear_blank;
uint8_t smear_frames[16 * 4];
uint8_t draw_vram, draw_blank;
uint8_t *draw_frames;

// Where each weapon's art went in sprite VRAM.
static uint8_t weapon_vram[WEAPON_COUNT];
static uint8_t stats_ready;

// Starting stats (see ROADMAP "Weapons" for the feel each one is going for).
static const WeaponStats presets[WEAPON_COUNT] = {
    // mode        fire tap pel spr jit speed range aim dmg kick push windup slow
    {FIRE_SEMI,     14,  6,  1,  0,  0,  768, 112, 112,  1,  2,   0,    0,   0},  // pistol: tap to fire faster
    {FIRE_AUTO,     40,  0,  5,  1,  0,  896,  56,  96,  1,  3,   2,    0,   0},  // shotgun: 5 pellets fanned, shoves you back
    {FIRE_RIFLE,     6,  4,  1,  0,  0,  896, 128, 128,  1,  1,   0,   12,   0},  // rifle: tap, burst (then a 12-frame pause) or auto
    {FIRE_CHARGE,   30,  0,  1,  0,  0, 1536, 224, 160,  2,  3,   0,   36,   1},  // sniper: steady for a critical, long reach
    // sword: tap to swing, 3-hit combo (the window: tap), reach 14 px, knockback
    // 12 px, deflects bullets; speed is unused (its table is the unit vectors)
    {FIRE_MELEE,     0, 12,  1,  0,  0,  256,  14,  96,  2, 12,   0,    0,   1},
};
static const char *const names[WEAPON_COUNT] = {"PISTOL ", "SHOTGUN", "RIFLE  ", "SNIPER ", "SWORD  "};
static const int8_t (*const places[WEAPON_COUNT])[16] = {
    weapon_pistol_place, weapon_shotgun_place, weapon_rifle_place, weapon_sniper_place, weapon_sword_place,
};
static const uint8_t blanks[WEAPON_COUNT] = {
    SPR_PISTOL_BLANK_COLUMN, SPR_SHOTGUN_BLANK_COLUMN, SPR_RIFLE_BLANK_COLUMN, SPR_SNIPER_BLANK_COLUMN,
    SPR_SWORD_BLANK_COLUMN,
};

void weapon_init(void) BANKED {
    // All the weapons' art lives in the Color's second VRAM bank (their frames
    // carry S_BANK), so bank 0 keeps its sprite tiles for the player, enemies
    // and effects.
    uint8_t v = 0;
    weapon_vram[WEAPON_PISTOL] = v;
    banked_sprite_data_vbk1(v, SPR_PISTOL_COLUMN_COUNT * 2, spr_pistol_tiles, BANK(spr_pistol));
    v += SPR_PISTOL_COLUMN_COUNT * 2;
    weapon_vram[WEAPON_SHOTGUN] = v;
    banked_sprite_data_vbk1(v, SPR_SHOTGUN_COLUMN_COUNT * 2, spr_shotgun_tiles, BANK(spr_shotgun));
    v += SPR_SHOTGUN_COLUMN_COUNT * 2;
    weapon_vram[WEAPON_RIFLE] = v;
    banked_sprite_data_vbk1(v, SPR_RIFLE_COLUMN_COUNT * 2, spr_rifle_tiles, BANK(spr_rifle));
    v += SPR_RIFLE_COLUMN_COUNT * 2;
    weapon_vram[WEAPON_SNIPER] = v;
    banked_sprite_data_vbk1(v, SPR_SNIPER_COLUMN_COUNT * 2, spr_sniper_tiles, BANK(spr_sniper));
    v += SPR_SNIPER_COLUMN_COUNT * 2;
    weapon_vram[WEAPON_SWORD] = v;
    banked_sprite_data_vbk1(v, SPR_SWORD_COLUMN_COUNT * 2, spr_sword_tiles, BANK(spr_sword));
    v += SPR_SWORD_COLUMN_COUNT * 2;
    smear_vram = v;
    smear_blank = SPR_SWORD_SMEAR_BLANK_COLUMN;
    banked_sprite_data_vbk1(v, SPR_SWORD_SMEAR_COLUMN_COUNT * 2, spr_sword_smear_tiles, BANK(spr_sword_smear));
    banked_memcpy(smear_frames, spr_sword_smear_frames, sizeof(smear_frames), BANK(spr_sword_smear));
    for (uint8_t i = 1; i < sizeof(smear_frames); i += 2) smear_frames[i] |= SPR_PAL_WEAPON | S_BANK;
    // First run: the presets. Later runs keep what was tuned (like movement).
    if (!stats_ready) {
        stats_ready = 1;
        memcpy(weapon_stats, presets, sizeof(weapon_stats));
        weapon_cur = WEAPON_PISTOL;
        wpn = weapon_stats[WEAPON_PISTOL];
    }
    weapon_select(weapon_cur);
}

void weapon_select(uint8_t w) BANKED {
    weapon_stats[weapon_cur] = wpn;
    weapon_cur = w;
    wpn = weapon_stats[w];
    gun_vram = weapon_vram[w];
    gun_blank = blanks[w];
    draw_frames = gun_frames;  // what draw_gun draws (the sword swaps in its smear: aim.c)
    draw_vram = gun_vram;
    draw_blank = gun_blank;
    switch (w) {
    case WEAPON_PISTOL: banked_memcpy(gun_frames, spr_pistol_frames, sizeof(gun_frames), BANK(spr_pistol)); break;
    case WEAPON_SHOTGUN: banked_memcpy(gun_frames, spr_shotgun_frames, sizeof(gun_frames), BANK(spr_shotgun)); break;
    case WEAPON_RIFLE: banked_memcpy(gun_frames, spr_rifle_frames, sizeof(gun_frames), BANK(spr_rifle)); break;
    case WEAPON_SNIPER: banked_memcpy(gun_frames, spr_sniper_frames, sizeof(gun_frames), BANK(spr_sniper)); break;
    default: banked_memcpy(gun_frames, spr_sword_frames, sizeof(gun_frames), BANK(spr_sword)); break;
    }
    for (uint8_t i = 1; i < sizeof(gun_frames); i += 2) gun_frames[i] |= SPR_PAL_WEAPON | S_BANK;
    // The placement tables are in this bank, so a plain copy reads them.
    memcpy(gun_place, places[w], sizeof(gun_place));
    // The tables' kick is 2 px; scale it to the weapon's (1: just its sign, 3: one more).
    uint8_t k = wpn.kick;
    for (uint8_t row = PLACE_KX; row <= PLACE_KY; row++) {
        for (uint8_t d = 0; d < 16; d++) {
            int8_t v = gun_place[row][d], sign = (v > 0) - (v < 0);
            gun_place[row][d] = k <= 1 ? sign : k == 2 ? v : v + sign;
        }
    }
}

void weapon_draw_name(uint8_t x, uint8_t y) BANKED {
    win_text(x, y, names[weapon_cur]);
}
