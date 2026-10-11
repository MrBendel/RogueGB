# RogueGB roadmap

A top-down roguelite shooter for the Game Boy: Rogue Legacy's run and heir structure
with Borderlands-style guns. See [docs/inspiration/](docs/inspiration/README.md) for
the design notes behind it.

## Decisions so far

| Topic | Decision |
|---|---|
| View | **Top-down**, at the scale of the Capcom-era (Oracle) Zelda games: 16×16 characters drawn as 2 hardware sprites in 8×16 mode, and 16×16 map blocks. Matches the `artwork/8bitadventure` pack |
| Rooms | **Mixed sizes.** Most rooms are one screen (10×8 blocks) and slide to the next one, Zelda style. Some are bigger (2×1, 1×2, 2×2 screens) with a camera that follows the player, for arenas and bosses. Room data allows any size from the start |
| Graphics | **Pre-rendered.** Nothing is drawn or combined pixel by pixel at runtime. Characters get one baked body set per **weapon type** (unarmed, melee, gun), not per weapon, so the 10-sprites-per-line budget stays at 2 sprites per character. Right-facing frames are mirrored for left. All frames live in ROM; VRAM holds only the frames on screen, swapped in when the animation changes (4 tiles = 64 bytes). Specific guns show in the HUD and pickup popup, not in the character's hands. Any combining of layers (body + weapon) happens in the build tools, not in the game. Cheap runtime effects: sprite flips (sprites only; DMG background tiles can't flip, so mirrored background art is pre-rendered by the build), palette changes, sprites stacked by the hardware where the line budget allows |
| Movement | Eased 8-way movement with per-axis acceleration, deceleration and reversal. **Default: SNAPPY at 1 px/frame** (5 frames to top speed, 4 to stop, 3 to turn around), chosen in the movement lab with the real player sprite (2 px/frame felt right with a placeholder box, but too fast with the character art). **Each character has a movement profile** (top speed + those three frame counts) from a data table, so heir classes, traits and enemies can move differently |
| Dynamic content | Only **data** is generated at runtime: floor layouts, room picks and mirroring, spawns, gun stats. It runs during fades, so it has no per-frame budget |
| HUD | One block row (16 px) on the window layer. **Bottom of the screen for M1**, because the hardware does that for free. A top HUD like Zelda's needs a scanline interrupt; it's an optional M6 polish item |
| Hardware | **DMG first** (4 shades), with GBC color added later in M6. Gameplay can't depend on color, so elements are shown by bullet shape and impact effect |
| Order | Get the **combat feel** right in one room before building systems around it |
| Tracking | This file has the overall plan. GitHub milestones match the sections below. Issues are filed only for the next one or two milestones |

### Controls

| Button | Action |
|---|---|
| D-pad | Move in 8 directions (the most a d-pad gives) |
| A | Fire the active weapon. **Auto-aim** picks the target (see below), so you can move freely, including backing away, while shooting |
| B | **Jump.** A second tap in mid-air **rolls** out of the jump toward the d-pad direction (or the facing direction) |
| A + B | **Class ability** (M4), when it's charged. When it isn't, the buttons do their normal jobs |
| SELECT | **Tap:** cycle to the next weapon in the loadout. **Hold:** open the weapon wheel |
| START | Pause, map (M2) |

**A + B has to mean pressing them together, not "B while A is held".** Players hold A to keep firing, so
"B while holding A" must still be a jump. The rule:

- The ability triggers when A and B are both **newly pressed within a few frames** of each other (about 4, to be tuned).
- If B comes first, the jump starts immediately. If A arrives within that window, the ability cancels the jump's first frames, so jumps never feel delayed.
- If A comes first, the shot fires immediately, and B within the window turns it into the ability.
- If the ability isn't charged, the press counts as a normal jump and shot.
- The input code (M1) should support detecting presses within a window, and tests should cover each order of presses.

### Auto-aim

This is a casual game, so the player picks *when* to shoot and the game picks *what* to shoot at.

- Holding A fires at the **nearest enemy in line of sight**, with a bias toward the facing direction, so you tend to shoot what you're running at.
- **Sticky targeting:** the target only changes when it dies, leaves line of sight, or another enemy is clearly closer. This stops aim from jittering between two enemies.
- A small **reticle sprite** marks the current target, so auto-aim is never a mystery.
- Aim isn't limited to 8 directions. Bullets use a 32-direction velocity table, so they fly straight at the target.
- With no target, shots go in the facing direction.
- Tuning to do in M1: aim range, the facing-bias angle, and whether some weapons (shotguns, lobbed grenades) aim differently.

### Jumping and height

Jumping makes the top-down world partly 3D, Link's Awakening style:

- The player has a height (`z`) as well as x/y. The sprite is drawn raised by `z` with a **shadow** on the ground, and jumping follows a short gravity arc.
- **Over enemies:** while high enough, ground enemies and ground-level bullets pass underneath. Flying enemies and "high" bullets can still hit.
- **Over holes:** pits are floor tiles you fall into if you land on them (lose some HP, respawn at the edge). Gaps up to about 2 blocks can be jumped.
- **Simulated platforms:** some blocks are **raised**. You can jump onto them and walk along them, and dropping off the edge lands you on the floor. Later: moving platforms over pits.
- Rolling out of a jump gives a burst of distance and a few invulnerability frames, so you can dodge in the air or extend a jump over a wider gap.

### Weapon loadout

- Up to **4 weapons**. Tapping SELECT cycles through them.
- Holding SELECT (about 12 frames) opens a **weapon wheel**: the 4 slots sit on the d-pad's
  up/right/down/left. Press a direction to highlight a slot, release SELECT to equip it.
- While the wheel is open the game pauses or drops to slow motion (to decide in playtests).
- Tapping SELECT acts on release, so a tap and the start of a hold can't be confused.

## How we work

- **Plan:** this file holds the decisions and the milestone outline. When a design decision changes, update it here in the same commit as the code.
- **Tasks:** [GitHub milestones](https://github.com/MrBendel/RogueGB/milestones) match the sections below, and each concrete task is an [issue](https://github.com/MrBendel/RogueGB/issues) with acceptance criteria. Issues are filed for the current and next milestone only; later ones get issues when we reach them.
- **Doing an issue:** branch, implement, add or update tests (`node tools/test.mjs`), and reference the issue in the commit (`Fixes #N`).
- **Verifying on a phone:** every push to `main` runs the GitHub Actions workflow (`.github/workflows/pages.yml`). It builds the ROM, runs the tests and deploys the playtest page to **https://mrbendel.github.io/RogueGB/**. The Claude-hosted playtest page (see CLAUDE.md) is the other option, and it collects bug reports.
- **Closing a milestone:** meet its "done when" list, do a playtest round, then close it on GitHub and mark it ✅ here.

## Milestones

Each milestone ends in a playable build on the playtest page and a short playtest round.
"Done when" lists the requirements for closing the milestone.

### M0 · Shell ✅
Title → play → pause → run over → title, scene state machine, fades, text and font
pipeline, and headless tests covering the whole loop.

### M1 · Combat feel (one room)
One room with real movement, shooting, enemies, damage and death.

- 8-way movement in 8.8 fixed point, with separate acceleration, deceleration and reversal rates (responsive, Gungeon-like rather than floaty), pixel-clean top speeds, one shared rounding rule for everything drawn, and diagonals the same speed as straight lines
- Tile collision that lets the player slide along walls
- Sprite pipeline: assets.mjs converts animation frames to 8×16 sprite tiles with frame tables (mirroring, deduplication), sets per weapon type, frames streamed from ROM into VRAM on animation change; 16×16 animated player
- Firing with auto-aim: nearest-target selection with a facing bias, sticky targeting, a reticle, 32-direction bullets, a bullet pool, and bullets that stop at walls
- Actor system: pools for enemies, bullets and pickups, sprite/OAM budget, flicker past 10 sprites per scanline
- Two enemy archetypes: a **chaser** (slime) and a **shooter** (peashooter, with a telegraph before it fires)
- Damage model: HP, hit flash, knockback, invulnerability frames, enemy bullets
- HUD row along the bottom on the window layer (hearts), death → run over with the cause of death
- Jump on B: height (`z`), a shadow sprite, a gravity arc, and passing over ground enemies and bullets while airborne
- Roll out of a jump with a second B tap: a burst toward the d-pad direction, with invulnerability frames
- Pits and raised blocks in the test room: falling into pits, jumping across gaps, standing on raised blocks
- Game feel: hitstop, decaying screen shake, white hit flash via the second sprite palette, muzzle flash and impact puffs, eased knockback, input buffering for jump and roll, placeholder sound effects
- Input: detect A+B pressed together within a few frames (the ability hook does nothing until M4), with tests for each order of presses
- Tuning values exposed as globals, so they can be adjusted live from the Watch panel

**Done when:** jumping, rolling and auto-aim feel good in playtests, a 60-second fight against both enemy types is fun to replay, holds 60 fps
with 6+ enemies and 10+ bullets on screen, and is covered by tests and replays.

### M2 · Dungeon (rooms and floors)
Turn one room into a seeded floor of hand-made rooms.

- Room authoring format and converter (proposal: [Tiled](https://www.mapeditor.org/) JSON → banked C data). Rooms can be any multiple of one screen (10×8 blocks)
- Indoor tileset pipeline: tile deduplication, with flags per block for collision, pit and height
- ROM banking for room data and tile sets
- Doors and Zelda-style scrolling between rooms
- Camera that follows the player in rooms larger than one screen: streaming map columns and rows into the 32×32 background buffer, clamping at room edges, easing toward its target and leading toward the auto-aim target
- Enemies outside the camera view are paused or simplified, and their sprites are skipped
- Seeded floor generator: start, combat, special and boss slots, guaranteed to connect, deterministic for a given seed. Variety from data: rooms mirrored horizontally or vertically (via a block mirror table, since DMG background tiles can't be flipped in hardware), per-room layout variants (pits, platforms, cover), random spawn and decoration picks
- Doors lock until the room is cleared; cleared rooms are remembered for the run
- Spawn tables per room
- Room features built on jumping: rooms with pits to cross, raised-platform layouts, moving platforms
- Minimap on the pause screen
- Floor exit → next floor; the run-over screen shows the floor reached

**Done when:** a run is 3 floors of about 8 rooms, the same seed always gives the
same layout (with a test for it), and moving between rooms has no visible glitches.

### M3 · Guns and loot
- Gun data: manufacturer, fire rate, spread, projectile count, speed, damage, element, joke name
- Seeded procedural gun generation with rarity tiers. A gun is rolled stats plus an icon assembled from pre-drawn part tiles (barrel, body, grip); no runtime pixel work
- Drops from enemies and chests, pickup and a stat-compare popup
- 4-weapon loadout: tap SELECT to cycle, hold SELECT for the weapon wheel (d-pad picks a slot); a full loadout means dropping or swapping a weapon on pickup
- Elements (Fire, Shock, Acid, plus a Slag-style "mark" debuff) against defense layers (flesh, shield, armor), with distinct bullet shapes and impact effects
- Gold drops and pickups
- "Badass" elite variants: ×2 HP, an extra attack, a guaranteed better drop

### M4 · Heirs and legacy (meta-progression)
- Heir selection: pick 1 of 3 (class + 0–2 traits) after the title screen
- 4–5 classes as stat multipliers plus one ability on A+B, with a cooldown or charge meter shown in the HUD
- Each class has its own **movement profile** (for example a fast, slippery Runner and a heavy Grenadier that ramps up slowly); traits can modify it
- Traits that are cheap on Game Boy: palette tricks, flipped screen, hidden HP, size changes
- Second Wind: at 0 HP, a short last stand where a kill revives you
- Manor upgrade tree (about 20 nodes), where every purchase raises all costs
- Gold tax when starting a new run
- Save data in SRAM (battery), with versioning and a checksum
- Unlocks: gun parts and manufacturers found during runs

### M5 · Zones and bosses (content)
- 4 zones, each with its own tileset, enemy roster (3–4 families with 3 tiers each) and special rooms
- One boss per zone that shows a single attack pattern clearly, with a weak-point hitbox
- Special rooms: shop, shrine (risk/reward), gamble, shooting gallery, gun stand
- Final area that opens once all zone bosses are dead
- Darkness effect for the last zone
- New Game+: all enemies one tier higher, +50% loot

### M6 · Sound and color
- Music and sound-effect driver (proposal: hUGEDriver), title, zone and boss themes
- A complete sound-effect set
- GBC palettes (elements, enemy tiers, zones) and a GBC-compatible header
- Title screen art, polish pass on transitions and UI
- Optional: move the HUD to the top of the screen (scanline interrupt switches the window off below it)

### M7 · Release
- Balance pass using recorded playtests
- Performance and timing checks in Emulicious/SameBoy and on real hardware
- Stress tests: worst-case sprite and CPU frames
- Credits, version number, release build and page
