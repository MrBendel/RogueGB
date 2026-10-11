; Write the queued VRAM updates (gfx.c) during vblank.
;
; Called from the vblank handler, after it has set the scroll registers. Vblank
; lasts ~4560 cycles and the sprite DMA uses ~700 of them, so this has to be fast:
; the C version took ~77 cycles per byte and could run past vblank (a scroll
; tear, and VRAM writes the hardware ignores). This is ~20-30 cycles per byte.
;
; Data layout, matching gfx.c:
;   copies[]: dst (2 bytes), src (2), len (1)              -> 5 bytes each
;   strips[]: x, y, vertical, len, tiles[STRIP_MAX_LEN=23] -> 27 bytes each
; x and y are background map tile coordinates (0-31); the map is at 0x9800.

    .module vram_queue
    .globl _copies, _copy_count, _strips, _strip_count

STRIP_SIZE = 27     ; 4 + STRIP_MAX_LEN (gfx.h)

    .area _DATA
q_left:   .ds 1     ; entries left
q_next:   .ds 2     ; address of the next entry
s_len:    .ds 1
s_rowlo:  .ds 1     ; low byte of the map row's address, (y & 7) << 5

    .area _CODE

; void vram_queue_write(void)
_vram_queue_write::
    ; ---- byte copies -------------------------------------------------------
    ld a, (_copy_count)
    or a, a
    jr z, 20$
    ld (q_left), a
    ld hl, #_copies
10$:
    ld a, (hl+)
    ld e, a
    ld a, (hl+)
    ld d, a             ; de = destination
    ld a, (hl+)
    ld c, a
    ld a, (hl+)
    ld b, a             ; bc = source
    ld a, (hl+)         ; length
    push hl             ; next entry
    ld h, b
    ld l, c
    ld c, a
11$:
    ld a, (hl+)
    ld (de), a
    inc de
    dec c
    jr nz, 11$
    pop hl
    ld a, (q_left)
    dec a
    ld (q_left), a
    jr nz, 10$

    ; ---- background strips --------------------------------------------------
20$:
    ld a, (_strip_count)
    or a, a
    ret z
    ld (q_left), a
    ld hl, #_strips
21$:
    ; remember where the next strip starts
    ld a, l
    add a, #STRIP_SIZE
    ld (q_next), a
    ld a, h
    adc a, #0
    ld (q_next + 1), a

    ld a, (hl+)
    ld c, a             ; c = x
    ld a, (hl+)         ; y
    ld b, a
    and a, #7
    swap a
    add a, a
    ld (s_rowlo), a     ; (y & 7) << 5
    ld a, b
    srl a
    srl a
    srl a
    add a, #0x98
    ld d, a             ; d = high byte of the row's address
    ld a, (hl+)
    ld b, a             ; b = vertical
    ld a, (hl+)
    ld (s_len), a       ; hl -> tiles
    ld a, b
    or a, a
    jr nz, 30$

    ; horizontal: x wraps within the row
    ld a, (s_rowlo)
    ld b, a
    ld a, (s_len)
    ld e, a             ; e as the counter for now
22$:
    push de
    ld a, c
    and a, #31
    or a, b
    ld e, a             ; de -> map[y][x]
    ld a, (hl+)
    ld (de), a
    inc c
    pop de
    dec e
    jr nz, 22$
    jr 40$

    ; vertical: step down a row (32 bytes), wrapping from the bottom to the top
30$:
    ld a, (s_rowlo)
    or a, c             ; c < 32
    ld e, a             ; de -> map[y][x]
    ld a, (s_len)
    ld c, a
31$:
    ld a, (hl+)
    ld (de), a
    ld a, e
    add a, #32
    ld e, a
    jr nc, 32$
    inc d
    ld a, d
    cp a, #0x9C
    jr nz, 32$
    ld d, #0x98         ; wrapped past the bottom of the map
32$:
    dec c
    jr nz, 31$

40$:
    ld a, (q_next)
    ld l, a
    ld a, (q_next + 1)
    ld h, a
    ld a, (q_left)
    dec a
    ld (q_left), a
    jr nz, 21$
    ret
