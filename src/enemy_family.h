#ifndef ENEMY_FAMILY_H
#define ENEMY_FAMILY_H
#include <gb/gb.h>
// actor_scripted: 0 ordinary AI, 1 boss machinery, 2..6 trial families.
#define FAMILY_HUSK 2
#define FAMILY_RAGEBOUND 3
#define FAMILY_CONTRACTOR 4
#define FAMILY_BASTION 5
#define FAMILY_HOUND 6
void enemy_family_load(void) BANKED;
void enemy_family_spawn(uint8_t family, uint8_t x, uint8_t y) BANKED;
void enemy_family_update(void) BANKED;
#endif
