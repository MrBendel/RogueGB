#ifndef BOSS_H
#define BOSS_H
#include <gb/gb.h>
#include <stdint.h>

// Standalone combat trials, not campaign progression. Timers are game frames.
#define BOSS_NONE 0
#define BOSS_CLAIMJAW 1
#define BOSS_WARDEN 2
#define BOSS_GUNSHIP 3
#define BOSS_DUO 4
#define BOSS_SCRAPBACK 5
#define BOSS_CRUISE 6
#define BOSS_LASER_WARN 7
#define BOSS_LASER 8
#define BOSS_RECOVER 0
#define BOSS_WARN 1
#define BOSS_CHARGE 2
#define BOSS_RELAYS 3
#define BOSS_REINFORCE 4
#define BOSS_WON 5
#define BOSS_RETREAT 9
#define BOSS_FLYNT 10
extern uint8_t boss_kind, boss_slot, boss_state, boss_timer, boss_phase;
extern uint8_t boss_hp, boss_hp_max, boss_relays, boss_reward;
extern uint16_t boss_attacks, boss_cycles;
extern int8_t boss_dx, boss_dy;
void boss_room(void) BANKED;
void boss_start(uint8_t kind) BANKED;
void boss_update(void) BANKED;
void boss_status(void) BANKED;
void duo_draw(void) BANKED;
#endif
