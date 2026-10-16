// The class's skill tree (docs/design/skill-trees.md; data: content/skills,
// tools/skills.mjs): 3 branches x 3 tiers, skill k = branch * 3 + tier. A skill
// costs a skill point (one a level) and needs the one above it in its branch.
// Learned skills are bits in profile.skills; their effects (skill_fx) modify
// the guns (skills_weapon, from weapon_select), health, the dash and shields.
#ifndef SKILLS_H
#define SKILLS_H

#include <gb/gb.h>
#include <stdint.h>
#include "weapon.h"

#define SKILL_LOCKED 0
#define SKILL_OPEN 1     // can be learned (given a point)
#define SKILL_LEARNED 2

// --- watchable state ---
extern uint16_t skill_fx;  // a bit per active effect (SKILL_FX_* - 1)

#define HAS_FX(fx) (skill_fx & (1 << ((fx) - 1)))

void skills_refresh(void) BANKED;          // skill_fx from the profile; apply to the player
uint8_t skill_state(uint8_t k) BANKED;
uint8_t skill_learn(uint8_t k) BANKED;     // spend a point on it: 1 if learned
void skills_weapon(WeaponStats *w) BANKED; // a gun's stats with the skills on top

#endif
