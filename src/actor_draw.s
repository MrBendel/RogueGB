; Fast sprite emission for actors and bullets, called from actors_draw (actor.c).
;
; Written in assembly because the compiler's version of these loops cost about
; 1000 CPU cycles per sprite (it recomputes array addresses and spills to the
; stack on every access); this is closer to 150. The C side keeps all the logic:
; it caches each actor's tile/flag bytes (actor_t0/p0/t1/p1) when its animation
; frame changes, and sets up oam_next, draw_rotation and the bullet tile/flags.
;
; Both routines walk 8 slots starting at draw_rotation (wrapping), skip empty or
; off-screen ones, and append entries to shadow_OAM at index oam_next, which they
; advance. They stop when OAM is full. shadow_OAM is at 0xC000 (256-aligned), so
; an entry's address is just (high byte, index * 4).

    .module actor_draw

    .globl _actor_type, _actor_x, _actor_y, _actor_flash
    .globl _actor_t0, _actor_p0, _actor_t1, _actor_p1
    .globl _bullet_alive, _bullet_x, _bullet_y
    .globl _bullet_draw_tile, _bullet_draw_prop
    .globl _cam_x, _cam_y, _draw_rotation, _oam_next, _shadow_OAM

    .area _DATA
slots_left: .ds 1   ; slots still to visit
slot:       .ds 1   ; current slot (before & 7)
scr_x:      .ds 1   ; screen x + 8 (hardware coordinate of the right column)
scr_y:      .ds 1   ; screen y + 16 (hardware coordinate)

    .area _CODE

; hl += c (c = byte offset into an array).
add_hl_c:
    ld a, l
    add a, c
    ld l, a
    ret nc
    inc h
    ret

; Screen coordinate of a 16-bit room coordinate.
; in:  hl -> uint16 coordinate, de = camera, b = offset to add, c = visible limit
; out: a = coordinate + offset - camera; carry set if 0 <= a < limit (visible)
coord:
    ld a, (hl+)
    add a, b
    ld b, a             ; b = low byte + offset
    ld a, (hl)
    adc a, #0
    ld h, a             ; h = high byte (+ carry)
    ld a, b
    sub a, e
    ld b, a             ; b = low byte of the result
    ld a, h
    sbc a, d            ; high byte of the result
    jr nz, 1$           ; off-screen (beyond 255, or negative)
    ld a, b
    cp a, c             ; carry if visible
    ret
1$:
    and a, a            ; clear carry: not visible
    ret

; c = (slot & 7), the array index for the current slot.
cur_index:
    ld a, (slot)
    and a, #7
    ld c, a
    ret

; Next slot; z set when all 8 are done.
next_slot:
    ld hl, #slot
    inc (hl)
    ld hl, #slots_left
    dec (hl)
    ret

; void actors_emit(void): two sprites (left and right 8x16 columns) per actor.
_actors_emit::
    ld a, #8
    ld (slots_left), a
    ld a, (_draw_rotation)
    ld (slot), a
2$:
    call cur_index
    ld hl, #_actor_type
    call add_hl_c
    ld a, (hl)
    or a, a
    jp z, 8$            ; empty slot

    ; x: right column's hardware x = room x + 16 - cam_x; visible while < 176
    sla c               ; 16-bit array index
    ld hl, #_actor_x
    call add_hl_c
    ld a, (_cam_x)
    ld e, a
    ld a, (_cam_x + 1)
    ld d, a
    ld b, #16
    ld c, #176
    call coord
    jp nc, 8$
    ld (scr_x), a

    ; y: hardware y = room y + 16 - cam_y; visible while < 144 (view 128 + 16)
    call cur_index
    sla c
    ld hl, #_actor_y
    call add_hl_c
    ld a, (_cam_y)
    ld e, a
    ld a, (_cam_y + 1)
    ld d, a
    ld b, #16
    ld c, #144
    call coord
    jp nc, 8$
    ld (scr_y), a

    ; room for two entries?
    ld a, (_oam_next)
    cp a, #39
    jp nc, 9$

    ; b = palette bit: the flash palette while flashing
    call cur_index
    ld hl, #_actor_flash
    call add_hl_c
    ld a, (hl)
    or a, a
    jr z, 3$
    ld a, #0x10
3$:
    ld b, a

    ; de -> shadow_OAM[oam_next]
    ld a, (_oam_next)
    add a, a
    add a, a
    ld e, a
    ld d, #>(_shadow_OAM)

    ; left column: y, x - 8, tile, flags
    ld a, (scr_y)
    ld (de), a
    inc e
    ld a, (scr_x)
    sub a, #8
    ld (de), a
    inc e
    ld hl, #_actor_t0
    call add_hl_c
    ld a, (hl)
    ld (de), a
    inc e
    ld hl, #_actor_p0
    call add_hl_c
    ld a, (hl)
    or a, b
    ld (de), a
    inc e

    ; right column: y, x, tile, flags
    ld a, (scr_y)
    ld (de), a
    inc e
    ld a, (scr_x)
    ld (de), a
    inc e
    ld hl, #_actor_t1
    call add_hl_c
    ld a, (hl)
    ld (de), a
    inc e
    ld hl, #_actor_p1
    call add_hl_c
    ld a, (hl)
    or a, b
    ld (de), a

    ld hl, #_oam_next
    inc (hl)
    inc (hl)
8$:
    call next_slot
    jp nz, 2$
9$:
    ret

; void bullets_emit(void): one sprite per bullet (its 8x16 column).
_bullets_emit::
    ld a, #8
    ld (slots_left), a
    ld a, (_draw_rotation)
    ld (slot), a
4$:
    call cur_index
    ld hl, #_bullet_alive
    call add_hl_c
    ld a, (hl)
    or a, a
    jr z, 6$

    ; x: hardware x = room x + 8 - cam_x; visible while < 168
    sla c
    ld hl, #_bullet_x
    call add_hl_c
    ld a, (_cam_x)
    ld e, a
    ld a, (_cam_x + 1)
    ld d, a
    ld b, #8
    ld c, #168
    call coord
    jr nc, 6$
    ld (scr_x), a

    call cur_index
    sla c
    ld hl, #_bullet_y
    call add_hl_c
    ld a, (_cam_y)
    ld e, a
    ld a, (_cam_y + 1)
    ld d, a
    ld b, #16
    ld c, #144
    call coord
    jr nc, 6$
    ld (scr_y), a

    ld a, (_oam_next)
    cp a, #40
    jr nc, 7$

    add a, a
    add a, a
    ld e, a
    ld d, #>(_shadow_OAM)
    ld a, (scr_y)
    ld (de), a
    inc e
    ld a, (scr_x)
    ld (de), a
    inc e
    ld a, (_bullet_draw_tile)
    ld (de), a
    inc e
    ld a, (_bullet_draw_prop)
    ld (de), a

    ld hl, #_oam_next
    inc (hl)
6$:
    call next_slot
    jr nz, 4$
7$:
    ret

; ---------------------------------------------------------------------------
; void bullets_move(void): position += velocity for every live bullet.
; Positions are 16-bit pixels plus an 8-bit fraction (bullet_x/_xs, bullet_y/_ys),
; velocities are 8.8 (bullet_vx/_vy): a 24-bit add done bytewise with the carry.

    .globl _bullet_xs, _bullet_ys, _bullet_vx, _bullet_vy

    .area _DATA
vel_base:   .ds 2   ; address of the velocity array for the current axis
v_lo:       .ds 1
v_hi:       .ds 1
v_sign:     .ds 1   ; 0x00 or 0xFF: the velocity's sign extension

    .area _CODE

; One axis of slot c. in: hl = fraction array, de = position array, vel_base set.
move_axis:
    call add_hl_c       ; hl -> fraction[i]
    ld a, c
    add a, a
    add a, e
    ld e, a
    ld a, d
    adc a, #0
    ld d, a             ; de -> position[i] (16-bit array)
    push hl
    ld a, (vel_base)
    ld l, a
    ld a, (vel_base + 1)
    ld h, a
    ld a, c
    add a, a
    add a, l
    ld l, a
    ld a, h
    adc a, #0
    ld h, a             ; hl -> velocity[i]
    ld a, (hl+)
    ld (v_lo), a
    ld a, (hl)
    ld (v_hi), a
    rla
    sbc a, a
    ld (v_sign), a
    pop hl
    ; fraction += low byte of velocity
    ld a, (v_lo)
    add a, (hl)
    ld (hl), a
    ; position low += high byte of velocity + carry (loads keep the flags)
    ld a, (v_hi)
    ld b, a
    ld a, (de)
    adc a, b
    ld (de), a
    inc de
    ; position high += sign extension + carry
    ld a, (v_sign)
    ld b, a
    ld a, (de)
    adc a, b
    ld (de), a
    ret

_bullets_move::
    xor a, a
    ld (slot), a
    ld a, #8
    ld (slots_left), a
5$:
    call cur_index
    ld hl, #_bullet_alive
    call add_hl_c
    ld a, (hl)
    or a, a
    jr z, 10$
    ld a, #<(_bullet_vx)
    ld (vel_base), a
    ld a, #>(_bullet_vx)
    ld (vel_base + 1), a
    ld hl, #_bullet_xs
    ld de, #_bullet_x
    call move_axis
    call cur_index
    ld a, #<(_bullet_vy)
    ld (vel_base), a
    ld a, #>(_bullet_vy)
    ld (vel_base + 1), a
    ld hl, #_bullet_ys
    ld de, #_bullet_y
    call move_axis
10$:
    call next_slot
    jr nz, 5$
    ret
