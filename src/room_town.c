#pragma bank 255
// The town (the hub, with a file): drawn with the ice tileset, the first art cut
// from the artist's sheet (content/tilesets/ice.json: its pieces, and the town's
// map as rows of characters). tools/tileset.mjs fits the art to the Color's
// palettes and works out each block's type and look at build time, so loading is
// copying tables.
#include <gb/gb.h>
#include <gb/cgb.h>
#include "room.h"
#include "gfx.h"
#include "talk.h"
#include "save.h"
#include "dialogue.h"
#include "tiles_ice.h"

#define TOWN_W TS_ICE_MAP_TOWN_W
#define TOWN_H TS_ICE_MAP_TOWN_H

void room_load_town(void) BANKED {
    uint8_t bank = BANK(tiles_ice);
    banked_bkg_data(SCENE_TILES, TILES_ICE_TILE_COUNT, tiles_ice_tiles, bank);
    banked_memcpy(pal_base_bg, tiles_ice_pals, TILES_ICE_PALETTES * 4 * sizeof(uint16_t), bank);
    // Each block's 4 tiles (as scene tiles) and their attributes (palette, flips).
    banked_memcpy(room_tile_lut, tiles_ice_blocks, TILES_ICE_BLOCKS * 4, bank);
    banked_memcpy(room_attr_lut, tiles_ice_attrs, TILES_ICE_BLOCKS * 4, bank);
    for (uint16_t i = 0; i < TILES_ICE_BLOCKS * 4; i++) room_tile_lut[i] += SCENE_TILES;
    room_attr_uniform = 0;  // palettes and flips per tile: stream attributes while scrolling

    room_w = TOWN_W;
    room_h = TOWN_H;
    room_px_w = (uint16_t)TOWN_W * BLOCK_SIZE;
    room_px_h = (uint16_t)TOWN_H * BLOCK_SIZE;
    for (uint16_t i = 0; i < sizeof(room_map); i++) room_map[i] = BLK_SOLID, room_vis[i] = TS_ICE_SNOW;
    for (uint8_t y = 0; y < TOWN_H; y++) {
        uint16_t k = (uint16_t)y << 5, from = (uint16_t)y * TOWN_W;
        banked_memcpy(room_map + k, tiles_ice_town_type + from, TOWN_W, bank);
        banked_memcpy(room_vis + k, tiles_ice_town_vis + from, TOWN_W, bank);
    }
    // Who's here (docs/design/voices.md): Echo's relay (walking up to it plays her
    // first contact once), the company's posts, Marro at his shop, and the lift down.
    talk_add(9, 3, TALK_RELAY, DLG_VOICE_ECHO);
    talk_zone(8, 2, 11, 5, DLG_RELAY_FIRST_CONTACT, STORY_MET_ECHO);
    talk_add(20, 4, TALK_POST, DLG_VOICE_CEO);
    talk_add(20, 13, TALK_POST, DLG_VOICE_MERIDIAN);
    talk_add(18, 10, TALK_MARRO, TALK_DO_SHOP);
    talk_add(4, 12, TALK_LIFT, TALK_DO_DOWN);
}
