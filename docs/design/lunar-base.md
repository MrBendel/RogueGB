# LunarBase: a generated outpost area (exterior and interior)

Status: direction agreed 2026-10-21 (generated, not hand-authored); details are
proposals. Art: the purchased LunarBase pack (exterior, interior and decal tile
sheets, an exterior example scene).

## The licence and the files

The repository is public and the pack's licence doesn't clearly allow sharing its
files, so the pack is **local only**: `artwork/tiles/lunarbase/` is gitignored.
Builds on a machine with the files use the art. Builds without it (the CI and Pages
build, other worktrees) need a fallback:
- **Placeholder art:** the tileset tool draws a plain stand-in for each piece when
  the sheet is missing, so the ROM still builds and plays.
- **Tests:** screenshot goldens of LunarBase scenes run only where the art exists.
- **Later, optional:** a GitHub secret holding the sheets (they're ~20 KB), decoded
  by the workflow, would put the real art in the Pages build without the files in
  the repo.

## What fits

| Sheet | Unique 8x8 tiles | Palettes (as drawn) | Fits a map (176 tiles, 6 palettes) |
|---|---|---|---|
| Exterior | 240 | 8 | about 3/4 of it per map |
| Interior | 225 | 7 (4 colours a tile everywhere) | about 3/4 of it per map |

- **Exterior and interior are separate maps.** A map is one scene, with one tileset.
  That matches Borderlands anyway: you go through a door into a module.
- **Variety by selection:** two or more exterior sets (a module yard, pipeworks) and
  interior sets (labs, quarters, the reactor), each a different 176-tile cut.
- The palettes convert with little loss (the converter merges to 6).

## The exterior: learned from the example

The pack's example scene maps back to the sheet: 405 of its 432 tiles are sheet
tiles (many flipped; the rest are decals such as "01" and "404"), 130 distinct.
That gives the assembly rules directly:
- **Ground:** regolith in light and dark patches, a handful of tiles, flipped freely,
  with craters (2x2) and boulders scattered.
- **Modules:** rectangular hull buildings: a riveted roof plate, a darker front with
  vents, orange hatches (doors), blue skylights on the roof.
- **Pipes** run between modules, with elbows and junctions; **crates** and vents
  sit beside them.

Generated as: open ground from the chunks; solid clusters become modules (roof
over front, autotiled from their neighbours) or rock; pipes along some module
edges; props in decoration slots.

## The interior: proposed (the pack has no example)

A 3/4 view like most Game Boy interiors (first mockup in the conversation of
2026-10-21; to confirm with the artist):
- **Floor:** the dark plate pattern (tiles 5a-9e as one repeating 5x5 pattern);
  light grated panels for a second room type.
- **Walls:** a light bevelled cap along the top, then a face of dark machinery where
  a wall meets the floor below (the same rule as cliffs in town). Starfield windows
  in some north walls.
- **Props:** orange containers, capsules, monitors and consoles as solid blocks;
  blue coolant grating as a hazard (pits).
- **Shape:** rooms joined by corridors, not open ground: a second chunk set.

## The area

- An area has exterior maps (most) and interior maps (some), linked like the
  descent's maps now. An opening between an exterior and an interior map is a
  module door on the outside and a corridor door inside.
- Each map loads its own tileset and theme when it's entered.

## Order

1. Tileset tool: local-only sheets with placeholder fallback; pieces cut as any
   4 tiles (the pack is built on 8x8 tiles, not 16x16 blocks).
2. Themes: rules (data in the tileset file) that turn the generator's blocks into
   looks from their neighbours, run when a map is built.
3. The exterior theme (ground, modules, pipes, props), on today's chunks.
4. Interior maps: a map kind, room-and-corridor chunks, the interior theme.
5. Doors between exterior and interior maps; tuning; the area's place in the descent.
