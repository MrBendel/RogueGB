# LunarBase: a generated outpost area (exterior and interior)

**Where we are (2026-10-21):** the descent's generated maps are drawn as the LunarBase
outpost (theme rules in content/tilesets/lunar_ext.json, chunks in
content/chunks/lunar.json): hull modules you walk behind, numbered roofs, crates,
craters. The **world viewer** (web/world.html, 2026-10-22) shows whole areas by
seed. Its first survey: **38 of 50 areas needed paths carved** (the generator cuts
a straight corridor where chunks wall a part off, through buildings): the lunar
chunks' shapes need tuning (lowering "complex" and "camp-yard" to 1 only brought
it to 27). Next: that tuning, then doors and interiors, then pipes and set
pieces. What each sprite is: [lunarbase-sprites.md](lunarbase-sprites.md).

Status: direction agreed 2026-10-21 (generated, not hand-authored); details are
proposals. Art: the purchased LunarBase pack (exterior, interior and decal tile
sheets, an exterior example scene).

## The licence and the files

The repository is public and the pack's licence doesn't clearly allow sharing its
files, so the pack is **local only**: the build reads it from the artist's synced
Google Drive (`G:\My Drive\Artwork\Assets\LunarBase`; tileset sources `pack:...`),
never from the repository.
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

Done (2026-10-24, after the artist's notes on the generated maps): a tidy pass
before the look (a one-block spur off a building, or a lone prop right beside one,
is cleared: it was drawn as a crate stuck to the building); buildings' bases (a door
with its step in the middle of each front three blocks or wider, ground vents under
some of the rest); calmer ground (mostly plain regolith, few patches and craters).

**Set pieces** (2026-10-24): parts of the pack's example cut out whole, as
`"prefab"` pieces in `content/tilesets/lunar_ext.json` (a grid of block types: `.`
ground, `#` solid, `r` a roof you walk behind): the "01" hab module with its door
step, the solar station on its legs, the tank, a small module, the grilled
machine, orange canisters on checker plates, a canister rack, a solar panel stand,
a crate pile, a pair of panel stands (the exterior is at 122 of its 128 blocks). level.c
(`plan_set_pieces`), once a map's connections are made:
- every building the chunks left, whatever its shape, is split into rectangles
  (from each free block, its solid run along the row, as deep as it stays full) and
  each is packed: the set piece that fits best along its front (by size, halved for
  each already on the map, so a map isn't all tanks), smaller ones in the rest, any
  leftover block ground (a lone block against a set piece too);
- then on open ground, about one big set piece (hab, solar, tank) per 120 blocks
  of map, with small ones beside its front; each is kept only if every point that
  must connect still does (a flood fill), and none goes on a camp's slot, a chest
  or a doorway's approach;
- lone crates are thinned to about one in three;
- raised platforms become buildings first (decided 2026-10-24): outdoors there are
  no stairs up to them, so they were walls drawn like buildings.
- **pipes** (`pipe_*`: `79 89 79x` across, `68 6a` down, the elbow `69` flipped
  for the bend) join neighbouring set pieces up to 8 blocks apart: across between
  two side by side, on the lowest row where both are solid; down from one to one
  below; or bent, across from one's side and down onto the other's top or up into
  its underside. Solid (low cover), and kept only if everything still connects;
  one pipe between two set pieces, two at most from any one, and nothing solid
  alongside a run (no pipes bunched side by side or hugging a piece).
They're stamped over the theme's look as the map is built; the lists live on the
stack meanwhile (40 set pieces, 12 pipe runs at most), since the look is drawn once:
an exit map's shaft is dug in the build too (its blocks kept clear of set pieces).

Next: the T-joints (`49 59`) for branching pipes;
**props in groups against buildings** (canister racks, crate stacks, posts, solar
panels) instead of lone crates in the open. The block limit is now 128 (2026-10-24:
room.h's LUT is a page per quadrant; maps went to 24 rows to make room), since the
example's vocabulary doesn't fit in 64.

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

**The look is settled** (2026-10-23): the artist's reference shot of a finished
interior, rebuilt as the entrance room (docs/design/runs.md, the map `entrance` in
`content/tilesets/lunar_int.json`; its pieces in docs/design/lunarbase-sprites.md).
It's the density to aim for in generated interiors:
- **Every back wall is full**: lights, screens, plates, a vent, windows; no bare
  stretch. A back wall is a white band (its top, seen from above) with a face below.
- **Each room has its own floor**: shiny square plates in one, studs in the next.
- **Furniture in mirrored pairs** (couches, counters behind them), with a clear
  walkway down the middle that lines up with the doorways.
- **Walls cast a shadow** on the floor below them; cyan floor lights here and there.

**Built** (2026-10-25): a hab module's door (`pf_hab`: the grate over its orange
step) leads into its interior. Walk up the step into the door (descent.c
`descent_door`: `DESCENT_INSIDE | k`); the interior loads in the interior tileset,
built from a seed of the area, the map and the door (level.c
`level_build_interior`), the player just inside its south door; out of that door
(`DESCENT_OUTSIDE`) the map is rebuilt (the same, from its seed) and the player is
back on the step. One screen (10x8 blocks):
- white walls, and under the top one a face strip mirrored across the middle
  (lights, plates, screens, vents) or a pair of starfield windows;
- one floor: plates, or studs with the wall's shadow and two cyan lights;
- couches either side of the walkway (3 rooms in 4), counters along the bottom
  either side of the door (3 in 4);
- a chest beside a couch: bit 3 + k of the map's `area_chests_opened` (pickups.c
  `chest_bit0`), so a looted one stays looted, and it's saved.
- (2026-10-26) a locker pair at the ends of the face strip, one building in two
  (the shared container art; their bits: building k's at 2k, 2k + 1 of the map's
  `area_inside_opened`, saved as `exp_inside`, save version 7);
- a guard, one building in two: a camp of three in the upper room (its cleared bit:
  2 + the door in the map's `area_camps_cleared`); the chest waits for it and is
  luckier.
Up to 4 doors a map (`DOORS_MAX`). Next: rooms of other sizes and kinds (a lab,
quarters), doors on other set pieces.

## The area

- The one exception: an area's entrance room is indoors (`lunar_int`, docs/design/runs.md).
- An area's other maps are all outdoors (the descent's grid of linked maps); interiors
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
