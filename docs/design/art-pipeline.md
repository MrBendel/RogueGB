# Art pipeline: sheets to tiles

Status: first version 2026-10-19 (the town in the artist's ice art).

## How a sheet gets into the game

1. **The sheet** goes in `artwork/tiles/` (e.g. `ice.png`): full-colour pixel art,
   transparency allowed (it becomes the backdrop colour, snow for ice).
2. **The tileset file** `content/tilesets/<name>.json` names its **pieces**: each a
   rectangle `[x, y, w, h]` in pixels, x and y on the 8 px grid, w and h in 16s
   (one 16x16 block or more).
3. **Maps** in the same file are rows of characters, one per block. The **legend**
   says which piece a character draws and whether it's solid. A piece bigger than a
   block is drawn by repeating its character over its whole size (a 3x2 roof is
   `RRR` on two rows); `"stretch": true` lets a run be any length (the piece's first
   and last columns at the ends, the middle repeated: a roof over a 4-wide front).
   `"vary": ["snow_specks", 9]` swaps in a variant now and then, by position.
4. **The build** (`node tools/tileset.mjs`, also run by `npm run build`):
   - fits the art into the Color's background palettes (6 for a scene: the HUD
     keeps 2). Each tile keeps its 4 best colours; similar palettes are merged, the
     pair that loses least first, keeping the art's own colours (nothing blended).
   - stores each tile once, flipped copies included.
   - writes `build/gen/tiles_<name>.c` and **`build/tileset-<name>.png`**: every
     piece as the Color draws it, beside the original, with the palettes. Check it
     after changing the sheet.

## Limits (the build fails past them)

- **176 tiles** per scene (background tiles 80-255). The ice town uses 139.
- **64 blocks** per tileset (the block look-up tables). The ice set has 54.
- **4 colours per 8x8 tile**, 6 palettes. Art that keeps to shared ramps (one
  outline colour, one snow white, a ramp per material) converts without loss.

## Notes for drawing

- Make a material change land on an 8 px line: a tile is one palette.
- Repeating, mirrored and flipped tiles are free; unique detail costs tiles.
- Building fronts are modules a block wide (window, door, shutter), so a map can
  mix them for variety; roofs are 3x2 and stretch.
- Interiors (the rug, tables, desk, door) will be their own tileset, on a wood
  floor backdrop.

## The ice set (content/tilesets/ice.json)

Ground: `snow`, `snow_specks`, `snow_dots` (the cliff's base), `cliff`, `fence`.
Buildings: `roof` (the middle cabin's, with the gradient top; the big rounded panels are floating ice sheets, for water later), fronts `win_a` `door_a` `win_b`
`door_b` `win_c` `win_d` `door_d` `shutter` `win_e`, `hut`, house signs `sign_a`
`sign_b`. Props: crate, rock, barrel, signpost, shelf, fire barrel, drum,
ammo tub, vending machine, lockers, chest (and open), safe, gun box (and open).

For water, later (not in the set yet): the big rounded ice sheets and the small
ice chunk on blue (sheet 96,144) float in water; they don't go on the snow.
