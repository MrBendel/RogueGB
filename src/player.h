// Player movement: 8.8 fixed point, eased toward the d-pad's target velocity,
// with collision against the room. See issue #1 (movement) and #2 (collision).
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// Velocities are 8.8 fixed point (1/256 px). Positions are 16-bit pixels plus an
// 8-bit fraction kept in a separate variable, so rooms can be larger than 256 px.
// The one rounding rule: what's drawn is the pixel part (floor), for every sprite
// and for the camera, so nothing jitters against the background.

// Facing, clockwise from up.
#define DIR_UP 0
#define DIR_UP_RIGHT 1
#define DIR_RIGHT 2
#define DIR_DOWN_RIGHT 3
#define DIR_DOWN 4
#define DIR_DOWN_LEFT 5
#define DIR_LEFT 6
#define DIR_UP_LEFT 7

// --- watchable state ---
extern uint16_t player_x;  // top-left of the 16x16 sprite, room pixels
extern uint16_t player_y;
extern uint8_t player_xs;  // fraction of a pixel, 1/256ths
extern uint8_t player_ys;
extern int16_t player_vx;  // px/frame, 8.8
extern int16_t player_vy;
extern uint8_t player_facing;
extern uint8_t player_anim_dir;    // art direction: 0 down, 1 up, 2 right, 3 left
extern uint8_t player_anim_frame;  // frame within the walk cycle (0 = standing)
extern uint16_t anim_step;         // tunable: px walked per animation frame, 8.8

// --- jump and dash ---
extern int16_t player_z;          // height above the ground, px 8.8
extern int16_t player_vz;
extern uint8_t player_air;
extern uint8_t player_dashed;
extern uint8_t player_dash_timer;
extern uint8_t player_invuln;
extern uint8_t player_fall_timer;
extern uint8_t player_blink;
extern uint16_t player_falls;
extern uint8_t player_roll_len;   // frames the current roll lasts (0 = not rolling)
extern uint8_t player_roll_t;
extern uint16_t player_jumps;
extern uint16_t player_dashes;
extern uint16_t jump_vel;          // tunables
extern uint8_t jump_grav;
extern uint16_t dash_speed;
extern uint8_t dash_frames;
extern uint8_t dash_iframes;

// --- tunables (changed by presets and the pause-screen tuner) ---
extern uint16_t move_top;            // top speed, px/frame 8.8
extern uint8_t move_accel_frames;    // frames from standstill to top speed
extern uint8_t move_decel_frames;    // frames from top speed to standstill
extern uint8_t move_reverse_frames;  // frames to cancel full speed when pushing the other way
extern uint8_t move_preset;          // index into the presets; MOVE_PRESET_CUSTOM once tuned by hand
extern uint16_t move_accel;          // derived per-frame rates, 8.8
extern uint16_t move_decel;
extern uint16_t move_reverse;

typedef struct {
    uint16_t top;                  // px/frame, 8.8
    uint8_t accel, decel, reverse; // frames (see the tunables above)
    const char *name;
} MoveProfile;

#define MOVE_PRESET_COUNT 4
extern const MoveProfile move_profiles[MOVE_PRESET_COUNT];
#define MOVE_PRESET_CUSTOM 0xFF

// Hitbox: the feet, inside the 16x16 frame (offsets from player_x/y).
#define PLAYER_HB_X 3
#define PLAYER_HB_W 10
#define PLAYER_HB_Y 8
#define PLAYER_HB_H 8
// While at least this high (px 8.8) off the floor, ground enemies and their
// bullets pass underneath.
#define PLAYER_CLEAR_Z (6 << 8)

// --- health (a minimal version; the damage model proper is issue #10) ---
#define PLAYER_HP_MAX 6
extern uint8_t player_hp;
extern uint16_t player_hits;
// Take damage from something at room pixel (from_x, from_y): knocked back away
// from it, then blinking and invulnerable for a moment. Ignored while already
// invulnerable (blinking, dashing) or falling into a pit.
void player_hurt(uint8_t dmg, uint16_t from_x, uint16_t from_y);
// Can ground-level things (enemies, bullets) hit the player right now?
uint8_t player_hittable(void);

#include <gb/gb.h>
void player_init(uint16_t px, uint16_t py) BANKED;  // player_setup.c (banked)
void player_update(void);
void player_draw(void);

void move_apply_preset(uint8_t preset) BANKED;
// Recompute the per-frame rates after changing a tunable.
void move_recalc(void) BANKED;
const char *move_preset_name(void);

// RAM copies of the sprite's frame and animation tables (player_init).
extern uint8_t player_frames[];
extern uint8_t player_anims[];

// 1/sqrt(2) in 0.8 fixed point, for diagonal speeds. diag_scale(v) = v * DIAG >> 8
// for a 16-bit v, in two 16-bit products (no 32-bit multiply, a large routine).
#define DIAG 181
#define diag_scale(v) ((int16_t)((((uint16_t)(v) >> 8) * DIAG) + ((((uint16_t)(v) & 0xFF) * DIAG) >> 8)))

#endif
