; Fill a strip buffer with background tiles for a row or column of the room,
; called from room_draw_row / room_draw_column (room.c) while the camera scrolls.
; In assembly because the compiled loops cost ~330 cycles per tile.
;
; Inputs (globals set by room.c): fill_dst (buffer), fill_tx, fill_ty (room tile
; coordinates), fill_n (tiles). The block map is at 0xD000 with rows of 32 blocks
; (block address = 0xD000 | by << 5 | bx), and the tile lookup table is at 0xD400:
; tile = lut[block << 2 | (ty & 1) << 1 | (tx & 1)].

    .module room_fill
    .globl _fill_dst, _fill_tx, _fill_ty, _fill_n

MAP_HI = 0xD0
LUT_HI = 0xD4

    .area _DATA
count:   .ds 1

    .area _CODE

; void room_fill_row(void)
_room_fill_row::
    ld a, (_fill_ty)
    ld c, a
    and a, #1
    add a, a
    ld (q_row), a         ; quadrant row bit, as lut index bit 1
    ld a, c
    srl a
    and a, #31            ; by
    ld c, a
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld (map_hi), a        ; high byte of the row's address
    ld a, c
    and a, #7
    swap a
    add a, a
    ld c, a               ; c = low byte of the row's address ((by & 7) << 5)
    ld a, (_fill_dst)
    ld e, a
    ld a, (_fill_dst + 1)
    ld d, a               ; de -> buffer
    ld a, (_fill_tx)
    ld b, a               ; b = tx
    ld a, (_fill_n)
    ld (count), a
1$:
    ld a, (map_hi)
    ld h, a
    ld a, b
    srl a
    and a, #31
    or a, c
    ld l, a
    ld a, (hl)            ; block type
    add a, a
    add a, a
    ld l, a
    ld a, b
    and a, #1
    or a, l
    ld l, a
    ld a, (q_row)
    or a, l
    ld l, a
    ld h, #LUT_HI
    ld a, (hl)            ; tile
    ld (de), a
    inc de
    inc b
    ld hl, #count
    dec (hl)
    jr nz, 1$
    ret

; void room_fill_column(void)
_room_fill_column::
    ld a, (_fill_tx)
    ld c, a
    and a, #1
    ld (q_col), a         ; quadrant column bit, as lut index bit 0
    ld a, c
    srl a
    and a, #31
    ld (bx), a
    ld a, (_fill_dst)
    ld e, a
    ld a, (_fill_dst + 1)
    ld d, a
    ld a, (_fill_ty)
    ld b, a               ; b = ty
    ld a, (_fill_n)
    ld (count), a
2$:
    ld a, b
    srl a
    and a, #31            ; by
    ld c, a
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld h, a
    ld a, c
    and a, #7
    swap a
    add a, a
    ld l, a
    ld a, (bx)
    or a, l
    ld l, a
    ld a, (hl)            ; block type
    add a, a
    add a, a
    ld l, a
    ld a, b
    and a, #1
    add a, a
    or a, l
    ld l, a
    ld a, (q_col)
    or a, l
    ld l, a
    ld h, #LUT_HI
    ld a, (hl)
    ld (de), a
    inc de
    inc b
    ld hl, #count
    dec (hl)
    jr nz, 2$
    ret

    .area _DATA
q_row:   .ds 1
q_col:   .ds 1
map_hi:  .ds 1
bx:      .ds 1
