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

## Next

1. **First finds** as an XP source (relics, new guns) once loot exists.
2. **The profile screen:** class, level and XP bar, credits, time, finds.
3. **The Wayfarer's skill tree:** the 3x3 triad from skill-trees.md, its screen,
   and the skills' real effects.
4. **Credits:** from camps and chests; spent at towns (later).
