#pragma bank 255
#include "enemy_trial.h"
#include "run.h"
#include "actor.h"
#include "health.h"
#include "gfx.h"
#include "hud.h"

#include "acid_skimmer.h"
#include "mercenary.h"
#include "acid_wolf.h"
#include "rift_stalker.h"
#include "wireweaver.h"

uint8_t enemy_trial_kind, enemy_trial_cleared;

void enemy_trial_start(void) BANKED {
    enemy_trial_cleared = 0;
    enemy_hp_bonus = shooter_spread = 0;
    shooter_interval = 80;
    bullet_speed = 384;
    slime_rest = 26;
    slime_speed = 192;
    health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
    if(enemy_trial_kind==4)wireweaver_start();
    else if(enemy_trial_kind==3)rift_stalker_start();
    else if(enemy_trial_kind==2)acid_wolf_start();
    else if(enemy_trial_kind==1)mercenary_start();else acid_skimmer_start();
    bkg_text(1, 0, enemy_trial_kind==4?"WIREWEAVER":enemy_trial_kind==3?"RIFT STALKER":enemy_trial_kind==2?"ACID WOLF":enemy_trial_kind?"MERCENARY":"ACID SKIMMER");
    bkg_text(1, 1, enemy_trial_kind==4?"JUMP OUT OF WEBS":enemy_trial_kind==3?"DODGE THEN PUNISH":enemy_trial_kind==2?"BAIT LEAP THEN HIT":enemy_trial_kind?"PUNISH THE RELOAD":"LEAVE ACID MARKS");
}

void enemy_trial_update(void) BANKED {
    if(enemy_trial_kind==4)wireweaver_update();
    else if(enemy_trial_kind==3)rift_stalker_update();
    else if(enemy_trial_kind==2)acid_wolf_update();
    else if(enemy_trial_kind==1)mercenary_update();else acid_skimmer_update();

    if (enemy_trial_cleared) return;
    for (uint8_t i = 0; i < ACTOR_MAX; i++)
        if (actor_type[i] != ACTOR_NONE) return;
    enemy_trial_cleared = 1;
    hud_say("CLEAR!");
}
