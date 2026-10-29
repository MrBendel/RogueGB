#pragma bank 255
// The peashooter's think step (actor.h ACTOR_PEASHOOTER), banked to keep bank 0
// for the code every enemy runs; actors_update calls it on each one's step.
#include <gb/gb.h>
#include "actor_internal.h"
#include "player.h"

void shooter_update(uint8_t i) BANKED {
    uint8_t t = actor_timer[i] = actor_timer[i] > AI_STEP ? actor_timer[i] - AI_STEP : 0;
    // Turning to face the player is the expensive part (16-bit distances), so a
    // turret re-aims every 4th think step (~7 times a second) and when it fires.
    if (!(t & (AI_STEP * 4 - 1)) || !t) {
        int16_t dx = (int16_t)(player_x - actor_x[i]);
        int16_t dy = (int16_t)(player_y - actor_y[i]);
        uint16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        actor_face[i] = ax > ay ? (dx > 0 ? FACE_RIGHT : FACE_LEFT) : (dy > 0 ? FACE_DOWN : FACE_UP);
        if (!t) {
            // Fire, aimed in 8 directions: diagonal when the axes are within 2:1.
            int8_t sx = (ax << 1) > ay ? (dx > 0 ? 1 : -1) : 0;
            int8_t sy = (ay << 1) > ax ? (dy > 0 ? 1 : -1) : 0;
            fire_volley(i, sx, sy);
            actor_timer[i] = shooter_interval;
        }
    }
    uint8_t telegraph = t && t < shooter_telegraph;
    // Warning: flash and open up before firing.
    actor_flash[i] = telegraph && (t & 4);
    actor_frame[i] = (actor_face[i] << 1) + telegraph;
}
