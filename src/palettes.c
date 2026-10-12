#pragma bank 255
// Color palettes for each scene (banked: set when a scene starts). The sprites'
// come from their art (tools/assets.mjs keeps each one's own colors); the room,
// the UI and the sprites drawn by our own tools (shots, reticle, weapons) are
// chosen here. A scene sets them before it fades in (gfx_set_fade uses them).
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"
#include "spr_player.h"
#include "spr_slime.h"
#include "spr_peashooter.h"
#include "spr_pea.h"

#define C(r, g, b) RGB8(r, g, b)

// Menus: plain greys (the title and run-over screens are text on the background).
static const uint16_t grey[4] = {C(248, 248, 248), C(168, 168, 168), C(80, 80, 80), C(0, 0, 0)};
// The UI (HUD, pause panel): parchment and ink.
static const uint16_t ui[4] = {C(248, 240, 216), C(200, 184, 152), C(112, 96, 80), C(24, 16, 24)};
// The HUD bars: parchment behind, a highlight, the body, ink.
static const uint16_t shield_bar[4] = {C(248, 240, 216), C(184, 236, 255), C(48, 128, 232), C(24, 16, 24)};
static const uint16_t health_bar[4] = {C(248, 240, 216), C(255, 168, 152), C(208, 32, 48), C(24, 16, 24)};
// The lab: sandy stone floor, its speckle, mid stone, dark mortar and pits.
static const uint16_t lab[4] = {C(232, 220, 192), C(200, 180, 144), C(128, 108, 88), C(40, 32, 40)};
// The player's shots and the muzzle flash: a hot yellow pellet.
static const uint16_t shot[4] = {0, C(255, 248, 168), C(240, 168, 40), C(72, 32, 16)};
// Held weapons: gunmetal (the sword shares it: a steel blade).
static const uint16_t weapon[4] = {0, C(208, 216, 224), C(112, 124, 144), C(24, 24, 32)};
// Reticle and shadow: white and black.
static const uint16_t ui_spr[4] = {0, C(248, 248, 248), C(96, 96, 96), C(0, 0, 0)};

static void set_bg(uint8_t p, const uint16_t *c) {
    memcpy(&pal_base_bg[p << 2], c, 8);
}
static void set_obj(uint8_t p, const uint16_t *c) {
    memcpy(&pal_base_obj[p << 2], c, 8);
}

void palettes_menu(void) BANKED {
    set_bg(BG_PAL_SCENE, grey);
    set_bg(BG_PAL_UI, ui);
}

void palettes_play(void) BANKED {
    set_bg(BG_PAL_SCENE, lab);
    set_bg(BG_PAL_UI, ui);
    set_bg(BG_PAL_SHIELD, shield_bar);
    set_bg(BG_PAL_HEALTH, health_bar);
    // From the art (in switchable banks: banked_memcpy switches to them).
    banked_memcpy(&pal_base_obj[SPR_PAL_PLAYER << 2], spr_player_palette, 8, BANK(spr_player));
    banked_memcpy(&pal_base_obj[SPR_PAL_SLIME << 2], spr_slime_palette, 8, BANK(spr_slime));
    banked_memcpy(&pal_base_obj[SPR_PAL_SHOOTER << 2], spr_peashooter_palette, 8, BANK(spr_peashooter));
    banked_memcpy(&pal_base_obj[SPR_PAL_BULLET << 2], spr_pea_palette, 8, BANK(spr_pea));
    set_obj(SPR_PAL_SHOT, shot);
    set_obj(SPR_PAL_WEAPON, weapon);
    set_obj(SPR_PAL_UI, ui_spr);
    // SPR_PAL_FLASH stays the inverted greys (gfx_init).
}
