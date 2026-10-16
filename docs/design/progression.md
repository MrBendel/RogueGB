# Saving and progression

Status: decided where marked; the rest is the working plan. Builds on
[play-loop.md](play-loop.md) (death and reconstruction) and
[skill-trees.md](skill-trees.md) (the Wayfarer's skills).

## Decisions (2026-10-15)

- **Autosave at safe moments.** No save button: the game saves when you enter a
  map, level up, and when a run ends. A continue starts at the entrance of the map
  you were in.
- **Three save files**, each its own character.
- **Light levels.** Guns stay the real power (Borderlands): a level gives a skill
  point and a little more health and shield. Level cap around 30 for the first slice.
- **The reconstruction fee is an eighth of your credits** (12.5%). The docs left
  the percentage open; an eighth keeps the game's math to shifts (no 32-bit
  division on this CPU's library).

## Built

- **Save files** (`src/save.c`): three slots in the cartridge's battery RAM, two
  copies each with a sequence number and checksum, so a write cut short keeps the
  older copy. The file holds the character (class, level, XP, skill points and
  skills, credits, time played, deepest depth, kills, the held gun) and the
  expedition under way (seed, depth, map, cleared camps).
- **The files screen:** START on the title shows the three files (level, deepest
  depth, time played, or "new"); A plays one, B goes back.
- **The playtest page** keeps each game's save files in the browser and puts them
  back on every boot and reset.

- **XP and levels** (`src/progress.c`): a slime is worth 4 XP, a peashooter 6,
  plus a point a depth per kill below the first; clearing a camp 10 + 5 a depth;
  going down the shaft 20 a depth. Level L to L + 1 takes 10 L² + 30 L XP (40 for
  level 2, 9280 for the last of 30); a first area is worth about three levels. A
  level: a skill point, +2 max health, health and shield refilled, a fanfare,
  "LEVEL n" on the HUD, and the file saved.

- **Classes:** a new file picks one of the four Wayfarer classes (Ironclad,
  Volt-Weaver, Ballistician, Ghost-Stalker; skill-trees.md). They share the
  player's look for now.
- **The pause menu** has pages, turned with B:
  - **Character:** class and role, level and XP bar, health and shield, credits,
    time, deepest depth, kills, skill points, and the loadout (each gun in its
    rarity's color, the one held marked).
  - **Skills:** the tree (below).
  - **Options**, kept on the cartridge for every file: text speed (slow, normal,
    fast, instant), screen shake (on, off), sound (off, low, full).
  - **Tune** (the movement and gun tuning panel), only in the lab and the yard,
    which open on it.
  - SELECT in a file **saves and quits** to the title (the expedition goes on from
    that map's entrance); in the lab and the yard it ends the run.
- **Skill trees** (`content/skills/wayfarer.json`, `src/skills.c`): 3 branches x 3
  tiers per class; a skill costs a point and needs the one above it. Learned
  skills save with the file. Ten have real effects so far:

  | Class | Skill | Effect |
  |---|---|---|
  | Ironclad | Plated Mesh | max health +15 |
  | Ironclad | Capacitor Surge | shields recharge 30 frames sooner |
  | Ironclad | Hair-Pin Draw | reloads take half the time |
  | Ironclad | Heavy Rifling | shots fly 25% faster |
  | Ironclad | Expanded Drums | magazines +2 |
  | Volt-Weaver | Blink Dash | longer dash, 4 more invulnerable frames |
  | Ballistician | Rapid Cycle | fire interval -2 frames |
  | Ballistician | Cluster Shot | shotguns +1 pellet |
  | Ballistician | Momentum | firing doesn't slow you |
  | Ghost-Stalker | Hawk Sight | auto-aim reach +32 px |

  The other 26 can be learned and say "(Not in yet.)": they need systems that
  don't exist yet (piercing, burning, decoys, turrets, elites).

- **Loot** (`src/loot.c`, `src/pickups.c`): guns are items: a base type (pistol,
  shotgun, rifle, sniper, sword) at a level and a rarity (common, uncommon, rare,
  epic, legendary: 60/25/10/4/1 in 100, luckier deeper), with bonus points (one a
  rarity step, one every third level) rolled into damage, fire rate, magazine or
  reload, and named for the biggest ("Rapid Shotgun"). A file holds 4 (SELECT
  cycles them), kept through death; a new character starts with a plain pistol.
  - Each camp's chest opens once the camp is cleared; hidden caches any time (and
    a little luckier). A chest drops credits and a gun; a quarter of kills drop a
    few credits. Opened chests stay open for the expedition.
  - Credits are taken by walking over them. Standing on a gun shows a card at the
    top: name and level in its rarity's color, damage, fire rate, magazine and
    reload against the gun held; A takes it, or swaps it for the held gun (which
    drops in its place) when the loadout is full.
  - Enemies have a point more health a depth below the first, so better guns
    matter deeper down.

## Next

1. **Spending credits:** towns and shops (M3); the fee already takes its eighth.
2. **More loot:** shields and relics as drops, gun parts or manufacturers, a
   legendary's special trait, rarity on the floor (a colored glint).
3. **The other skills' systems**, starting with whichever class we play most.
4. **Class looks:** each class its own sprite (skill-trees.md names them).
