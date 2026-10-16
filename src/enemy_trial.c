#pragma bank 255
#include "enemy_trial.h"
#include "run.h"
#include "actor.h"
#include "health.h"
#include "gfx.h"
#include "hud.h"

#include "acid_skimmer.h"

uint8_t enemy_trial_kind, enemy_trial_cleared;

void enemy_trial_start(void) BANKED {
    enemy_trial_cleared = 0;
    enemy_hp_bonus = shooter_spread = 0;
    shooter_interval = 80;
    bullet_speed = 384;
    slime_rest = 26;
    slime_speed = 192;
    health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
    acid_skimmer_start();
    bkg_text(1, 0, "ACID SKIMMER");
    bkg_text(1, 1, "LEAVE ACID MARKS");
}

void enemy_trial_update(void) BANKED {
    acid_skimmer_update();

    if (enemy_trial_cleared) return;
    for (uint8_t i = 0; i < ACTOR_MAX; i++)
        if (actor_type[i] != ACTOR_NONE) return;
    enemy_trial_cleared = 1;
    hud_say("CLEAR!");
}
