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
- **The pause menu** has pages, turned with B: **Profile** (class, level and XP
  bar, credits, time, deepest depth, kills, skill points), **Skills** and **Tune**
  (the movement and gun tuning panel; the lab and the yard open on it).
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

## Next

1. **The other skills' systems**, starting with whichever class we play most.
2. **Credits:** from camps and chests; spent at towns (later). The fee already
   takes its eighth.
3. **First finds** as an XP source (relics, new guns) once loot exists.
4. **Class looks:** each class its own sprite (skill-trees.md names them).
