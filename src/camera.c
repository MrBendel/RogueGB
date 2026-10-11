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

// Camera position with 8 fraction bits, so easing moves smoothly.
static int32_t cam_fx;
static int32_t cam_fy;
// Top-left tile of the area currently drawn in the background map.
static uint8_t drawn_tx;
static uint8_t drawn_ty;

static int32_t clamp_view(int32_t v, uint16_t room_px, uint16_t view) {
    int32_t max = room_px > view ? (int32_t)(room_px - view) << 8 : 0;
    if (v < 0) return 0;
    if (v > max) return max;
    return v;
}

// Where the camera wants to be: the target centered in the view, plus the lead.
static void target_of(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy, int32_t *ox, int32_t *oy) {
    *ox = clamp_view(((int32_t)tx - VIEW_W / 2) * 256 + (int32_t)vx * cam_lead, room_px_w, VIEW_W);
    *oy = clamp_view(((int32_t)ty - VIEW_H / 2) * 256 + (int32_t)vy * cam_lead, room_px_h, VIEW_H);
}

// Close 1/2^cam_ease of the gap, but always at least 1/256 px, so the camera
// lands exactly on the target instead of stalling a fraction short of it.
static int32_t ease_step(int32_t gap) {
    int32_t step = gap >> cam_ease;
    if (!step && gap) step = gap > 0 ? 1 : -1;
    return step;
}

static void apply(void) {
    cam_x = (uint16_t)(cam_fx >> 8);
    cam_y = (uint16_t)(cam_fy >> 8);
    scroll_x = (uint8_t)cam_x;
    scroll_y = (uint8_t)cam_y;
}

void camera_init(uint16_t tx, uint16_t ty) {
    target_of(tx, ty, 0, 0, &cam_fx, &cam_fy);
    apply();
    drawn_tx = cam_x >> 3;
    drawn_ty = cam_y >> 3;
    for (uint8_t i = 0; i < DRAW_ROWS; i++) room_draw_row(drawn_tx, drawn_ty + i, DRAW_COLS);
    move_bkg(scroll_x, scroll_y);
}

void camera_update(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy) {
    int32_t gx, gy;
    target_of(tx, ty, vx, vy, &gx, &gy);
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
            // Jumped further than a screen: redraw everything.
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
