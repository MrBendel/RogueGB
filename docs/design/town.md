# Town layout and interiors

## Outdoor rules

Sands is a compact 16x19-block settlement, using the original 240x192 composition
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
Rescued residents stand clear of door approaches at (3,11), (11,8), (7,13),
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
slots; paletteOverrides applies warm stone colours and a pale sand highlight
after indexing, preserving the source's shading detail. The outdoor scene uses one palette, with sand in slot 1 (#e0c888 authored,
#e7ce8c on hardware). Container faces use their original dark pixel indices. The pale sand (#f0d8a0) is closer to that ground tone than the old paper white.

Optional layout authoring: python tools/sands-town.py (requires Pillow). Compile
with node tools/tileset.mjs sands. The helper removes unused pieces; the normal
compiler deduplicates all 8x8 tiles including horizontal, vertical and combined
flips. Outdoor: 364 quadrant references -> 214 stored (including flipped reuse).
Interiors: 84 references -> 74 unflipped -> 47 stored, shared by all three rooms.

Only the current room's art is loaded. Sands uses 172 tiles in VRAM bank 0 and
42 in bank 1 at background indices 80..121. Town HUD names use seven on-demand
glyphs at 44..50; closing a menu restores outdoor detail tiles. The interiors fit
entirely in bank 0. Tests cover source shading preservation, absence of unused
pieces, service/door reachability, transitions, purchases and graphics restoration.

## Storage yard

The source container example is an assembly of light tops, darker vertical faces,
and front doors, not repeated roof strips with different palettes. The left
container uses source (32,224), 64x32 at town (1,13). The right cluster copies the
complete source (16,224), 80x96 at town (10,12), preserving its staggered overlap,
dark faces, doors and grass. Its silhouette is solid; the grass and empty cells
remain walkable. The southern yard extends by three rows to fit the full assembly.
Preview screenshots must place the player on verified walkable ground.

## Depth and cliff traversal

The tower tank at (6,12) and upper supports at (6,13) are walkable background
priority pieces: the player passes behind them. The grounded base (6,14) stays
solid. Use this separation for tall props instead of blocking their silhouette.

The south-facing cliff faces at row 10, columns 3..5 and 9..14, stay solid to
walking and upward jumps. From the north lip, Down + jump starts a committed hop
to the clear ground below. Releasing or reversing input mid-hop still completes
the crossing, then normal gravity finishes the landing. Other cliff walls remain
solid until they have an authored safe drop route. The traversal is town-only;
combat movement and collision rules remain unchanged.

## Ground transitions

Four ground cells in row 11 (columns 2, 6, 7, 8) close the pale sand tongues
before the departure yard. They reuse the irregular edges from source row 9,
columns 3..5, keeping the original cracks and sparse grains instead of introducing
a repeated texture everywhere. Source art and collision stay unchanged; only
these four map cells change their visuals. The reused edges add no hardware tiles.

### Cliff collision audit

The wall beside the central stairs is solid at (6,5) and (7,5); only (8,5)
is traversable. Cliff caps and feet at (14,9), (13,6), (14,6), and (1,9)
are solid as drawn. The attached ledge at (10,8)..(10,9) is also solid, so
Okafor stands beside it at (11,8). Open ground below completed cliffs remains
walkable. Explicit row-10 downward jumps remain the only cliff-face crossings.
