#pragma bank 255
#include "enemy_trial.h"
#include "run.h"
#include "actor.h"
#include "health.h"
#include "gfx.h"
#include "hud.h"
#include "enemy_family.h"

uint8_t enemy_trial_kind, enemy_trial_cleared;

void enemy_trial_start(void) BANKED {
    enemy_trial_cleared = 0;
    enemy_hp_bonus = shooter_spread = 0;
    shooter_interval = 80;
    bullet_speed = 384;
    slime_rest = 26;
    slime_speed = 192;
    health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
    if (enemy_trial_kind >= 3) {
        enemy_family_load();
        if (enemy_trial_kind == 8) {
            enemy_family_spawn(FAMILY_HOUND, 40, 32);
            enemy_family_spawn(FAMILY_HOUND, 104, 32);
        } else if (enemy_trial_kind == 7) {
            enemy_family_spawn(FAMILY_HUSK, 32, 56);
            enemy_family_spawn(FAMILY_RAGEBOUND, 112, 56);
            enemy_family_spawn(FAMILY_CONTRACTOR, 32, 24);
            enemy_family_spawn(FAMILY_BASTION, 112, 24);
        } else if (enemy_trial_kind == 3) {
            enemy_family_spawn(FAMILY_HUSK, 32, 32);
            enemy_family_spawn(FAMILY_HUSK, 72, 24);
            enemy_family_spawn(FAMILY_HUSK, 112, 32);
        } else {
            enemy_family_spawn(enemy_trial_kind - 1, 72, 32);
        }
    } else if (enemy_trial_kind == 2) {
        actor_spawn(ACTOR_PEASHOOTER, 32, 32);
        actor_spawn(ACTOR_PEASHOOTER, 112, 32);
        actor_spawn(ACTOR_SLIME, 48, 64);
        actor_spawn(ACTOR_SLIME, 96, 64);
    } else {
        actor_spawn(enemy_trial_kind ? ACTOR_PEASHOOTER : ACTOR_SLIME, 72, 32);
    }
    static const char *const names[] = {"SLIME TRIAL", "PEASHOOTER TRIAL", "MIXED GROUP TRIAL",
        "SPORE HUSKS", "RAGEBOUND", "CONTRACTOR", "BASTION DRONE", "FACTION MIX", "SCRAP HOUNDS"};
    static const char *const tips[] = {"KEEP MOVING", "DODGE THE SHOTS", "PICK YOUR TARGET",
        "DO NOT GET BOXED IN", "SIDESTEP THE RUSH", "DODGE THEN RETURN", "HIT THE OPEN CORE", "PRIORITIZE THREATS", "BAIT THEN SIDESTEP"};
    bkg_text(1, 0, names[enemy_trial_kind]);
    bkg_text(1, 1, tips[enemy_trial_kind]);
}

void enemy_trial_update(void) BANKED {
    if (enemy_trial_kind >= 3) enemy_family_update();
    if (enemy_trial_cleared) return;
    for (uint8_t i = 0; i < ACTOR_MAX; i++)
        if (actor_type[i] != ACTOR_NONE) return;
    enemy_trial_cleared = 1;
    hud_say("CLEAR!");
}
