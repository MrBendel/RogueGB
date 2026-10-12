; Per-frame camera helpers for camera.c, in assembly (the compiled versions cost
; ~1,000 and ~600 cycles): the look-ahead (lead) sliding toward its goal, and
; turning the 12.4 camera position into pixels and scroll registers.
;
; void cam_slide(CamLead *l, uint16_t rate): l in de, rate (12.4 per frame,
; 1-127) in bc. CamLead (camera_internal.h): goal (0-1), lead (2-3), vel (4-5),
; int16 each, 12.4.
;   - settled (lead == goal): vel = 0;
;   - want = gap / 8 (ease out), at least LEAD_MIN toward the goal so the last
;     pixel doesn't creep, at most rate either way;
;   - vel moves toward want by at most LEAD_ACCEL a frame (ease in, no sudden turns);
;   - if vel would reach or pass the goal, stop there; else lead += vel.

    .module camera_lead

LEAD_ACCEL = 4      ; 1/4 px/frame per frame
LEAD_MIN = 3        ; 3/16 px/frame

    .area _DATA
s_rate: .ds 1
s_ptr:  .ds 2       ; l
s_gap:  .ds 2       ; goal - lead

    .area _CODE

_cam_slide::
    ld a, c
    ld (s_rate), a
    ld a, e
    ld (s_ptr), a
    ld a, d
    ld (s_ptr + 1), a
    ld h, d
    ld l, e
    ; gap = goal - lead
    ld a, (hl+)
    ld c, a
    ld a, (hl+)
    ld b, a                 ; bc = goal
    ld a, (hl+)
    ld e, a
    ld a, (hl+)
    ld d, a                 ; de = lead, hl -> vel
    ld a, c
    sub a, e
    ld c, a
    ld a, b
    sbc a, d
    ld b, a                 ; bc = gap
    or a, c
    jr nz, 1$
    xor a, a                ; settled: vel = 0
    ld (hl+), a
    ld (hl), a
    ret
1$:
    ld a, c
    ld (s_gap), a
    ld a, b
    ld (s_gap + 1), a
    ld d, b                 ; d = gap high byte (its sign)
    ; want = gap >> 3 (arithmetic)
    sra b
    rr c
    sra b
    rr c
    sra b
    rr c
    ; at least LEAD_MIN toward the goal
    bit 7, d
    jr nz, 2$
    ld a, b                 ; gap > 0, so want >= 0
    or a, a
    jr nz, 3$
    ld a, c
    cp a, #LEAD_MIN
    jr nc, 3$
    ld c, #LEAD_MIN
    jr 3$
2$:
    ld a, b                 ; gap < 0, so want <= -1
    inc a
    jr nz, 3$
    ld a, c
    cp a, #256 - LEAD_MIN
    jr c, 3$
    ld c, #256 - LEAD_MIN
3$:
    ; at most rate either way
    ld a, (s_rate)
    call clamp_bc
    ; vel += clamp(want - vel, LEAD_ACCEL)
    ld a, (s_ptr)
    add a, #4
    ld l, a
    ld a, (s_ptr + 1)
    adc a, #0
    ld h, a                 ; hl -> vel
    ld a, (hl+)
    ld e, a
    ld d, (hl)              ; de = vel
    ld a, c
    sub a, e
    ld c, a
    ld a, b
    sbc a, d
    ld b, a                 ; bc = want - vel
    ld a, #LEAD_ACCEL
    call clamp_bc
    ld a, c
    add a, e
    ld e, a
    ld a, b
    adc a, d
    ld d, a                 ; de = new vel
    ; reaching or passing the goal: gap - vel is 0 or has the other sign
    ld a, (s_gap)
    sub a, e
    ld c, a
    ld a, (s_gap + 1)
    sbc a, d
    ld b, a                 ; bc = gap - vel
    or a, c
    jr z, 8$
    ld a, (s_gap + 1)
    xor a, b
    bit 7, a
    jr nz, 8$
    ; vel = de; lead += vel
    ld (hl), d
    dec hl
    ld (hl), e
    dec hl                  ; hl -> lead high byte
    ld a, (hl-)
    ld b, a
    ld a, (hl)              ; hl -> lead low byte
    add a, e
    ld (hl+), a
    ld a, b
    adc a, d
    ld (hl), a
    ret
8$:
    ; arrive: lead = goal, vel = 0
    xor a, a
    ld (hl-), a
    ld (hl-), a             ; hl -> lead high byte
    ld a, (s_ptr)
    ld e, a
    ld a, (s_ptr + 1)
    ld d, a                 ; de -> goal
    ld a, (de)
    inc de
    ld c, a
    ld a, (de)
    ld (hl-), a
    ld (hl), c
    ret

; Clamp bc (int16) to -a..a, for a limit a in 1-127. Keeps de and hl.
clamp_bc:
    push de
    ld e, a
    bit 7, b
    jr nz, 2$
    ld a, b                 ; bc >= 0: too big if b != 0 or c > limit
    or a, a
    jr nz, 1$
    ld a, e
    cp a, c
    jr nc, 9$
1$:
    ld c, e
    ld b, #0
    jr 9$
2$:
    ld a, b                 ; bc < 0: too small if b != 0xFF or c < 256 - limit
    inc a
    jr nz, 3$
    xor a, a
    sub a, e
    ld d, a
    ld a, c
    cp a, d
    jr nc, 9$
3$:
    xor a, a
    sub a, e
    ld c, a
    ld b, #0xFF
9$:
    pop de
    ret

; void camera_apply(void): cam_x/cam_y = cam_fx/cam_fy >> 4 (12.4 to pixels), and
; scroll_x/scroll_y their low bytes. A 4-bit shift is a nibble swap on this CPU.
    .globl _cam_fx, _cam_fy, _cam_x, _cam_y, _scroll_x, _scroll_y

.macro TO_PIXELS fx, px, scroll
    ld a, (fx)
    swap a
    and a, #0x0F
    ld c, a                 ; low nibble of the result: fraction byte's high nibble
    ld a, (fx + 1)
    swap a
    ld b, a
    and a, #0xF0
    or a, c
    ld (px), a              ; low byte
    ld (scroll), a
    ld a, b
    and a, #0x0F
    ld (px + 1), a          ; high byte
.endm

_camera_apply::
    TO_PIXELS _cam_fx, _cam_x, _scroll_x
    TO_PIXELS _cam_fy, _cam_y, _scroll_y
    ret
