// XP and levels (docs/design/progression.md): light levels next to the guns.
// Kills, cleared camps and going deeper give XP; each level gives a skill point
// and a little more health, refills health and shield, and saves the file.
// Works in the lab and the yard too (a profile that isn't saved).
#ifndef PROGRESS_H
#define PROGRESS_H

#include <gb/gb.h>
#include <stdint.h>

#define LEVEL_CAP 30
#define HP_PER_LEVEL 2
#define XP_SLIME 4
#define XP_PEASHOOTER 6
#define XP_HOUND 8
#define XP_DUELIST 10
#define XP_CAMP 10        // + 5 a depth: clearing a camp
#define XP_SHAFT 20       // x depth: down the shaft into the next area
#define XP_MINIBOSS 40    // x depth: an area's miniboss, the way down opened

// --- watchable state ---
extern uint16_t xp_pending;   // XP from kills this frame (actor.c), awarded by play
extern uint16_t level_ups;    // this session
extern uint8_t xp_on;         // XP counts in this scene (play.c: not in the combat playtests)

void progress_award(void) BANKED;        // xp_pending (and the kills' depth bonus) into the profile
void xp_gain(uint16_t n) BANKED;         // n XP: levels up as it fills
uint16_t xp_need(uint8_t level) BANKED;  // XP from this level to the next (0 at the cap)
void progress_apply(void) BANKED;        // at a scene start: max health from the level

#endif
