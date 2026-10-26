#pragma bank 255
// Far work RAM: the Game Boy Color's work RAM banks 2-7 (0xD000-0xDFFF, the window
// bank 1 sits in the rest of the time: SVBK). GBDK doesn't support WRAM banking
// (the stack and our globals past 0xD000 live in that window), so these copy a
// byte at a time: bank 2 in, the byte into a register, bank 1 back, then the near
// side's write. The stack is never touched while bank 2 is in, and interrupts are
// off throughout (an interrupt would push onto it). The near side can be anywhere
// but another far bank. Parameters are globals (far.h), read before switching.
// Banked (no room in bank 0), so a call costs a bank switch: for cold paths only
// (the menu opening), a few dozen cycles a byte.
#include <gb/gb.h>
#include "far.h"

const uint8_t *far_src;
uint8_t *far_dst;
uint8_t far_n;

// Each loads hl = far_src, de = far_dst, c = far_n (0: nothing to do) first.
void far_get(void) BANKED __naked {
    __asm
    ld hl, #_far_dst
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_far_src
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    ld a, (_far_n)
    ld c, a
    or a, a
    ret z
    di
1$:
    ld a, #FAR_BANK
    ldh (0x70), a
    ld b, (hl)
    inc hl
    ld a, #1
    ldh (0x70), a
    ld a, b
    ld (de), a
    inc de
    dec c
    jr nz, 1$
    ei
    ret
    __endasm;
}

void far_put(void) BANKED __naked {
    __asm
    ld hl, #_far_dst
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_far_src
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    ld a, (_far_n)
    ld c, a
    or a, a
    ret z
    di
1$:
    ld b, (hl)
    inc hl
    ld a, #FAR_BANK
    ldh (0x70), a
    ld a, b
    ld (de), a
    inc de
    ld a, #1
    ldh (0x70), a
    dec c
    jr nz, 1$
    ei
    ret
    __endasm;
}
