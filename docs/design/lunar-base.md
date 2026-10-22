# LunarBase: a generated outpost area (exterior and interior)

**Where we are (2026-10-21):** the descent's generated maps are drawn as the LunarBase
outpost (theme rules in content/tilesets/lunar_ext.json, chunks in
content/chunks/lunar.json): hull modules you walk behind, numbered roofs, crates,
craters. Next, in order: a **world viewer** web page (proposed, waiting on a yes:
the real ROM in the browser like the chunk editor, a seed's whole area drawn with
the theme, overlays, live tweaks), then doors and interiors, then pipes and set
pieces. What each sprite is: [lunarbase-sprites.md](lunarbase-sprites.md).

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

## Decals

The decal sheet is white marks on transparency (digits 0-5, numbered plates,
hazard stripes and chevrons, arrows, a warning triangle, a stop hand, icons): stamps
for other tiles, as the example's "01" and stripes on a roof and "404" plate show.
The Color can't layer background tiles, so a stamp is baked into the tile it sits
on at build time (a tileset piece's `"stamp"`), costing a tile per combination.
In use: a building's number on its roof's bottom-left block (one per building),
hazard stripes mid-roof. (Digits span two tile
rows of the sheet: stamp both.)

## Walking behind buildings

Roofs are walkable and drawn over sprites (`"behind"` pieces); only a building's
front row is solid. Raised platforms use copies without it, so you're seen on top.

## The interior: only inside buildings (the pack has no example)

Decided 2026-10-21: interiors are the inside of buildings you walk into, not maps
of their own in the area grid. Some modules get a door in their front row; through
it, a small interior (a room or two, sized to the building) loads; leaving puts you
back outside the door.

A first mockup was a guess and needs work. The construction proposed:

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

- An area's maps are all outdoors (the descent's grid of linked maps); interiors
  hang off buildings' doors.
- An interior loads its own tileset and theme when it's entered.

## Order

1. Done: tileset tool: local-only sheets with stand-ins; pieces from 8x8 tiles; decals.
2. Done: themes: rules that turn the generator's blocks into looks from neighbours.
3. Done (first pass): the exterior theme (ground, modules, props, numbers) and lunar
   chunks. Next: pipes, skylight modules, equipment banks with their ends, tanks.
4. Doors in building fronts; entering and leaving an interior (the scene swap, the
   way back out).
5. Interiors: room-and-corridor chunks sized to the building, the interior theme,
   dense props (lockers, consoles, crates, capsules).
