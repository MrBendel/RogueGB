#pragma bank 255
#include <gb/gb.h>
#include <gb/metasprites.h>
#include "town.h"
#include "room.h"
#include "gfx.h"
#include "camera.h"
#include "player.h"
#include "input.h"
#include "actor.h"
#include "talk.h"
#include "dialogue.h"
#include "pickups.h"
#include "hud.h"
#include "state.h"
#include "tiles_town_inside.h"
#include "far.h"
#include "descent.h"

uint8_t town_inside = TOWN_OUTSIDE, town_changing;
uint8_t town_saved_rooms, town_drop_end;
extern uint8_t pickups_map, floor_icon[DROP_GUNS][4];
// Facade door tiles, matching the intact buildings in Sands.png.
static const uint8_t door_x[TOWN_BUILDINGS] = {5, 7, 10};
static const uint8_t door_y[TOWN_BUILDINGS] = {8, 3, 6};
extern uint16_t safe_x, safe_y, player_anim_dist;
extern int16_t safe_z;
extern uint8_t player_jump_buffer;

void town_reset(void) BANKED {
    town_inside = TOWN_OUTSIDE;
    town_changing = town_saved_rooms = town_drop_end = 0;
    edge_push = 0;
}

static void load_inside(void) {
    uint8_t bank = BANK(tiles_town_inside);
    const uint8_t *type, *vis;
    if (town_inside == TOWN_QUARTERS) {
        type = tiles_town_inside_quarters_type;
        vis = tiles_town_inside_quarters_vis;
    } else if (town_inside == TOWN_RELAY) {
        type = tiles_town_inside_relay_type;
        vis = tiles_town_inside_relay_vis;
    } else {
        type = tiles_town_inside_market_type;
        vis = tiles_town_inside_market_vis;
    }
    banked_bkg_data(SCENE_TILES, TILES_TOWN_INSIDE_TILE_COUNT, tiles_town_inside_tiles, bank);
    banked_memcpy(pal_base_bg, tiles_town_inside_pals, TILES_TOWN_INSIDE_PALETTES * 8, bank);
    for (uint8_t v = 0; v < TILES_TOWN_INSIDE_BLOCKS; v++) {
        uint8_t t[4], a[4];
        banked_memcpy(t, tiles_town_inside_blocks + (uint16_t)v * 4, 4, bank);
        banked_memcpy(a, tiles_town_inside_attrs + (uint16_t)v * 4, 4, bank);
        for (uint8_t q = 0; q < 4; q++) {
            ROOM_TILE(v, q) = SCENE_TILES + t[q];
            ROOM_ATTR(v, q) = a[q];
        }
    }
    room_attr_uniform = 0;
    room_w = 10; room_h = 8;
    room_px_w = 160; room_px_h = 128;
    for (uint16_t i = 0; i < sizeof(room_map); i++) {
        room_map[i] = BLK_SOLID;
        room_vis[i] = TS_TOWN_INSIDE_FLOOR;
    }
    for (uint8_t y = 0; y < 8; y++) {
        banked_memcpy(room_map + ((uint16_t)y << 5), type + y * 10, 10, bank);
        banked_memcpy(room_vis + ((uint16_t)y << 5), vis + y * 10, 10, bank);
    }
    if (town_inside == TOWN_MARKET) {
        talk_add(4, 3, TALK_MARRO, TALK_DO_SHOP);
        talk_add(7, 4, TALK_COMMS, TALK_DO_VEND_GUN);
        talk_add(8, 4, TALK_COMMS, TALK_DO_VEND_SHIELD);
    } else if (town_inside == TOWN_RELAY) {
        talk_add(6, 3, TALK_RELAY, DLG_VOICE_ECHO);
    } else {
        talk_add(7, 2, TALK_POST, DLG_VOICE_MERIDIAN);
    }
}

// Floor guns belong to their room, including uncollected vending purchases.
// Cold state uses far RAM; four rooms cost one byte of persistent near RAM.
static void store_loot(uint8_t room) {
    uint8_t *at = FAR_TOWN_LOOT + ((uint16_t)(room & 3) << 7);
    FAR_PUT(at, drop_kind, sizeof(drop_kind));
    FAR_PUT(at + 16, drop_x, sizeof(drop_x));
    FAR_PUT(at + 32, drop_y, sizeof(drop_y));
    FAR_PUT(at + 48, drop_value, sizeof(drop_value));
    FAR_PUT(at + 64, floor_gun, sizeof(floor_gun));
    FAR_PUT(at + 96, floor_icon, sizeof(floor_icon));
    town_saved_rooms |= 1 << (room & 3);
}

static void load_loot(uint8_t room) {
    uint8_t *at = FAR_TOWN_LOOT + ((uint16_t)(room & 3) << 7);
    pickups_load_map(0);
    if (!(town_saved_rooms & (1 << (room & 3)))) return;
    FAR_GET(drop_kind, at, sizeof(drop_kind));
    FAR_GET(drop_x, at + 16, sizeof(drop_x));
    FAR_GET(drop_y, at + 32, sizeof(drop_y));
    FAR_GET(drop_value, at + 48, sizeof(drop_value));
    FAR_GET(floor_gun, at + 64, sizeof(floor_gun));
    FAR_GET(floor_icon, at + 96, sizeof(floor_icon));
    for (uint8_t i = 0; i < DROP_MAX; i++) if (drop_kind[i]) pickups_on = 1;
}

static void change_room(uint8_t next) {
    uint8_t previous = town_inside;
    pickups_card_off();
    store_loot(previous);
    fade_out();
    DISPLAY_OFF;
    gfx_sprites(0);
    actors_clear();
    talk_clear();
    town_changing = 1;
    town_inside = next;
    if (next == TOWN_OUTSIDE) {
        room_load_town();
        player_x = (uint16_t)door_x[previous] * 16;
        // Keep the entire foot hitbox inside the one-tile doorstep. An extra
        // vertical offset embeds its bottom in the cliff below and makes the
        // first sideways step snap to the opposite edge of that solid block.
        player_y = ((uint16_t)door_y[previous] + 2) * 16 - PLAYER_HB_Y - PLAYER_HB_H;
        player_facing = DIR_DOWN;
        player_anim_dir = 0;
    } else {
        load_inside();
        player_x = 4 * 16;
        player_y = 5 * 16 + 4;
        player_facing = DIR_UP;
        player_anim_dir = 1;
    }
    load_loot(next);
    town_changing = 0;
    player_vx = player_vy = player_xs = player_ys = 0;
    player_z = player_vz = player_air = player_dash_timer = 0;
    player_anim_frame = player_anim_dist = player_jump_buffer = 0;
    safe_x = player_x; safe_y = player_y; safe_z = 0;
    hud_init();
    hud_draw();
    talk_init();
    camera_init(player_x + 8, player_y + 8);
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
    player_draw();
    DISPLAY_ON;
    gfx_sprites(1);
    fade_in();
}

// South-facing cliff faces: row 10, columns 3..5 and 9..14. Collision stays
// solid; a committed hop crosses the whole face without stranding the player.
uint8_t town_move(void) BANKED {
    if (town_inside != TOWN_OUTSIDE) return 0;
    if (!town_drop_end) {
        if (!player_air || player_vz <= 0 || !(joy & J_DOWN) || player_dash_timer || player_stun) return 0;
        uint16_t feet = player_y + PLAYER_HB_Y + PLAYER_HB_H;
        uint16_t left = player_x + PLAYER_HB_X, right = left + PLAYER_HB_W - 1;
        if (feet < 158 || feet > 160) return 0;
        if (!((left >= 3*16 && right < 6*16) || (left >= 9*16 && right < 15*16))) return 0;
        town_drop_end = 11*16 - PLAYER_HB_Y;
    }
    player_vx = player_vy = player_xs = player_ys = 0;
    player_facing = DIR_DOWN;
    player_anim_dir = 0;
    player_y += 2;
    player_z += player_vz;
    player_vz -= jump_grav;
    if (player_z < (2 << 8)) player_z = 2 << 8;
    player_air = 1;
    if (player_blink) player_blink--;
    if (player_invuln) player_invuln--;
    if (player_y >= town_drop_end) {
        player_y = town_drop_end;
        town_drop_end = 0;
    }
    return 1;
}

uint8_t town_update(void) BANKED {
    if (player_air || player_z || player_roll_len || player_dash_timer || player_stun) return 0;
    uint8_t x = (player_x + 8) >> 4;
    if (town_inside == TOWN_OUTSIDE && (x == 7 || x == 8) && player_y + 12 >= room_px_h - 5 && (joy & J_DOWN)) {
        if (++edge_push >= EDGE_PUSH) { edge_push = 0; return 2; }
    } else edge_push = 0;
    if (town_inside != TOWN_OUTSIDE) {
        if ((joy & J_DOWN) && (x == 4 || x == 5) && player_y + 12 >= 7 * 16) {
            change_room(TOWN_OUTSIDE);
            return 1;
        }
    } else if (joy & J_UP) {
        for (uint8_t i = 0; i < TOWN_BUILDINGS; i++) {
            uint16_t step = ((uint16_t)door_y[i] + 1) * 16;
            if (x == door_x[i] && player_y + 8 <= step + 2 && player_y + 14 > step) {
                change_room(i);
                return 1;
            }
        }
    }
    return 0;
}
