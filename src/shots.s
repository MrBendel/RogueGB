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
; each check is a short walk through it. A hit takes shot_damage HP, sets
; its hurt timer, and sets shots_killed when that was its last HP; actors_update
; removes it.

    .module shots

    .globl _shots, _shot_count, _shots_hit, _shots_killed, _shot_parity, _shot_damage
    .globl _shot_draw_tile, _shot_draw_prop
    .globl _actor_type, _actor_hp, _actor_x, _actor_y, _actor_hurt
    .globl _cam_x, _cam_y, _oam_next, _shadow_OAM

SHOT_MAX = 16       ; must match aim.h
B_SIZE = 13         ; sizeof(Bullet): alive 0, vx 1-2, vx_sign 3, xs 4, x 5-6,
                    ;                 vy 7-8, vy_sign 9, ys 10, y 11-12
ACTOR_MAX = 8       ; actor.h
HURT_STEPS = 4      ; think steps an enemy flashes when hit (actor.c)
BLK_SOLID = 1       ; room.h
BLK_RAISED = 3
MAP_HI = 0xD4       ; room map at 0xD400 (room.h ROOM_MAP_ADDR), rows of 32 blocks
E_SIZE = 5          ; enemy table entry: x - 4 (2 bytes), y - 4 (2), slot

    .area _DATA
left:       .ds 1   ; records left to visit
live_left:  .ds 1   ; live shots not yet visited: stop when it reaches 0
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

    ld a, (_shot_count)
    ld (live_left), a
    ld a, #SHOT_MAX
    ld (left), a
    ld hl, #_shots
10$:
    ld a, (hl)
    or a, a
    jr nz, 11$
    ld de, #B_SIZE          ; free slot
    add hl, de
    jp 91$
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
    ld b, a                 ; (c is the enemy slot, still needed; de is free now)
    ld a, (_shot_damage)
    ld e, a
    ld a, b
    sub a, e                ; hp - damage, stopping at 0
    jr nc, 42$
    xor a, a
42$:
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
    ld a, (live_left)       ; a live shot done: the rest of the pool may be empty
    dec a
    ld (live_left), a
    ret z
91$:
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

; ---------------------------------------------------------------------------
; void shots_spawn(void): fire spawn_n shots (1 or more) into free slots of the
; pool (fewer if it's full). Each starts at (spawn_x, spawn_y) with 1/2-px
; fractions, flies spawn_life frames, and takes its velocity from the tables at
; spawn_vx / spawn_vy (32 int16 entries each) for its direction: spawn_dir for the
; first, then + spawn_spread for each next one (a fan). Updates shot_count and
; shots_fired. aim.c sets these up (fire()); this is the per-pellet work, which
; cost ~2,200 cycles a pellet compiled.
    .globl _spawn_n, _spawn_x, _spawn_y, _spawn_dir, _spawn_spread, _spawn_life
    .globl _spawn_vx, _spawn_vy, _shots_fired

    .area _DATA
_spawn_n::      .ds 1
_spawn_x::      .ds 2
_spawn_y::      .ds 2
_spawn_dir::    .ds 1
_spawn_spread:: .ds 1
_spawn_life::   .ds 1
_spawn_vx::     .ds 2   ; int16_t * (32 entries)
_spawn_vy::     .ds 2
sp_slots:       .ds 1   ; pool slots left to look at
sp_rec:         .ds 2   ; the record being written

    .area _CODE
_shots_spawn::
    ld a, #SHOT_MAX
    ld (sp_slots), a
    ld hl, #_shots
1$:
    ld a, (hl)
    or a, a
    jr z, 3$                ; a free slot
2$:
    ld de, #B_SIZE
    add hl, de
    ld a, (sp_slots)
    dec a
    ld (sp_slots), a
    jr nz, 1$
    ret                     ; pool full
3$:
    ld a, l
    ld (sp_rec), a
    ld a, h
    ld (sp_rec + 1), a
    ld a, (_spawn_life)
    ld (hl+), a             ; alive = frames to fly
    ; de = 2 * direction: the offset into the velocity tables
    ld a, (_spawn_dir)
    and a, #31
    add a, a
    ld e, a
    ld d, #0
    ; x axis: vx (2), its sign (1), fraction (1), x (2)
    push hl
    ld a, (_spawn_vx)
    ld l, a
    ld a, (_spawn_vx + 1)
    ld h, a
    add hl, de
    ld a, (hl+)
    ld c, a
    ld b, (hl)              ; bc = vx
    pop hl
    call 20$
    ld a, (_spawn_x)
    ld (hl+), a
    ld a, (_spawn_x + 1)
    ld (hl+), a
    ; y axis: the same layout
    push hl
    ld a, (_spawn_vy)
    ld l, a
    ld a, (_spawn_vy + 1)
    ld h, a
    add hl, de
    ld a, (hl+)
    ld c, a
    ld b, (hl)              ; bc = vy
    pop hl
    call 20$
    ld a, (_spawn_y)
    ld (hl+), a
    ld a, (_spawn_y + 1)
    ld (hl), a
    ; counted
    ld hl, #_shot_count
    inc (hl)
    ld hl, #_shots_fired
    inc (hl)
    jr nz, 4$
    inc hl
    inc (hl)
4$:
    ; the next pellet, if any: the next direction of the fan
    ld a, (_spawn_n)
    dec a
    ld (_spawn_n), a
    ret z
    ld a, (_spawn_spread)
    ld b, a
    ld a, (_spawn_dir)
    add a, b
    ld (_spawn_dir), a
    ld a, (sp_rec)
    ld l, a
    ld a, (sp_rec + 1)
    ld h, a
    jr 2$                   ; on from this record

; (hl+) <- velocity bc (low, high), its sign byte (0 or 0xFF), a 1/2-px fraction
20$:
    ld a, c
    ld (hl+), a
    ld a, b
    ld (hl+), a
    rlca                    ; the sign bit into carry: 0x00 or 0xFF
    sbc a, a
    ld (hl+), a
    ld a, #0x80
    ld (hl+), a
    ret

; ---------------------------------------------------------------------------
; void melee_scan(void): which enemies and enemy bullets are inside the sword's
; hit box this frame (aim.c's swing_hits acts on them; it cost ~15,000 cycles a
; frame compiled). In: the box's left/top, shifted per kind so the test is on the
; actor's own coordinates: melee_ex = left - 8, melee_ey = top - 11 (an enemy's
; hitbox middle is at +8, +11); melee_bx = left - 4, melee_by = top - 8 (a
; bullet's at +4, +8). Inside: 0 <= x - ex < MELEE_W and 0 <= y - ey < MELEE_H.
; Out: melee_enemies (a bit per live enemy slot inside), melee_bullets (3 bytes,
; a bit per live bullet slot inside, slot 0 = bit 0 of the first byte).
MELEE_W = 28        ; 2 * SWING_HALF_W in aim.c
MELEE_H = 24        ; 2 * SWING_HALF_H
BULLET_MAX = 20     ; actor.h
    .globl _melee_ex, _melee_ey, _melee_bx, _melee_by, _melee_enemies, _melee_bullets
    .globl _bullets, _bullet_count

    .area _DATA
_melee_ex::      .ds 2
_melee_ey::      .ds 2
_melee_bx::      .ds 2
_melee_by::      .ds 2
_melee_enemies:: .ds 1
_melee_bullets:: .ds 3
ms_bit:          .ds 1
ms_left:         .ds 1
ms_out:          .ds 2

    .area _CODE
_melee_scan::
    xor a, a
    ld (_melee_enemies), a
    ld (_melee_bullets), a
    ld (_melee_bullets + 1), a
    ld (_melee_bullets + 2), a
    ; ---- enemies ----
    ld c, #0
    ld a, #1
    ld (ms_bit), a
1$:
    IDX8 _actor_type
    ld a, (hl)
    or a, a
    jr z, 3$
    IDX8 _actor_hp
    ld a, (hl)
    or a, a
    jr z, 3$
    ld a, c
    add a, a
    ld b, a
    ld hl, #_actor_x
    call add_b
    ld de, #_melee_ex
    call ms_inside
    jr nc, 3$
    ld hl, #_actor_y
    call add_b
    ld de, #_melee_ey
    call ms_inside_h
    jr nc, 3$
    ld a, (ms_bit)
    ld hl, #_melee_enemies
    or a, (hl)
    ld (hl), a
3$:
    ld a, (ms_bit)
    add a, a
    ld (ms_bit), a
    inc c
    ld a, c
    cp a, #ACTOR_MAX
    jr nz, 1$
    ; ---- enemy bullets ----
    ld a, (_bullet_count)
    or a, a
    ret z
    ld a, #BULLET_MAX
    ld (ms_left), a
    ld a, #1
    ld (ms_bit), a
    ld hl, #_melee_bullets
    ld a, l
    ld (ms_out), a
    ld a, h
    ld (ms_out + 1), a
    ld hl, #_bullets
5$:
    ld a, (hl)
    or a, a
    jr z, 7$
    push hl
    ld de, #5               ; x
    add hl, de
    ld de, #_melee_bx
    call ms_inside
    jr nc, 6$
    pop hl
    push hl
    ld de, #11              ; y
    add hl, de
    ld de, #_melee_by
    call ms_inside_h
    jr nc, 6$
    ld a, (ms_out)
    ld l, a
    ld a, (ms_out + 1)
    ld h, a
    ld a, (ms_bit)
    or a, (hl)
    ld (hl), a
6$:
    pop hl
7$:
    ld de, #B_SIZE
    add hl, de
    ld a, (ms_bit)          ; next bit, and the next byte every 8
    add a, a
    jr nz, 8$
    ld a, (ms_out)
    inc a
    ld (ms_out), a
    jr nz, 9$
    ld a, (ms_out + 1)
    inc a
    ld (ms_out + 1), a
9$:
    ld a, #1
8$:
    ld (ms_bit), a
    ld a, (ms_left)
    dec a
    ld (ms_left), a
    jr nz, 5$
    ret

; carry set if 0 <= (hl) - (de) < MELEE_W (16-bit values), for x; keeps bc, hl
ms_inside:
    ld a, #MELEE_W
    jr ms_test
; ... and < MELEE_H, for y
ms_inside_h:
    ld a, #MELEE_H
ms_test:
    push bc
    push hl
    ld b, a                 ; b = the limit
    ld a, (de)
    ld c, a
    inc de
    ld a, (hl+)
    sub a, c
    ld c, a                 ; c = low byte of the difference
    ld a, (de)
    ld e, a
    ld a, (hl)
    sbc a, e
    jr nz, 1$               ; high byte not 0: outside
    ld a, c
    cp a, b                 ; carry if c < limit
    pop hl
    pop bc
    ret
1$:
    pop hl
    pop bc
    or a, a                 ; a != 0 here: clears carry
    ret
