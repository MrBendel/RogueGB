#include <gb/gb.h>
#include "camera.h"
#include "camera_internal.h"
#include "room.h"
#include "gfx.h"
#include "state.h"


uint16_t cam_x;
uint16_t cam_y;
uint8_t cam_lead = 16;
uint16_t cam_lead_rate = 256;  // 1 px/frame

// Camera position in 12.4 fixed point (1/16 px): smooth enough for easing, and
// 16 bits cover rooms up to 2048 px, so the camera needs no 32-bit math (a 32-bit
// multiply is a long software routine on this CPU).
int16_t cam_fx;
int16_t cam_fy;
// Largest camera position per axis (12.4), so the view stays inside the room.
int16_t cam_max_fx;
int16_t cam_max_fy;
// Where the lead is heading (velocity * cam_lead, 12.4), for the velocity it was
// computed from: a multiply, so it's only redone when the velocity or the tunable
// changes. And the lead itself (12.4), which moves toward it at cam_lead_rate.
int16_t lead_vx, lead_vy, lead_goal_x, lead_goal_y;
int16_t lead_x, lead_y;
uint8_t lead_frames;
// Top-left tile of the area currently drawn in the background map.
uint8_t drawn_tx;
uint8_t drawn_ty;

// How the camera moves: it stays a fixed offset (the lead) from the target, so in
// steady movement it steps exactly with the player's pixel steps and the player
// can't wobble against the background. When the velocity changes, the lead
// slides to its new value at a steady cam_lead_rate (1 px/frame), so the camera's
// speed changes by at most that much a frame. (An eased camera moved fastest
// right as the player turned around, up to 4 px a frame, and in uneven
// 1-2-1-2 px steps while it caught up: that read as jerky.)

// The camera moves at most CAM_MAX_STEP per axis per frame (4 px in 12.4). The
// drawn area catches up one strip per frame, alternating axes when both lag, so
// each axis gets a strip at least every other frame: 8 px per 2 frames. Faster,
// and the camera could outrun it (a teleport, a respawn), forcing a full, very
// slow redraw. It also turns those jumps into a quick pan.
#define CAM_MAX_STEP 64

// The lead, one frame closer to its goal (rate in 12.4).
static int16_t lead_step(int16_t lead, int16_t goal, int16_t rate) {
    int16_t d = goal - lead;
    if (d > rate) return lead + rate;
    if (d < -rate) return lead - rate;
    return goal;
}

// One axis: the camera at target + lead (pos, 12.4), kept in the room and at
// most CAM_MAX_STEP from where it was (f).
static int16_t follow(int16_t f, int16_t pos, int16_t max) {
    CLAMP(pos, max);
    int16_t d = pos - f;
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
        lead_goal_x = (vx >> 4) * cam_lead;
    }
    if (vy != lead_vy) {
        lead_vy = vy;
        lead_goal_y = (vy >> 4) * cam_lead;
    }
    int16_t rate = cam_lead_rate >> 4;
    lead_x = lead_step(lead_x, lead_goal_x, rate);
    lead_y = lead_step(lead_y, lead_goal_y, rate);
    cam_fx = follow(cam_fx, (((int16_t)tx - VIEW_W / 2) << 4) + lead_x, cam_max_fx);
    cam_fy = follow(cam_fy, (((int16_t)ty - VIEW_H / 2) << 4) + lead_y, cam_max_fy);
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
