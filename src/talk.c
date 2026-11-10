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
#include "spr_talk_lift.h"
#include "spr_person_tess.h"
#include "spr_person_okafor.h"
#include "spr_person_nell.h"
#include "spr_person_pip.h"
#include "save.h"

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
uint8_t zone_story[TALK_ZONES];
uint16_t talks;
uint8_t talk_frames[(TALK_KINDS + TALK_PEOPLE) * 4];  // each kind's frame (then each person's): left column, flags, right column, flags
uint8_t talk_people_at = TALK_VRAM;
uint8_t person_vram[TALK_PEOPLE];  // where each person standing here has their tiles
uint8_t talk_tick;
uint8_t bubble_oam = 0xFF;  // where the bubble was drawn last frame

// The bubble: an "A" in a white speech bubble (UI palette), over a blank tile.
static const uint8_t bubble_tiles[32] = {
    0x7E, 0x7E, 0xFF, 0x81, 0xFF, 0x99, 0xFF, 0xA5, 0xFF, 0xBD, 0xFF, 0xA5, 0x7E, 0x7E, 0x18, 0x18,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Each kind's palette slot: the enemies' (talk.h), the lift the flash's.
static const uint8_t kind_pal[TALK_KINDS] = {SPR_PAL_SLIME, SPR_PAL_SHOOTER, SPR_PAL_BULLET, SPR_PAL_FLASH};
uint8_t talk_action;
uint16_t flash_saved[4];  // the flash palette, while the lift has it
uint8_t flash_borrowed;

void talk_clear(void) BANKED {
    if (flash_borrowed) {
        for (uint8_t i = 0; i < 4; i++) pal_base_obj[(SPR_PAL_FLASH << 2) + i] = flash_saved[i];
        flash_borrowed = 0;
        gfx_set_fade(0);
    }
    talk_action = 0;
    talk_count = zone_count = 0;
    zones_done = 0;
    talk_near = TALK_NONE;
    talk_people_at = TALK_VRAM;
}

void talk_add(uint8_t bx, uint8_t by, uint8_t kind, uint8_t script) BANKED {
    if (talk_count >= TALK_MAX) return;
    uint8_t i = talk_count++;
    talk_bx[i] = bx;
    talk_by[i] = by;
    talk_kind[i] = kind;
    talk_script[i] = script;
    room_map[((uint16_t)by << 5) | bx] = BLK_SOLID;  // solid (not a chest's step: no jumping over people)
}

void talk_remove(uint8_t i) BANKED {
    room_map[((uint16_t)talk_by[i] << 5) | talk_bx[i]] = BLK_FLOOR;
    talk_count--;
    for (; i < talk_count; i++) {
        talk_bx[i] = talk_bx[i + 1];
        talk_by[i] = talk_by[i + 1];
        talk_kind[i] = talk_kind[i + 1];
        talk_script[i] = talk_script[i + 1];
    }
    talk_near = TALK_NONE;
}

void talk_zone(uint8_t bx0, uint8_t by0, uint8_t bx1, uint8_t by1, uint8_t script, uint8_t story) BANKED {
    if (zone_count >= TALK_ZONES || (profile.story & story)) return;
    uint8_t i = zone_count++;
    zone_x0[i] = bx0;
    zone_y0[i] = by0;
    zone_x1[i] = bx1;
    zone_y1[i] = by1;
    zone_script[i] = script;
    zone_story[i] = story;
}

// The people's art (TALK_PERSON + who): its tiles, frames (their palettes aren't used).
static const uint8_t *const person_tiles[TALK_PEOPLE] = {spr_person_tess_tiles, spr_person_okafor_tiles, spr_person_nell_tiles, spr_person_pip_tiles};
static const uint8_t *const person_frames[TALK_PEOPLE] = {spr_person_tess_frames, spr_person_okafor_frames, spr_person_nell_frames, spr_person_pip_frames};
static uint8_t person_bank(uint8_t p) {
    return p == 0 ? BANK(spr_person_tess) : p == 1 ? BANK(spr_person_okafor) : p == 2 ? BANK(spr_person_nell) : BANK(spr_person_pip);
}

static void load_person(uint8_t p, uint8_t v) {
    person_vram[p] = v;
    uint8_t bank = person_bank(p);
    banked_sprite_data(v, 4, person_tiles[p], bank);
    banked_memcpy(&talk_frames[(TALK_KINDS + p) << 2], person_frames[p], 4, bank);
}

void talk_person_load(uint8_t who) BANKED {
    load_person(who, talk_people_at);
}

uint8_t talk_person_draw(uint8_t oam, uint8_t who, uint16_t x, uint16_t y, uint8_t pal) BANKED {
    uint16_t sx = x - cam_x + 8, sy = y - cam_y + 16;
    if (sx >= 168 + 8 || sy >= VIEW_H + 16 + 16) return oam;
    const uint8_t *f = &talk_frames[(TALK_KINDS + who) << 2];
    uint8_t v = person_vram[who];
    set_sprite_tile(oam, v + (f[0] << 1));
    set_sprite_prop(oam, f[1] | pal);
    move_sprite(oam++, (uint8_t)sx, (uint8_t)sy);
    set_sprite_tile(oam, v + (f[2] << 1));
    set_sprite_prop(oam, f[3] | pal);
    move_sprite(oam++, (uint8_t)sx + 8, (uint8_t)sy);
    return oam;
}

static void load_kind(uint8_t k, const uint8_t *tiles, const uint8_t *frames, const uint16_t *pal, uint8_t bank) {
    banked_sprite_data(TALK_VRAM + (k << 2), 4, tiles, bank);
    banked_memcpy(&talk_frames[k << 2], frames, 4, bank);
    banked_memcpy(&pal_base_obj[kind_pal[k] << 2], pal, 8, bank);
}

void talk_init(void) BANKED {
    if (!talk_count && !zone_count) return;
    // Only the kinds standing here: their palettes are borrowed.
    uint8_t kinds = 0, people = 0;
    for (uint8_t i = 0; i < talk_count; i++) {
        uint8_t k = talk_kind[i];
        if (k >= TALK_PERSON) people |= 1 << (k - TALK_PERSON);
        else kinds |= 1 << k;
    }
    uint8_t v = talk_people_at;
    for (uint8_t p = 0; p < TALK_PEOPLE; p++) {
        if (!(people & (1 << p))) continue;
        load_person(p, v);
        v += 4;
    }
    if (kinds & (1 << TALK_RELAY)) load_kind(TALK_RELAY, spr_talk_relay_tiles, spr_talk_relay_frames, spr_talk_relay_palette, BANK(spr_talk_relay));
    if (kinds & (1 << TALK_POST)) load_kind(TALK_POST, spr_talk_post_tiles, spr_talk_post_frames, spr_talk_post_palette, BANK(spr_talk_post));
    if (kinds & (1 << TALK_MARRO)) load_kind(TALK_MARRO, spr_talk_marro_tiles, spr_talk_marro_frames, spr_talk_marro_palette, BANK(spr_talk_marro));
    if (kinds & (1 << TALK_LIFT)) {
        if (!flash_borrowed) {
            for (uint8_t i = 0; i < 4; i++) flash_saved[i] = pal_base_obj[(SPR_PAL_FLASH << 2) + i];
            flash_borrowed = 1;
        }
        load_kind(TALK_LIFT, spr_talk_lift_tiles, spr_talk_lift_frames, spr_talk_lift_palette, BANK(spr_talk_lift));
    }
    set_sprite_data(BUBBLE_VRAM, 2, bubble_tiles);
    gfx_set_fade(0);
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
            if (zone_story[i]) {
                profile.story |= zone_story[i];
                save_write();
            }
            dlg_start(zone_script[i]);
            return;
        }
    }
    // The nearest in reach (two side by side, like a vendor post's machines: the one
    // you're in front of).
    talk_near = TALK_NONE;
    uint8_t best = 0xFF;
    for (uint8_t i = 0; i < talk_count; i++) {
        uint16_t tx = ((uint16_t)talk_bx[i] << 4) + 8, ty = ((uint16_t)talk_by[i] << 4) + 8;
        uint16_t dx = px > tx ? px - tx : tx - px, dy = py > ty ? py - ty : ty - py;
        if (dx < REACH && dy < REACH && (uint8_t)(dx + dy) < best) {
            best = (uint8_t)(dx + dy);
            talk_near = i;
        }
    }
}

void talk_start(void) BANKED {
    talks++;
    if (bubble_oam != 0xFF) shadow_OAM[bubble_oam].y = 0;  // no bubble while they talk
    uint8_t s = talk_script[talk_near];
    if (s & TALK_DO) talk_action = s;  // the shop or a lift: play.c's
    else dlg_start(s);
}

uint8_t talk_draw(uint8_t oam) BANKED {
    if (!talk_count) return oam;
    talk_tick++;
    bubble_oam = 0xFF;
    for (uint8_t i = 0; i < talk_count; i++) {
        // Screen position (hardware: +8, +16); skip those off the view.
        uint16_t sx = ((uint16_t)talk_bx[i] << 4) - cam_x + 8, sy = ((uint16_t)talk_by[i] << 4) - cam_y + 16;
        if (sx >= 168 + 8 || sy >= VIEW_H + 16 + 16) continue;
        uint8_t k = talk_kind[i];
        if (k != TALK_COMMS) {  // (the comms device is the map's)
            const uint8_t *f;
            uint8_t v, pal;
            if (k >= TALK_PERSON) {
                k -= TALK_PERSON;
                f = &talk_frames[(TALK_KINDS + k) << 2];
                v = person_vram[k];
                pal = SPR_PAL_PLAYER;
            } else {
                f = &talk_frames[k << 2];
                v = TALK_VRAM + (k << 2);
                pal = kind_pal[k];
            }
            set_sprite_tile(oam, v + (f[0] << 1));
            set_sprite_prop(oam, f[1] | pal);
            move_sprite(oam++, (uint8_t)sx, (uint8_t)sy);
            set_sprite_tile(oam, v + (f[2] << 1));
            set_sprite_prop(oam, f[3] | pal);
            move_sprite(oam++, (uint8_t)sx + 8, (uint8_t)sy);
        }
        if (i == talk_near && sy >= BUBBLE_UP + 16) {
            // The bubble bobs a pixel, every 16 frames.
            bubble_oam = oam;
            set_sprite_tile(oam, BUBBLE_VRAM);
            set_sprite_prop(oam, SPR_PAL_UI);
            move_sprite(oam++, (uint8_t)sx + 4, (uint8_t)sy - BUBBLE_UP - ((talk_tick >> 4) & 1));
        }
    }
    return oam;
}
