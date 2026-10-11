// Camera: follows a target with easing and a lead in the direction of travel,
// clamped to the room, and streams the background as it scrolls. Issue #19.
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>

extern uint16_t cam_x;  // top-left of the view in room pixels (what's drawn)
extern uint16_t cam_y;
// Tunables.
extern uint8_t cam_lead;  // frames of velocity the camera looks ahead (0 = none)
extern uint8_t cam_ease;  // 0 = locked to the target, n = close 1/2^n of the gap per frame

// Snap to the target and draw the whole view (call with the display off).
void camera_init(uint16_t tx, uint16_t ty);
// Ease toward the target (room pixels of the point to keep centered), plus
// vx/vy * cam_lead (velocities in px/frame 8.8), then stream any newly
// visible tiles and queue the scroll for the next vblank.
void camera_update(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy);

#endif
