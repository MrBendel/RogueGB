# Memory: where things live, and how the game grows

Status: 2026-10-27. Decisions: one world at a time; far RAM for cold data; the world
table and the bosses' shared RAM are in.

## The budgets

| Memory | Size | Used | The limit that matters |
|---|---|---|---|
| ROM (cartridge, MBC5) | 16 KB banks, up to 512 (8 MB) | ~12 banks of a 256 KB ROM; it grows by itself | **bank 0** (always mapped: hot code), 90% cap in the build |
| Work RAM bank 0 (0xC000) | 4 KB | ~96%: globals | |
| Work RAM bank 1 (0xD000) | 4 KB | globals' overflow, the room map, looks, block tables, the stack | the build prints the bytes left before the room map |
| **Work RAM banks 2-7** | 6 x 4 KB | bank 2: far RAM (below) | |
| Video RAM | 2 x 8 KB | per scene: 176 background tiles, 128 blocks, 6 + 2 palettes, sprite tiles | per map, not per game |
| Save RAM (battery) | 8 KB (MBC5: up to 128 KB) | three files, settings | |

Bank 1's layout (src/room.h): globals up to 0xD3BF, the bosses' state 0xD3C0
(below), the room map (block types) 0xD400, the looks 0xD700, the block tables (a page per quadrant) 0xDA00, the stack
0xDE00-0xDFFF. Maps are at most 32 x 24 blocks.

## Detailed authored backgrounds

Sands town explicitly uses a second VRAM bank for background detail. Its first
172 tiles use the normal scene range in bank 0; up to 48 additional tiles use
bank 1 background indices 80..127 ($9500..$97ff), above the weapon and portrait
sprite area. The authored loader splits uploads and sets attribute bit 3 per tile.
`vramBanks: 2` opts into a 220-tile compiler budget; generated themes cannot opt in
without a loader change. Visual-block RAM remains limited to 128. Scene exit and
menu/dialogue attribute restoration still apply normally.

## The bosses' shared RAM (src/bosses.s, room.h BOSSES_ADDR)

Only one boss fights at a time, so the boss files (tools/build.mjs `BOSS_FILES`:
the Cragback, Flynt, Globfather, Gunship, Scrapback, Warden) compile their globals
into one area, `_BOSSES`, which the linker overlays (OVR) at 0xD3C0: each file's
state starts there and the biggest sets its size (43 of 64 bytes). Their names
stay as they were (tests and the harness read them). `play_enter` zeroes the area
for every fight, so each boss starts as if the console were just switched on.
It freed ~160 bytes. A boss belongs in the list only if nothing outside its own
fight touches its state; the ordinary enemy kinds (acid wolf, bulwark, ...) stay
out: several may share a level, and play.c clears the webs every frame. The build
checks the area's place and size; tests/banks.test.mjs checks the overlay.

## Far RAM (src/far.c, src/far.h)

Work RAM banks 2-7 share the 0xD000 window with bank 1, and GBDK doesn't support
switching them (the stack is in that window). far.c copies a byte at a time: bank
2 in, the byte into a register, bank 1 back, the other side's write; the stack is
never touched while bank 2 is in, interrupts are off throughout. It's banked code
(no room in bank 0), so for cold paths only: data that's needed now and then,
copied in or out. Each thing in it has a fixed address in far.h.

Uses so far: the background palettes the pause menu borrows (`FAR_MENU_PAL`).
Candidates next: a world's cold tables, dialogue text, the containers' opened bits.

Also: data only needed while a map is built can borrow the looks map (`room_vis`)
as scratch before its looks are drawn: the chunk tables are copied there for each
build (level.c), and the flood fill's ring uses its first 512 bytes.

## One world at a time (decided 2026-10-25)

A run only ever visits one world (LunarBase now; others later), plus the town. So:
- **Per-world data is loaded on entering the world** (tilesets, chunk set, enemy
  roster and art, its miniboss and boss), and only the current world's counts
  against video RAM, RAM and CPU.
- **World-specific state shares one RAM region** (a union, one member per world):
  the biggest world sets its size; adding worlds adds no RAM.
- **The world table** (src/world.c, world.h) lists each world's parts: its map
  pieces (chunk theme), its camps' enemy roster (a tier per depth), its minibosses in turn with
  depth, its enemies' two palettes (creatures, machines), and its tilesets
  (world_load_tiles). A run enters a world (world_enter; a continue, the save's
  exp_world) and the descent, level generator and miniboss flow read from it. LunarBase is
  world 0 and the only one so far; a new world adds a row, its tilesets and a case
  in world_load_tiles. Still LunarBase's own: level.c's looks (the set pieces,
  interiors and entrance use its piece names); a second world gives them its own
  look functions or shares the piece names. The world isn't saved yet: a later
  field in the profile reads as 0 (LunarBase) from older files.
- Shared everywhere: the player, guns, HUD, loot, camera, the town, and things the
  user wants the same in every world (lockers and loot boxes).

Town HUD names use seven on-demand glyph tiles at 44..50 instead of the full
64..127 font. Pause/menu text borrows the detail range while covering the scene;
HUD initialization reloads the detail tiles on close. Dialogue/loot-card tiles
192..253 and weapon sprite tiles remain separate.

Town floor-loot caches: far bank 2, 0xD040..0xD23F, four 128-byte slots.
Each room keeps its six drop entries, three gun records and icon frames. Near RAM
uses a valid-room bitmask; room changes do not copy an entire map or reset inventory.
