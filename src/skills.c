#pragma bank 255
// Skills (skills.h). Banked: on scene starts, weapon switches and in the menu.
#include <gb/gb.h>
#include "skills.h"
#include "skilldata.h"
#include "save.h"
#include "health.h"
#include "player.h"
#include "sfx.h"
#include "gfx.h"

#define DASH_EXTRA_FRAMES 2   // Blink Dash: about 8 px further ...
#define DASH_EXTRA_IFRAMES 4  // ... and 4 more frames untouchable
#define SHIELD_SOONER 30      // Capacitor Surge: frames off the recharge delay
#define AIM_FURTHER 32        // Hawk Sight

uint16_t skill_fx;
uint8_t dash_bonus;  // Blink Dash's extra is on the dash (to take off again)

static uint8_t effect_of(uint8_t k) {
    uint8_t fx;
    banked_memcpy(&fx, &skill_effect[profile.cls][k], 1, BANK(skilldata));
    return fx;
}

void skills_refresh(void) BANKED {
    skill_fx = 0;
    for (uint8_t k = 0; k < SKILLS_PER_CLASS; k++) {
        if (!(profile.skills & (1 << k))) continue;
        uint8_t fx = effect_of(k);
        if (fx) skill_fx |= 1 << (fx - 1);
    }
    // The dash (the movement presets set its base): on or off, once.
    uint8_t want = HAS_FX(SKILL_FX_DASH_FAR) ? 1 : 0;
    if (want != dash_bonus) {
        if (want) dash_frames += DASH_EXTRA_FRAMES, dash_iframes += DASH_EXTRA_IFRAMES;
        else dash_frames -= DASH_EXTRA_FRAMES, dash_iframes -= DASH_EXTRA_IFRAMES;
        dash_bonus = want;
    }
    shield_delay = HAS_FX(SKILL_FX_SHIELD_FAST) ? SHIELD_START_DELAY - SHIELD_SOONER : SHIELD_START_DELAY;
}

uint8_t skill_state(uint8_t k) BANKED {
    if (profile.skills & (1 << k)) return SKILL_LEARNED;
    uint8_t tier = k % 3;
    return tier == 0 || (profile.skills & (1 << (k - 1))) ? SKILL_OPEN : SKILL_LOCKED;
}

uint8_t skill_learn(uint8_t k) BANKED {
    if (!profile.skill_points || skill_state(k) != SKILL_OPEN) return 0;
    profile.skill_points--;
    profile.skills |= 1 << k;
    uint8_t had_hp = HAS_FX(SKILL_FX_HP15) ? 1 : 0;
    skills_refresh();
    if (!had_hp && HAS_FX(SKILL_FX_HP15)) {
        player_hp_max += 15;
        player_hp += 15;
    }
    weapon_select(weapon_cur);  // the gun with the new skills on top
    sfx_play(SFX_RELOADED);
    save_write();
    return 1;
}

void skills_weapon(WeaponStats *w) BANKED {
    if (HAS_FX(SKILL_FX_MAG2) && w->mag) w->mag += 2;
    if (HAS_FX(SKILL_FX_RELOAD_HALF) && w->reload > 1) w->reload >>= 1;
    if (HAS_FX(SKILL_FX_SHOT_SPEED)) w->speed += w->speed >> 2;
    if (HAS_FX(SKILL_FX_FIRE_FAST)) {
        if (w->fire_interval > 3) w->fire_interval -= 2;
        if (w->tap_interval > 3) w->tap_interval -= 2;
    }
    if (HAS_FX(SKILL_FX_PELLET) && w->pellets > 1) w->pellets++;
    if (HAS_FX(SKILL_FX_NO_SLOW)) w->slow = 0;
    if (HAS_FX(SKILL_FX_AIM_FAR)) w->aim_range = w->aim_range > 255 - AIM_FURTHER ? 255 : w->aim_range + AIM_FURTHER;
}
