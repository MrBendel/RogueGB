# Level generation, depth, and towns

Status: agreed direction (2026-10-12) with working numbers. See [decisions](decisions.md).
Numbers below are starting targets to tune in playtests, not fixed rules.

## Agreed direction

- **Expedition levels are generated in game** from the run's seed, so each trip is
  different. The same seed always gives the same level (replays, bug reports, tests).
- **Rooms are built from authored chunks**, not generated freely. Designers make
  small room pieces with wildcards; the generator picks, flips, and combines them.
- **Rulesets decide how visuals group together.** The generator places logical
  blocks; a visual ruleset per theme picks the tiles, palettes, and decorations.
- **Towns are authored**, not generated: fixed, familiar places on each planet with
  NPCs, shops, bars, missions, and more. They contrast with the changing descent.

## Three layers of generation

### 1. Layer layout: which rooms, and how they connect

A small grid of rooms with a guaranteed main path from the landing (or the shaft
from above) to the way down, plus optional branches.

- **Guarantees:** the objective is reachable, a way back exists, and the room-type
  mix fits the depth. Every seed is checked by the test harness, so no unwinnable
  layouts ship.
- **Landmarks** are fixed pieces the generator must place: the relic chamber, an
  Echo relay, a guardian arena, a beacon. Story beats survive randomness this way.
- **Room types** (from the design plan's vocabulary): entry, combat, connector,
  cache, event, rest/beacon, challenge, guardian, core. Build only what the first
  expedition needs.
- Missions taken in town can request content from the generator ("recover X from
  layer 2", "clear the corporate site").

### 2. Room shape: authored chunks

- A **chunk is a quarter screen: 5×4 blocks** (80×64 px). Rooms are assembled from
  chunks on a grid.
- Chunks contain **wildcards**: "maybe a wall", "an obstacle cluster from this set",
  "an enemy slot", "a pit or a bridge", "a decoration slot". The generator fills them.
- Chunks can be flipped horizontally and vertically to multiply variety.
- Each chunk declares which edges are open, so neighbours join cleanly and exits line up.

### 3. Visuals: rulesets

The generator only places logical blocks (floor, wall, pit, raised, decoration
slots). A **theme** (per planet or depth layer) supplies:

- **Autotiling:** a block's tile depends on its neighbours (corners, edges, ends).
- **Grouping rules,** for example:
  - cracks only on walls next to floor;
  - moss in clusters near water;
  - pillars in pairs;
  - rubble within two blocks of a collapsed wall;
  - this decoration never twice in view.
- **The tileset and palettes,** so the same layout looks like a different place.

Rules are data, authored in a tool and converted to ROM tables like sprites are.
Engine work needed: rooms use one background palette today; grouping by palette
needs per-tile palette attributes streamed with the map.

## Room size

The playfield is one screen: 10×8 blocks (160×128 px).

| Room type | Size | Purpose |
|---|---|---|
| Connector / corridor | 1 screen (2×2 chunks), often long and thin | Pacing, a trap, a single enemy |
| Standard combat | 1.5–2 screens (3×2 to 4×2 chunks) | The main fights |
| Set piece | 2×2 screens (4×4 chunks) | Guardian arenas, relic chambers, landmarks |

Why cap rooms around two screens:

- Ranged enemies must be on screen to be fair; the view is small.
- Eight enemies at once is the sprite and CPU budget; it fills one or two screens.
- A room should be readable in a glance or two.

## Depth

The descent goes toward each planet's center in **layers**.

| | Rooms | Time | Contents |
|---|---|---|---|
| One layer | 8–12 | 6–10 min | Main path to the shaft down, 1–2 optional branches, a cache, a beacon, a landmark |
| Full descent | 4 layers | 30–40 min | Surface, two middle layers, the core with its guardian |
| Repeat trip | 1–2 layers | 10–20 min | Start deeper at an unlocked beacon |

- Combat rooms take about 30–60 seconds; connectors and caches 10–20.
- A **beacon** ends each layer: a save and bail-out point, and a landing site for
  later trips (see "unlocked landing depths" in the play loop).
- **The opening** is a shortened layer (6–8 rooms) on a fixed seed with fixed
  landmarks: it plays like a hand-made tutorial through the same pipeline.

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

- Planet count, and whether later planets have more layers.
- Exact rooms per layer and room-type mix (from playtests).
- The authoring tool for chunks, rulesets, and towns: a tab in the Sprite Studio, or
  Tiled with a converter.
- Town size and structure (one screen hub, several connected screens, interiors).
- Currency, prices, and what shops and bars actually offer.
- Mission types and how many can be active.
- Whether layer layouts persist between trips (landmarks persist; routes remix).
