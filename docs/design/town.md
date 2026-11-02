# Town layout and interiors

## Outdoor rules

Sands is a compact 16x16-block settlement, using the original 240x192 composition
plus its southern departure yard. The duplicate eastern market wing and repeated
southern cliff formation are removed. The original three complete buildings are
quarters, Echo's relay room, and Marro's shop.

- Keep connected cliff caps, faces and bases together; do not repeat isolated edge
  tiles as ground or paste another whole cliff behind an existing one.
- Preserve each complete roof/frontage silhouette. Its solid facade and visible
  doorway must agree with collision.
- Every door has an unobstructed ground approach connected to the town's paths.
  Keep props and NPCs out of those approaches.
- Furniture stays against room edges or in small groups, leaving the central aisle
  from the south doorway clear. Tables, desks, machines and walls are solid.
- Walking UP into a facade enters; walking DOWN through the interior's south door
  exits onto the same outdoor doorstep. Jumping/rolling cannot trigger a doorway.

Outdoor doors (facade block coordinates): quarters (5,8), relay (7,3), shop (11,6).
Rescued residents stand clear of door approaches at (3,11), (10,8), (7,13),
and (2,12). The lift (4,12) and contracts board (7,10) stay outside. Company posts are at
(12,8) and (8,12). Marro and the gun/shield machines are inside the shop.

## Interior art and behavior

content/tilesets/town_inside.json cuts the wooden room trim, floorboards, tables,
chairs, desk and bench from the user's artwork/tiles/ice.png. Three authored 10x8
rooms share one tile set. Their southern two-block doorway is the exit.

Town room changes preserve health, shields, ammo, inventory, shop stock, mission
state and elapsed run state. Floor loot is cached separately for all four rooms
in far RAM; an uncollected vending purchase stays in the shop during other room
visits. This is session-local, like other floor loot. Loading a saved town file
starts outdoors rather than inside a building.

## Reuse and colours

Source image: artwork/tiles/sands.png, unchanged. The source greens are shading
slots; paletteOverrides applies Cragback's warm stone colours and the menu's
paper highlight after indexing, preserving the source's shading detail.

Optional layout authoring: python tools/sands-town.py (requires Pillow). Compile
with node tools/tileset.mjs sands. The helper removes unused pieces; the normal
compiler deduplicates all 8x8 tiles including horizontal, vertical and combined
flips. Outdoor: 340 quadrant references -> 249 unique unflipped -> 198 stored.
Interiors: 84 references -> 74 unflipped -> 47 stored, shared by all three rooms.

Only the current room's art is loaded. Sands uses 172 tiles in VRAM bank 0 and
26 in bank 1 at background indices 80..105. Town HUD names use seven on-demand
glyphs at 44..50; closing a menu restores outdoor detail tiles. The interiors fit
entirely in bank 0. Tests cover source shading preservation, absence of unused
pieces, service/door reachability, transitions, purchases and graphics restoration.

## Storage yard

The ribbed pieces at (1,14)..(4,14) are storage containers, not fencing.
The left stack occupies columns 1..4, rows 13..14; the right stack occupies
columns 10..13, rows 12..14. Lower tiers use progressively darker warm ramps,
following the source demo. All tiers are solid; the lift and resident approaches
remain open. Palette attributes reuse identical indexed tile patterns: no extra
hardware tiles are needed for the darker tiers.
