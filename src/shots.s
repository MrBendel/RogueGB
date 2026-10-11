; The player's shots, once per frame: age, move, collide and draw in one pass
; (actor.c calls shots_update from actors_update, after the camera has moved).
; Same record and movement as enemy bullets (bullets.s, Bullet in actor.h); here
; `alive` counts the frames of flight left.
;
; Walls and enemies are checked on alternate frames per record (bit 0 of the
; record's address, which alternates since records are 13 bytes). At 3 px/frame a
; shot moves 6 px between checks, less than a wall (16) or the overlap window with
; an enemy (17 x 15), so it can't pass through either.
;
; Enemies: a table of the live ones (adjusted top-left, slot) is built first, so
; each check is a short walk through it. A hit takes 1 HP from the enemy, sets
; its hurt timer, and sets shots_killed when that was its last HP; actors_update
; removes it.

    .module shots

    .globl _shots, _shot_count, _shots_hit, _shots_killed, _shot_parity
    .globl _shot_draw_tile, _shot_draw_prop
    .globl _actor_type, _actor_hp, _actor_x, _actor_y, _actor_hurt
    .globl _cam_x, _cam_y, _oam_next, _shadow_OAM

SHOT_MAX = 6        ; must match aim.h
B_SIZE = 13         ; sizeof(Bullet): alive 0, vx 1-2, vx_sign 3, xs 4, x 5-6,
                    ;                 vy 7-8, vy_sign 9, ys 10, y 11-12
ACTOR_MAX = 8       ; actor.h
HURT_STEPS = 4      ; think steps an enemy flashes when hit (actor.c)
BLK_SOLID = 1       ; room.h
BLK_RAISED = 3
MAP_HI = 0xD0       ; room map at 0xD000, rows of 32 blocks
E_SIZE = 5          ; enemy table entry: x - 4 (2 bytes), y - 4 (2), slot

    .area _DATA
left:       .ds 1   ; records left to visit
e_count:    .ds 1   ; live enemies in the table
e_left:     .ds 1
dlo:        .ds 1   ; low byte of a difference
cam8_x:     .ds 2   ; cam_x - 8 and cam_y - 16: hardware coordinate = position - these
cam16_y:    .ds 2
etab:       .ds E_SIZE * ACTOR_MAX

    .area _CODE

; One axis, inline (as in bullets.s). in: hl -> the axis's velocity low byte.
; out: hl -> the field after the axis, bc = new position.
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

; hl = arr + c (byte index c)
.macro IDX8 arr
    ld hl, #arr
    ld a, l
    add a, c
    ld l, a
    adc a, h
    sub a, l
    ld h, a
.endm

; ---------------------------------------------------------------------------
; void shots_update(void)
;   in:  shots[], shot_parity, shot_draw_tile/_prop, cam_x/_y, oam_next, actors
;   out: shot_count (decremented per removal), oam_next, shots_hit, shots_killed,
;        actor_hp/actor_hurt of enemies hit
_shots_update::
    ; ---- table of live enemies: (x - 4, y - 4, slot) ----
    ; A shot (6x6 box at +1,+5) overlaps an enemy (12x10 box at +2,+6) when
    ; 0 <= shot x - (enemy x - 4) < 17 and 0 <= shot y - (enemy y - 4) < 15.
    ld de, #etab
    xor a, a
    ld (e_count), a
    ld c, a                 ; c = slot
1$:
    IDX8 _actor_type
    ld a, (hl)
    or a, a
    jr z, 2$
    IDX8 _actor_hp
    ld a, (hl)
    or a, a
    jr z, 2$
    ld a, c
    add a, a
    ld b, a                 ; b = byte offset of a 16-bit element
    ld hl, #_actor_x
    call add_b
    ld a, (hl+)
    sub a, #4
    ld (de), a
    inc de
    ld a, (hl)
    sbc a, #0
    ld (de), a
    inc de
    ld hl, #_actor_y
    call add_b
    ld a, (hl+)
    sub a, #4
    ld (de), a
    inc de
    ld a, (hl)
    sbc a, #0
    ld (de), a
    inc de
    ld a, c
    ld (de), a
    inc de
    ld hl, #e_count
    inc (hl)
2$:
    inc c
    ld a, c
    cp a, #ACTOR_MAX
    jr nz, 1$

    ; ---- camera offsets for drawing ----
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

    ld a, #SHOT_MAX
    ld (left), a
    ld hl, #_shots
10$:
    ld a, (hl)
    or a, a
    jr nz, 11$
    ld de, #B_SIZE          ; free slot
    add hl, de
    jp 90$
11$:
    dec a                   ; one frame less to fly
    ld (hl+), a
    jr nz, 12$
    ld de, #B_SIZE - 1      ; out of range: free the slot
    add hl, de
    ld a, (_shot_count)
    dec a
    ld (_shot_count), a
    jp 90$
12$:
    MOVE_AXIS               ; x
    push bc
    MOVE_AXIS               ; y
    ld d, b
    ld e, c                 ; de = y
    pop bc                  ; bc = x
    push hl                 ; hl is now at the next record

    ; walls on alternate frames per record, enemies on the others
    ld a, (_shot_parity)
    xor a, l
    rra
    jr c, 30$
    ; block column = (x + 4) >> 4, block row = (y + 8) >> 4 (see bullets.s)
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
    jp z, 80$
    cp a, #BLK_RAISED
    jp z, 80$
    jr 60$

30$:
    ; ---- enemies ----
    ld a, (e_count)
    or a, a
    jr z, 60$
    ld (e_left), a
    ld hl, #etab
31$:
    ; dx = x - (enemy x - 4): the high byte must be 0 and the low byte < 17
    ld a, c
    sub a, (hl)
    inc hl
    ld (dlo), a
    ld a, b
    sbc a, (hl)
    inc hl
    jr nz, 33$
    ld a, (dlo)
    cp a, #17
    jr nc, 33$
    ; dy = y - (enemy y - 4): high byte 0, low byte < 15
    ld a, e
    sub a, (hl)
    inc hl
    ld (dlo), a
    ld a, d
    sbc a, (hl)
    inc hl
    jr nz, 34$
    ld a, (dlo)
    cp a, #15
    jr c, 40$               ; hit
    jr 34$
33$:
    inc hl                  ; skip the y bytes
    inc hl
34$:
    inc hl                  ; skip the slot
    ld a, (e_left)
    dec a
    ld (e_left), a
    jr nz, 31$
    jr 60$

40$:
    ; ---- hit enemy (hl) -> slot ----
    ld c, (hl)
    IDX8 _actor_hp
    ld a, (hl)
    or a, a
    jr z, 41$               ; already down this frame
    dec a
    ld (hl), a
    jr nz, 41$
    ld a, #1
    ld (_shots_killed), a
41$:
    IDX8 _actor_hurt
    ld (hl), #HURT_STEPS
    ld hl, #_shots_hit
    inc (hl)
    jr nz, 80$
    inc hl
    inc (hl)
    jr 80$

60$:
    ; ---- still flying: draw it if visible ----
    ld a, (cam8_x)
    ld l, a
    ld a, c
    sub a, l
    ld c, a
    ld a, (cam8_x + 1)
    ld l, a
    ld a, b
    sbc a, l
    jr nz, 70$
    ld a, c
    cp a, #168
    jr nc, 70$              ; c = hardware x
    ld a, (cam16_y)
    ld l, a
    ld a, e
    sub a, l
    ld e, a
    ld a, (cam16_y + 1)
    ld l, a
    ld a, d
    sbc a, l
    jr nz, 70$
    ld a, e
    cp a, #144
    jr nc, 70$
    ld b, a                 ; b = hardware y
    ld a, (_oam_next)
    cp a, #40
    jr nc, 70$              ; OAM full
    add a, a
    add a, a
    ld l, a
    ld h, #>(_shadow_OAM)   ; shadow_OAM is 256-aligned (0xC000)
    ld a, b
    ld (hl+), a
    ld a, c
    ld (hl+), a
    ld a, (_shot_draw_tile)
    ld (hl+), a
    ld a, (_shot_draw_prop)
    ld (hl), a
    ld hl, #_oam_next
    inc (hl)
    jr 70$

80$:
    pop hl                  ; remove: alive = 0
    push hl
    ld de, #-B_SIZE
    add hl, de
    ld (hl), #0
    ld hl, #_shot_count
    dec (hl)
70$:
    pop hl                  ; the next record
90$:
    ld a, (left)
    dec a
    ld (left), a
    jp nz, 10$
    ret

; hl += b
add_b:
    ld a, l
    add a, b
    ld l, a
    adc a, h
    sub a, l
    ld h, a
    ret
