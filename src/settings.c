#pragma bank 255
// Options (settings.h). Banked: read at boot, written from the pause menu.
//
// Cartridge RAM at 0xA300 (after the files' 3 x 256 bytes): 'O' 'P', a checksum,
// the Settings.
#include <gb/gb.h>
#include <string.h>
#include "settings.h"

#define AT ((uint8_t *)0xA300)
#define HEADER 4

Settings settings;
uint8_t settings_loaded;

static const Settings defaults = {TEXT_NORMAL, 1, SOUND_FULL};
static const uint8_t frames_per_letter[TEXT_SPEEDS] = {3, 2, 1, 0};

static uint8_t sum(const uint8_t *p) {
    uint8_t c = 0x5A;
    for (uint8_t i = 0; i < sizeof(Settings); i++) c = (c << 1 | c >> 7) ^ p[i];
    return c;
}

void settings_apply(void) BANKED {
    if (settings.sound == SOUND_OFF) {
        NR51_REG = 0x00;  // every channel off both speakers
    } else {
        NR51_REG = 0xFF;
        NR50_REG = settings.sound == SOUND_LOW ? 0x33 : 0x77;
    }
}

void settings_load(void) BANKED {
    ENABLE_RAM;
    SWITCH_RAM(0);
    if (AT[0] == 'O' && AT[1] == 'P' && AT[2] == sum(AT + HEADER)) memcpy(&settings, AT + HEADER, sizeof(Settings));
    else settings = defaults;
    DISABLE_RAM;
    if (settings.text_speed >= TEXT_SPEEDS || settings.sound >= SOUND_LEVELS || settings.shake > 1) settings = defaults;
    settings_loaded = 1;
    settings_apply();
}

void settings_save(void) BANKED {
    ENABLE_RAM;
    SWITCH_RAM(0);
    AT[0] = 0;
    memcpy(AT + HEADER, &settings, sizeof(Settings));
    AT[2] = sum(AT + HEADER);
    AT[1] = 'P';
    AT[0] = 'O';
    DISABLE_RAM;
}

uint8_t text_frames_per_letter(void) BANKED {
    return frames_per_letter[settings.text_speed];
}
