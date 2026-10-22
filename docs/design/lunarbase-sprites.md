# LunarBase: what the sprites are

A reference for the purchased LunarBase pack: where its files are, what each tile
is, and how it goes together. **Confirmed** means taken from the pack's own example
scene (mapped back to the sheet tile by tile); **proposed** means read from the
sheet and still to be checked. The plan for the area itself is
[lunar-base.md](lunar-base.md); the pipeline is [art-pipeline.md](art-pipeline.md).

## The files

- **Where:** Google Drive, `Artwork/Assets/LunarBase/` (sub-folders for tiles,
  sprites and examples). On this PC: `artwork/tiles/lunarbase/`, **gitignored**:
  the repository is public and the licence doesn't clearly allow sharing the files.
  Builds without them draw stand-ins (docs: lunar-base.md, "The licence").
- **Sheets** (128x128 px, 16x16 tiles of 8x8 each), each in two versions:
  `_Standard` (full colour: what we use) and `_GBS` (GB Studio's 4 shades).
  - `LunarBase_Tiles_Exterior_Standard.png`: the moon surface and hull modules.
  - `LunarBase_Tiles_Interior_Standard.png`: walls, floors, machines, windows.
  - `LunarBase_Tiles_Decals_Standard.png`: white marks to stamp onto other tiles.
  - `LunarBase_Example_Exterior_Standard.png` (192x144): a scene built from the
    exterior sheet. There is no interior example.
  - `LunarBase_Sprite_*`: wall screens (A, B, C) and a desk screen: small animated
    sprites for interiors (not looked at yet).
- **Getting them again:** download from Drive into `artwork/tiles/lunarbase/` (the
  Standard sheets, the decals, the example). They're small (5-8 KB each).

## Reading a sheet: tile codes

A tile is named by its column then its row on the sheet, in hex: `00` is the top
left, `f0` the top right, `0f` the bottom left. A flipped tile adds `x` (mirrored
left to right), `y` (upside down) or `xy`. Tileset pieces use these codes
(`content/tilesets/lunar_ext.json`: `"tiles": ["02 12", "03 13"]` is one 16x16
block, two codes per row), and so does `tools/sheet-codes.mjs`:

```bash
node tools/sheet-codes.mjs label artwork/tiles/lunarbase/LunarBase_Tiles_Exterior_Standard.png
```

writes `build/codes-<sheet>.png`: the sheet enlarged with every tile's code on it.
`map <sheet> <scene>` prints how a scene is built from the sheet (`??` for decals);
`compose <sheet> <codes.txt> [out] [decals]` draws a mockup from rows of codes
(see `docs/design/lunarbase/module.txt`; `13+00` stamps decal `00` over tile `13`).

## Exterior (confirmed from the example)

The example uses 129 distinct sheet tiles; 27 of its 432 tiles are decals (the "01"
and stripes on a roof, the "404" plate). Flips are used everywhere.

| What | Tiles | Notes |
|---|---|---|
| Ground, light | `00 10 20 21` | mixed freely, flipped freely |
| Ground, dark / patches | `70 80 90 91`, `50 60 51 61` | dark regolith; the 5x/6x ones blend light into dark |
| Crater | `30 40 / 31 41` | 2x2 tiles (one block) |
| Small rocks | `01 11` | |
| Boulder | `e0 f0 / e1 f1`; `f2 f3` | 2x2 |
| **Module roof** | `02 12 22 / 03 13 23 / 04 14 24` | a 9-slice: corners, edges, centre `13`; the bottom row carries the lower edge |
| Roof vent | `a4 a4x` | in a roof's top half |
| **Module front** | `05 15 .. 05x` over `06 16 .. 06x` | two tile rows under the roof; `05`/`06` the end caps |
| Front variants | `65`/`67`, `75 85`/`76 86 96` | panels; a garage door |
| Grill corridor and door | `d6 e6 / d7 e7`, then `fe fex / ff ffx` | sticks out of a front, an orange hatch at its end: the way in |
| Skylight module | `ca da .. cax / cb db eb fb cbx / cc dc ec fc ccx / cb dd ed fd cbx / cd fa .. cdx` | glass roof, front `05 07 27 27x 65 05x / 06 08 28 38 67 06x` |
| Tank | `0a`-`0e` (and flipped), `2a`-`2c`, `3e 4e` | a cylinder lying on its side |
| Pipes | `6a` `79` `7a` (up and down), `29 89 99` (across), `39x 59` (elbows) | |
| Pipe rack | `17 19x 17x` between posts `c7` / `c8` | |
| Vent | `3a 4a 5a` | a floor grate, 3 tiles wide |
| Orange crate | `b9 b9x / ba bax` | symmetric: each half is the other flipped |
| Small crate | `2f` | 8x8; four make a stack |
| Equipment bank | `72 82 / 73 83` | repeats in a run; its end pieces aren't found yet: don't use it alone |
| Poles | `f5 f6 f7` | a post with an orange box |

## Interior (proposed: no example in the pack)

A 3/4 view like most Game Boy interiors. First mockup in the conversation of
2026-10-21; the artist found it needs work. To confirm:

| What | Tiles | Notes |
|---|---|---|
| Plain fills | `00` white, `10` light, `20` mid, `30` black, `40` dark | |
| Floor plates | `5a`-`9e` | one 5x5 pattern: repeat it as a block, don't scatter its tiles |
| Light floor panels | `0b`-`4f` | grated and plain |
| Wall caps (bevelled panels) | `01`-`44` | |
| Wall faces (machinery) | `63`-`94` | where a wall meets the floor below |
| Big white pipes | `50`-`92` | L-shapes and straights |
| Starfield window | `c4`-`f5`, frame `d2`-`f3` | space outside |
| Blue grating (coolant?) | `c7`-`f9`, cyan pattern `e8 f8 e9 f9 ea` | |
| Cyan lights | `54 56 57`, `27 37 47`, `67` | |
| Orange container | `77 87 87x 77x / 78 88 88x 78x / 79 89 89x 79x` | mirrored: the right half is the left flipped (confirmed by eye: without it the edge is cut off) |
| Orange doors / crates | `ad`-`fe` | |
| Monitors | `cb db`, `bc` | |
| Capsule | `a9 b9 / aa ba` | |
| Crate (grey) | `a6 b6 / a7 b7` | |

## Decals

White marks on transparency, meant to be stamped onto tiles (the Color can't layer
background tiles, so the build bakes a stamp into the tile under it, a tile per
combination: a piece's `"stamp"` in the tileset file).

| What | Tiles | Notes |
|---|---|---|
| Digits 0-5 | `00 10 20 30 40 50` over `01 11 21 31 41 51` | **each digit spans two tile rows**: stamp both |
| Diagonal stripes | `90 a0 b0 c0 d0` over `91`-`d1` | two rows, like the digits |
| Big digits / plates 1-5 | `b9 c9 d9 e9 f9`, plates `99`-`9b` row | |
| Warning triangle | `e0`-`f3` | |
| Stop hand | `e6`-`f7` | |
| Arrows, brackets | `56`-`68`, `86`-`a8` | |
| Circle icons | `0b`-`9d` (2x2 each) | |
| Check / cross boxes | `4d`-`9e` (2x2 each) | |
| Chevrons | `ac`-`fd` | |

In use now: a digit on each building's bottom-left roof block, stripes mid-roof.

## What the game uses (content/tilesets/lunar_ext.json)

Ground `g_a`-`g_d`, `crater`, `rocks`; module roof pieces `roof_*` (walk-behind),
one-row roofs `roof_sl roof_s roof_sr`, numbered corners `roof_bl1`-`roof_bl5`,
striped `roof_cst`, vent `roof_v`; fronts `face_l face_c face_c2 face_c3 face_r`;
raised platforms `deck_*` (the roof without walk-behind); props `crate`,
`crate_stack`, `boulder`; pits `pit` (the vent, for now).
