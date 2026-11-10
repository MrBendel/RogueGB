#pragma bank 255
#include "opening.h"
#include "desert.h"
#include "world.h"
#include "save.h"
#include "player.h"
#include "level.h"
#include "health.h"
#include "hud.h"
#include "dialog.h"
#include "dialogue.h"
#include "run.h"

uint8_t opening_timer, opening_notice;

void opening_setup(void) BANKED {
    opening_timer = 0;
    if (play_mode == MODE_DESCENT && world_id == WORLD_DESERT &&
        (profile.story & STORY_OPENING) && !(profile.story & STORY_AWAKENED)) {
        opening_timer = 45;
        player_stun = 1; // the existing prone pose, held until the wake-up
    }
}

uint8_t opening_update(void) BANKED {
    if (opening_notice && play_mode == MODE_YARD) {
        opening_notice = 0;
        dlg_start(DLG_OPENING_TOWN);
        return 1;
    }
    if (save_slot == SAVE_NONE || !(profile.story & STORY_OPENING)) return 0;
    if (opening_timer) {
        if (--opening_timer) return 1;
        player_stun = 0;
        player_draw();
        profile.story |= STORY_AWAKENED;
        save_write();
        char goal[8] = "SHIELD ";
        hud_say(goal);
        dlg_start(DLG_OPENING_WAKE);
        return 1;
    }
    if (profile.has_shield && !(profile.story & STORY_SHIELD_TOLD)) {
        profile.story |= STORY_SHIELD_TOLD;
        save_write();
        char goal[8] = "TOWN ^ ";
        hud_say(goal);
        dlg_start(DLG_OPENING_SHIELD);
        return 1;
    }
    return 0;
}

// The guaranteed first find is equipped from its cache, not a random gun roll.
uint8_t opening_shield(uint8_t chest) BANKED {
    if (world_id != WORLD_DESERT || level_map != DESERT_WAKE || chest || profile.has_shield) return 0;
    profile.has_shield = 1;
    health_give_shield(SHIELD_TIER_CAP(1), SHIELD_START_DELAY, SHIELD_START_RATE);
    char label[8] = "SHIELD!";
    hud_say(label);
    profile.exp_chests[level_map] = area_chests_opened[level_map];
    save_write();
    return 1;
}

void opening_arrive(void) BANKED {
    if (!(profile.story & STORY_OPENING)) return;
    profile.story = (profile.story & ~STORY_OPENING) | STORY_MET_ECHO;
    opening_notice = 1;
}
