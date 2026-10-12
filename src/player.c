#include <gb/gb.h>
#include "player.h"
#include "oam.h"
#include "aim.h"
#include "input.h"
#include "room.h"
#include "sprite_slot.h"
#include "spr_player.h"
#include "camera.h"

// Hitbox inside the 16x16 sprite: the feet, narrower than the art so the
// player fits through 1-block gaps comfortably.
#define HB_X PLAYER_HB_X
#define HB_W PLAYER_HB_W
#define HB_Y PLAYER_HB_Y
#define HB_H PLAYER_HB_H
// Knockback speed when hurt (px/frame 8.8); the normal easing slows it down.
#define KNOCKBACK 640
// Brushing a corner by up to this many pixels slides around it instead of stopping.
#define NUDGE 4


// The player uses hardware sprites 0-1 and VRAM sprite tiles 0-7; the jump
// shadow uses sprites 2-3 (drawn under the player) and VRAM tiles 8-9.
#define PLAYER_OAM OAM_PLAYER
#define PLAYER_VRAM 0
#define SHADOW_OAM OAM_SHADOW
#define SHADOW_VRAM 8

// Unit vectors for the 8 facings (clockwise from up), used for a dash with no d-pad input.
static const int8_t facing_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t facing_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
// Height of raised blocks, 8.8.
#define RAISED_Z (RAISED_PX << 8)
// Falling into a pit: the spin plays over FALL_FRAMES, then the player reappears
// at the last safe spot and blinks for BLINK_FRAMES.
#define FALL_FRAMES 32
#define BLINK_FRAMES 60
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
// The dash plays a roll: the crouched pose tumbling through 4 rotations (generated
// at build time by tools/assets.mjs), one full turn from the dash until landing.
static const uint8_t roll_anim[4] = {
    SPR_PLAYER_ANIM_ROLL_DOWN, SPR_PLAYER_ANIM_ROLL_UP, SPR_PLAYER_ANIM_ROLL_RIGHT, SPR_PLAYER_ANIM_ROLL_LEFT,
};
#define ROLL_FRAMES 4

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
uint8_t player_xs;
uint8_t player_ys;
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
uint8_t player_fall_timer; // frames left in a pit fall (0 = not falling)
uint8_t player_blink;      // frames left blinking after reappearing
uint16_t player_falls;     // pits fallen into (each costs 1 HP)
uint8_t player_hp;
uint16_t player_hits;      // times damaged
uint16_t safe_x;           // last spot standing on solid ground, for reappearing
uint16_t safe_y;
int16_t safe_z;
uint8_t player_roll_len;   // frames the current roll lasts (0 = not rolling)
uint8_t player_roll_t;     // frames into the roll
uint16_t player_jumps;     // counters, handy for tests and the Watch panel
uint16_t player_dashes;
// Tunables. Defaults: ~13 px high, ~28 frames in the air (about 2 blocks at 1 px/frame).
uint16_t jump_vel = 448;   // takeoff speed, px/frame 8.8 (1.75)
uint8_t jump_grav = 32;    // gravity, px/frame^2 in 1/256ths
uint16_t dash_speed = 768; // px/frame 8.8
uint8_t dash_frames = 8;
uint8_t dash_iframes = 12;

SpriteSlot player_slot;  // also used by player_setup.c
// The player's frame and animation tables, copied to RAM by player_init: the
// sprite is in a switchable bank, and these are read every frame. The tiles stay
// in ROM; the vblank handler switches their bank in to copy a frame.
uint8_t player_frames[SPR_PLAYER_FRAME_COUNT * 4];
uint8_t player_anims[SPR_PLAYER_ANIM_COUNT * 2];

uint16_t move_top;
uint8_t move_accel_frames;
uint8_t move_decel_frames;
uint8_t move_reverse_frames;
uint8_t move_preset;
uint16_t move_accel;
uint16_t move_decel;
uint16_t move_reverse;
uint16_t move_top_diag;  // top speed per axis on a diagonal (top / sqrt 2), derived

const char *move_preset_name(void) {
    return move_preset < MOVE_PRESET_COUNT ? move_profiles[move_preset].name : "CUSTOM";
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

// Does the pixel (x, y) block the player? Walls always do; raised blocks only
// while the player is lower than their top.
static uint8_t blocked(uint16_t x, uint16_t y) {
    uint8_t t = room_block_at(x, y);
    return t == BLK_SOLID || (t == BLK_RAISED && player_z < RAISED_Z);
}

// Does the hitbox fit with the sprite's top-left at pixel (x, y)?
static uint8_t box_free(uint16_t x, uint16_t y) {
    uint16_t l = x + HB_X, r = x + HB_X + HB_W - 1;
    uint16_t t = y + HB_Y, b = y + HB_Y + HB_H - 1;
    return !blocked(l, t) && !blocked(r, t) && !blocked(l, b) && !blocked(r, b);
}

// Ground height under the hitbox: the top of a raised block if any corner is on
// one (so you can stand on an edge), otherwise the floor.
static int16_t ground_under(void) {
    uint16_t l = player_x + HB_X, r = player_x + HB_X + HB_W - 1;
    uint16_t t = player_y + HB_Y, b = player_y + HB_Y + HB_H - 1;
    if (room_block_at(l, t) == BLK_RAISED || room_block_at(r, t) == BLK_RAISED ||
        room_block_at(l, b) == BLK_RAISED || room_block_at(r, b) == BLK_RAISED) return RAISED_Z;
    return 0;
}

// Pits are judged by the middle of the feet, so clipping an edge is forgiven.
static uint8_t over_pit(void) {
    return room_block_at(player_x + 8, player_y + 12) == BLK_PIT;
}

// Position + velocity: add the fraction bytes, carry into the 16-bit pixel part,
// then add the velocity's whole pixels (its high byte, sign included). This is
// the 24-bit add done in 8/16-bit pieces, avoiding 32-bit math.
static uint16_t moved(uint16_t pos, uint8_t frac, int16_t v, uint8_t *new_frac) {
    uint16_t f = (uint16_t)frac + (uint8_t)v;
    *new_frac = (uint8_t)f;
    return pos + (int8_t)((uint16_t)v >> 8) + (f >> 8);
}

static void move_x(void) {
    if (!player_vx) return;
    uint8_t nf;
    uint16_t px = moved(player_x, player_xs, player_vx, &nf), py = player_y;
    uint16_t edge = player_vx > 0 ? px + HB_X + HB_W - 1 : px + HB_X;
    uint16_t top = py + HB_Y, bot = py + HB_Y + HB_H - 1;
    uint8_t hit_top = blocked(edge, top), hit_bot = blocked(edge, bot);
    if (!hit_top && !hit_bot) {
        player_x = px;
        player_xs = nf;
        return;
    }
    // Blocked. Snap flush against the wall.
    if (player_vx > 0) player_x = (edge & 0xFFF0) - HB_W - HB_X;
    else player_x = ((edge | 0x0F) + 1) - HB_X;
    player_xs = 0;
    // Clipping a corner by a few pixels: slide around it, keep the speed.
    if (hit_top && !hit_bot && 16 - (top & 0x0F) <= NUDGE && box_free(player_x, py + 1)) {
        player_y++;
        return;
    }
    if (hit_bot && !hit_top && (bot & 0x0F) + 1 <= NUDGE && box_free(player_x, py - 1)) {
        player_y--;
        return;
    }
    player_vx = 0;
}

static void move_y(void) {
    if (!player_vy) return;
    uint8_t nf;
    uint16_t py = moved(player_y, player_ys, player_vy, &nf), px = player_x;
    uint16_t edge = player_vy > 0 ? py + HB_Y + HB_H - 1 : py + HB_Y;
    uint16_t left = px + HB_X, right = px + HB_X + HB_W - 1;
    uint8_t hit_l = blocked(left, edge), hit_r = blocked(right, edge);
    if (!hit_l && !hit_r) {
        player_y = py;
        player_ys = nf;
        return;
    }
    if (player_vy > 0) player_y = (edge & 0xFFF0) - HB_H - HB_Y;
    else player_y = ((edge | 0x0F) + 1) - HB_Y;
    player_ys = 0;
    if (hit_l && !hit_r && 16 - (left & 0x0F) <= NUDGE && box_free(px + 1, player_y)) {
        player_x++;
        return;
    }
    if (hit_r && !hit_l && (right & 0x0F) + 1 <= NUDGE && box_free(px - 1, player_y)) {
        player_x--;
        return;
    }
    player_vy = 0;
}

static void update_anim(int8_t dx, int8_t dy) {
    if (player_roll_len) return;  // the roll keeps its direction; drawing picks the frame
    // Which way the art faces. On a diagonal, keep the current direction if it's part
    // of the diagonal (so the sprite doesn't flip back and forth); otherwise face sideways.
    uint8_t h = dx > 0 ? ANIM_RIGHT : dx < 0 ? ANIM_LEFT : 0xFF;
    uint8_t v = dy > 0 ? ANIM_DOWN : dy < 0 ? ANIM_UP : 0xFF;
    if (h != 0xFF && v != 0xFF) {
        if (player_anim_dir != h && player_anim_dir != v) player_anim_dir = h;
    } else if (h != 0xFF) player_anim_dir = h;
    else if (v != 0xFF) player_anim_dir = v;
    // Shooting: face the aim instead (aim.c), so walking away while firing backpedals.
    if (aim_facing != 0xFF) player_anim_dir = aim_facing;

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
        uint8_t count = player_anims[(walk_anim[player_anim_dir] << 1) + 1];
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
    int16_t sp = (dx && dy) ? diag_scale(dash_speed) : (int16_t)dash_speed;
    player_vx = dx > 0 ? sp : dx < 0 ? -sp : 0;
    player_vy = dy > 0 ? sp : dy < 0 ? -sp : 0;
    player_dashed = 1;
    player_dash_timer = dash_frames;
    // Roll the way the dash goes (sideways wins on a diagonal).
    player_anim_dir = dx > 0 ? ANIM_RIGHT : dx < 0 ? ANIM_LEFT : dy < 0 ? ANIM_UP : ANIM_DOWN;
    player_vz = 0;  // hold height during the dash, then fall from there
    if (player_invuln < dash_iframes) player_invuln = dash_iframes;
    // The roll spans the dash plus the fall that follows it, so the last quarter
    // turn ends on landing. The fall starts from the held height with no speed.
    int16_t z = player_z, vz = 0;
    uint8_t fall = 0;
    while (z > 0 && fall < 200) {
        z += vz;
        vz -= jump_grav;
        fall++;
    }
    player_roll_len = dash_frames + fall;
    player_roll_t = 0;
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
    if (player_roll_len) player_roll_t++;
    if (!player_air) {
        // Walked off a raised block: start falling (a dash is still available).
        // On the floor there's nothing to walk off, so skip the lookups.
        if (player_z && ground_under() < player_z) {
            player_air = 1;
            player_vz = 0;
            player_dashed = 0;
        }
        return;
    }
    if (player_dash_timer) {
        player_dash_timer--;
        return;
    }
    player_z += player_vz;
    player_vz -= jump_grav;
    int16_t ground = ground_under();
    if (player_vz < 0 && player_z <= ground) {
        player_z = ground;
        player_vz = 0;
        player_air = 0;
        player_dashed = 0;
        player_roll_len = 0;
        if (player_jump_buffer) start_jump();
    } else if (player_jump_buffer) {
        player_jump_buffer--;
    }
}

static void take_damage(uint8_t dmg) {
    player_hits++;
    player_hp = dmg >= player_hp ? 0 : player_hp - dmg;
    player_blink = BLINK_FRAMES;
}

uint8_t player_hittable(void) {
    return !player_blink && !player_invuln && !player_fall_timer && player_z < PLAYER_CLEAR_Z;
}

void player_hurt(uint8_t dmg, uint16_t from_x, uint16_t from_y) {
    if (player_blink || player_invuln || player_fall_timer) return;
    take_damage(dmg);
    // Knocked away from the source (8 directions); straight down if on top of it.
    int16_t dx = (int16_t)(player_x + 8 - from_x);
    int16_t dy = (int16_t)(player_y + 12 - from_y);
    player_vx = dx > 2 ? KNOCKBACK : dx < -2 ? -KNOCKBACK : 0;
    player_vy = dy > 2 ? KNOCKBACK : dy < -2 ? -KNOCKBACK : 0;
    if (!player_vx && !player_vy) player_vy = KNOCKBACK;
}

static void start_pit_fall(void) {
    player_fall_timer = FALL_FRAMES;
    player_vx = player_vy = 0;
    player_roll_len = 0;
    player_falls++;
}

// Falling into a pit: no control; at the end, reappear at the last safe spot.
static void update_pit_fall(void) {
    if (--player_fall_timer) return;
    player_x = safe_x;
    player_y = safe_y;
    player_xs = player_ys = 0;
    player_z = safe_z;
    take_damage(1);  // also starts the blink
}

// Is the player against a wall in direction dx (x axis) or dy (y axis)? Yes if
// the whole side touches, or one corner touches by more than NUDGE pixels. A
// corner clipped by NUDGE or less isn't a wall: move_x/move_y slide around it.
static uint8_t wall_x(int8_t dx) {
    uint16_t edge = dx > 0 ? player_x + HB_X + HB_W : player_x + HB_X - 1;
    uint16_t top = player_y + HB_Y, bot = player_y + HB_Y + HB_H - 1;
    uint8_t ht = blocked(edge, top), hb = blocked(edge, bot);
    if (ht && hb) return 1;
    if (ht) return 16 - (top & 0x0F) > NUDGE;
    if (hb) return (bot & 0x0F) + 1 > NUDGE;
    return 0;
}

static uint8_t wall_y(int8_t dy) {
    uint16_t edge = dy > 0 ? player_y + HB_Y + HB_H : player_y + HB_Y - 1;
    uint16_t left = player_x + HB_X, right = player_x + HB_X + HB_W - 1;
    uint8_t hl = blocked(left, edge), hr = blocked(right, edge);
    if (hl && hr) return 1;
    if (hl) return 16 - (left & 0x0F) > NUDGE;
    if (hr) return (right & 0x0F) + 1 > NUDGE;
    return 0;
}

void player_update(void) {
    if (player_blink) player_blink--;
    if (player_fall_timer) {
        update_pit_fall();
        return;
    }
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
        // Pushing into a wall: no speed builds up against it (it would creep in
        // sub-pixel steps and make the camera's lead bob), and with a diagonal
        // input the player slides along it at full speed rather than the diagonal
        // component (0.71 px/frame, which steps unevenly).
        int8_t mx = dx, my = dy;
        if (mx && wall_x(mx)) {
            mx = 0;
            player_vx = 0;
            player_xs = 0;
        }
        if (my && wall_y(my)) {
            my = 0;
            player_vy = 0;
            player_ys = 0;
        }
        int16_t speed = (mx && my) ? (int16_t)move_top_diag : (int16_t)move_top;
        player_vx = step_axis(player_vx, mx > 0 ? speed : mx < 0 ? -speed : 0);
        player_vy = step_axis(player_vy, my > 0 ? speed : my < 0 ? -speed : 0);
    }

    move_x();
    move_y();
    if (!player_air) {
        uint8_t pit = player_z == 0 && over_pit();
        if (pit) {
            start_pit_fall();
            return;
        }
        // Remember where solid ground was, to reappear there after a fall.
        // (Standing on a raised block over a pit isn't possible: pits are floor-level.)
        {
            safe_x = player_x;
            safe_y = player_y;
            safe_z = player_z;
        }
    }
    update_anim(dx, dy);
}

void player_draw(void) {
    uint8_t anim, frame;
    if (player_fall_timer) {
        anim = SPR_PLAYER_ANIM_FALL;
        frame = (uint8_t)((FALL_FRAMES - player_fall_timer) * 4 / FALL_FRAMES);
    } else if (player_roll_len) {
        // One full turn over the roll: 0, 90, 180, 270 degrees.
        anim = roll_anim[player_anim_dir];
        frame = (uint8_t)((uint16_t)(player_roll_t - 1) * ROLL_FRAMES / player_roll_len);
    } else {
        anim = walk_anim[player_anim_dir];
        frame = player_air ? AIR_FRAME : player_anim_frame;
    }
    slot_set_frame(&player_slot, spr_player_tiles, BANK(spr_player), player_frames, player_anims[anim << 1] + frame);
    // Screen position = room position - camera (the camera keeps the player in view).
    // The body is raised by its height; near the top edge the subtraction wraps,
    // which the hardware's 8-bit coordinates handle.
    uint8_t x = (uint8_t)(player_x - cam_x), y = (uint8_t)(player_y - cam_y);
    if (player_blink & 4) slot_hide(&player_slot);
    else slot_move(&player_slot, x, y - (uint8_t)(player_z >> 8));
    if (player_air) {
        // The shadow sits on whatever is below: the floor or a raised block's top.
        y -= (uint8_t)(ground_under() >> 8);
        move_sprite(SHADOW_OAM, x + 8, y + 16);
        move_sprite(SHADOW_OAM + 1, x + 16, y + 16);
    } else {
        hide_sprite(SHADOW_OAM);
        hide_sprite(SHADOW_OAM + 1);
    }
}
