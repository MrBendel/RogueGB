# What makes a good run

Status: proposal (2026-10-22), for discussion. Builds on the expedition loop
([play-loop.md](play-loop.md)), the generator ([level-generation.md](level-generation.md)),
the trials ([bosses.md](bosses.md), [enemies.md](enemies.md)) and progression
([progression.md](progression.md)). The world viewer (web/world.html) is how we'll
measure it.

## Where runs are today

- Town → the lift → an **area** of 6-9 linked maps → the exit map's shaft drops into
  a deeper area, without end. The start map's lift goes back up (everything kept);
  death goes back too, minus an eighth of your credits. Autosave on every map.
- **Enemies in the descent:** slimes (chasers) and peashooters (shooters) in camps.
  Everything else is a standalone trial from the title menu: enemies Acid Skimmer,
  Acid Wolf, Mercenary; minibosses Claimjaw, Scrapback, Cragback; bosses Relay
  Warden, Captain Flynt.
- **Scaling:** +1 enemy health per depth, bigger camps deeper, luckier chests; guns
  roll at your level. Nothing scales with your power, and nothing ends a run but you.

## The shape (proposed)

A run is an **expedition**: a few areas that build to a boss, then a way home.

```
town ──lift──▶ area 1 ──▶ area 2 ──▶ area 3 ──▶ boss ──▶ extraction (home with everything)
               (miniboss) (miniboss) (miniboss)
                  ▲ beacon     ▲ beacon     ▲ beacon: next time, start here
```

- **Three areas, then the boss.** Each area ends in a **miniboss** guarding the shaft
  down. After the third, the boss arena; beating it unlocks the next region (planet
  part, story beat) and calls the extraction ship.
- **Length:** an area of 5-7 maps at about 2 minutes a map is 10-15 minutes; an
  expedition 35-50 minutes, in pieces (it saves on every map, and you can quit).
- **Push your luck:** after the boss, the shaft keeps going ("deep space"): harder,
  better loot, no boss, until you lift out or die. Optional, for farming.

## Each area: what's in it

The generator places these as **landmark maps** (the layout already knows the start,
the exit and the farthest map):

| Map | Where | What |
|---|---|---|
| **Start** | as now | the lift (up to town, or a beacon), a **vending machine** (ammo), no enemies |
| **Fields** | most maps | camps, crates, the theme's buildings |
| **Cache** | a dead end | no camp; a chest worth the detour, maybe locked behind a puzzle or a jump |
| **Vendor post** | about halfway | 1-2 vending machines (a different kind than the start's), a breather |
| **Miniboss arena** | the exit map | an authored arena chunk in the middle; the shaft opens when it falls; a guaranteed good chest |
| **Story beat** | some areas | an Echo relay, a log, a named NPC (authored, inserted like a landmark) |

The viewer's survey reports per area: enemies, chests, vending machines, landmark
placement, and the walk from start to exit, so we can see every seed meets the plan.

## Enemies: a roster by role

Camps are **squads** built from roles, so a fight is a problem to read, not a crowd:

| Role | Job | Have | Lunar outpost version |
|---|---|---|---|
| Chaser | closes in, punishes standing still | slime | maintenance drone |
| Shooter | holds range, makes you move | peashooter | security turret |
| Duelist | strafes and trades shots | Mercenary | corporate merc |
| Flanker | fast, comes from the side | Acid Wolf | patrol hound |
| Area denial | leaves hazards | Acid Skimmer | coolant leaker |
| Tank / shielded | soaks damage, wants electro or acid | (new) | armored loader |
| Support | heals or shields the others | (new) | medic drone |

- **Squad templates** by depth: area 1 is chasers and shooters; duelists and flankers
  join in area 2; tanks and support in area 3. A camp is 3-6 enemies.
- **Elites** (Borderlands' "badass"): one in some camps: more health, a name, one
  modifier (shielded, fast, acid trail, explodes), and a better drop.
- **Themes pick the look:** the roles are code; each planet re-skins them (lunar: drones
  and mercs; ice: wolves and skimmers), within the sprite budget.

## Minibosses and bosses

- **Minibosses** (Claimjaw, Scrapback, Cragback, more from the candidate roster): one
  per area, in its exit map's arena. Which one: from the area's theme and depth.
  Mandatory (the way down), so the run has beats; the arena is skippable on repeat
  trips only by starting deeper (beacons).
- **Bosses** (Relay Warden, Captain Flynt, then the candidate roster): one per
  expedition, after area 3, in its own arena map. Each teaches its own pattern; the
  late "Network Bailiff" combines them.
- **The final boss** of a planet is the last expedition's: the story's corporate
  heart (Meridian). Proposal: the Network Bailiff (relays and hazard lanes, taught
  separately by earlier fights), with a second phase that uses the player's own gun
  manufacturer against them.

## Saving, beacons, vending machines

- **Saving:** keep autosave on every map (a handheld: quit anytime, resume at the map's
  entrance). No save points to find.
- **Beacons** (the lifts): each area's start map. Reaching one unlocks it in town: the
  next expedition can start at area 2 or 3 (but not past the boss you haven't beaten).
  Beating the boss unlocks the next expedition. This is the "checkpoint" that matters.
- **Vending machines** (one sprite, recoloured per kind, as the artist planned): ammo,
  shields, guns, health. Placement rules: the start map always has ammo; the vendor
  post has one or two others; one before every miniboss and boss; never two of the
  same kind in an area. Prices scale with the area's level.

## Scaling: the player gets stronger, and so does the world

Two axes: **the area's level** (from depth) and **the player's power** (level, gear,
skills). Proposed, Borderlands-style:

- **Enemy level = the area's level**, but never more than 2 below the player's level
  (going back to area 1 at level 20 isn't trivial, but it's easier: you feel stronger).
- **Area level** rises with depth: area 1 of the first expedition is level 1, each area
  +1, each expedition +3. So the level-cap curve is the content curve.
- **Per enemy level:** health base + base x level / 8 (a table lookup: no division on the
  Game Boy), damage + level / 4, elites from area 2, more per camp deeper.
- **Loot drops at the enemy's level**, rarity luck from depth (as now). XP and credits
  scale with the enemy's level, less for enemies far below you.
- **Bosses** scale with the area's level like everything else; their patterns get one
  extra beat per expedition (faster, more bullets), so a replay is a remix, not a sponge.
- **Shop and vending prices** scale with the area's level.

The lab and the trials stay at level 1, for tuning.

## To decide

1. The expedition shape: three areas and a boss (proposed), or endless with a boss every
   N areas?
2. Minibosses mandatory (the way down) or optional (better loot)?
3. Scaling: enemies at the area's level with the "no more than 2 below you" floor
   (proposed), or fully scaled to the player, or fixed by depth only?
4. The vending kinds: ammo, shields, guns, health: any others (skins, upgrades)?

## Order (once agreed)

1. Landmark maps in the generator (vendor post, miniboss arena, cache) and the viewer's
   checks for them.
2. Vending machines (one sprite, recoloured; the shop page, stocked by kind).
3. Minibosses in the descent: the trials' arenas as landmark maps, the shaft gated.
4. Enemy levels and the scaling tables; squad templates; elites.
5. Expedition structure: beacons in town, the boss arena after area 3, extraction.
6. More roles (tank, support) and the lunar re-skins.
