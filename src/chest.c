#pragma bank 255
// The arena's chest and the shield in it: see chest.h. Banked: a few compares a
// frame. The chest's block is BLK_PROP (solid to the player, drawn as floor),
// with the chest drawn over it as a sprite on the enemy pea's palette (wood and
// brass); the shield uses the weapons' gunmetal. Both have their own hardware
// sprites (OAM_PROP: 2 for the chest, 2 for the shield).
#include <gb/gb.h>
#include "chest.h"
#include "room.h"
#include "camera.h"
#include "player.h"
#include "health.h"
#include "actor.h"
#include "gfx.h"
#include "oam.h"
#include "spr_chest.h"
#include "spr_shield_item.h"

extern uint8_t wave_spawned;  // play.c

// Sprite tiles (bank 0): above the effects' tiles, below 128 (from 128 on, sprite
// tiles share VRAM with background tiles 128-255).
#define CHEST_VRAM 96
#define ITEM_VRAM (CHEST_VRAM + SPR_CHEST_COLUMN_COUNT * 2)

#define ITEM_UP (3 << 8)    // the shield's pop: upward speed, px/frame 8.8 ...
#define ITEM_GRAV 0x28      // ... and gravity
#define ITEM_DROP 18        // px it lands in front of (below) the chest
#define OPEN_REACH 14       // px between the player's feet and the chest's middle to open it
#define TAKE_REACH 10       // ... and the shield's, to take it

uint8_t chest_state;
uint16_t item_x, item_y;
int16_t item_z, item_vz;
uint8_t item_landed;
uint8_t item_drop;   // px still to drift down toward where it lands
uint8_t chest_tick;
uint8_t chest_frames[SPR_CHEST_FRAME_COUNT * 4];
uint8_t item_frames[SPR_SHIELD_ITEM_FRAME_COUNT * 4];

#define CHEST_X ((uint16_t)CHEST_BX * BLOCK_SIZE)
#define CHEST_Y ((uint16_t)CHEST_BY * BLOCK_SIZE)

void chest_init(void) BANKED {
    banked_sprite_data(CHEST_VRAM, SPR_CHEST_COLUMN_COUNT * 2, spr_chest_tiles, BANK(spr_chest));
    banked_memcpy(chest_frames, spr_chest_frames, sizeof(chest_frames), BANK(spr_chest));
    banked_sprite_data(ITEM_VRAM, SPR_SHIELD_ITEM_COLUMN_COUNT * 2, spr_shield_item_tiles, BANK(spr_shield_item));
    banked_memcpy(item_frames, spr_shield_item_frames, sizeof(item_frames), BANK(spr_shield_item));
    chest_state = CHEST_LOCKED;
    item_landed = 0;
    chest_hide();
}

void chest_hide(void) BANKED {
    for (uint8_t i = 0; i < 4; i++) hide_sprite(OAM_PROP + i);
}

// Put a 16x16 frame (frames: left column, flags, right column, flags) at room
// pixel (x, y) in sprites oam and oam+1, or hide them when off screen.
static void draw16(uint8_t oam, const uint8_t *f, uint8_t vram, uint8_t pal, uint16_t x, uint16_t y) {
    int16_t sx = (int16_t)(x - cam_x), sy = (int16_t)(y - cam_y);
    if (sx <= -16 || sx >= VIEW_W || sy <= -16 || sy >= VIEW_H) {
        hide_sprite(oam);
        hide_sprite(oam + 1);
        return;
    }
    set_sprite_tile(oam, vram + (f[0] << 1));
    set_sprite_prop(oam, (f[1] & 0xF8) | pal);
    set_sprite_tile(oam + 1, vram + (f[2] << 1));
    set_sprite_prop(oam + 1, (f[3] & 0xF8) | pal);
    move_sprite(oam, (uint8_t)sx + 8, (uint8_t)sy + 16);
    move_sprite(oam + 1, (uint8_t)sx + 16, (uint8_t)sy + 16);
}

// Is the player's feet point within reach px (each axis) of (x, y)?
static uint8_t near(uint16_t x, uint16_t y, uint8_t reach) {
    int16_t dx = (int16_t)(player_x + 8 - x), dy = (int16_t)(player_y + 12 - y);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx <= reach && dy <= reach;
}

void chest_update(void) BANKED {
    chest_tick++;
    switch (chest_state) {
    case CHEST_LOCKED:
        if (wave_spawned && !actors_active) chest_state = CHEST_READY;
        break;
    case CHEST_READY:
        if (near(CHEST_X + 8, CHEST_Y + 8, OPEN_REACH)) {
            chest_state = CHEST_OPEN;
            item_x = CHEST_X;
            item_y = CHEST_Y;
            item_z = 0;
            item_vz = ITEM_UP;
            item_drop = ITEM_DROP;
            item_landed = 0;
        }
        break;
    case CHEST_OPEN:
        if (!item_landed) {
            // Pops up out of the chest and drifts forward until it lands.
            item_z += item_vz;
            item_vz -= ITEM_GRAV;
            if (item_drop) {
                item_y++;
                item_drop--;
            }
            if (item_z <= 0 && item_vz < 0) {
                item_z = 0;
                item_y += item_drop;
                item_drop = 0;
                item_landed = 1;
            }
        } else if (near(item_x + 8, item_y + 8, TAKE_REACH)) {
            // Equipped: it starts empty and charges up, so the bar fills on the HUD.
            health_give_shield(SHIELD_START_CAP, SHIELD_START_DELAY, SHIELD_START_RATE);
            shield_hp = 0;
            shield_wait = 0;
            shield_flash = 8;
            chest_state = CHEST_TAKEN;
        }
        break;
    }

    // The chest: closed (glowing gold, on the shots' brighter palette, for a
    // moment every half second once it can be opened), or open.
    uint8_t f = chest_state >= CHEST_OPEN ? 4 : 0;
    uint8_t pal = chest_state == CHEST_READY && (chest_tick & 16) ? SPR_PAL_SHOT : SPR_PAL_BULLET;
    draw16(OAM_PROP, &chest_frames[f], CHEST_VRAM, pal, CHEST_X, CHEST_Y);
    // The shield, bobbing once it's down.
    if (chest_state == CHEST_OPEN) {
        uint8_t bob = item_landed && (chest_tick & 32) ? 1 : 0;
        draw16(OAM_PROP + 2, item_frames, ITEM_VRAM, SPR_PAL_WEAPON, item_x, item_y - (uint8_t)(item_z >> 8) - bob);
    } else {
        hide_sprite(OAM_PROP + 2);
        hide_sprite(OAM_PROP + 3);
    }
}
