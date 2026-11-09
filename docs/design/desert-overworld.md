# Desert overworld

Status: first playable section implemented 2026-11-08; the full eight-map region
below remains a working proposal. Requested: a connected desert world around
the Sands town, with the exploration feel of Borderlands.

## Playable now

New characters begin south of Crossroads in **Dustwake**, a 20x24 desert map.
The unknown entity directs them to a guaranteed shield, then past two groups of
three spitters toward Crossroads and town. See [opening](opening.md) for the
sequence, controls and retry behavior. Crossroads has a two-way southern road
back to Dustwake. Existing saves retain their normal start.

Walk south out of the town's container yard, along the pale road beside the
waymarker. Dust Crossroads is a safe 20x16 map with a cache, a water-tower landmark
and a direction post. Its east edge leads to Rustyard: another 20x16 map with
container cover, riflemen and patrol hounds, reinforcements and a guarded chest.
Walk west back to Crossroads, then north to town. Hold toward a map edge briefly
to cross it, as in the existing descent. START opens the surface map.

This slice uses fixed maps; camps and chests stay cleared/open while backtracking,
and refresh when you return to town and begin another outing. Battery saves resume
the correct surface map. Death uses the existing reconstruction fee and returns
to town once the introduction is complete; during it, retries start in Dustwake. Normal returns preserve current health and inventory. Floor loot follows
the existing descent behavior: collect it before leaving a map.

The town lift still starts the generated underground expedition. The surface does
not yet connect to an excavation, add persistent shortcut unlocks, or offer new
contracts. Region names identify the three maps on the pause screen. The southern
town exit adapts the proposal below to the existing authored settlement; the
full-region graph can be oriented around it as more maps are built.

Implementation: `WORLD_DESERT` uses the existing expedition save fields, graph,
camp controller and loot system. `desert.c` supplies an authored three-map graph and
map metadata, `sands_overworld.py` composes the maps for `sands-town.py`, and
`room_load_sands_tiles` shares the town atlas with the surface. No save-format
change is required. The shared atlas uses 214/220 tiles, 97/128 blocks and six
palettes; the extra light-sand fill and flipped ground edge reuse stored patterns.

## What the player does

Leave town on foot, follow a landmark, fight over salvage, find a side route,
and bring something useful home. The surface introduces the planet and its
conflict; excavation entrances lead into the existing repeatable descents.
Start with one coherent desert planet before adding other planets.

Recommended structure: recognizable surface geography, with refreshed encounters
and loot on a new expedition. Generated underground areas retain the existing
roguelite loop. This would revise the current generated-only expedition structure;
the first playable section uses fixed surface routes following approval to proceed.

## First region: the Dry Reach (working name)

Eight connected maps, including the existing town. Lines below are walkable,
two-way connections; the central dotted connection is an unlockable shortcut.
The gaps are transitions between maps, not empty travel screens.

```text
Echo Bluff -------- Meridian Dig -------- Pumpworks
    |                    :                   |
Splitjaw Pass ----- Dust Crossroads ----- Acid Wash
                         |                   |
                     Sands Town -------- Rustyard
```

Town, Crossroads and Rustyard form the first playable section. The two longer
routes converge on Meridian Dig, giving a reason to explore both sides. Opening
the Dig's south gate completes a short return route to town.

| Map | Shape and landmark | Encounter and reward |
|---|---|---|
| Sands Town | Existing 16x19 settlement; shop, relay, homes and stacked containers | Safe hub, contracts, selling and recovery; preserve current interiors |
| Dust Crossroads | 20x16 open junction around a broken communications mast; north gate visible but closed | Light patrol, first cache, clear signs toward both routes |
| Rustyard | 20x16 container lanes and a broad clearing around a wreck | Mercenary camp, gun chest, optional supply errand; no enemies in the arrival lane |
| Acid Wash | 20x16 dry riverbed with small acid pockets and sandy islands | Acid skimmers; an optional cache reached by a timed jump, with a safe main path |
| Pumpworks | 20x16 industrial yard; tank silhouette and short enterable service building | Mercenaries guarding a supply stop; building provides a second route through the site |
| Splitjaw Pass | 10x24 winding canyon, alternating wide fighting pockets and narrow bends | Acid wolves; readable approach and landing space; side alcove containing salvage |
| Echo Bluff | 20x16 elevated relay site with a visible return ledge | Cragback encounter and Echo transmission pointing to the ancient structure under the Dig |
| Meridian Dig | 20x24 terraced excavation, central drill and sealed descent entrance | Region objective, guarded relic, shortcut gate; first prototype reuses a proven miniboss |

Existing trial enemies are encounter candidates. A trial is not automatically
available in the campaign actor pool: check its integration, acid rendering and
sprite budget before assigning it to a live map. No new boss is required just to
validate the overworld.

## First journey

1. A town contract asks for a component from Rustyard. The player can see the
   broken mast beyond the town exit before accepting a job.
2. Rustyard teaches that a place has cover, a guarded reward and an onward route.
   Returning to town is always possible; a job never locks the player in a map.
3. At Crossroads the player chooses wildlife in the canyon or corporate salvage
   sites by the wash. Both lead toward the Dig. Neither route requires clearing
   every camp.
4. Echo's relay supplies context for the buried structure. Side errands reuse
   familiar destinations with new objectives rather than replacing their layout.
5. The Dig encounter opens its descent entrance and the south shortcut. A later
   expedition can head directly there or revisit the longer routes for loot.

## Layout and art rules

- Give every map one recognizable landmark and a distinct route shape. A palette
  swap alone does not make another region.
- Use the authored Sands cliffs, ground transitions, stacked containers and
  building assemblies. Preserve connected silhouettes; do not scatter arbitrary
  source tiles or repeat disconnected cliff faces.
- Keep ochre sand continuous across connections. Use rust in Rustyard, sage and
  acid accents in the wash, warm rock in the pass and cool corporate metal at the
  Dig. Palette variants must keep exposed ground colors consistent.
- Ground routes are at least two blocks wide except deliberate doorways. Fight
  spaces need room to dodge; place narrow bends between encounters.
- Place reciprocal exits at corresponding edges, with the full player hitbox on
  clear landing tiles. Do not spawn enemies, props or loot on transition cells.
- Cliffs block walking. Mark jump-down routes and provide a walking route back;
  avoid trapping a player who leaves a ledge without a movement upgrade.
- Tank tops and roof overhangs can cover the player; supports remain solid. Keep
  tall foreground props away from critical combat telegraphs.
- A side route earns its space with loot, a viewpoint, an encounter or a shortcut.
  Avoid long empty corridors and mandatory repeated fights on a return trip.

## Repeat visits and progression

Proposed persistence rules:

| State | On leaving and returning to a map | On starting another expedition |
|---|---|---|
| Geography and named landmarks | Same | Same |
| Opened shortcut and story objective | Kept | Kept in the character's campaign |
| Cleared camp and opened chest | Kept for this expedition | Refresh |
| Uncollected floor loot | Follow the existing bounded loot persistence system | Reset |
| Unfinished combat encounter | Follow current campaign reset rules | Refresh |

Save changes need explicit versioning/migration. Do not repurpose the current
depth counter as a surface location: returning from a descent must restore the
correct surface entrance. Preserve the user's existing character and inventory.

## Build sequence

1. **Town, Crossroads, Rustyard:** author connected maps, a visible town exit,
   reliable return transitions, one camp and one chest. Use this to judge scale,
   navigation and art before multiplying content.
2. **Complete both loops:** add the canyon and wash routes, region names on the
   pause map, landmarks, caches and encounter variety.
3. **Dig and descent:** add the objective, shortcut unlock, descent connection and
   return location. Add campaign persistence once location IDs are stable.
4. **Repeatable trips:** reset camps/loot at an explicit expedition boundary,
   connect contracts to named destinations, tune rewards and travel time.

The first section is done when someone can leave town, find the guarded chest,
return without a teleport or collision snag, and describe the route without a
debug overlay. The complete region is done when both loops and the unlocked
shortcut work, every destination is reachable, and save/resume preserves location.

## Engine fit and verification

The engine already has edge transitions, backtracking, camp persistence, loot,
indoor maps and a pause map. Its current area graph is 4x3 cells, enough for this
layout. Maps remain at most 32x24 blocks; only the active map is loaded.

Implementation needs an authored surface map provider, a shared Sands loader
(currently town-specific), a surface connection to town, and saved location
identity distinct from descent depth. Keep authored layouts in banked ROM, not
additional always-resident RAM. Existing saves and generated descents must still
load normally.

Sands currently uses 214 of 220 hardware tiles and 95 of 128 visual blocks.
Compose regions from those pieces, reuse flipped patterns, and split regional
atlases only when actual new art exceeds the per-map budget. Do not expand the
eight-enemy pool for a larger world.

Verify graph reciprocity, flood-fill reachability, entry hitboxes, both directions
of every transition, camp/chest state after backtracking, interior return points,
save/resume and death recovery. Inspect whole-map renders and in-game screenshots
for cliff continuity, ground seams and misleading doors. Profile the busiest
fight with the normal hardware and frame budgets.
