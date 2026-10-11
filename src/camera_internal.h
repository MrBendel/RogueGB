// Camera state shared by camera.c (per frame, bank 0) and camera_setup.c (banked).
#ifndef CAMERA_INTERNAL_H
#define CAMERA_INTERNAL_H

#include <stdint.h>

// Tiles drawn around the view. The view (160x128) spans at most 21x17 tiles; the
// drawn area is 23x19, centered on it with a spare tile on every side. So it can
// trail the camera by a tile in any direction and still cover the view, which
// lets each frame catch up by at most one strip (a row or a column) instead of
// sometimes both at once.
#define DRAW_COLS 23
#define DRAW_ROWS 19

// Keep a camera coordinate in 0..max. One unsigned compare in the common case.
#define CLAMP(pos, max) \
    if ((uint16_t)(pos) > (uint16_t)(max)) pos = (pos) < 0 ? 0 : (max)

extern int16_t cam_fx, cam_fy;          // camera position, 12.4
extern int16_t cam_max_fx, cam_max_fy;  // largest camera position per axis, 12.4
extern uint16_t last_tx, last_ty;       // the target last frame
extern int16_t lead_vx, lead_vy, lead_x, lead_y;  // cached lead and its velocity
extern uint8_t lead_frames;             // cam_lead the lead was computed with
extern uint8_t drawn_tx, drawn_ty;      // top-left tile of the drawn area

// cam_x/cam_y and the scroll registers from cam_fx/cam_fy.
void camera_apply(void);

#endif
