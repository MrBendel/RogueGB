// Camera: follows a target with easing and a lead in the direction of travel,
// clamped to the room, and streams the background as it scrolls. Issue #19.
#ifndef CAMERA_H
#define CAMERA_H

#include <gb/gb.h>
#include <stdint.h>

extern uint16_t cam_x;  // top-left of the view in room pixels (what's drawn)
extern uint16_t cam_y;
// Tunables.
extern uint8_t cam_lead;  // frames of velocity the camera looks ahead (0 = none)
extern uint16_t cam_lead_rate;  // how fast the lead changes, px/frame 8.8

// Snap to the target and draw the whole view (call with the display off).
void camera_init(uint16_t tx, uint16_t ty) BANKED;  // camera_setup.c
// Follow the target (room pixels of the point to keep centered), ahead of it by a
// lead that slides toward vx/vy * cam_lead (velocities in px/frame 8.8) at
// cam_lead_rate, then stream any newly visible tiles and queue the scroll for the
// next vblank.
void camera_update(uint16_t tx, uint16_t ty, int16_t vx, int16_t vy);

// Screen shake: the view jitters around the camera for `frames`, the size
// falling from up to cam_shake_max px (3 by default, tunable; 0 = off) to 1 as
// it runs out. A stronger shake replaces a weaker one. Only what's drawn moves:
// the camera itself (and so aiming, streaming) doesn't.
#define SHAKE_SMALL 4    // an enemy killed, a shield hit, a sword hit
#define SHAKE_MEDIUM 8   // the shotgun, a sniper critical, a fall into a pit
#define SHAKE_BIG 14     // hurt, the shield breaking
extern uint8_t cam_shake;      // frames of shake left
extern uint8_t cam_shake_max;  // px (tunable)
void camera_shake(uint8_t frames) BANKED;  // camera_setup.c

#endif
