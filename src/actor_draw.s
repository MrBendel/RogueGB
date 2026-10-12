; Fast sprite emission for actors, called from actors_update (actor.c). Bullets are
; in bullets.s.
;
; Written in assembly because the compiler's version of this loop cost about
; 1000 CPU cycles per sprite (it recomputes array addresses and spills to the
; stack on every access). Array addresses are computed inline (IDX8/IDX16). The C side keeps all the logic:
; it caches each actor's tile/flag bytes (actor_t0/p0/t1/p1) when its animation
; frame changes, and sets up oam_next and draw_rotation.
;
; Walks the 8 actor slots from draw_rotation (wrapping), skips empty or off-screen
; ones, and appends two entries per actor to shadow_OAM at index oam_next, which
; it advances. It stops when OAM is full. shadow_OAM is at 0xC000 (256-aligned), so
; an entry's address is just (high byte, index * 4).

    .module actor_draw

    .globl _actor_type, _actor_x, _actor_y, _actor_flash
    .globl _actor_t0, _actor_p0, _actor_t1, _actor_p1
    .globl _cam_x, _cam_y, _draw_rotation, _oam_next, _shadow_OAM


    .area _DATA
slots_left: .ds 1   ; slots still to visit
slot:       .ds 1   ; current slot (before & 7)
scr_x:      .ds 1   ; screen x + 8 (hardware coordinate of the right column)
scr_y:      .ds 1   ; screen y + 16 (hardware coordinate)

    .area _CODE

; hl = arr + c (c = byte index), without a call: adds c to the low byte and
; carries into the high byte (a = l + h + carry - l).
.macro IDX8 arr
    ld hl, #arr
    ld a, l
    add a, c
    ld l, a
    adc a, h
    sub a, l
    ld h, a
.endm

; hl = arr + b (b = byte offset of a 16-bit element, 2 * index).
.macro IDX16 arr
    ld hl, #arr
    ld a, l
    add a, b
    ld l, a
    adc a, h
    sub a, l
    ld h, a
.endm

; void actors_emit(void): two sprites (left and right 8x16 columns) per actor.
_actors_emit::
    ld a, #8
    ld (slots_left), a
    ld a, (_draw_rotation)
    ld (slot), a
2$:
    ld a, (slot)
    and a, #7
    ld c, a                 ; c = index
    IDX8 _actor_type
    ld a, (hl)
    or a, a
    jp z, 8$                ; empty slot
    ld a, c
    add a, a
    ld b, a                 ; b = 2 * index

    ; x: right column's hardware x = room x + 16 - cam_x; visible while < 176
    IDX16 _actor_x
    ld a, (hl+)
    add a, #16
    ld e, a
    ld a, (hl)
    adc a, #0
    ld d, a
    ld a, (_cam_x)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (_cam_x + 1)
    ld l, a
    ld a, d
    sbc a, l
    jp nz, 8$
    ld a, e
    cp a, #176
    jp nc, 8$
    ld (scr_x), a

    ; y: hardware y = room y + 16 - cam_y; visible while < 144 (view 128 + 16)
    IDX16 _actor_y
    ld a, (hl+)
    add a, #16
    ld e, a
    ld a, (hl)
    adc a, #0
    ld d, a
    ld a, (_cam_y)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (_cam_y + 1)
    ld l, a
    ld a, d
    sbc a, l
    jp nz, 8$
    ld a, e
    cp a, #144
    jp nc, 8$
    ld (scr_y), a

    ; room for two entries?
    ld a, (_oam_next)
    cp a, #39
    jp nc, 9$

    ; b = 1 while flashing (flash_pal swaps in sprite palette 1, SPR_PAL_FLASH)
    IDX8 _actor_flash
    ld a, (hl)
    or a, a
    jr z, 3$
    ld a, #1
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
    IDX8 _actor_t0
    ld a, (hl)
    ld (de), a
    inc e
    IDX8 _actor_p0
    ld a, (hl)
    call flash_pal          ; flashing: the flash palette instead of the type's
    ld (de), a
    inc e

    ; right column: y, x, tile, flags
    ld a, (scr_y)
    ld (de), a
    inc e
    ld a, (scr_x)
    ld (de), a
    inc e
    IDX8 _actor_t1
    ld a, (hl)
    ld (de), a
    inc e
    IDX8 _actor_p1
    ld a, (hl)
    call flash_pal          ; flashing: the flash palette instead of the type's
    ld (de), a

    ld hl, #_oam_next
    inc (hl)
    inc (hl)
8$:
    ld hl, #slot
    inc (hl)
    ld hl, #slots_left
    dec (hl)
    jp nz, 2$
9$:
    ret

; a = sprite flags; if b (flashing), their palette (bits 0-2) becomes 1.
flash_pal:
    inc b
    dec b
    ret z
    and a, #0xF8
    or a, #1
    ret
