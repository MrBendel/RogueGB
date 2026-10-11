#include <gb/gb.h>
#include "player.h"
#include "input.h"
#include "room.h"

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

// Hardware sprites (8x16 mode): the placeholder body takes two, plus a facing marker.
#define SPR_BODY_L 0
#define SPR_BODY_R 1
#define SPR_POINTER 2
#define TILE_BODY_L 0  // tiles 0-1
#define TILE_BODY_R 2  // tiles 2-3
#define TILE_POINTER 4 // tiles 4-5
// Marker offset from the body's center, per facing.
static const int8_t pointer_dx[8] = {0, 8, 11, 8, 0, -8, -11, -8};
static const int8_t pointer_dy[8] = {-11, -8, 0, 8, 11, 8, 0, -8};

// Movement profiles. Each character (and later each heir class, M4) points at one,
// so different characters can move differently. The lab cycles through them with SELECT.
const MoveProfile move_profiles[MOVE_PRESET_COUNT] = {
    {512, 5, 4, 3, "SNAPPY"},    // default: Gungeon-like, 2 px/frame, quick to speed and to stop
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

// Build the placeholder art: a rounded 16x16 box (light fill, dark outline)
// and a 4x4 dot for the facing marker.
static void load_placeholder_tiles(void) {
    static const uint16_t shape[16] = {
        0x3FFC, 0x7FFE, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFE, 0x3FFC,
    };
    static const uint16_t outline[16] = {
        0x3FFC, 0x4002, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001,
        0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x8001, 0x4002, 0x3FFC,
    };
    uint8_t data[6 * 16];
    for (uint8_t r = 0; r < 16; r++) {
        // Left half: tiles 0-1 (rows 0-15 run through both), right half: tiles 2-3.
        data[r * 2] = shape[r] >> 8;
        data[r * 2 + 1] = outline[r] >> 8;
        data[32 + r * 2] = shape[r] & 0xFF;
        data[32 + r * 2 + 1] = outline[r] & 0xFF;
        data[64 + r * 2] = data[64 + r * 2 + 1] = r < 4 ? 0xF0 : 0x00;
    }
    set_sprite_data(0, 6, data);
}

void player_init(uint8_t px, uint8_t py) {
    load_placeholder_tiles();
    set_sprite_tile(SPR_BODY_L, TILE_BODY_L);
    set_sprite_tile(SPR_BODY_R, TILE_BODY_R);
    set_sprite_tile(SPR_POINTER, TILE_POINTER);
    player_x = (uint16_t)px << 8;
    player_y = (uint16_t)py << 8;
    player_vx = player_vy = 0;
    player_facing = DIR_UP;
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

void player_update(void) {
    uint8_t dpad = joy & 0x0F;
    int8_t dx = 0, dy = 0;
    if ((dpad & (J_LEFT | J_RIGHT)) == J_RIGHT) dx = 1;
    if ((dpad & (J_LEFT | J_RIGHT)) == J_LEFT) dx = -1;
    if ((dpad & (J_UP | J_DOWN)) == J_DOWN) dy = 1;
    if ((dpad & (J_UP | J_DOWN)) == J_UP) dy = -1;
    if (facing_for_dpad[dpad] != 0xFF) player_facing = facing_for_dpad[dpad];

    int16_t speed = (dx && dy) ? (int16_t)(((uint32_t)move_top * DIAG) >> 8) : (int16_t)move_top;
    player_vx = step_axis(player_vx, dx > 0 ? speed : dx < 0 ? -speed : 0);
    player_vy = step_axis(player_vy, dy > 0 ? speed : dy < 0 ? -speed : 0);

    move_x();
    move_y();
}

void player_draw(void) {
    // Playfield (0,0) is screen (0,0); hardware sprites are offset by (8, 16).
    uint8_t sx = FX_PX(player_x) + 8, sy = FX_PX(player_y) + 16;
    move_sprite(SPR_BODY_L, sx, sy);
    move_sprite(SPR_BODY_R, sx + 8, sy);
    // Marker: a 4x4 dot centered on (body center + offset); the dot is the tile's top-left.
    move_sprite(SPR_POINTER, sx + 6 + pointer_dx[player_facing], sy + 6 + pointer_dy[player_facing]);
}
