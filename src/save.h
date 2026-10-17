// Saving: three profiles in the cartridge's battery-backed RAM (8 KB at 0xA000).
// A profile is the character (class, level, XP, skills, credits, time played)
// and, while one is under way, the expedition (its seed, depth and map: a
// continue starts at that map's entrance). The game saves on its own at safe
// moments (save_write): entering a map, a level up, the end of a run.
//
// Each slot keeps two copies, written in turn, each with a sequence number and
// a checksum, so a write cut short (the battery, a reset) leaves the other one;
// a slot whose copies both fail their checksum reads as empty.
#ifndef SAVE_H
#define SAVE_H

#include <gb/gb.h>
#include <stdint.h>
#include "loot.h"

#define SAVE_SLOTS 3
#define SAVE_NONE 0xFF
#define SAVE_VERSION 3  // 2: guns as items (loot.h); 3: story flags
#define DEATH_FEE_SHIFT 3  // the corporation's cut for rebuilding you: credits >> 3 (12.5%; a shift, no 32-bit division)

typedef struct {
    uint8_t cls;            // class (0: the Wayfarer, docs/design/skill-trees.md)
    uint8_t level;          // 1..
    uint16_t xp;            // toward the next level
    uint8_t skill_points;   // unspent
    uint16_t skills;        // learned, a bit each
    uint32_t credits;
    uint16_t play_minutes;  // all runs together ...
    uint8_t play_seconds;   // ... and seconds (16-bit math only: no 32-bit division on this CPU's library)
    uint8_t best_depth;
    uint16_t kills;
    uint8_t has_shield;
    Gun guns[LOADOUT];      // the loadout (loot.h); type GUN_NONE: an empty slot
    uint8_t gun_sel;        // the one held
    uint32_t last_fee;      // credits the last reconstruction cost
    // The expedition under way (exp_active), resumed at its map's entrance.
    uint8_t exp_active;
    uint16_t exp_seed;
    uint8_t exp_depth;
    uint8_t exp_map;
    uint8_t exp_camps[12];  // camps cleared, a bit each, per map (level.h AREA_CELLS)
    uint8_t exp_chests[12]; // chests opened, a bit each, per map
    uint8_t story;          // STORY_*: what's happened in the story (once per file)
} Profile;

#define STORY_MET_ECHO 1    // her first contact at the town's relay has played

// --- watchable state ---
extern Profile profile;          // the one being played
extern uint8_t save_slot;        // its slot, or SAVE_NONE (the lab, the yard: nothing saved)
extern uint16_t saves_written;
// What each slot holds, for the file screen (save_scan).
extern uint8_t slot_used[SAVE_SLOTS], slot_level[SAVE_SLOTS], slot_depth[SAVE_SLOTS];
extern uint16_t slot_minutes[SAVE_SLOTS];

void save_scan(void) BANKED;              // fill slot_* from the cartridge
void save_open(uint8_t slot) BANKED;      // play this slot: its profile, or a new one (saved at once)
void save_new(uint8_t slot, uint8_t cls) BANKED;  // a new character of this class in the slot, saved
void save_none(void) BANKED;              // play without a file (the lab, the yard): a new profile, never saved
void save_write(void) BANKED;             // the profile into its slot (if there is one)
void save_add_time(uint16_t seconds) BANKED;  // play time since the last call, into the profile

#endif
