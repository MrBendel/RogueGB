#pragma bank 255
// Camera setup, banked: placing the camera when a room starts. The per-frame
// follow and streaming are in camera.c (bank 0).
#include <gb/gb.h>
#include "camera.h"
#include "camera_internal.h"
#include "room.h"
#include "gfx.h"

// The camera position that centers t (no lead), clamped to the room.
static int16_t centered(uint16_t t, uint8_t half, int16_t max) {
    int16_t pos = ((int16_t)t - half) << 4;
    CLAMP(pos, max);
    return pos;
}

void camera_init(uint16_t tx, uint16_t ty) BANKED {
    cam_max_fx = room_px_w > VIEW_W ? (int16_t)((room_px_w - VIEW_W) << 4) : 0;
    cam_max_fy = room_px_h > VIEW_H ? (int16_t)((room_px_h - VIEW_H) << 4) : 0;
    lead_vx = lead_vy = lead_goal_x = lead_goal_y = 0;
    lead_x = lead_y = 0;
    lead_frames = cam_lead;
    cam_fx = centered(tx, VIEW_W / 2, cam_max_fx);
    cam_fy = centered(ty, VIEW_H / 2, cam_max_fy);
    camera_apply();
    drawn_tx = (cam_x >> 3) - 1;
    drawn_ty = (cam_y >> 3) - 1;
    // The display is off here, so the strips can be written straight away.
    for (uint8_t i = 0; i < DRAW_ROWS; i++) {
        room_draw_row(drawn_tx, drawn_ty + i, DRAW_COLS);
        gfx_flush();
    }
    move_bkg(scroll_x, scroll_y);
}

