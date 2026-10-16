// Options (the pause menu's last page): how the game behaves for whoever holds
// the cartridge, not per file. Kept in battery RAM after the three files
// (save.c's layout), with their own checksum; defaults if missing or broken.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <gb/gb.h>
#include <stdint.h>

#define TEXT_SLOW 0
#define TEXT_NORMAL 1
#define TEXT_FAST 2
#define TEXT_INSTANT 3
#define TEXT_SPEEDS 4

#define SOUND_OFF 0
#define SOUND_LOW 1
#define SOUND_FULL 2
#define SOUND_LEVELS 3

typedef struct {
    uint8_t text_speed;  // TEXT_*: the dialogue box's typing
    uint8_t shake;       // screen shake on hits and kills (1 on)
    uint8_t sound;       // SOUND_*
} Settings;

// --- watchable state ---
extern Settings settings;
extern uint8_t settings_loaded;

void settings_load(void) BANKED;   // from the cartridge (or the defaults), and applied
void settings_save(void) BANKED;   // into the cartridge
void settings_apply(void) BANKED;  // the sound's volume (the rest is read where it's used)
uint8_t text_frames_per_letter(void) BANKED;  // 0: a page at once

#endif
