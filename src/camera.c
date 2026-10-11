#include <gb/gb.h>
#include "camera.h"
#include "camera_internal.h"
#include "room.h"
#include "gfx.h"
#include "state.h"


uint16_t cam_x;
uint16_t cam_y;
uint8_t cam_lead = 16;
uint8_t cam_ease = 3;

// Camera position in 12.4 fixed point (1/16 px): smooth enough for easing, and
// 16 bits cover rooms up to 2048 px, so the camera needs no 32-bit math (a 32-bit
// multiply is a long software routine on this CPU).
int16_t cam_fx;
int16_t cam_fy;
// Largest camera position per axis (12.4), so the view stays inside the room.
int16_t cam_max_fx;
int16_t cam_max_fy;
// The target point last frame, to move the camera by the same pixel step.
uint16_t last_tx;
uint16_t last_ty;
// The lead (velocity * cam_lead, 12.4) for the velocity it was computed from: a
// multiply, so it's only redone when the velocity or the tunable changes.
int16_t lead_vx, lead_vy, lead_x, lead_y;
uint8_t lead_frames;
// Top-left tile of the area currently drawn in the background map.
uint8_t drawn_tx;
uint8_t drawn_ty;

// Close 1/2^cam_ease of the gap. Within a pixel of the target, lock onto it: in
// steady movement the camera then moves in exactly the player's pixel steps, so
// the player can't wobble a pixel against the background (easing at fractional
// positions rounds differently from the player's own steps).
#define CAM_LOCK 16  // 1 px in 12.4

// The camera moves at most CAM_MAX_STEP per axis per frame (4 px in 12.4). The
// drawn area catches up one strip per frame, alternating axes when both lag, so
// each axis gets a strip at least every other frame: 8 px per 2 frames. Faster,
// and the camera could outrun it (a hit's knockback flips the lead, and the ease
// alone jumped 7-8 px a frame), forcing a full, very slow redraw.
#define CAM_MAX_STEP 64


// One axis of the camera, f -> its new position. goal is where the camera wants
// to be (target centered plus the lead); step is the target's pixel step this
// frame (12.4). Move with that step first, then ease only what's left (the lead,
// or catching up): an eased camera alone trails a moving player by a few pixels
// and steps on different frames, so the player would wobble against the background.
static int16_t follow(int16_t f, int16_t step, int16_t goal, int16_t max) {
    CLAMP(goal, max);
    int16_t n;
    if (cam_ease) {
        n = f + step;
        CLAMP(n, max);
        int16_t gap = goal - n;
        if ((uint16_t)(gap + CAM_LOCK) > 2 * CAM_LOCK) gap >>= cam_ease;
        n += gap;
    } else {
        n = goal;
    }
    int16_t d = n - f;
    if ((uint16_t)(d + CAM_MAX_STEP) > 2 * CAM_MAX_STEP) d = d < 0 ? -CAM_MAX_STEP : CAM_MAX_STEP;
    return f + d;
}

void camera_apply(void) {
    cam_x = (uint16_t)cam_fx >> 4;
    cam_y = (uint16_t)cam_fy >> 4;
    scroll_x = (uint8_t)cam_x;
    scroll_y = (uint8_t)cam_y;
}

void camera_update(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy) {
    if (lead_frames != cam_lead) {
        lead_frames = cam_lead;
        lead_vx = ~vx;  // force both leads to be recomputed
        lead_vy = ~vy;
    }
    if (vx != lead_vx) {
        lead_vx = vx;
        lead_x = (vx >> 4) * cam_lead;
    }
    if (vy != lead_vy) {
        lead_vy = vy;
        lead_y = (vy >> 4) * cam_lead;
    }
    cam_fx = follow(cam_fx, (int16_t)(tx - last_tx) << 4,
                    (((int16_t)tx - VIEW_W / 2) << 4) + lead_x, cam_max_fx);
    cam_fy = follow(cam_fy, (int16_t)(ty - last_ty) << 4,
                    (((int16_t)ty - VIEW_H / 2) << 4) + lead_y, cam_max_fy);
    last_tx = tx;
    last_ty = ty;
    camera_apply();

    // Stream in rows and columns as the camera moves. The drawn area's target is
    // one tile up-left of the view's top-left tile. Strips are drawn just outside
    // the view and written in the next vblank, so nothing is drawn where it shows.
    uint8_t want_tx = (cam_x >> 3) - 1, want_ty = (cam_y >> 3) - 1;
    int8_t lag_x = (int8_t)(want_tx - drawn_tx), lag_y = (int8_t)(want_ty - drawn_ty);
    if (!lag_x && !lag_y) return;
    if (lag_x > 1 || lag_x < -1 || lag_y > 1 || lag_y < -1) {
        // Jumped more than a tile (a teleport, say): redraw everything. This
        // writes during the frame (flushing as the queue fills), so it's slow,
        // but it's rare.
        drawn_tx = want_tx;
        drawn_ty = want_ty;
        for (uint8_t i = 0; i < DRAW_ROWS; i++) room_draw_row(want_tx, want_ty + i, DRAW_COLS);
        return;
    }
    // One strip per frame: the column if only x lags, the row if only y lags,
    // and when both lag, alternate by frame (the other one is still covered).
    if (lag_x && (!lag_y || ((uint8_t)frame_count & 1))) {
        if (lag_x > 0) room_draw_column(drawn_tx++ + DRAW_COLS, drawn_ty, DRAW_ROWS);
        else room_draw_column(--drawn_tx, drawn_ty, DRAW_ROWS);
    } else {
        if (lag_y > 0) room_draw_row(drawn_tx, drawn_ty++ + DRAW_ROWS, DRAW_COLS);
        else room_draw_row(drawn_tx, --drawn_ty, DRAW_COLS);
    }
}
