#include <gb/gb.h>
#include "player.h"
#include "input.h"
#include "room.h"
#include "sprite_slot.h"
#include "spr_player.h"

// Hitbox inside the 16x16 sprite: the feet, narrower than the art so the
// player fits through 1-block gaps comfortably.
#define HB_X 3
#define HB_W 10
#define HB_Y 8
#define HB_H 8
// Brushing a corner by up to this many pixels slides around it instead of stopping.
#define NUDGE 4

// Diagonal speed factor, 1/sqrt(2) in 0.8 fixed point.
#define DIAG 181

// The player uses hardware sprites 0-1 and VRAM sprite tiles 0-7; the jump
// shadow uses sprites 2-3 (drawn under the player) and VRAM tiles 8-9.
#define PLAYER_OAM 0
#define PLAYER_VRAM 0
#define SHADOW_OAM 2
#define SHADOW_VRAM 8

// Shadow: a 14x3 ellipse in color 2 at the bottom of an 8x16 column. This is the
// left half; the right half is the same column drawn flipped.
static const uint8_t shadow_tiles[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x3F, 0x00, 0x7F, 0x00, 0x3F,
};

// Unit vectors for the 8 facings (clockwise from up), used for a dash with no d-pad input.
static const int8_t facing_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t facing_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
// While airborne the art holds this walk frame (legs apart).
#define AIR_FRAME 1
// Frames a B press is remembered for, so pressing just before landing still jumps.
#define JUMP_BUFFER 4

// Animation directions (4) and the walk/hurt animation for each.
#define ANIM_DOWN 0
#define ANIM_UP 1
#define ANIM_RIGHT 2
#define ANIM_LEFT 3
static const uint8_t walk_anim[4] = {
    SPR_PLAYER_ANIM_WALK_DOWN, SPR_PLAYER_ANIM_WALK_UP, SPR_PLAYER_ANIM_WALK_RIGHT, SPR_PLAYER_ANIM_WALK_LEFT,
};

// Movement profiles. Each character (and later each heir class, M4) points at one,
// so different characters can move differently. The lab cycles through them with SELECT.
const MoveProfile move_profiles[MOVE_PRESET_COUNT] = {
    {256, 5, 4, 3, "SNAPPY"},    // default: 1 px/frame, quick to speed and to stop (Gungeon-like response)
    {384, 10, 8, 6, "SMOOTH"},
    {384, 20, 24, 14, "FLOATY"},
    {384, 1, 1, 1, "CLASSIC"},   // instant start/stop, like most GB games
};

// Facing for each d-pad combination (J_RIGHT=1, J_LEFT=2, J_UP=4, J_DOWN=8).
// 0xFF = no change (nothing held, or opposite directions).
static const uint8_t facing_for_dpad[16] = {
    0xFF, DIR_RIGHT, DIR_LEFT, 0xFF,
    DIR_UP, DIR_UP_RIGHT, DIR_UP_LEFT, DIR_UP,
    DIR_DOWN, DIR_DOWN_RIGHT, DIR_DOWN_LEFT, DIR_DOWN,
    0xFF, DIR_RIGHT, DIR_LEFT, 0xFF,
};

uint16_t player_x;
uint16_t player_y;
int16_t player_vx;
int16_t player_vy;
uint8_t player_facing;
uint8_t player_anim_dir;    // ANIM_DOWN/UP/RIGHT/LEFT: which way the art faces
uint8_t player_anim_frame;  // frame within the current animation
uint16_t player_anim_dist;  // distance walked since the last frame step, 8.8
uint16_t anim_step = 8 << 8; // tunable: px walked per animation frame

// --- jump and dash (issues #6, #7) ---
int16_t player_z;          // height above the ground, px 8.8 (0 = on the ground)
int16_t player_vz;         // vertical speed, px/frame 8.8 (positive = up)
uint8_t player_air;        // 1 while airborne
uint8_t player_dashed;     // 1 once this jump's dash is used
uint8_t player_dash_timer; // frames of dash left
uint8_t player_invuln;     // invulnerability frames left
uint8_t player_jump_buffer;
uint16_t player_jumps;     // counters, handy for tests and the Watch panel
uint16_t player_dashes;
// Tunables. Defaults: ~16 px high, ~32 frames in the air (2 blocks at 1 px/frame).
uint16_t jump_vel = 512;   // takeoff speed, px/frame 8.8
uint8_t jump_grav = 32;    // gravity, px/frame^2 in 1/256ths
uint16_t dash_speed = 768; // px/frame 8.8
uint8_t dash_frames = 8;
uint8_t dash_iframes = 12;

static SpriteSlot player_slot;

uint16_t move_top;
uint8_t move_accel_frames;
uint8_t move_decel_frames;
uint8_t move_reverse_frames;
uint8_t move_preset;
uint16_t move_accel;
uint16_t move_decel;
uint16_t move_reverse;

// Rate that covers `top` in at most `frames` frames (rounded up so it never takes longer).
static uint16_t rate_for(uint8_t frames) {
    if (!frames) frames = 1;
    return (move_top + frames - 1) / frames;
}

void move_recalc(void) {
    move_accel = rate_for(move_accel_frames);
    move_decel = rate_for(move_decel_frames);
    move_reverse = rate_for(move_reverse_frames);
}

void move_apply_preset(uint8_t preset) {
    const MoveProfile *p = &move_profiles[preset];
    move_preset = preset;
    move_top = p->top;
    move_accel_frames = p->accel;
    move_decel_frames = p->decel;
    move_reverse_frames = p->reverse;
    move_recalc();
}

const char *move_preset_name(void) {
    return move_preset < MOVE_PRESET_COUNT ? move_profiles[move_preset].name : "CUSTOM";
}

void player_init(uint8_t px, uint8_t py) {
    slot_init(&player_slot, PLAYER_OAM, PLAYER_VRAM);
    player_x = (uint16_t)px << 8;
    player_y = (uint16_t)py << 8;
    player_vx = player_vy = 0;
    player_facing = DIR_DOWN;
    player_anim_dir = ANIM_DOWN;
    player_anim_frame = 0;
    player_anim_dist = 0;
    player_z = player_vz = 0;
    player_air = player_dashed = player_dash_timer = player_invuln = player_jump_buffer = 0;
    player_jumps = player_dashes = 0;
    set_sprite_data(SHADOW_VRAM, 2, shadow_tiles);
    set_sprite_tile(SHADOW_OAM, SHADOW_VRAM);
    set_sprite_tile(SHADOW_OAM + 1, SHADOW_VRAM);
    set_sprite_prop(SHADOW_OAM, 0);
    set_sprite_prop(SHADOW_OAM + 1, S_FLIPX);
    hide_sprite(SHADOW_OAM);
    hide_sprite(SHADOW_OAM + 1);
}

// Move v one step toward target t, using the rate that fits the situation.
static int16_t step_axis(int16_t v, int16_t t) {
    uint16_t rate;
    if (t == 0) rate = move_decel;
    else if ((t > 0 && v < 0) || (t < 0 && v > 0)) rate = move_reverse;
    else if ((t > 0 && v < t) || (t < 0 && v > t)) rate = move_accel;
    else rate = move_decel;  // faster than the target (e.g. just turned diagonal): ease down
    if (v < t) {
        v += rate;
        if (v > t) v = t;
    } else if (v > t) {
        v -= rate;
        if (v < t) v = t;
    }
    return v;
}

// Does the hitbox fit with the sprite's top-left at pixel (x, y)?
static uint8_t box_free(uint8_t x, uint8_t y) {
    uint8_t l = x + HB_X, r = x + HB_X + HB_W - 1;
    uint8_t t = y + HB_Y, b = y + HB_Y + HB_H - 1;
    return !room_solid_at(l, t) && !room_solid_at(r, t) && !room_solid_at(l, b) && !room_solid_at(r, b);
}

static void move_x(void) {
    if (!player_vx) return;
    uint16_t nx = player_x + player_vx;
    uint8_t px = FX_PX(nx), py = FX_PX(player_y);
    uint8_t edge = player_vx > 0 ? px + HB_X + HB_W - 1 : px + HB_X;
    uint8_t top = py + HB_Y, bot = py + HB_Y + HB_H - 1;
    uint8_t hit_top = room_solid_at(edge, top), hit_bot = room_solid_at(edge, bot);
    if (!hit_top && !hit_bot) {
        player_x = nx;
        return;
    }
    // Blocked. Snap flush against the wall.
    if (player_vx > 0) player_x = (uint16_t)((edge & 0xF0) - HB_W - HB_X) << 8;
    else player_x = (uint16_t)(((edge | 0x0F) + 1) - HB_X) << 8;
    uint8_t cx = FX_PX(player_x);
    // Clipping a corner by a few pixels: slide around it, keep the speed.
    if (hit_top && !hit_bot && 16 - (top & 0x0F) <= NUDGE && box_free(cx, py + 1)) {
        player_y += FX_ONE;
        return;
    }
    if (hit_bot && !hit_top && (bot & 0x0F) + 1 <= NUDGE && box_free(cx, py - 1)) {
        player_y -= FX_ONE;
        return;
    }
    player_vx = 0;
}

static void move_y(void) {
    if (!player_vy) return;
    uint16_t ny = player_y + player_vy;
    uint8_t px = FX_PX(player_x), py = FX_PX(ny);
    uint8_t edge = player_vy > 0 ? py + HB_Y + HB_H - 1 : py + HB_Y;
    uint8_t left = px + HB_X, right = px + HB_X + HB_W - 1;
    uint8_t hit_l = room_solid_at(left, edge), hit_r = room_solid_at(right, edge);
    if (!hit_l && !hit_r) {
        player_y = ny;
        return;
    }
    if (player_vy > 0) player_y = (uint16_t)((edge & 0xF0) - HB_H - HB_Y) << 8;
    else player_y = (uint16_t)(((edge | 0x0F) + 1) - HB_Y) << 8;
    uint8_t cy = FX_PX(player_y);
    if (hit_l && !hit_r && 16 - (left & 0x0F) <= NUDGE && box_free(px + 1, cy)) {
        player_x += FX_ONE;
        return;
    }
    if (hit_r && !hit_l && (right & 0x0F) + 1 <= NUDGE && box_free(px - 1, cy)) {
        player_x -= FX_ONE;
        return;
    }
    player_vy = 0;
}

static void update_anim(int8_t dx, int8_t dy) {
    // Which way the art faces. On a diagonal, keep the current direction if it's part
    // of the diagonal (so the sprite doesn't flip back and forth); otherwise face sideways.
    uint8_t h = dx > 0 ? ANIM_RIGHT : dx < 0 ? ANIM_LEFT : 0xFF;
    uint8_t v = dy > 0 ? ANIM_DOWN : dy < 0 ? ANIM_UP : 0xFF;
    if (h != 0xFF && v != 0xFF) {
        if (player_anim_dir != h && player_anim_dir != v) player_anim_dir = h;
    } else if (h != 0xFF) player_anim_dir = h;
    else if (v != 0xFF) player_anim_dir = v;

    if (!dx && !dy && !player_vx && !player_vy) {
        // Standing still: the first walk frame is the standing pose.
        player_anim_frame = 0;
        player_anim_dist = 0;
        return;
    }
    // Step the walk cycle by distance covered, so faster movement animates faster.
    uint16_t ax = player_vx < 0 ? -player_vx : player_vx;
    uint16_t ay = player_vy < 0 ? -player_vy : player_vy;
    uint16_t step = ax > ay ? ax : ay;
    if (!step) step = move_top;  // pushing against a wall: walk in place
    player_anim_dist += step;
    if (player_anim_dist >= anim_step) {
        player_anim_dist -= anim_step;
        uint8_t count = spr_player_anims[(walk_anim[player_anim_dir] << 1) + 1];
        if (++player_anim_frame >= count) player_anim_frame = 0;
    }
}

static void start_jump(void) {
    player_air = 1;
    player_dashed = 0;
    player_vz = jump_vel;
    player_jump_buffer = 0;
    player_jumps++;
}

static void start_dash(int8_t dx, int8_t dy) {
    if (!dx && !dy) {
        dx = facing_dx[player_facing];
        dy = facing_dy[player_facing];
    }
    int16_t sp = (dx && dy) ? (int16_t)(((uint32_t)dash_speed * DIAG) >> 8) : (int16_t)dash_speed;
    player_vx = dx > 0 ? sp : dx < 0 ? -sp : 0;
    player_vy = dy > 0 ? sp : dy < 0 ? -sp : 0;
    player_dashed = 1;
    player_dash_timer = dash_frames;
    player_vz = 0;  // hold height during the dash, then fall from there
    if (player_invuln < dash_iframes) player_invuln = dash_iframes;
    player_dashes++;
}

// B: jump from the ground; in the air, the first press dashes and later presses
// are buffered so a press just before landing jumps again on touchdown.
static void update_jump(int8_t dx, int8_t dy) {
    if (joy_pressed & J_B) {
        if (!player_air) start_jump();
        else if (!player_dashed) start_dash(dx, dy);
        else player_jump_buffer = JUMP_BUFFER;
    }
    if (player_invuln) player_invuln--;
    if (!player_air) return;
    if (player_dash_timer) {
        player_dash_timer--;
        return;
    }
    player_z += player_vz;
    player_vz -= jump_grav;
    if (player_z <= 0) {
        player_z = 0;
        player_vz = 0;
        player_air = 0;
        player_dashed = 0;
        if (player_jump_buffer) start_jump();
    } else if (player_jump_buffer) {
        player_jump_buffer--;
    }
}

void player_update(void) {
    uint8_t dpad = joy & 0x0F;
    int8_t dx = 0, dy = 0;
    if ((dpad & (J_LEFT | J_RIGHT)) == J_RIGHT) dx = 1;
    if ((dpad & (J_LEFT | J_RIGHT)) == J_LEFT) dx = -1;
    if ((dpad & (J_UP | J_DOWN)) == J_DOWN) dy = 1;
    if ((dpad & (J_UP | J_DOWN)) == J_UP) dy = -1;
    if (facing_for_dpad[dpad] != 0xFF) player_facing = facing_for_dpad[dpad];

    update_jump(dx, dy);
    // A dash ignores steering; afterwards the normal easing brings the speed back down.
    if (!player_dash_timer) {
        int16_t speed = (dx && dy) ? (int16_t)(((uint32_t)move_top * DIAG) >> 8) : (int16_t)move_top;
        player_vx = step_axis(player_vx, dx > 0 ? speed : dx < 0 ? -speed : 0);
        player_vy = step_axis(player_vy, dy > 0 ? speed : dy < 0 ? -speed : 0);
    }

    move_x();
    move_y();
    update_anim(dx, dy);
}

void player_draw(void) {
    uint8_t anim = walk_anim[player_anim_dir];
    uint8_t frame = player_air ? AIR_FRAME : player_anim_frame;
    slot_set_frame(&player_slot, spr_player_tiles, spr_player_frames, spr_player_anims[anim << 1] + frame);
    // Playfield (0,0) is screen (0,0). The body is raised by its height; near the top
    // edge the subtraction wraps, which the hardware's 8-bit coordinates handle.
    uint8_t x = FX_PX(player_x), y = FX_PX(player_y);
    slot_move(&player_slot, x, y - FX_PX(player_z));
    if (player_air) {
        move_sprite(SHADOW_OAM, x + 8, y + 16);
        move_sprite(SHADOW_OAM + 1, x + 16, y + 16);
    } else {
        hide_sprite(SHADOW_OAM);
        hide_sprite(SHADOW_OAM + 1);
    }
}
