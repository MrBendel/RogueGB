; Auto-aim's per-frame work in bank 0, called from aim.c: scoring one enemy,
; picking the target, drawing the reticle.
;
; Scoring one enemy (aim.c calls it once a frame, round-robin). In
; assembly because the compiled version spilled nearly every 16-bit value to the
; stack (~3,500 cycles a call).
;
; uint8_t aim_score_of(uint8_t i): i in a, result in a. The score is the
; octagonal distance from the shot's start (player + 8, + 8) to the middle of the
; enemy's hitbox (enemy + 8, + 11): max(|dx|, |dy|) + min / 2, less aim_bias if
; the enemy is in front of the player (within 90 degrees of facing). 0xFF
; (AIM_NONE) if the slot is empty, the enemy is down, out of aim_range, or out of
; line of sight (room_line_clear).

    .module aim_score

    .globl _actor_type, _actor_hp, _actor_x, _actor_y
    .globl _player_x, _player_y, _player_facing
    .globl _aim_range, _aim_bias
    .globl _line_fx, _line_fy, _line_sx, _line_sy, _line_n, _room_line_clear
    .globl _aim_score, _aim_target, _aim_sticky
    .globl _cam_x, _cam_y, _frame_count, _shadow_OAM, _reticle_vram, _reticle_frames

AIM_NONE = 0xFF
ENEMY_DY = 3        ; (enemy + 11) - (player + 8); x offsets cancel (8 - 8)

    .area _DATA
dx:     .ds 2
dy:     .ds 2
ax:     .ds 1       ; |dx|, |dy| (8-bit once in range)
ay:     .ds 1
amax:   .ds 1
score:  .ds 1

    .area _CODE

; Facing (DIR_UP = 0, clockwise) as signs: 1, 0 or -1 (0xFF).
facing_x: .db 0, 1, 1, 1, 0, 0xFF, 0xFF, 0xFF
facing_y: .db 0xFF, 0xFF, 0, 1, 1, 1, 0, 0xFF

; hl = arr + c
.macro IDX8 arr
    ld hl, #arr
    ld a, l
    add a, c
    ld l, a
    adc a, h
    sub a, l
    ld h, a
.endm

; hl = arr + b
.macro IDX16 arr
    ld hl, #arr
    ld a, l
    add a, b
    ld l, a
    adc a, h
    sub a, l
    ld h, a
.endm

_aim_score_of::
    ld c, a
    IDX8 _actor_type
    ld a, (hl)
    or a, a
    jp z, 90$
    IDX8 _actor_hp
    ld a, (hl)
    or a, a
    jp z, 90$
    ld a, c
    add a, a
    ld b, a
    ; dx = actor_x - player_x
    IDX16 _actor_x
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_player_x
    ld a, e
    sub a, (hl)
    ld e, a
    inc hl
    ld a, d
    sbc a, (hl)
    ld d, a
    ld hl, #dx
    ld a, e
    ld (hl+), a
    ld (hl), d
    call abs_in_range
    jp c, 90$
    ld (ax), a
    ; dy = actor_y - player_y + ENEMY_DY
    IDX16 _actor_y
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_player_y
    ld a, e
    sub a, (hl)
    ld e, a
    inc hl
    ld a, d
    sbc a, (hl)
    ld d, a
    ld a, e
    add a, #ENEMY_DY
    ld e, a
    ld a, d
    adc a, #0
    ld d, a
    ld hl, #dy
    ld a, e
    ld (hl+), a
    ld (hl), d
    call abs_in_range
    jp c, 90$
    ld (ay), a

    ; distance = max + min / 2 (fits 8 bits: range <= 170)
    ld b, a                 ; b = ay
    ld a, (ax)
    cp a, b
    jr nc, 10$
    ld c, a                 ; ax < ay: max = ay, min = ax
    ld a, b
    ld b, c
    jr 11$
10$:                        ; ax >= ay: max = ax (a), min = ay (b)
11$:
    ld (amax), a
    srl b
    add a, b
    jp c, 90$
    ld hl, #_aim_range
    cp a, (hl)
    jr z, 12$
    jp nc, 90$
12$:
    ld (score), a

    ; In front? dot = fx * dx + fy * dy > 0, with fx, fy in {-1, 0, 1}.
    ld a, (_player_facing)
    and a, #7
    ld c, a
    ld de, #0               ; de = dot
    IDX8 facing_x
    ld a, (hl)
    ld hl, #dx
    call add_signed
    ld a, (_player_facing)
    and a, #7
    ld c, a
    IDX8 facing_y
    ld a, (hl)
    ld hl, #dy
    call add_signed
    ld a, d
    bit 7, a
    jr nz, 20$              ; negative: behind
    or a, e
    jr z, 20$               ; 0: square to the side
    ld a, (_aim_bias)
    ld b, a
    ld a, (score)
    sub a, b
    jr nc, 13$
    xor a, a
13$:
    ld (score), a
20$:
    ; Line of sight, unless it's within a block (the ends are open floor).
    ld a, (amax)
    cp a, #17
    jr c, 40$
    ; start = (player + 8) << 4, in 12.4
    ld hl, #_player_x
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    ld de, #8
    add hl, de
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld a, l
    ld (_line_fx), a
    ld a, h
    ld (_line_fx + 1), a
    ld hl, #_player_y
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    add hl, de
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld a, l
    ld (_line_fy), a
    ld a, h
    ld (_line_fy + 1), a
    ; Split the line into 2, 4 or 8 parts (steps of at most 16 px): step =
    ; d << (4 - log2 parts) in 12.4, and parts - 1 samples.
    ld a, (amax)
    ld b, #3                ; shift for 2 parts (d / 2 = d << 3 in 12.4)
    ld c, #1
    cp a, #33
    jr c, 30$
    ld b, #2                ; 4 parts
    ld c, #3
    cp a, #65
    jr c, 30$
    ld b, #1                ; 8 parts
    ld c, #7
30$:
    ld a, c
    ld (_line_n), a
    ld hl, #dx
    ld a, (hl+)
    ld h, (hl)
    ld l, a
    ld c, b
31$:
    add hl, hl
    dec c
    jr nz, 31$
    ld a, l
    ld (_line_sx), a
    ld a, h
    ld (_line_sx + 1), a
    ld hl, #dy
    ld a, (hl+)
    ld h, (hl)
    ld l, a
32$:
    add hl, hl
    dec b
    jr nz, 32$
    ld a, l
    ld (_line_sy), a
    ld a, h
    ld (_line_sy + 1), a
    call _room_line_clear
    or a, a
    jr z, 90$
40$:
    ld a, (score)
    ret
90$:
    ld a, #AIM_NONE
    ret

; de += (hl) * sign a, for the 16-bit value at hl and a in {1, 0, 0xFF}.
add_signed:
    or a, a
    ret z
    inc a
    jr z, 1$
    ld a, (hl+)             ; +1: de += v
    add a, e
    ld e, a
    ld a, (hl)
    adc a, d
    ld d, a
    ret
1$:
    ld a, e                 ; -1: de -= v
    sub a, (hl)
    ld e, a
    inc hl
    ld a, d
    sbc a, (hl)
    ld d, a
    ret

; a = |de| with carry clear if it's at most aim_range; carry set if not.
abs_in_range:
    ld a, d
    or a, a
    jr z, 1$
    inc a
    jr nz, 9$               ; high byte not 0 or 0xFF: far out
    xor a, a                ; negative: a = -e (e = 0 would be -256)
    sub a, e
    jr z, 9$
    jr 2$
1$:
    ld a, e
2$:
    ld hl, #_aim_range
    cp a, (hl)
    jr c, 3$
    jr nz, 9$
3$:
    or a, a                 ; clear carry
    ret
9$:
    scf
    ret

; ---------------------------------------------------------------------------
; void aim_select(void): the best (lowest) score; it becomes the target if there
; is none, the target can't be seen any more, or it's better than the target's
; score by more than aim_sticky.
_aim_select::
    ld hl, #_aim_score
    ld b, #AIM_NONE         ; best score
    ld c, #AIM_NONE         ; best slot
    ld e, #0
1$:
    ld a, (hl+)
    cp a, b
    jr nc, 2$
    ld b, a
    ld c, e
2$:
    inc e
    ld a, e
    cp a, #8
    jr nz, 1$
    ld a, (_aim_target)
    cp a, #AIM_NONE
    jr z, 8$
    ld hl, #_aim_score      ; d = the target's score
    add a, l
    ld l, a
    adc a, h
    sub a, l
    ld h, a
    ld d, (hl)
    ld a, d
    cp a, #AIM_NONE
    jr z, 8$
    ld a, (_aim_sticky)     ; switch if best + sticky < target's
    add a, b
    ret c
    cp a, d
    ret nc
8$:
    ld a, c
    ld (_aim_target), a
    ret

; ---------------------------------------------------------------------------
; void aim_draw_reticle(void): hardware sprites 9 and 10 around the target's
; 16x16 frame (pulsing every 8 frames), or hidden.
RETICLE_OAM = 9       ; OAM_RETICLE in oam.h
VIEW_H = 128
_aim_draw_reticle::
    ld a, (_aim_target)
    cp a, #AIM_NONE
    jp z, 9$
    add a, a
    ld b, a
    ; hardware x = actor_x - cam_x + 8, on screen while < 168
    IDX16 _actor_x
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_cam_x
    ld a, e
    sub a, (hl)
    ld e, a
    inc hl
    ld a, d
    sbc a, (hl)
    ld d, a
    ld a, e
    add a, #8
    ld e, a
    ld a, d
    adc a, #0
    jp nz, 9$
    ld a, e
    cp a, #168
    jp nc, 9$
    ld c, e                 ; c = hardware x
    ; hardware y = actor_y - cam_y + 16, on screen while < VIEW_H + 16
    IDX16 _actor_y
    ld a, (hl+)
    ld e, a
    ld d, (hl)
    ld hl, #_cam_y
    ld a, e
    sub a, (hl)
    ld e, a
    inc hl
    ld a, d
    sbc a, (hl)
    ld d, a
    ld a, e
    add a, #16
    ld e, a
    ld a, d
    adc a, #0
    jp nz, 9$
    ld a, e
    cp a, #VIEW_H + 16
    jp nc, 9$
    ld b, e                 ; b = hardware y
    ; frame: ((frame_count >> 3) & 1) * 4 bytes into reticle_frames
    ld a, (_frame_count)
    rrca
    and a, #4
    ld hl, #_reticle_frames
    add a, l
    ld l, a
    adc a, h
    sub a, l
    ld h, a
    ld d, h
    ld e, l                 ; de -> left column, left flags, right column, right flags
    ld hl, #_shadow_OAM + RETICLE_OAM * 4
    ld a, b
    ld (hl+), a
    ld a, c
    ld (hl+), a
    ld a, (de)
    inc de
    add a, a
    push hl
    ld hl, #_reticle_vram
    add a, (hl)
    pop hl
    ld (hl+), a
    ld a, (de)
    inc de
    ld (hl+), a
    ld a, b
    ld (hl+), a
    ld a, c
    add a, #8
    ld (hl+), a
    ld a, (de)
    inc de
    add a, a
    push hl
    ld hl, #_reticle_vram
    add a, (hl)
    pop hl
    ld (hl+), a
    ld a, (de)
    ld (hl), a
    ret
9$:
    xor a, a
    ld (_shadow_OAM + RETICLE_OAM * 4), a
    ld (_shadow_OAM + RETICLE_OAM * 4 + 4), a
    ret
