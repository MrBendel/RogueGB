#pragma bank 255
// Sands settlement: authored 16px blocks, cut from the original four-colour sheet.
#include <gb/gb.h>
#include <gb/cgb.h>
#include "room.h"
#include "gfx.h"
#include "talk.h"
#include "save.h"
#include "dialogue.h"
#include "tiles_sands.h"
#include "level.h"
#include "pickups.h"

#if TILES_SANDS_TILE_COUNT <= 172 || TILES_SANDS_TILE_COUNT > 220
#error Sands loader expects 173..220 tiles across two VRAM banks
#endif

#define TOWN_W TS_SANDS_MAP_TOWN_W
#define TOWN_H TS_SANDS_MAP_TOWN_H

void room_load_town(void) BANKED {
    uint8_t bank = BANK(tiles_sands);
    // Preserve the authored terrain instead of dropping unique edge/detail tiles.
    // The extra bank-1 tiles use $9500..$97ff, above weapon and portrait art.
    banked_bkg_data(SCENE_TILES, 172, tiles_sands_tiles, bank);
    VBK_REG = 1;
    banked_bkg_data(SCENE_TILES, TILES_SANDS_TILE_COUNT - 172, tiles_sands_tiles + 172u * 16u, bank);
    VBK_REG = 0;
    banked_memcpy(pal_base_bg, tiles_sands_pals, TILES_SANDS_PALETTES * 4 * sizeof(uint16_t), bank);
    // Each block's 4 tiles (as scene tiles) and their attributes (palette, flips).
    for (uint8_t v = 0; v < TILES_SANDS_BLOCKS; v++) {
        uint8_t t[4], a[4];
        banked_memcpy(t, tiles_sands_blocks + (uint16_t)v * 4, 4, bank);
        banked_memcpy(a, tiles_sands_attrs + (uint16_t)v * 4, 4, bank);
        for (uint8_t q = 0; q < 4; q++) {
            ROOM_TILE(v, q) = SCENE_TILES + (t[q] < 172 ? t[q] : t[q] - 172);
            ROOM_ATTR(v, q) = a[q] | (t[q] < 172 ? 0 : 8);
        }
    }
    room_attr_uniform = 0;  // palettes and flips per tile: stream attributes while scrolling

    room_w = TOWN_W;
    room_h = TOWN_H;
    room_px_w = (uint16_t)TOWN_W * BLOCK_SIZE;
    room_px_h = (uint16_t)TOWN_H * BLOCK_SIZE;
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID, room_vis[i] = TS_SANDS_SAND;
    for (uint8_t y = 0; y < TOWN_H; y++) {
        uint16_t k = (uint16_t)y << 5, from = (uint16_t)y * TOWN_W;
        banked_memcpy(room_map + k, tiles_sands_town_type + from, TOWN_W, bank);
        banked_memcpy(room_vis + k, tiles_sands_town_vis + from, TOWN_W, bank);
    }
    // Who's here (docs/design/voices.md): Echo's relay (walking up to it plays her
    // first contact once), the company's posts, Marro at his shop, and the lift down.
    talk_add(7, 4, TALK_RELAY, DLG_VOICE_ECHO);
    talk_zone(7, 4, 10, 6, DLG_RELAY_FIRST_CONTACT, STORY_MET_ECHO);
    talk_add(20, 5, TALK_POST, DLG_VOICE_CEO);
    talk_add(20, 13, TALK_POST, DLG_VOICE_MERIDIAN);
    talk_add(18, 10, TALK_MARRO, TALK_DO_SHOP);
    // Beside his shop, the vending machines (drawn by the map): guns, shields.
    talk_add(20, 9, TALK_COMMS, TALK_DO_VEND_GUN);
    talk_add(21, 9, TALK_COMMS, TALK_DO_VEND_SHIELD);
    talk_add(4, 12, TALK_LIFT, TALK_DO_DOWN);
    // The company's contracts board, by the yard: jobs to take down (board.c).
    talk_add(7, 10, TALK_POST, TALK_DO_BOARD);
    // No chests or containers (the last area's are still listed): the floor
    // only holds what the gun machine drops.
    level_chests = level_conts = 0;
    pickups_load_map(0);
}
