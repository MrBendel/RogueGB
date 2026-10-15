#pragma bank 255
// Talking (talk.h). Banked: a few compares a frame, and only in maps with talkers.
#include <gb/gb.h>
#include "talk.h"
#include "dialog.h"
#include "gfx.h"
#include "room.h"
#include "player.h"
#include "camera.h"
#include "spr_talk_relay.h"
#include "spr_talk_post.h"
#include "spr_talk_marro.h"

// Sprite tiles (bank 0): after the chest's, below 128 (see chest.c). 4 a kind
// (one 16x16 frame), then the bubble's 2.
#define TALK_VRAM 108
#define BUBBLE_VRAM (TALK_VRAM + TALK_KINDS * 4)
#define REACH 20        // px between the player's middle and a talker's to talk
#define BUBBLE_UP 14    // px the bubble floats above a talker's top

uint8_t talk_count;
uint8_t talk_bx[TALK_MAX], talk_by[TALK_MAX], talk_kind[TALK_MAX], talk_script[TALK_MAX];
uint8_t talk_near = TALK_NONE;
uint8_t zone_count;
uint8_t zone_x0[TALK_ZONES], zone_y0[TALK_ZONES], zone_x1[TALK_ZONES], zone_y1[TALK_ZONES], zone_script[TALK_ZONES];
uint8_t zones_done;
uint16_t talks;
uint8_t talk_frames[TALK_KINDS * 4];  // each kind's frame: left column, flags, right column, flags
uint8_t talk_tick;

// The bubble: an "A" in a white speech bubble (UI palette), over a blank tile.
static const uint8_t bubble_tiles[32] = {
    0x7E, 0x7E, 0xFF, 0x81, 0xFF, 0x99, 0xFF, 0xA5, 0xFF, 0xBD, 0xFF, 0xA5, 0x7E, 0x7E, 0x18, 0x18,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Each kind's palette slot: the enemies' (talk.h).
static const uint8_t kind_pal[TALK_KINDS] = {SPR_PAL_SLIME, SPR_PAL_SHOOTER, SPR_PAL_BULLET};

void talk_clear(void) BANKED {
    talk_count = zone_count = 0;
    zones_done = 0;
    talk_near = TALK_NONE;
}

void talk_add(uint8_t bx, uint8_t by, uint8_t kind, uint8_t script) BANKED {
    if (talk_count >= TALK_MAX) return;
    uint8_t i = talk_count++;
    talk_bx[i] = bx;
    talk_by[i] = by;
    talk_kind[i] = kind;
    talk_script[i] = script;
    room_map[((uint16_t)by << 5) | bx] = BLK_PROP;  // solid to walk into, drawn as what's under it
}

void talk_zone(uint8_t bx0, uint8_t by0, uint8_t bx1, uint8_t by1, uint8_t script) BANKED {
    if (zone_count >= TALK_ZONES) return;
    uint8_t i = zone_count++;
    zone_x0[i] = bx0;
    zone_y0[i] = by0;
    zone_x1[i] = bx1;
    zone_y1[i] = by1;
    zone_script[i] = script;
}

static void load_kind(uint8_t k, const uint8_t *tiles, const uint8_t *frames, const uint16_t *pal, uint8_t bank) {
    banked_sprite_data(TALK_VRAM + (k << 2), 4, tiles, bank);
    banked_memcpy(&talk_frames[k << 2], frames, 4, bank);
    banked_memcpy(&pal_base_obj[kind_pal[k] << 2], pal, 8, bank);
}

void talk_init(void) BANKED {
    if (!talk_count && !zone_count) return;
    load_kind(TALK_RELAY, spr_talk_relay_tiles, spr_talk_relay_frames, spr_talk_relay_palette, BANK(spr_talk_relay));
    load_kind(TALK_POST, spr_talk_post_tiles, spr_talk_post_frames, spr_talk_post_palette, BANK(spr_talk_post));
    load_kind(TALK_MARRO, spr_talk_marro_tiles, spr_talk_marro_frames, spr_talk_marro_palette, BANK(spr_talk_marro));
    set_sprite_data(BUBBLE_VRAM, 2, bubble_tiles);
}

void talk_update(void) BANKED {
    if (!talk_count && !zone_count) return;
    uint16_t px = player_x + 8, py = player_y + 10;  // the player's middle (a little low: the feet matter)
    uint8_t bx = px >> 4, by = py >> 4;
    // Zones: the first time you walk in, their script plays.
    for (uint8_t i = 0; i < zone_count; i++) {
        uint8_t bit = 1 << i;
        if (zones_done & bit) continue;
        if (bx >= zone_x0[i] && bx <= zone_x1[i] && by >= zone_y0[i] && by <= zone_y1[i]) {
            zones_done |= bit;
            dlg_start(zone_script[i]);
            return;
        }
    }
    talk_near = TALK_NONE;
    for (uint8_t i = 0; i < talk_count; i++) {
        uint16_t tx = ((uint16_t)talk_bx[i] << 4) + 8, ty = ((uint16_t)talk_by[i] << 4) + 8;
        uint16_t dx = px > tx ? px - tx : tx - px, dy = py > ty ? py - ty : ty - py;
        if (dx < REACH && dy < REACH) {
            talk_near = i;
            return;
        }
    }
}

void talk_start(void) BANKED {
    talks++;
    dlg_start(talk_script[talk_near]);
}

uint8_t talk_draw(uint8_t oam) BANKED {
    if (!talk_count) return oam;
    talk_tick++;
    for (uint8_t i = 0; i < talk_count; i++) {
        // Screen position (hardware: +8, +16); skip those off the view.
        uint16_t sx = ((uint16_t)talk_bx[i] << 4) - cam_x + 8, sy = ((uint16_t)talk_by[i] << 4) - cam_y + 16;
        if (sx >= 168 + 8 || sy >= VIEW_H + 16 + 16) continue;
        const uint8_t *f = &talk_frames[talk_kind[i] << 2];
        uint8_t v = TALK_VRAM + (talk_kind[i] << 2), pal = kind_pal[talk_kind[i]];
        set_sprite_tile(oam, v + (f[0] << 1));
        set_sprite_prop(oam, f[1] | pal);
        move_sprite(oam++, (uint8_t)sx, (uint8_t)sy);
        set_sprite_tile(oam, v + (f[2] << 1));
        set_sprite_prop(oam, f[3] | pal);
        move_sprite(oam++, (uint8_t)sx + 8, (uint8_t)sy);
        if (i == talk_near && sy >= BUBBLE_UP + 16) {
            // The bubble bobs a pixel, every 16 frames.
            set_sprite_tile(oam, BUBBLE_VRAM);
            set_sprite_prop(oam, SPR_PAL_UI);
            move_sprite(oam++, (uint8_t)sx + 4, (uint8_t)sy - BUBBLE_UP - ((talk_tick >> 4) & 1));
        }
    }
    return oam;
}
