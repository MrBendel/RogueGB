#pragma bank 255
// Saving (save.h). Banked: it runs at map changes and on menus, never per frame.
//
// Cartridge RAM layout: slot s, copy c at 0xA000 + s * 512 + c * 256 (the
// options follow the files, settings.c):
//   0-1  'S' 'K'     (written last: a copy cut short has no magic)
//   2    SAVE_VERSION
//   3    sequence number (the newer copy wins; it wraps)
//   4-5  checksum of the data
//   6    data size
//   8..  the Profile
#include <gb/gb.h>
#include <string.h>
#include "save.h"

#define SRAM ((uint8_t *)0xA000)
#define COPY_SIZE 256
#define HEADER 8

Profile profile;
uint8_t save_slot = SAVE_NONE;
uint16_t saves_written;
uint8_t slot_used[SAVE_SLOTS], slot_level[SAVE_SLOTS], slot_depth[SAVE_SLOTS];
uint16_t slot_minutes[SAVE_SLOTS];
uint8_t slot_seq;  // the sequence number of the copy last read or written

static uint16_t checksum(const uint8_t *p, uint8_t n) {
    uint16_t c = 0x5AFE;
    while (n--) c = ((c << 1) | (c >> 15)) ^ *p++;
    return c;
}

static uint8_t *copy_at(uint8_t slot, uint8_t copy) {
    return SRAM + ((uint16_t)slot << 9) + (copy ? COPY_SIZE : 0);
}

static uint8_t valid(const uint8_t *r) {
    if (r[0] != 'S' || r[1] != 'K' || r[2] != SAVE_VERSION || r[6] != sizeof(Profile)) return 0;
    return checksum(r + HEADER, sizeof(Profile)) == (r[4] | ((uint16_t)r[5] << 8));
}

// The newer valid copy of a slot, or 0. Cartridge RAM must be enabled.
static uint8_t *newest(uint8_t slot) {
    uint8_t *a = copy_at(slot, 0), *b = copy_at(slot, 1);
    uint8_t va = valid(a), vb = valid(b);
    if (va && vb) return (int8_t)(b[3] - a[3]) > 0 ? b : a;
    return va ? a : vb ? b : 0;
}

void save_scan(void) BANKED {
    ENABLE_RAM;
    SWITCH_RAM(0);
    for (uint8_t s = 0; s < SAVE_SLOTS; s++) {
        uint8_t *r = newest(s);
        slot_used[s] = r != 0;
        if (r) {
            const Profile *p = (const Profile *)(r + HEADER);
            slot_level[s] = p->level;
            slot_depth[s] = p->best_depth;
            slot_minutes[s] = p->play_minutes;
        }
    }
    DISABLE_RAM;
}

static void new_profile(void) {
    memset(&profile, 0, sizeof(profile));
    profile.level = 1;
    // A plain pistol to start with; the rest of the loadout empty.
    for (uint8_t s = 1; s < LOADOUT; s++) profile.guns[s].type = GUN_NONE;
    gun_plain(&profile.guns[0], WEAPON_PISTOL);
    profile.pack_size = PACK_START;
    for (uint8_t s = 0; s < PACK_MAX; s++) profile.pack[s].type = GUN_NONE;
}

void save_new(uint8_t slot, uint8_t cls) BANKED {
    save_slot = slot;
    new_profile();
    profile.cls = cls;
    ENABLE_RAM;
    SWITCH_RAM(0);
    uint8_t *r = newest(slot);  // (a new file over an old one: keep counting its copies on)
    slot_seq = r ? r[3] : 0;
    DISABLE_RAM;
    save_write();
}

void save_none(void) BANKED {
    save_slot = SAVE_NONE;
    new_profile();
}

void save_open(uint8_t slot) BANKED {
    save_slot = slot;
    ENABLE_RAM;
    SWITCH_RAM(0);
    uint8_t *r = newest(slot);
    if (r) {
        memcpy(&profile, r + HEADER, sizeof(profile));
        slot_seq = r[3];
    } else {
        slot_seq = 0;
    }
    DISABLE_RAM;
    if (!r) {
        new_profile();
        save_write();
    }
}

void save_write(void) BANKED {
    if (save_slot == SAVE_NONE) return;
    ENABLE_RAM;
    SWITCH_RAM(0);
    // Over the older copy (or a bad one), so the newer stays whole until this is.
    uint8_t *a = copy_at(save_slot, 0), *b = copy_at(save_slot, 1);
    uint8_t *w = !valid(a) ? a : !valid(b) ? b : (int8_t)(b[3] - a[3]) > 0 ? a : b;
    w[0] = 0;  // no magic while it's being written
    memcpy(w + HEADER, &profile, sizeof(profile));
    uint16_t c = checksum(w + HEADER, sizeof(profile));
    w[2] = SAVE_VERSION;
    w[3] = ++slot_seq;
    w[4] = c & 0xFF;
    w[5] = c >> 8;
    w[6] = sizeof(profile);
    w[1] = 'K';
    w[0] = 'S';
    DISABLE_RAM;
    saves_written++;
}

void save_add_time(uint16_t seconds) BANKED {
    profile.play_minutes += seconds / 60;
    profile.play_seconds += seconds % 60;
    if (profile.play_seconds >= 60) {
        profile.play_seconds -= 60;
        profile.play_minutes++;
    }
}
