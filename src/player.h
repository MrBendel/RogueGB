// Player movement: 8.8 fixed point, eased toward the d-pad's target velocity,
// with collision against the room. See issue #1 (movement) and #2 (collision).
#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// Fixed point: 8 integer bits, 8 fraction bits (1/256 px).
#define FX_ONE 256
// The one rounding rule: fixed point -> pixel is floor. Everything drawn uses this.
#define FX_PX(v) ((uint8_t)((v) >> 8))

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
extern uint16_t player_x;  // top-left of the 16x16 sprite, playfield coords, 8.8
extern uint16_t player_y;
extern int16_t player_vx;  // px/frame, 8.8
extern int16_t player_vy;
extern uint8_t player_facing;
extern uint8_t player_anim_dir;    // art direction: 0 down, 1 up, 2 right, 3 left
extern uint8_t player_anim_frame;  // frame within the walk cycle (0 = standing)
extern uint16_t anim_step;         // tunable: px walked per animation frame, 8.8

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

void player_init(uint8_t px, uint8_t py);
void player_update(void);
void player_draw(void);

void move_apply_preset(uint8_t preset);
// Recompute the per-frame rates after changing a tunable.
void move_recalc(void);
const char *move_preset_name(void);

#endif
