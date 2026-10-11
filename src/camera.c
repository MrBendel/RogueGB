#include <gb/gb.h>
#include "camera.h"
#include "room.h"
#include "gfx.h"

// Tiles drawn around the view: 21 columns x 17 rows covers 160x128 at any
// sub-tile scroll offset.
#define DRAW_COLS 21
#define DRAW_ROWS 17

uint16_t cam_x;
uint16_t cam_y;
uint8_t cam_lead = 16;
uint8_t cam_ease = 3;

// Camera position in 12.4 fixed point (1/16 px): smooth enough for easing, and
// 16 bits cover rooms up to 2048 px, so the camera needs no 32-bit math (a 32-bit
// multiply is a long software routine on this CPU).
static int16_t cam_fx;
static int16_t cam_fy;
// Top-left tile of the area currently drawn in the background map.
static uint8_t drawn_tx;
static uint8_t drawn_ty;

// Where the camera wants to be on one axis: the target centered in the view,
// plus the lead (velocity in px/frame 8.8, times cam_lead frames), clamped to the room.
static int16_t target_axis(uint16_t t, uint8_t half, int16_t v, uint16_t room_px, uint8_t view) {
    int16_t pos = (((int16_t)t - half) << 4) + (v >> 4) * cam_lead;
    int16_t max = room_px > view ? (int16_t)((room_px - view) << 4) : 0;
    if (pos < 0) return 0;
    if (pos > max) return max;
    return pos;
}

// Close 1/2^cam_ease of the gap, but always at least 1/16 px, so the camera
// lands exactly on the target instead of stalling a fraction short of it.
static int16_t ease_step(int16_t gap) {
    int16_t step = gap >> cam_ease;
    if (!step && gap) step = gap > 0 ? 1 : -1;
    return step;
}

static void apply(void) {
    cam_x = (uint16_t)cam_fx >> 4;
    cam_y = (uint16_t)cam_fy >> 4;
    scroll_x = (uint8_t)cam_x;
    scroll_y = (uint8_t)cam_y;
}

void camera_init(uint16_t tx, uint16_t ty) {
    cam_fx = target_axis(tx, VIEW_W / 2, 0, room_px_w, VIEW_W);
    cam_fy = target_axis(ty, VIEW_H / 2, 0, room_px_h, VIEW_H);
    apply();
    drawn_tx = cam_x >> 3;
    drawn_ty = cam_y >> 3;
    // The display is off here, so the strips can be written straight away.
    for (uint8_t i = 0; i < DRAW_ROWS; i++) {
        room_draw_row(drawn_tx, drawn_ty + i, DRAW_COLS);
        gfx_flush();
    }
    move_bkg(scroll_x, scroll_y);
}

void camera_update(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy) {
    int16_t gx = target_axis(tx, VIEW_W / 2, vx, room_px_w, VIEW_W);
    int16_t gy = target_axis(ty, VIEW_H / 2, vy, room_px_h, VIEW_H);
    if (cam_ease) {
        cam_fx += ease_step(gx - cam_fx);
        cam_fy += ease_step(gy - cam_fy);
    } else {
        cam_fx = gx;
        cam_fy = gy;
    }
    apply();

    // Stream in the columns and rows that came into view. The new area is drawn
    // before the scroll takes effect at the next vblank, and it sits just outside
    // the old view, so nothing is drawn where it can be seen.
    uint8_t ntx = cam_x >> 3, nty = cam_y >> 3;
    if (ntx != drawn_tx || nty != drawn_ty) {
        if ((uint8_t)(ntx - drawn_tx + DRAW_COLS) > 2 * DRAW_COLS || (uint8_t)(nty - drawn_ty + DRAW_ROWS) > 2 * DRAW_ROWS) {
            // Jumped further than a screen: redraw everything. This writes during the
            // frame (flushing as the queue fills), so it's slow, but it's rare.
            for (uint8_t i = 0; i < DRAW_ROWS; i++) room_draw_row(ntx, nty + i, DRAW_COLS);
        } else {
            while (drawn_tx < ntx) room_draw_column(drawn_tx++ + DRAW_COLS, nty, DRAW_ROWS);
            while (drawn_tx > ntx) room_draw_column(--drawn_tx, nty, DRAW_ROWS);
            while (drawn_ty < nty) room_draw_row(ntx, drawn_ty++ + DRAW_ROWS, DRAW_COLS);
            while (drawn_ty > nty) room_draw_row(ntx, --drawn_ty, DRAW_COLS);
        }
        drawn_tx = ntx;
        drawn_ty = nty;
    }
}
