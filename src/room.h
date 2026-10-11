// The current room: a grid of 16x16 blocks and collision queries against it.
// For now there is one hand-written test room; M2 replaces this with authored rooms.
#ifndef ROOM_H_INCLUDED
#define ROOM_H_INCLUDED

#include <stdint.h>

#define BLOCK_SIZE 16
#define ROOM_W 10  // blocks; one screen is 10x8 blocks above the HUD row
#define ROOM_H 8
#define ROOM_PX_W (ROOM_W * BLOCK_SIZE)
#define ROOM_PX_H (ROOM_H * BLOCK_SIZE)

#define BLK_FLOOR 0
#define BLK_SOLID 1

extern uint8_t room_map[ROOM_H][ROOM_W];

// Load the movement-lab room and draw it to the background.
void room_load_lab(void);

// Is the playfield pixel (x, y) inside a solid block? Anything outside the
// room counts as solid, including coordinates that wrapped below zero.
uint8_t room_solid_at(uint8_t x, uint8_t y);

#endif
