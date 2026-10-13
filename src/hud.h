// The HUD row (issue #11): shield and health bars on the window layer, along the
// bottom of the screen, with the held weapon's name beside them. The bars grow
// with the max values (1 px per point up to 88 px, then they scale), redraw only
// when a value changes, and flash the player's colors when the shield is hit.
#ifndef HUD_H
#define HUD_H

#include <gb/gb.h>
#include <stdint.h>

// --- watchable state (what's drawn) ---
extern uint8_t hud_shield_px, hud_shield_len;  // fill and outline length, px
extern uint8_t hud_hp_px, hud_hp_len;
extern uint8_t hud_fx;  // palette effects shown (HUD_FX_*)
extern uint8_t hud_ammo;  // the magazine count shown (0xFF: none)
#define HUD_FX_SHIELD 1  // the player tinted shield blue (a hit the shield took)
#define HUD_FX_WHITE 2   // the player flashing white (the shield just broke)
#define HUD_FX_LOW 4     // the health bar pulsing (low health)

void hud_init(void) BANKED;   // the HUD's tiles (once per play scene)
void hud_draw(void) BANKED;   // the whole HUD (scene start, after the pause panel)
void hud_hide(void) BANKED;   // before the pause panel takes the window
void hud_update(void) BANKED; // per frame: redraw a bar when its value changed
// Show up to 7 characters (s in RAM) where the weapon's name shows, for a second.
void hud_say(const char *s) BANKED;

#endif
