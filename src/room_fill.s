; Fill a strip buffer with background tiles for a row or column of the room,
; called from room_draw_row / room_draw_column (room.c) while the camera scrolls.
;
; Inputs (globals set by room.c): fill_dst (buffer), fill_tx, fill_ty (room tile
; coordinates), fill_n (tiles, at least 1). The block map is at 0xD000 with rows of
; 32 blocks (block address = 0xD000 | by << 5 | bx), and the tile lookup table is at
; 0xD400: tile = lut[block << 2 | (ty & 1) << 1 | (tx & 1)].
;
; A block is 2x2 tiles, so the loops read each block once and write both of its
; tiles along the strip (~90 cycles per tile; a tile at a time was ~260). The
; quadrant bit that stays fixed along the strip is a macro argument, so each
; routine has two copies of its loop. Block coordinates wrap at 32, like the map.

    .module room_fill
    .globl _fill_dst, _fill_tx, _fill_ty, _fill_n, _room_w, _room_h

MAP_HI = 0xD0
LUT_HI = 0xD4

    .area _DATA
count:   .ds 1      ; block pairs left
odd:     .ds 1      ; 1 if a lone tile is left after the pairs

    .area _CODE

; hl = 0xD000 | (fill_ty >> 1 & 31) << 5 | (fill_tx >> 1 & 31); de = fill_dst
.macro FILL_SETUP
    ld a, (_fill_ty)
    srl a
    and a, #31              ; by
    ld b, a
    and a, #7
    swap a
    add a, a
    ld l, a
    ld a, (_fill_tx)
    srl a
    and a, #31              ; bx
    or a, l
    ld l, a
    ld a, b
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld h, a
    ld a, (_fill_dst)
    ld e, a
    ld a, (_fill_dst + 1)
    ld d, a
.endm

; Tile of the block at hl, quadrant q, to (de++).
.macro PUT q
    ld a, (hl)
    add a, a
    add a, a
    or a, #q
    ld c, a
    ld b, #LUT_HI
    ld a, (bc)
    ld (de), a
    inc de
.endm

; count = fill_n / 2 pairs, odd = fill_n & 1 (after any lone first tile)
.macro SPLIT_COUNT
    ld a, (_fill_n)
    srl a
    ld (count), a
    ld a, #0
    rla
    ld (odd), a
.endm

; One row (q = quadrant row bit, 0 or 2): left/right halves of each block.
.macro ROW_FILL q
    ld a, (_fill_tx)
    rra
    jr nc, 1$
    ; starts on the right half of a block
    PUT q+1
    ld a, (_fill_n)
    dec a
    ret z
    ld (_fill_n), a
    inc l                   ; next block, wrapping within the row
    ld a, l
    and a, #31
    jr nz, 1$
    ld a, l
    sub a, #32
    ld l, a
1$:
    SPLIT_COUNT
    ld a, (count)
    or a, a
    jr z, 3$
2$:
    ld a, (hl)
    add a, a
    add a, a
    or a, #q
    ld c, a
    ld b, #LUT_HI
    ld a, (bc)
    ld (de), a
    inc de
    inc c
    ld a, (bc)
    ld (de), a
    inc de
    inc l
    ld a, l
    and a, #31
    jr nz, 4$
    ld a, l
    sub a, #32
    ld l, a
4$:
    ld a, (count)
    dec a
    ld (count), a
    jr nz, 2$
3$:
    ld a, (odd)
    or a, a
    ret z
    PUT q
    ret
.endm

; One column (q = quadrant column bit, 0 or 1): top/bottom halves of each block.
.macro COL_FILL q
    ld a, (_fill_ty)
    rra
    jr nc, 1$
    ; starts on the bottom half of a block
    PUT q+2
    ld a, (_fill_n)
    dec a
    ret z
    ld (_fill_n), a
    ld a, l                 ; next block row, wrapping at the bottom of the map
    add a, #32
    ld l, a
    jr nc, 1$
    inc h
    ld a, h
    cp a, #MAP_HI + 4
    jr nz, 1$
    ld h, #MAP_HI
1$:
    SPLIT_COUNT
    ld a, (count)
    or a, a
    jr z, 3$
2$:
    ld a, (hl)
    add a, a
    add a, a
    or a, #q
    ld c, a
    ld b, #LUT_HI
    ld a, (bc)
    ld (de), a
    inc de
    inc c
    inc c
    ld a, (bc)
    ld (de), a
    inc de
    ld a, l
    add a, #32
    ld l, a
    jr nc, 4$
    inc h
    ld a, h
    cp a, #MAP_HI + 4
    jr nz, 4$
    ld h, #MAP_HI
4$:
    ld a, (count)
    dec a
    ld (count), a
    jr nz, 2$
3$:
    ld a, (odd)
    or a, a
    ret z
    PUT q
    ret
.endm

; void room_fill_row(void)
_room_fill_row::
    FILL_SETUP
    ld a, (_fill_ty)
    rra
    jp c, fill_row_bottom
    ROW_FILL 0
fill_row_bottom:
    ROW_FILL 2

; void room_fill_column(void)
_room_fill_column::
    FILL_SETUP
    ld a, (_fill_tx)
    rra
    jp c, fill_col_right
    COL_FILL 0
fill_col_right:
    COL_FILL 1

; uint8_t room_block_at(uint16_t x, uint16_t y)
; Block type at room pixel (x, y); outside the room (including coordinates that
; wrapped below zero) reads as solid. Compiler convention: x in de, y in bc,
; result in a. The most-called lookup (collision, pits, enemies), hence assembly.
; Rooms are at most 32 blocks (512 px) a side, so a coordinate with a high byte
; of 2 or more is outside, and the rest fit 8-bit block coordinates.
_room_block_at::
    ld a, d
    cp a, #2
    jr nc, 9$               ; x >= 512, or negative
    ld a, b
    cp a, #2
    jr nc, 9$
    ; column = x >> 4: high nibble of e, plus d (0 or 1) as bit 4
    ld a, e
    and a, #0xF0
    or a, d
    swap a
    ld hl, #_room_w
    cp a, (hl)
    jr nc, 9$               ; column >= room width
    ld e, a
    ; row = y >> 4
    ld a, c
    and a, #0xF0
    or a, b
    swap a
    ld hl, #_room_h
    cp a, (hl)
    jr nc, 9$
    ; hl = 0xD000 | row << 5 | column
    ld c, a
    and a, #7
    swap a
    add a, a
    or a, e
    ld l, a
    ld a, c
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld h, a
    ld a, (hl)
    ret
9$:
    ld a, #1                ; BLK_SOLID
    ret

; uint8_t room_line_clear(void)
; Is the line from (line_fx, line_fy) clear of walls and raised blocks? Samples
; line_n points (1 or more), adding (line_sx, line_sy) before each. Positions are 12.4 fixed point,
; so a position's high byte is its block coordinate (16 px blocks): each sample is
; two 16-bit adds and a map read. Both ends must be inside the room.
; Returns 1 if clear. Used by auto-aim (aim.c) for line of sight.
    .globl _line_fx, _line_fy, _line_sx, _line_sy, _line_n

    .area _DATA
_line_fx:: .ds 2
_line_fy:: .ds 2
_line_sx:: .ds 2
_line_sy:: .ds 2
_line_n::  .ds 1
samples:   .ds 1

    .area _CODE
_room_line_clear::
    ld a, (_line_n)
    ld (samples), a
    ld hl, #_line_fx
    ld a, (hl+)
    ld c, a
    ld b, (hl)              ; bc = fx
    ld hl, #_line_fy
    ld a, (hl+)
    ld e, a
    ld d, (hl)              ; de = fy
20$:
    ld hl, #_line_sx        ; fx += sx
    ld a, c
    add a, (hl)
    ld c, a
    inc hl
    ld a, b
    adc a, (hl)
    ld b, a                 ; b = block column (high byte of 12.4)
    ld hl, #_line_sy        ; fy += sy
    ld a, e
    add a, (hl)
    ld e, a
    inc hl
    ld a, d
    adc a, (hl)
    ld d, a                 ; d = block row
    ; hl = 0xD000 | row << 5 | column. No bounds check: both ends of the line
    ; are inside the room, a rectangle, so every point between them is too.
    ld a, d
    and a, #7
    swap a
    add a, a
    or a, b
    ld l, a
    ld a, d
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld h, a
    ld a, (hl)
    cp a, #1                ; BLK_SOLID
    jr z, 29$
    cp a, #3                ; BLK_RAISED
    jr z, 29$
    ld hl, #samples
    dec (hl)
    jr nz, 20$
    ld a, #1
    ret
29$:
    xor a, a
    ret
