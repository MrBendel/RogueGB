#pragma bank 255
// Camera setup, banked: placing the camera when a room starts. The per-frame
// follow and streaming are in camera.c (bank 0).
#include <gb/gb.h>
#include "camera.h"
#include "camera_internal.h"
#include "room.h"
#include "gfx.h"
#include "state.h"

// The camera position that centers t (no lead), clamped to the room.
static int16_t centered(uint16_t t, uint8_t half, int16_t max) {
    int16_t pos = ((int16_t)t - half) << 4;
    CLAMP(pos, max);
    return pos;
}

void camera_init(uint16_t tx, uint16_t ty) BANKED {
    cam_max_fx = room_px_w > VIEW_W ? (int16_t)((room_px_w - VIEW_W) << 4) : 0;
    cam_max_fy = room_px_h > VIEW_H ? (int16_t)((room_px_h - VIEW_H) << 4) : 0;
    lead_vx = lead_vy = 0;
    lead_h.goal = lead_h.lead = lead_h.vel = 0;
    lead_v.goal = lead_v.lead = lead_v.vel = 0;
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

// --- Screen shake (camera.h) ---
extern int8_t shake_dx, shake_dy;

void camera_shake(uint8_t frames) BANKED {
    if (frames > cam_shake) cam_shake = frames;
}

// A fixed jitter pattern (no random numbers needed): directions per frame.
static const int8_t shake_sx[8] = {1, -1, 1, 0, -1, 1, -1, 0};
static const int8_t shake_sy[8] = {0, 1, -1, 1, 0, -1, 1, -1};

// (Banked: a call a frame from camera_update, in bank 0, which has no room.)
// Offset what's drawn this frame (cam_x/cam_y, which sprites draw from, and the
// scroll) by the shake. An axis whose drawn area is still catching up (a strip
// behind) isn't shaken this frame: its margin might be too thin. Near the
// room's edge, the shake points inward.
void camera_apply_shake(uint8_t lag_x, uint8_t lag_y) BANKED {
    shake_dx = shake_dy = 0;
    if (!cam_shake) return;
    cam_shake--;
    uint8_t mag = (cam_shake + 3) >> 2;
    if (mag > cam_shake_max) mag = cam_shake_max;
    if (!mag) return;
    uint8_t i = (uint8_t)frame_count & 7;
    if (!lag_x && shake_sx[i]) {
        int16_t x = (int16_t)cam_x + (shake_sx[i] > 0 ? mag : -(int8_t)mag);
        if (x < 0 || x > (cam_max_fx >> 4)) x = (int16_t)cam_x - (shake_sx[i] > 0 ? mag : -(int8_t)mag);
        if (x >= 0 && x <= (cam_max_fx >> 4)) {
            shake_dx = (int8_t)(x - (int16_t)cam_x);
            cam_x = x;
            scroll_x = (uint8_t)x;
        }
    }
    if (!lag_y && shake_sy[i]) {
        int16_t y = (int16_t)cam_y + (shake_sy[i] > 0 ? mag : -(int8_t)mag);
        if (y < 0 || y > (cam_max_fy >> 4)) y = (int16_t)cam_y - (shake_sy[i] > 0 ? mag : -(int8_t)mag);
        if (y >= 0 && y <= (cam_max_fy >> 4)) {
            shake_dy = (int8_t)(y - (int16_t)cam_y);
            cam_y = y;
            scroll_y = (uint8_t)y;
        }
    }
}
