#pragma bank 255
// First authored surface region. Uses the expedition's graph, camps and save
// fields; WORLD_DESERT distinguishes it from generated underground expeditions.
#include "desert.h"
#include "level.h"
#include "room.h"
#include "gfx.h"
#include "tiles_sands.h"

uint8_t desert_return;

void desert_generate(void) BANKED {
    area_seen = 0;
    for (uint8_t i = 0; i < AREA_CELLS; i++) {
        area_type[i] = area_links[i] = area_cw[i] = area_ch[i] = 0;
        area_camps_cleared[i] = area_chests_opened[i] = 0;
        area_conts_opened[i] = area_inside_opened[i] = 0;
    }
    area_start = DESERT_CROSSROADS;
    area_exit = DESERT_RUSTYARD; // objective on the map; no shaft in this slice
    area_maps = 3;
    area_type[area_start] = MAP_START;
    area_type[area_exit] = MAP_FIELD;
    area_links[area_start] = EDGE_E | EDGE_S;
    area_links[area_exit] = EDGE_W;
    area_cw[area_start] = area_cw[area_exit] = 4;
    area_ch[area_start] = area_ch[area_exit] = 4;
    area_type[DESERT_WAKE] = MAP_FIELD;
    area_links[DESERT_WAKE] = EDGE_N;
    area_cw[DESERT_WAKE] = 4; area_ch[DESERT_WAKE] = 6;
}

void desert_build(uint8_t map) BANKED {
    uint8_t cross = map == DESERT_CROSSROADS;
    uint8_t wake = map == DESERT_WAKE;
    const uint8_t *types = wake ? tiles_sands_wake_type : cross ? tiles_sands_crossroads_type : tiles_sands_rustyard_type;
    const uint8_t *looks = wake ? tiles_sands_wake_vis : cross ? tiles_sands_crossroads_vis : tiles_sands_rustyard_vis;
    room_w = 20; room_h = wake ? 24 : 16;
    room_px_w = 320; room_px_h = (uint16_t)room_h * 16;
    for (uint16_t k = 0; k < sizeof(room_map); k++) {
        room_map[k] = BLK_SOLID;
        room_vis[k] = TS_SANDS_SAND;
    }
    for (uint8_t y = 0; y < room_h; y++) {
        banked_memcpy(room_map + ((uint16_t)y << 5), types + (uint16_t)y * 20, 20, BANK(tiles_sands));
        banked_memcpy(room_vis + ((uint16_t)y << 5), looks + (uint16_t)y * 20, 20, BANK(tiles_sands));
    }
    level_roofed = level_doors = level_conts = level_vends = level_carved = 0;
    level_open[0] = 8;
    level_open[1] = level_open[3] = 7;
    level_open[2] = 8;
    level_camps = wake ? 2 : cross ? 0 : 1;
    level_chests = 1;
    chest_x[0] = (wake ? 9 : cross ? 3 : 15) * 16;
    chest_y[0] = (wake ? 18 : cross ? 9 : 10) * 16;
    chest_camp[0] = cross || wake ? 0xFF : 0;
    if (wake) {
        static const uint8_t sx[6] = {7, 10, 12, 7, 11, 14}, sy[6] = {12, 11, 13, 5, 5, 7};
        for (uint8_t c = 0; c < 2; c++) {
            camp_x[c] = 10 * 16; camp_y[c] = (c ? 6 : 12) * 16;
            camp_slots[c] = 3;
            for (uint8_t i = 0; i < 3; i++) {
                camp_slot_x[c][i] = (uint16_t)sx[c * 3 + i] * 16;
                camp_slot_y[c][i] = (uint16_t)sy[c * 3 + i] * 16;
            }
        }
    } else if (!cross) {
        static const uint8_t sx[3] = {11, 15, 11}, sy[3] = {6, 9, 11};
        camp_x[0] = 13 * 16; camp_y[0] = 9 * 16;
        camp_slots[0] = 3;
        for (uint8_t i = 0; i < 3; i++) {
            camp_slot_x[0][i] = (uint16_t)sx[i] * 16;
            camp_slot_y[0][i] = (uint16_t)sy[i] * 16;
        }
    }
}
