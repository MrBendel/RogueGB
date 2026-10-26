// Far work RAM (far.c): the Game Boy Color's spare work RAM banks, for data that's
// only needed now and then, copied in and out (cold paths: a banked call each).
#ifndef FAR_H
#define FAR_H

#include <stdint.h>
#include <gb/gb.h>

#define FAR_BANK 2  // the bank far.c uses (banks 3-7: unused so far)

// What lives in it (bank 2, 0xD000-0xDFFF), each at a fixed address:
#define FAR_MENU_PAL ((uint8_t *)0xD000)  // menu.c: the background palettes under the menu (48)

extern const uint8_t *far_src;
extern uint8_t *far_dst;
extern uint8_t far_n;  // bytes: 1-255 (0 copies nothing)
void far_get(void) BANKED;  // far_n bytes, far_src (far) to far_dst (near)
void far_put(void) BANKED;  // far_n bytes, far_src (near) to far_dst (far)

#define FAR_GET(dst, src, n) (far_dst = (uint8_t *)(dst), far_src = (const uint8_t *)(src), far_n = (n), far_get())
#define FAR_PUT(dst, src, n) (far_dst = (uint8_t *)(dst), far_src = (const uint8_t *)(src), far_n = (n), far_put())

#endif
