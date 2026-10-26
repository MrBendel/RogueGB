# What makes a good run

Status: direction decided 2026-10-22 (see "Decided"); numbers are proposals. Builds on the expedition loop
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

## Decided (2026-10-22)

- **The shape: missions, some of which end in a boss.** A blend of "a few areas then a
  boss" and missions from town: the mission picks the destination, the goal and how
  many areas it takes (not always three). Scouting or retrieval missions can be one
  or two areas; a boss mission runs several areas to its boss. See "Missions" below.
- **Minibosses are mandatory:** one guards each area's way down.
- **Scaling:** enemies are the area's level, never more than 2 below the player's.
- **Vending machines in the descent: ammo and health only.** Gun and shield vending
  machines are in **town** (beside Marro), not in the descent.

## The entrance room (built 2026-10-22)

Every area begins in an **entrance room**, like the entrance of a Zelda dungeon: you
arrive through its **south door** (the way back to town: walk out of it to leave, with
everything), **doors on 2 or 3 of its other sides** lead into the area, and a **comms
device** gives the objective when you talk to it (for now a general briefing from
Meridian dispatch; with missions, the mission's own). The generator puts it on the
area grid's bottom row, so nothing lies south of it.

It's **indoors**, drawn with the LunarBase interior tileset
(`content/tilesets/lunar_int.json`), while the area's other maps use the exterior one
(descent.c loads the tileset by map). It's **made by hand**: the map `entrance` in
that tileset, 10x12 blocks (2x3 chunks: it scrolls a little), two rooms deep after
the artist's reference (2026-10-23):
- **The lower room** (you arrive here): the studded floor with cyan floor lights,
  orange couches, black-topped counters along the south wall. Its back wall is a white
  wall across the room with a doorway in the middle, and a machinery face below it
  (orange lights, riveted plates, screens, a vent) casting a shadow on the floor.
- **The upper room**: a floor of shiny square plates. On its back wall, the **comms
  device** (the framed monitor, 2x2 blocks: `COMMS_X`, `COMMS_Y` in level.h) between a
  starfield window and orange lights. You talk to it from below: the two blocks under
  it are talkers (`TALK_COMMS`, with no sprite). The east and west doors open off
  this room.
- **Doorways** are one block (16 px) wide. The map has the outer ones closed: level.c
  opens the ones the area links to (the doorway, the jambs either side). The north
  one is beside the monitor (`ENTRANCE_DOOR_N`) and cuts through the wall face.

The side doorways are stand-ins: the sheet has no side-on doorway. Doors that open
and close need the artist's art.

## The miniboss at the shaft (built 2026-10-25)

An area's exit map guards its shaft: within 3 blocks of it (a file's descent), the
game saves the expedition and goes to the area's miniboss's arena (MODE_MINIBOSS:
the Claimjaw, Scrapback and Cragback in turn with depth; descent.c
`descent_miniboss`). Won, a moment for the "CLEAR", then XP (`XP_MINIBOSS` x depth),
and back to the descent, rebuilt from the file, beside the shaft, open: the beaten
flag is bit 7 of the exit map's chest bits (`AREA_BOSS_BEATEN`), so it's saved.
Dying there ends the run like dying anywhere in the descent. Still to do: the arena
in the world's own art (it's the combat trials' room), your health carried in
(entering refills it now), the vendor post before it.

A whole area now plays through: the entrance room's briefing, its maps (camps,
chests, buildings to go into), the miniboss at the shaft, and down to the next.

## Containers (built 2026-10-25)

Lockers, chests and loot boxes stand against an exterior map's set pieces (level.c
`place_containers`): 3 lockers, a chest, and a loot box one map in two, each beside
a set piece's front and kept only if everything still connects (they're solid:
cover). Walk up to one and it opens (pickups.c): its open look, and its loot in
front of it: a locker, credits and rounds for a gun you carry; a chest, a gun and
credits; a loot box, a better gun (luckier), credits and rounds. Opened ones stay
open for the expedition (`area_conts_opened`, saved as `profile.exp_conts`; save
version 6, which still reads version 5 files).

The art is shared by every world (the user's lockers, chest and gun box from the
ice sheet: `artwork/tiles/containers.png`, in LunarBase's light lilac, dark grey
and orange so they sit in its palettes); a world's tileset stamps it over its
ground (tileset `"sheets"`, a piece's `"stamp_from"`).

## Missions

The board in town (a terminal or an NPC) offers a few missions at a time; each says
where, what and how far:

| Kind | Goal | Length | Ends |
|---|---|---|---|
| Scout | reach an area's exit map | 1-2 areas | its miniboss, then extraction |
| Retrieve | find a marked item (a relic, a log, a part) | 1-3 areas | the item's map; extraction |
| Bounty | kill a named elite somewhere in the areas | 2-3 areas | the elite; extraction |
| Boss | reach and beat the region's boss | 3-5 areas | the boss arena; extraction; the next region opens |

- Every area still ends in its miniboss (the way down). A mission that's done calls
  the extraction ship at the next lift or the current area's exit, so you can go home
  with everything (or keep going, for more risk and loot, until you lift out).
- The areas, their levels and the boss come from the mission's region and depth.

## The shape (first proposal, before the decisions)

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
- **Vending machines** (one sprite, recoloured per kind, as the artist planned).
  In the descent: **ammo and health** only. Placement: the start map always has ammo;
  the vendor post has health (and maybe ammo); one of each before every miniboss and
  boss arena; never two of the same kind in one map. Prices scale with the area's
  level. **Guns and shields** are sold by vending machines in town (and Marro).

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

## Still open

- How missions are offered (a board, Echo, NPCs) and how many at once.
- Extraction: a ship at the next lift, or anywhere once the goal is met?
- Whether "deep space" endless diving exists, and what it rewards.

## Order (once agreed)

1. Landmark maps in the generator (vendor post, miniboss arena, cache) and the viewer's
   checks for them.
2. Vending machines (one sprite, recoloured; the shop page, stocked by kind): ammo and
   health in the descent, guns and shields in town.
3. Minibosses in the descent: the trials' arenas as landmark maps, the shaft gated.
4. Enemy levels and the scaling tables; squad templates; elites.
5. Missions: the board in town, mission kinds and lengths, beacons, the boss arena,
   extraction.
6. More roles (tank, support) and the lunar re-skins.
