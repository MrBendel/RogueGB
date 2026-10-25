# LunarBase: what the sprites are

A reference for the purchased LunarBase pack: where its files are, what each tile
is, and how it goes together. **Confirmed** means taken from the pack's own example
scene (mapped back to the sheet tile by tile); **proposed** means read from the
sheet and still to be checked. The plan for the area itself is
[lunar-base.md](lunar-base.md); the pipeline is [art-pipeline.md](art-pipeline.md).

## The files

- **Where:** the artist's Google Drive, `Artwork/Assets/LunarBase/` (`Tiles/`,
  `Sprites/`, `Examples/`), synced on their PC at `G:\My Drive\Artwork\Assets`. The
  build reads it there (tileset sources `pack:LunarBase/Tiles/...`; see
  art-pipeline.md, "Art packs"). **Never copied into the repository:** it's public,
  and the licence doesn't clearly allow sharing the files. Builds without it (CI,
  the Pages build) draw stand-ins.
- **Sheets** (128x128 px, 16x16 tiles of 8x8 each), each in two versions:
  `_Standard` (full colour: what we use) and `_GBS` (GB Studio's 4 shades).
  - `LunarBase_Tiles_Exterior_Standard.png`: the moon surface and hull modules.
  - `LunarBase_Tiles_Interior_Standard.png`: walls, floors, machines, windows.
  - `LunarBase_Tiles_Decals_Standard.png`: white marks to stamp onto other tiles.
  - `LunarBase_Example_Exterior_Standard.png` (192x144): a scene built from the
    exterior sheet. There is no interior example.
  - `LunarBase_Sprite_*`: wall screens (A, B, C) and a desk screen: small animated
    sprites for interiors (not looked at yet).
- **Elsewhere:** set `ROGUEGB_ART_PACKS` to a folder laid out like `Artwork/Assets`,
  or put the packs in `artwork/packs/` (gitignored).

## Reading a sheet: tile codes

A tile is named by its column then its row on the sheet, in hex: `00` is the top
left, `f0` the top right, `0f` the bottom left. A flipped tile adds `x` (mirrored
left to right), `y` (upside down) or `xy`. Tileset pieces use these codes
(`content/tilesets/lunar_ext.json`: `"tiles": ["02 12", "03 13"]` is one 16x16
block, two codes per row), and so does `tools/sheet-codes.mjs`:

```bash
node tools/sheet-codes.mjs label "G:/My Drive/Artwork/Assets/LunarBase/Tiles/LunarBase_Tiles_Exterior_Standard.png"
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

## Interior: the entrance room (in use, `content/tilesets/lunar_int.json`)

Built 2026-10-22 after the artist's reference shots (a finished interior, and the
pack's props assembled). What they showed:

- **Walls are the thick white bands** (the tiles this page called "big white
  pipes"), seen from above, with a lilac face below the top band.
- **A doorway** is a gap in a band, with the band turning or ending beside it (the
  jambs) and horizontal slats across it.
- **The comms device** is the framed wall monitor with its blue screen.

| Piece | Tiles | Notes |
|---|---|---|
| Floor `floor`, `floor_shadow` | ours: `artwork/tiles/lunar_int_extra.png` `00` studs, `10` studs in shadow, `20` shine (not used yet) | the reference's floor isn't in the sheet: drawn in the pack's colours (white, lilac, grey, dark grey: one palette), stamped over. Shadow: the top half of a floor block under a wall (the theme's `shade` style) or under the monitor |
| North/south wall `wall_n` | `60 70 / 61 71` | the white band's top, then its lilac face |
| West / east wall `wall_w` `wall_e` | `51` (`51x`) + a column of floor | |
| Corners | `50` (a band's rounded corner), its flips; `30` black below the south corners | |
| Band end caps | `53` (and flips) | the jambs beside a doorway |
| North/south doorway `door_ns` | `48 48x / 49 49x` | the slats |
| Side doorways | `19` + the floor's column | stand-in: no side-on doorway in the sheet |
| Wall face `face_lights` `face_plate` `face_screen` `face_vent` | `ed fd / ee fe`, `96 a6 / 97 a7`, `cb db / cc dc`, `c9 d9 / ca da` | the row under the north wall: orange lights, riveted plate, screen with glare, vent |
| Shiny square plates `plates` | `04 14 / 05 15` | the floor under the wall face |
| Counters `counter` `counter_l` `counter_r` | `f0 f0 / f1 f1`, ends `f2 f3` (and mirrored) | black-topped, along the south wall |
| Orange couch `couch` (2 blocks) | `bd cd cd dd / be ce ce de` | |
| Starfield window `window` (2x2 blocks) | `f6 c6 e6 f6x / f6 c4 d5 f6x / f6 30 e4 f6x / f6 c6y e6y f6x` | pillars `f6`, the vented frame `c6 e6` (flipped for the bottom), stars `b4`-`f5` on black |
| Wall tee, plates beside walls | `51 70 / 51 71`, `51 14 / 51 15` (and mirrored) | where an inner wall meets a side wall; a side wall in a room of plates |
| Cyan floor light `floor_light` | ours: `lunar_int_extra.png` `30` | the studs round it lose their shadow (4 colours a tile) |
| Comms monitor `monitor` (2x2 blocks) | `c0 d0 d0x c0x / c1 d7 d7 c1x / c1 d8 f9 c1x / c0y d0y d0y c0xy` | the frame is `c0 d0 c1` and flips; `d7` the screen's top, `d8` its middle, `f9` the window icon |

The room is a map made by hand (`maps.entrance` in the tileset), not the theme's
rules; level.c opens its doors. Props in the reference not used yet:

| Prop | Tiles |
|---|---|
| Riveted plate | `96 a6 / 97 a7` |
| Orange container | `77 87 87x 77x / 78 88 88x 78x / 79 89 89x 79x` |
| Chair or capsule (white) | `a8 b8 / a9 b9 / aa ba / ab bb` |
| Small screen with glare | `cb db / cc dc` (the pack's DeskScreen and WallScreen sprites animate screens like it) |
| Orange crates | `bd cd dd / be ce de` |
| Screen icon lines | `e7 f7 / e8 f8 / e9 ea` (cyan lines that join the `f9` icon) |

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

Set pieces (`pf_*`, cut from the example; level.c stamps them whole):

| Set piece | Size (blocks) | From the example |
|---|---|---|
| `pf_hab` | 4x5 | the "01" module (widened by a column to an even width): roof with `a4` vents, the "01" and stripes as decals, the front with the door's grate `d6 e6`, the step `fe ff` and ground vents below |
| `pf_solar` | 3x4 | the solar station: frame `ca da cb cc cd`, panels `db`-`fd`, front `05 07 27 65`, legs `09 17 79` |
| `pf_tank` | 2x3 | the tank `0a 2a 2ax 0ax` down to its hatch `3e 4e` |
| `pf_grill` | 3x1 | the grilled machine `c7 17 19 19 17x c7` |
| `pf_pads` | 2x1 | orange canisters on checker plates `72 82` |
| `pf_rack` | 1x2 | the canister rack `f5 f6 f7` |
| `pf_panel` | 1x2 | a solar panel stand `8b 9b`-`8e` |

Building bases (level.c `building_bases`, after the theme's rules): `face_door`
(`65 65 / d6 e6`: the front with the door's grate) with `step` below it (`fe fex /
ff ffx`, the orange step out on the ground), and `vents` (`3a 5a`, two black ground
vents) under some of the rest of a front. Read from the example: under its big
module the grate runs on down (`d7 e7`) to the step; pipes (`29 89 99`, `39`, `6a
7a`, `17 17x`), canister racks (`f5 f6 f7`), posts (`2f`) and solar panels (`8b 9b /
8c 9c / 8d 9d`, base `8e`) sit around the modules: not used yet.
