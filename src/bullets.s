; Enemy bullets, once per frame: move, collide and draw in one pass (actor.c calls
; bullets_update after the camera has moved, and handles what a hit means).
;
; Each bullet is a 13-byte record (the Bullet struct in actor.h). Per axis the
; fields are velocity low, high, sign, then position fraction, low, high, so a
; move is one straight walk through the record with the carry flowing along.
; Separate passes with parallel arrays cost ~2300 cycles per bullet; this is
; roughly a third of that.

    .module bullets

    .globl _bullets, _bullet_count, _bullets_active
    .globl _bullet_hb_x, _bullet_hb_y, _bullet_hittable, _bullet_parity
    .globl _bullet_hit, _bullet_hit_x, _bullet_hit_y
    .globl _bullet_draw_start, _bullet_draw_tile, _bullet_draw_prop
    .globl _cam_x, _cam_y, _oam_next, _shadow_OAM

BULLET_MAX = 20     ; must match actor.h
B_SIZE = 13         ; sizeof(Bullet): alive 0, vx 1-2, vx_sign 3, xs 4, x 5-6,
                    ;                 vy 7-8, vy_sign 9, ys 10, y 11-12
BLK_SOLID = 1       ; must match room.h
BLK_RAISED = 3
MAP_HI = 0xD4       ; room map at 0xD400 (room.h ROOM_MAP_ADDR), rows of 32 blocks

    .area _DATA
left:       .ds 1   ; slots left to visit
cam8_x:     .ds 2   ; cam_x - 8 and cam_y - 16: hardware coordinate = position - these
cam16_y:    .ds 2

    .area _CODE

; One axis, inline. in: hl -> the axis's velocity low byte. Reads velocity low,
; high, sign into c, b, e, then adds them to the position fraction, low, high with
; the carry flowing along. out: hl -> the field after the axis, bc = new position.
.macro MOVE_AXIS
    ld a, (hl+)
    ld c, a
    ld a, (hl+)
    ld b, a
    ld a, (hl+)
    ld e, a
    ld a, (hl)
    add a, c
    ld (hl+), a         ; fraction
    ld a, (hl)
    adc a, b
    ld (hl+), a         ; position low
    ld c, a
    ld a, (hl)
    adc a, e
    ld (hl+), a         ; position high
    ld b, a
.endm

; ---------------------------------------------------------------------------
; void bullets_update(void)
;   in:  bullet_hb_x/_y (player hitbox top-left), bullet_hittable, bullet_parity,
;        bullet_draw_start (rotating first record),
;        bullet_draw_tile/_prop, cam_x/_y, oam_next
;   out: bullets_active, bullet_count (decremented per removal), oam_next, and
;        when a bullet hits the player: bullet_hit = 1, bullet_hit_x/_y = its center
_bullets_update::
    ; hardware x = x + 8 - cam_x = x - (cam_x - 8); likewise y with 16
    ld a, (_cam_x)
    sub a, #8
    ld (cam8_x), a
    ld a, (_cam_x + 1)
    sbc a, #0
    ld (cam8_x + 1), a
    ld a, (_cam_y)
    sub a, #16
    ld (cam16_y), a
    ld a, (_cam_y + 1)
    sbc a, #0
    ld (cam16_y + 1), a
    xor a, a
    ld (_bullets_active), a
    ld a, #BULLET_MAX
    ld (left), a
    ld a, (_bullet_draw_start)
    ld l, a
    ld a, (_bullet_draw_start + 1)
    ld h, a
1$:
    ld a, (hl+)
    or a, a
    jr nz, 2$
    ld de, #B_SIZE - 1      ; free slot: on to the next record
    add hl, de
    jp 9$
2$:
    MOVE_AXIS               ; x
    push bc
    MOVE_AXIS               ; y
    ld d, b
    ld e, c                 ; de = y
    pop bc                  ; bc = x
    push hl                 ; hl is now at the next record

    ; --- walls and raised blocks on alternate frames per record, the player on
    ;     the others: one check per bullet per frame ---
    ; (at 1.5 px/frame a bullet can't cross a 16-px block between checks)
    ; Which check is by record: 13-byte records alternate odd and even
    ; addresses, so bit 0 of the address picks the half (hl -> next record).
    ld a, (_bullet_parity)
    xor a, l
    rra
    jr c, 3$
    ; Rooms are under 512 px, so a coordinate >> 4 is (high bit 0) << 4 | low >> 4:
    ; rotate bit 0 of the high byte in through the carry, then shift.
    ; block column = (x + 4) >> 4
    ld a, c
    add a, #4
    ld l, a
    ld a, b
    adc a, #0
    rra
    ld a, l
    rra
    srl a
    srl a
    srl a
    ld l, a
    ; block row = (y + 8) >> 4
    ld a, e
    add a, #8
    ld h, a
    ld a, d
    adc a, #0
    rra
    ld a, h
    rra
    srl a
    srl a
    srl a
    ld h, a
    ; hl = 0xD400 | row << 5 | column
    swap a
    add a, a
    and a, #0xE0
    or a, l
    ld l, a
    ld a, h
    srl a
    srl a
    srl a
    add a, #MAP_HI
    ld h, a
    ld a, (hl)
    cp a, #BLK_SOLID
    jp z, 8$
    cp a, #BLK_RAISED
    jp z, 8$
    jr 6$                   ; walls this frame, the player next frame
3$:
    ; --- the player, on the other frames: hitboxes overlap when
    ;     0 <= x + 6 - hb_x < 15 and 0 <= y + 10 - hb_y < 13 (bullet box 6x6 at
    ;     (1, 5); player box 10x8). At 1.5 px/frame a bullet can't cross the
    ;     player's 8-px hitbox between checks. ---
    ld a, (_bullet_hittable)
    or a, a
    jr z, 6$
    ; dx = x + 6 - hb_x: compute x - hb_x; the overlap needs it in -6..8, so the
    ; high byte must be 0x00 or 0xFF, which rejects most bullets right away.
    ld a, (_bullet_hb_x)
    ld l, a
    ld a, c
    sub a, l
    ld l, a
    ld a, (_bullet_hb_x + 1)
    ld h, a
    ld a, b
    sbc a, h
    inc a                   ; 0x00 or 0xFF -> 0x01 or 0x00
    cp a, #2
    jr nc, 6$
    dec a
    ld h, a
    ld a, l
    add a, #6
    ld l, a
    ld a, h
    adc a, #0
    jr nz, 6$
    ld a, l
    cp a, #15
    jr nc, 6$
    ld a, (_bullet_hb_y)
    ld l, a
    ld a, (_bullet_hb_y + 1)
    ld h, a
    ld a, e
    sub a, l
    ld l, a
    ld a, d
    sbc a, h
    ld h, a
    ld a, l
    add a, #10
    ld l, a
    ld a, h
    adc a, #0
    jr nz, 6$
    ld a, l
    cp a, #13
    jr nc, 6$
    ; Hit: report the bullet's center, allow one hit per frame, remove the bullet.
    ld a, c
    add a, #4
    ld (_bullet_hit_x), a
    ld a, b
    adc a, #0
    ld (_bullet_hit_x + 1), a
    ld a, e
    add a, #8
    ld (_bullet_hit_y), a
    ld a, d
    adc a, #0
    ld (_bullet_hit_y + 1), a
    ld a, #1
    ld (_bullet_hit), a
    xor a, a
    ld (_bullet_hittable), a
    jp 8$

6$:
    ; --- still alive: count it and draw it if visible ---
    ld hl, #_bullets_active
    inc (hl)
    ; hardware x = x - (cam_x - 8), visible while < 168
    ld a, (cam8_x)
    ld l, a
    ld a, c
    sub a, l
    ld c, a
    ld a, (cam8_x + 1)
    ld l, a
    ld a, b
    sbc a, l
    jr nz, 7$
    ld a, c
    cp a, #168
    jr nc, 7$               ; c = hardware x
    ; hardware y = y - (cam_y - 16), visible while < 144 (view 128 + 16)
    ld a, (cam16_y)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (cam16_y + 1)
    ld l, a
    ld a, d
    sbc a, l
    jr nz, 7$
    ld a, e
    cp a, #144
    jr nc, 7$
    ld b, a                 ; b = hardware y
    ld a, (_oam_next)
    cp a, #40
    jr nc, 7$               ; OAM full
    add a, a
    add a, a
    ld l, a
    ld h, #>(_shadow_OAM)   ; shadow_OAM is 256-aligned (0xC000)
    ld a, b
    ld (hl+), a
    ld a, c
    ld (hl+), a
    ld a, (_bullet_draw_tile)
    ld (hl+), a
    ld a, (_bullet_draw_prop)
    ld (hl), a
    ld hl, #_oam_next
    inc (hl)
    jr 7$

8$:
    pop hl                  ; remove: alive = 0
    push hl
    ld de, #-B_SIZE
    add hl, de
    ld (hl), #0
    ld hl, #_bullet_count
    dec (hl)
7$:
    pop hl                  ; the next record
9$:
    ; wrap from the end of the pool to the first record
    ld a, l
    cp a, #<(_bullets + BULLET_MAX * B_SIZE)
    jr nz, 10$
    ld a, h
    cp a, #>(_bullets + BULLET_MAX * B_SIZE)
    jr nz, 10$
    ld hl, #_bullets
10$:
    ld a, (left)
    dec a
    ld (left), a
    jp nz, 1$
    ret
