#pragma bank 255
// Player setup, banked: initial state and movement-preset changes. The per-frame
// work is in player.c (bank 0). The preset table and names stay in bank 0, so
// move_preset_name's strings are readable from any bank.
#include <gb/gb.h>
#include <string.h>
#include "player.h"
#include "spr_player.h"
#include "sprite_slot.h"

extern SpriteSlot player_slot;

// Must match player.c.
#define PLAYER_OAM 0
#define PLAYER_VRAM 0
#define SHADOW_OAM 2
#define SHADOW_VRAM 8
#define ANIM_DOWN 0

extern uint8_t player_anim_dir;
extern uint16_t player_anim_dist;
extern uint8_t player_jump_buffer;
extern uint8_t player_roll_len;
extern uint8_t player_roll_t;
extern uint16_t safe_x;
extern uint16_t safe_y;
extern int16_t safe_z;
extern uint16_t move_top_diag;

// Shadow: a 14x3 ellipse in color 2 at the bottom of an 8x16 column. This is the
// left half; the right half is the same column drawn flipped.
static const uint8_t shadow_tiles[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x3F, 0x00, 0x7F, 0x00, 0x3F,
};

// Rate that covers `top` in at most `frames` frames (rounded up so it never takes longer).
static uint16_t rate_for(uint8_t frames) {
    if (!frames) frames = 1;
    return (move_top + frames - 1) / frames;
}

void move_recalc(void) BANKED {
    move_accel = rate_for(move_accel_frames);
    move_decel = rate_for(move_decel_frames);
    move_reverse = rate_for(move_reverse_frames);
    move_top_diag = diag_scale(move_top);  // once per change, not per frame
}

void move_apply_preset(uint8_t preset) BANKED {
    const MoveProfile *p = &move_profiles[preset];
    move_preset = preset;
    move_top = p->top;
    move_accel_frames = p->accel;
    move_decel_frames = p->decel;
    move_reverse_frames = p->reverse;
    move_recalc();
}


void player_init(uint16_t px, uint16_t py) BANKED {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(BANK(spr_player));
    memcpy(player_frames, spr_player_frames, SPR_PLAYER_FRAME_COUNT * 4);
    memcpy(player_anims, spr_player_anims, SPR_PLAYER_ANIM_COUNT * 2);
    SWITCH_ROM(saved);
    slot_init(&player_slot, PLAYER_OAM, PLAYER_VRAM);
    player_x = px;
    player_y = py;
    player_xs = player_ys = 0;
    player_vx = player_vy = 0;
    player_facing = DIR_DOWN;
    player_anim_dir = ANIM_DOWN;
    player_anim_frame = 0;
    player_anim_dist = 0;
    player_z = player_vz = 0;
    player_air = player_dashed = player_dash_timer = player_invuln = player_jump_buffer = 0;
    player_roll_len = player_roll_t = 0;
    player_fall_timer = player_blink = 0;
    player_falls = 0;
    player_hp = PLAYER_HP_MAX;
    player_hits = 0;
    safe_x = px;
    safe_y = py;
    safe_z = 0;
    player_jumps = player_dashes = 0;
    set_sprite_data(SHADOW_VRAM, 2, shadow_tiles);
    set_sprite_tile(SHADOW_OAM, SHADOW_VRAM);
    set_sprite_tile(SHADOW_OAM + 1, SHADOW_VRAM);
    set_sprite_prop(SHADOW_OAM, 0);
    set_sprite_prop(SHADOW_OAM + 1, S_FLIPX);
    hide_sprite(SHADOW_OAM);
    hide_sprite(SHADOW_OAM + 1);
}

