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
#include "player.h"
#include "spr_slime.h"
#include "spr_peashooter.h"
#include "spr_pea.h"

#define C(r, g, b) RGB8(r, g, b)

// Menus: plain greys (the title and run-over screens are text on the background).
static const uint16_t grey[4] = {C(248, 248, 248), C(168, 168, 168), C(80, 80, 80), C(0, 0, 0)};
// The UI (HUD, pause panel): parchment and ink.
static const uint16_t ui[4] = {C(248, 240, 216), C(200, 184, 152), C(112, 96, 80), C(24, 16, 24)};
// The HUD: parchment behind, a highlight, the body, ink. The blue one also draws
// the gun icon (steel blue) and text (ink); the red one is the health bar.
static const uint16_t hud_blue[4] = {C(248, 240, 216), C(184, 236, 255), C(48, 128, 232), C(24, 16, 24)};
static const uint16_t hud_red[4] = {C(248, 240, 216), C(255, 168, 152), C(208, 32, 48), C(24, 16, 24)};
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

// The title: night navy behind, the display font in gold, the compact font in
// white, dim blue for the hints. Palette 3 is the blinking prompt (title.c).
#define NAVY C(32, 36, 60)
static const uint16_t title_text[4] = {NAVY, C(139, 155, 180), C(90, 105, 136), C(232, 236, 244)};
static const uint16_t title_gold[4] = {NAVY, C(254, 231, 97), C(254, 174, 52), C(24, 20, 37)};
static const uint16_t title_hint[4] = {NAVY, C(90, 105, 136), C(90, 105, 136), C(122, 138, 168)};

void palettes_title(void) BANKED {
    set_bg(TITLE_PAL_TEXT, title_text);
    set_bg(TITLE_PAL_GOLD, title_gold);
    set_bg(TITLE_PAL_HINT, title_hint);
    set_bg(TITLE_PAL_PROMPT, title_text);
}

void palettes_menu(void) BANKED {
    set_bg(BG_PAL_SCENE, grey);
    set_bg(BG_PAL_UI, ui);
}

void palettes_play(void) BANKED {
    set_bg(BG_PAL_SCENE, lab);
    set_bg(BG_PAL_SHIELD, hud_blue);
    set_bg(BG_PAL_HEALTH, hud_red);
    // From the art (in switchable banks: banked_memcpy switches to them).
    player_select_character(player_character);
    banked_memcpy(&pal_base_obj[SPR_PAL_SLIME << 2], spr_slime_palette, 8, BANK(spr_slime));
    banked_memcpy(&pal_base_obj[SPR_PAL_SHOOTER << 2], spr_peashooter_palette, 8, BANK(spr_peashooter));
    banked_memcpy(&pal_base_obj[SPR_PAL_BULLET << 2], spr_pea_palette, 8, BANK(spr_pea));
    set_obj(SPR_PAL_SHOT, shot);
    set_obj(SPR_PAL_WEAPON, weapon);
    set_obj(SPR_PAL_UI, ui_spr);
    // SPR_PAL_FLASH stays the inverted greys (gfx_init).
}

// Palette 6 on screen now, too (play runs unfaded, so the displayed palette is
// the base): the pause panel's parchment, or the HUD's blue back.
extern uint16_t pal_bg[PAL_COUNT * 4];
extern volatile uint8_t pal_dirty;
static void window_pal(const uint16_t *c) {
    set_bg(BG_PAL_UI, c);
    memcpy(&pal_bg[BG_PAL_UI << 2], c, 8);
    pal_dirty = 1;
}
void palettes_window_ui(void) BANKED { window_pal(ui); }
void palettes_window_hud(void) BANKED { window_pal(hud_blue); }
