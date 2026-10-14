# Level generation, depth, and towns

Status: agreed direction (2026-10-12, revised 2026-10-13) with working numbers. See
[decisions](decisions.md). Numbers below are starting targets to tune in playtests.

## Agreed direction

- **Expeditions are explored, not cleared room by room.** An **area** is a handful
  of linked **maps**; each map is open ground you roam freely, and its edges open
  onto neighbouring maps. (Revised 2026-10-13: replaces a room grid with doors.)
- **Progression is loot-driven, like Borderlands:** guns are the real level-up
  mechanic. Play is roaming, looting chests, and killing enemies.
- **Areas are generated in game** from the run's seed, so each trip is different.
  The same seed always gives the same area (replays, bug reports, tests).
- **Maps are built from authored chunks** with wildcards; the generator picks,
  flips, and combines them.
- **Rulesets decide how visuals group together.** The generator places logical
  blocks; a visual ruleset per theme picks the tiles, palettes, and decorations.
- **Towns are authored**, not generated: fixed, familiar places on each planet with
  NPCs, shops, bars, missions, and more. They contrast with the changing descent.

## Three layers of generation

### 1. Area layout: which maps, and how they link

- An area is **6–9 maps** on a small grid, grown outward from the start so every map
  is reachable, plus **one or two extra links** so there are loops and alternate
  routes, not just dead ends.
- **The way onward** (deeper, or out) is in the map farthest from the start.
- **Guarantees:** every map, opening, camp, and cache is reachable; the harness
  checks thousands of seeds.
- **Landmarks** are fixed pieces the generator must place: the relic chamber, an
  Echo relay, a guardian arena, a beacon. Story beats survive randomness this way.
- Missions taken in town can request content ("recover X from the second area",
  "clear the corporate site").

### 2. Map shape: chunks and points of interest

- A **chunk is a quarter screen: 5×4 blocks** (80×64 px). Maps are grids of chunks.
- Chunk wildcards ("maybe a wall", "an enemy slot", "a pit or a bridge") are filled
  by the generator; chunks flip horizontally and vertically for variety.
- Each chunk cell of a map gets a **role**:
  - **open ground** (most of it);
  - **rock**: a cluster of solid chunks that gives the map a natural shape;
  - **camp**: enemies around a chest;
  - **cache**: a hidden chest, off the main routes.
- **Edges** are cliffs and walls, except **openings** (4 blocks wide, anywhere along
  the edge) onto linked maps.

### 3. Visuals: rulesets

The generator only places logical blocks (floor, wall, pit, raised, decoration
slots). A **theme** (per planet or depth) supplies:

- **Autotiling:** a block's tile depends on its neighbours (corners, edges, ends).
- **Grouping rules,** for example:
  - cracks only on walls next to floor;
  - moss in clusters near water;
  - pillars in pairs;
  - rubble within two blocks of a collapsed wall;
  - this decoration never twice in view.
- **The tileset and palettes,** so the same layout looks like a different place.

Rules are data, authored in a tool and converted to ROM tables like sprites are.
Engine work needed: maps use one background palette today; grouping by palette
needs per-tile palette attributes streamed with the map.

## Map size

The playfield is one screen: 10×8 blocks (160×128 px). The engine handles maps up
to 32×32 blocks with a scrolling camera.

| Map | Size | Notes |
|---|---|---|
| Start, way onward | 2×2 screens (4×4 chunks) | Arrival and departure |
| Field | 3×2 to 3×4 screens (6×4 to 6×8 chunks) | Open ground, 1–2 camps, sometimes a cache |

## Enemies and camps

- Enemies live in **camps**. A camp **wakes up as the player approaches** (about a
  screen away), Borderlands-style, rather than spawning when a map is entered.
- Eight enemies at once is the sprite and CPU budget: a camp is 3–5 enemies, and
  only nearby camps are active.
- A cleared camp stays cleared for the expedition; an uncleared one resets when the
  player leaves the map.
- Camps guard chests. Chests and drops hold the loot that drives progression.

## Depth

The descent goes toward each planet's center through **areas**.

- An area takes roughly 8–15 minutes to explore fully. Rushing through is faster.
- A **beacon** ends each area: a save and bail-out point, and a landing site for
  later trips (see "unlocked landing depths" in the play loop).
- **The opening** is a short area on a fixed seed with fixed landmarks: it plays
  like a hand-made tutorial through the same pipeline.
- How many areas make a full descent: open.

## Towns

Each planet has at least one authored town: the landing site and the way down.

- **NPCs** with dialogue (portraits, typewriter text) and simple schedules or states.
- **Shops:** buy, sell, maybe upgrade. Needs a currency and an economy.
- **Bars:** rumours, leads, flavour; possibly hiring or rare encounters.
- **Missions:** taken in town, tracked as campaign state, completed in the descent.
- Story moments with Echo, the corporation, and local factions.

Towns pull forward engine systems that the descent also needs: a dialogue box with
portraits, interaction (talk, buy, pick up), trigger zones, mission state, and
save/load.

## Open

- Whether characters level at all (XP and skills alongside gear, or gear only).
- Planet count; how many areas make a descent.
- Map transitions: a fade (now) or a scroll.
- The chunk authoring tool: a tab in the Sprite Studio (decided: our tool) with a
  layout preview.
- Town size and structure (one screen hub, several connected screens, interiors).
- Currency, prices, and what shops and bars actually offer.
- Mission types and how many can be active.
- Whether area layouts persist between trips (landmarks persist; routes remix).

## Built so far (2026-10-13)

The skeleton runs in game: START on the title begins a descent, SELECT opens the
test lab.

- **Chunks** are JSON in `content/chunks/<theme>.json`, converted at build time by
  `tools/chunks.mjs`. Each chunk is 4 rows of 5 characters, with role tags (`open`,
  `rock`, `camp`, `cache`, `start`, `exit`) and a weight:

  | Char | Means |
  |---|---|
  | `.` | floor |
  | `#` | wall |
  | `o` | pit |
  | `=` | raised block |
  | `?` `:` `+` | maybe a wall, pit or raised block (else floor) |
  | `e` | enemy slot (in a camp chunk) |
  | `*` | a chest (in a camp or cache chunk) |
- **Areas** (`src/level.c`) are 6–9 maps on a 4×3 grid, grown from the start with one
  or two extra links for loops; the exit is the map farthest from the start.
- **Maps** are rebuilt from their seed each time they're entered. Sizes:
  - start and exit: 4×4 chunks;
  - fields: 6×4 to 6×8 chunks.

  The build:
  - each chunk cell gets a role: open ground, 1–2 rock clusters, 1–2 camps (fields), sometimes a cache along an edge;
  - chunks are picked by weight for that role and flipped at random;
  - edges are wall except a 4-block opening per link, anywhere along the side;
  - openings, camps, chests and the middle are guaranteed connected (a path is carved if a wildcard cut one off).
- **The descent** (`src/descent.c`):
  - walking out through an opening fades to the linked map;
  - camps wake when the player comes within about a screen, and stay cleared once beaten;
  - the exit map has a 2×2 shaft in its middle: dropping in leads to a new area, one deeper, with no fall damage;
  - camp size grows with depth.
- **The visual layer** (engine side, built):
  - A second map says which **visual block** to draw at each spot (64 per theme). The collision map keeps the block types.
  - Each visual block has 4 tiles and 4 **attributes** (palette, VRAM bank, flips), so every 8×8 tile can pick its own palette.
  - Scrolling streams attributes into VRAM bank 1 alongside the tiles, at about 2.8k cycles on scrolling frames. When a theme's attributes are all the same, they're written once at load and streaming skips them.
  - Scenes have background palettes 0–5. The HUD keeps 6–7, and the pause panel borrows 6.
  - For now each visual block mirrors its block type; visual rules will choose variants.
- **Not yet:** chests aren't drawn or opened (they come with loot), there's no minimap, and everything uses the lab's placeholder tiles.
- **Tests** check:
  - 300 seeds for map counts, links, reachability and the farthest exit;
  - every map of 40 seeds for edges, openings and connectivity;
  - determinism, walking between maps, camps waking and staying cleared, and the shaft.
