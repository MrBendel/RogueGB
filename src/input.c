#include <gb/gb.h>
#include "input.h"

uint8_t joy;
uint8_t joy_prev;
uint8_t joy_pressed;
uint8_t joy_released;

void input_update(void) {
    joy_prev = joy;
    joy = joypad();
    joy_pressed = joy & ~joy_prev;
    joy_released = joy_prev & ~joy;
}
