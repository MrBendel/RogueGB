# RogueGB Hardware Layout, Sprite Architecture & OAM Guide

This document is the authoritative engineering reference for Game Boy / Game Boy Color hardware constraints, OAM sprite allocation, screen layout, VRAM mapping, and banking rules in **RogueGB**.

All agents and contributors modifying rendering, actors, projectiles, boss fights, or UI **must** adhere to the layout rules described here to prevent sprite starvation, visual clipping, or ROM bank overflows.

---

## 1. Hardware Sprite (OAM) Architecture

The Game Boy PPU has a strict hardware limit of **40 sprites** total (160 bytes in OAM at `0xC000`, manipulated through `shadow_OAM[40]`), with a maximum of **10 sprites per scanline**.

### 1.1 Complete OAM Allocation Map

RogueGB uses Game Boy 8×16 sprite mode (`LCDC_OBJ16`). In this mode, each hardware sprite entry occupies an 8×16 pixel slice. Metasprites (such as 16×16 characters) require **two adjacent hardware sprites** (a left 8×16 column and a right 8×16 column).

| OAM Slot(s) | Constant | Sprites Used | Subsystem / Owner | Critical Invariants |
| :--- | :--- | :---: | :--- | :--- |
| **0** | `OAM_FLASH` | 1 | Muzzle flash | Drawn on top of gun and player body |
| **1 – 2** | `OAM_GUN_FRONT` | 2 | Held weapon | Drawn when aiming down/lateral (in front of body) |
| **3 – 4** | `OAM_PLAYER` | 2 | **Player character metasprite** | **NEVER ZERO OR OVERWRITE SLOT 4!** Slot 3 is the left 8×16 half; Slot 4 is the right 8×16 half. Zeroing slot 4 erases half the player! |
| **5 – 6** | `OAM_GUN_BACK` | 2 | Held weapon | Drawn when aiming up (behind player body) |
| **7 – 8** | `OAM_SHADOW` | 2 | Player jump shadow | Only drawn when `player_z > 0` |
| **9 – 10** | `OAM_RETICLE` | 2 | Aim reticle / reload bar | Reticle when aiming; fills over head during weapon reload |
| **11 – 14** | `OAM_PROP` | 4 | Arena chest & shield item (or **Boss Overlays**) | Reserved for chest/shield in descent/lab. **In boss fights, slots 11–14 are free for boss overlays** (e.g., Bewm's shadow and grenade reticle). |
| **15 – 39** | `OAM_FIRST_ACTOR` | **25** | Dynamic actor pool, player shots, enemy bullets | Shared dynamically every frame by `actors_update()`. |

> [!CAUTION]
> **Never touch slots 0..10 in custom encounter drawing functions!**
> Custom boss layers (such as ground shadows, telegraph reticles, or orbital debris) must exclusively use `OAM_PROP` (slots 11..14). Always clear them using:
> ```c
> for (uint8_t i = OAM_PROP; i < OAM_FIRST_ACTOR; i++) shadow_OAM[i].y = 0;
> ```

---

## 2. Dynamic Actor & Projectile Pool (`OAM_FIRST_ACTOR`)

Slots 15 through 39 provide 25 hardware sprite slots for all combat entities.

### 2.1 Metasprites vs. Single-Sprite Projectiles
- **Enemies / Bosses / NPCs**: 16×16 metasprites (use 2 OAM slots: `actor_t0` left, `actor_t1` right).
- **Player Shots**: Single 8×16 sprite each (`shots.s`, max 16 active).
- **Enemy Bullets / Missiles**: Single 8×16 sprite each (`bullets.s`, max 20 active).

### 2.2 OAM Starvation & Drawing Priority
When 3 actors are on screen (6 sprites), 8 player shots are fired (8 sprites), and an enemy fires a 10-bullet barrage (10 sprites):
$$6 + 8 + 10 = 24 \text{ sprites}$$
This comfortably fits within the 25 available slots ($15 + 24 = 39 \le 40$).

However, if `play_actor_oam` is offset upwards (e.g., set to $15 + 8 = 23$), OAM begins at slot 23, leaving only 17 slots. As soon as total entities exceed 17 sprites, `oam_next >= 40`, and excess projectiles **disappear from the screen immediately**.

```c
// INVARIANT in src/play.c:
// Only Warden floor fields require OAM_FIRST_ACTOR + 8.
// All standard runs, encounters, and Boss Duo MUST use FIRST_ACTOR_OAM (15).
if (play_mode == MODE_BOSS && enemy_trial_kind == 0) {
    play_actor_oam = FIRST_ACTOR_OAM + 8; // Warden floor fields
} else {
    play_actor_oam = FIRST_ACTOR_OAM;     // 15: All other modes
}
```

### 2.3 Render Pass Order in `src/actor.c`
In `actors_update()`:
1. Active actors emit their metasprites via `actors_emit()` (`actor_draw.s`).
2. **`update_shots()` (player projectiles) runs FIRST.** Player shots are guaranteed OAM slots so the player never experiences "phantom bullets" that disappear upon firing.
3. **`update_bullets()` (enemy projectiles) runs SECOND.**
4. Remaining unused slots up to `oam_end_last` are zeroed out.

---

## 3. Screen Geometry, Viewport & Map Coordinates

| Dimension | Pixels | Blocks (16×16) | Tiles (8×8) | Notes |
| :--- | :---: | :---: | :---: | :--- |
| **Game Boy Screen** | 160 × 144 | 10 × 9 | 20 × 18 | Physical LCD display |
| **Combat Playfield** | 160 × 128 | 10 × 8 | 20 × 16 | Visible play area above the HUD |
| **HUD Row** | 160 × 16 | 10 × 1 | 20 × 2 | Window layer positioned at line $y = 128$ (`HUD_WY`) |

### 3.1 Hardware Scanline Sprite Cutoff
The HUD is displayed via the Game Boy Window layer. To prevent enemy sprites or bullets from showing through or on top of the HUD text:
```c
gfx_sprite_split(hud_y); // Enforces hardware interrupt cutoff at line 128
```

### 3.2 Arena Room Bounds & Projectile Collision Rules
The authored boss arena map is $10 \times 8$ blocks (160×128 px):
- **Row 0** ($y = 0..15$): Outer perimeter wall (`BLK_SOLID`).
- **Row 7** ($y = 112..127$): Bottom perimeter wall (`BLK_SOLID`).
- **Col 0** ($x = 0..15$): Left perimeter wall (`BLK_SOLID`).
- **Col 9** ($x = 144..159$): Right perimeter wall (`BLK_SOLID`).
- **Active Floor**: Columns 1..8 ($x = 16..143$), Rows 1..6 ($y = 16..111$).

> [!WARNING]
> **Projectile Spawn Clamping Rule:**
> In `bullets.s`, a bullet checking its tile coordinate at $(x+4 >> 4, y+8 >> 4)$ that hits `BLK_SOLID` or `BLK_RAISED` is **immediately deleted**.
> If an actor is airborne with visual altitude $z$ (e.g. Bewm at $y = 24$, $z = 14$), **do not subtract $z$ from projectile spawn coordinates**. Doing so spawns the projectile at $y = 10$ (Row 0 ceiling), instantly deleting the projectile on frame 1.
> Always clamp bullet spawn coordinates:
> ```c
> if (x < 16) x = 16; else if (x > 144) x = 144;
> if (y < 20) y = 20; else if (y > 108) y = 108;
> ```

---

## 4. VRAM Tile Memory Map

The Game Boy Color provides two 8KB VRAM banks (Bank 0 and Bank 1).

### 4.1 Sprite VRAM (Bank 0, `0x8000 – 0x8FFF`)
Tile numbers 0..127:

```
[000 .. 009] : Player body frames & air jump shadow (SHADOW_VRAM = 8)
[010 .. 015] : Duelist actor tiles, columns 0-2 (duelist.c; the descent, the lab)
[016 .. 023] : Slime actor tiles (SPR_SLIME)
[024 .. 039] : Peashooter actor tiles (SPR_PEASHOOTER)
[040 .. 043] : Bullet projectile tiles (SPR_PEA)
[044 .. 045] : Reticle tiles
[088 .. 095] : Hound actor tiles (hound.c; the descent, the lab)
[106 .. 107] : Duelist actor tiles, column 3 (after the chest's item, outside boss fights)
[096 .. 127] : MACHINE_VRAM (32 tiles = 8 metasprite frames, 4 tiles each)
               - Frame 0: Closed Warden
               - Frame 1: Exposed Warden
               - Frame 2: Claimjaw
               - Frame 3: Energy Relay
               - Frame 4: Big Bertha (Cannon Straight Down)
               - Frame 5: Boom on Foot
               - Frame 6: Bewm Jetpack Flusher
               - Frame 7: Big Bertha (Cannon Angled Down-Left)
```

### 4.2 Sprite VRAM (Bank 1, CGB Only)
- Held weapon sprites (`spr_pistol`, `spr_shotgun`, `spr_rifle`, `spr_sniper`, `spr_sword`) are loaded into **VRAM Bank 1**, keeping Bank 0 free for actors and projectiles.

### 4.3 Background VRAM (`0x9000 / 0x8800`)
- **0 .. 63**: ASCII font ($32..95$, tile = `char - 32`).
- **64 .. 72**: Dialog/menu 9-slice frame ($3 \times 3$).
- **80 .. 127**: `SCENE_TILES` (Room floor, wall blocks, pits, raised blocks).

---

## 5. 16×16 Metasprite Art & Hardware Mirroring Rules

Each 16×16 metasprite is composed of two 8×16 hardware sprites:
- `actor_t0`: Left 8×16 column (tiles $T, T+1$).
- `actor_t1`: Right 8×16 column (tiles $T+2, T+3$).

### 5.1 Horizontal Mirroring (`S_FLIPX`)
To mirror a 16×16 metasprite horizontally using Game Boy hardware flags:
1. Apply `0x20` (`S_FLIPX`) to both `actor_p0` and `actor_p1`.
2. **Swap the tile pointers**: The left column must receive the right tile (`base + 2`), and the right column must receive the left tile (`base`).

```c
static void art_flip(uint8_t i, uint8_t frame, uint8_t pal, uint8_t flip_x) {
    uint8_t base = MACHINE_VRAM + (frame << 2);
    if (flip_x) {
        actor_t0[i] = base + 2;        // Right tile drawn on left
        actor_t1[i] = base;            // Left tile drawn on right
        actor_p0[i] = actor_p1[i] = pal | 0x20; // S_FLIPX
    } else {
        actor_t0[i] = base;
        actor_t1[i] = base + 2;
        actor_p0[i] = actor_p1[i] = pal;
    }
}
```

Using this pattern, directional aiming art (like Big Bertha's heavy cannon) only needs **one** angled frame in VRAM (Angled Left); Angled Right is dynamically generated via hardware flip with zero extra tile cost.

---

## 6. ROM Banking & Performance Constraints

1. **Bank 0 Hard Limit**: ROM Bank 0 (the unbanked root at `0x0000..0x3FFF`) is strictly limited to 16KB. `tools/build.mjs` enforces a **hard cap at 90% usage** (`checkBank0`). If Bank 0 reaches 90%, the build fails.
2. **Bank Switching (`#pragma bank 255`)**:
   - Boss encounter logic (`boss.c`, `warden.c`, `gunship.c`), enemy trials, dialog, and level generators must reside in switchable ROM banks using `#pragma bank 255` (bankpack auto-allocates them).
   - Only hot per-frame dispatchers (`play.c`, `actor.c`, `camera.c`) live in Bank 0.
3. **Integer Arithmetic Only**:
   - Never use 32-bit `long` arithmetic, floating-point numbers, or division `/` in per-frame tick functions. Use 8.8 fixed-point integer math (`int16_t`) and bit-shifts (`>>`, `<<`).
