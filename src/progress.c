#pragma bank 255
// XP and levels (progress.h). Banked: called when XP arrives, not every frame.
#include <gb/gb.h>
#include "progress.h"
#include "save.h"
#include "health.h"
#include "hud.h"
#include "sfx.h"
#include "descent.h"
#include "skills.h"
#include "skilldata.h"

uint16_t xp_pending;
uint16_t level_ups;
uint16_t xp_kills_seen;  // enemies_killed when XP was last awarded

extern uint16_t enemies_killed;  // aim.c

// XP from level L to L + 1: 10 L^2 + 30 L (40 for level 2, 9280 for the last).
static const uint16_t need_table[LEVEL_CAP - 1] = {
    40, 100, 180, 280, 400, 540, 700, 880, 1080, 1300, 1540, 1800, 2080, 2380, 2700,
    3040, 3400, 3780, 4180, 4600, 5040, 5500, 5980, 6480, 7000, 7540, 8100, 8680, 9280,
};

uint16_t xp_need(uint8_t level) BANKED {
    return level && level < LEVEL_CAP ? need_table[level - 1] : 0;
}

void progress_apply(void) BANKED {
    if (!profile.level) profile.level = 1;
    skills_refresh();
    player_hp_max = HEALTH_START + (uint8_t)((profile.level - 1) * HP_PER_LEVEL) + (HAS_FX(SKILL_FX_HP15) ? 15 : 0);
    player_hp = player_hp_max;
    xp_kills_seen = enemies_killed;
}

void xp_gain(uint16_t n) BANKED {
    if (profile.level >= LEVEL_CAP || !n) return;
    profile.xp += n;
    uint8_t gained = 0;
    uint16_t need;
    while ((need = xp_need(profile.level)) && profile.xp >= need) {
        profile.xp -= need;
        profile.level++;
        profile.skill_points++;
        gained++;
    }
    if (profile.level >= LEVEL_CAP) profile.xp = 0;
    if (!gained) return;
    // The level up: more health, everything refilled, a fanfare, the HUD says so.
    player_hp_max += gained * HP_PER_LEVEL;
    player_hp = player_hp_max;
    shield_hp = shield_cap;
    level_ups += gained;
    sfx_play(SFX_LEVEL);
    char t[8] = "LEVEL  ";
    uint8_t l = profile.level;
    if (l < 10) t[6] = '0' + l;
    else t[5] = '0' + l / 10, t[6] = '0' + l % 10;
    hud_say(t);
    save_write();
}

void progress_award(void) BANKED {
    // Deeper enemies are worth more: a point a depth per kill.
    uint16_t kills = enemies_killed - xp_kills_seen;
    xp_kills_seen = enemies_killed;
    uint16_t n = xp_pending + kills * (layer_depth > 1 ? layer_depth - 1 : 0);
    xp_pending = 0;
    xp_gain(n);
}
