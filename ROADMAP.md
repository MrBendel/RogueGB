# Skavengar roadmap

A top-down space exploration shooter for the Game Boy Color: develop one persistent
character through a campaign of repeatable expeditions, gun loot, and discoveries
from a lost civilization. A megacorporation led by the concealed **CEO** seeks
galactic control. See [docs/design/](docs/design/README.md) for the story, play loop,
and decision log; [docs/inspiration/](docs/inspiration/README.md) holds historical
reference notes.

Campaign direction changed on 2026-10-12: the heir/Manor loop is superseded.
Story details marked as proposals in the design notes remain provisional. The
milestones below describe planned work; campaign systems are not implied to exist
in the combat prototype. Local milestone revisions have not updated GitHub issues.

## Decisions so far

| Topic | Decision |
|---|---|
| Campaign | Choose a character/class at the start and develop that same character. Explore, collect, and repeat within a progressive space campaign. Planets and a ship hub are the working structure; see [play loop](docs/design/play-loop.md) |
| View | **Top-down**, at the scale of the Capcom-era (Oracle) Zelda games: 16×16 characters drawn as 2 hardware sprites in 8×16 mode, and 16×16 map blocks. Matches the `artwork/8bitadventure` pack |
| Rooms | **Mixed sizes.** Most rooms are one screen (10×8 blocks) and slide to the next one, Zelda style. Some are bigger (2×1, 1×2, 2×2 screens) with a camera that follows the player, for arenas and bosses. Room data allows any size from the start |
| Graphics | **Pre-rendered.** Nothing is drawn or combined pixel by pixel at runtime. Characters' bodies are 2 sprites; right-facing frames are mirrored for left. All frames live in ROM; VRAM holds only the frames on screen, swapped in when the animation changes (4 tiles = 64 bytes). **The held weapon is its own sprite (1-2 sprites) over the body**, pre-drawn in 16 aiming directions by `tools/make-weapon-art.mjs` (see "Weapons" below). Any combining or rotating of art happens in the build tools, not in the game. Cheap runtime effects: sprite flips (sprites only; DMG background tiles can't flip, so mirrored background art is pre-rendered by the build), palette changes, sprites stacked by the hardware where the line budget allows |
| Movement | Eased 8-way movement with per-axis acceleration, deceleration and reversal. **Default: SNAPPY at 1 px/frame** (5 frames to top speed, 4 to stop, 3 to turn around), chosen in the movement lab with the real player sprite (2 px/frame felt right with a placeholder box, but too fast with the character art). **Each character has a movement profile** (top speed + those three frame counts) from a data table, so character classes, upgrades and enemies can move differently |
| Performance | A frame is 140,448 CPU cycles at the Color's double speed (131,328 after vblank; it was 70,224 / 65,664 at normal speed). VRAM updates are queued and written in vblank; per-frame code avoids 32-bit math and division; the hottest loops are hand-written assembly; enemies think at 30 Hz; the camera streams one strip per frame and moves at most 4 px a frame. Tests guard the budget: walking under half a frame, an arena fight under 48k, full pools (8 enemies + 20 bullets) at most 1% of frames over (#26). `harness/profile.mjs` shows where cycles go |
| Dynamic content | Only **data** is generated at runtime: floor layouts, room picks and mirroring, spawns, gun stats. It runs during fades, so it has no per-frame budget |
| HUD | One block row (16 px) on the window layer. **Bottom of the screen for M1**, because the hardware does that for free. A top HUD like Zelda's needs a scanline interrupt; it's an optional M6 polish item |
| Hardware | **Game Boy Color only** (decided 2026-10, replacing "DMG first"): the ROM header is CGB-only, the CPU runs at double speed, and the Color's extra hardware is ours to use: 8 background + 8 sprite palettes, a second VRAM bank (512 sprite tiles), per-tile palettes and flips for the background, HDMA. The game won't run on an original Game Boy. Color arrives in steps: grey palettes matching the old look first (done), per-asset palettes (done: each sprite keeps its art's colors, `src/palettes.c` picks the room, HUD and tool-drawn art), the second VRAM bank (done: all weapon art, so bank 0's sprite tiles end at 52 of 256), then the dimmed weapon wheel, and color for elements, rarity and zones |
| Health and shields | **Borderlands-style layers.** A shield takes damage first and recharges after a delay without hits. Health regeneration can come from character or equipment upgrades. **You start without a shield** and earn the first one from a chest. Falling into a pit skips the shield. Shown as **bars, not hearts**: they grow as max values grow, and exact numbers are on the pause screen. Persistent character growth is agreed; XP and upgrade rules remain open |
| Loot | **As close to Borderlands as the hardware allows**, so there's always a newer, better gun to find. Procedural guns from a few manufacturers, each with one gimmick (explodes when reloaded, burst fire, semi-auto heavy hitter, always elemental, accuracy that improves while firing, huge magazines). Rarity tiers, elements, chests with a loot fountain, and a stat card that compares a find with what you hold (see "Loot" below) |
| Order | Get the **combat feel** right in one room before building systems around it |
| Tracking | This file has the overall plan. GitHub milestones match the sections below. Issues are filed only for the next one or two milestones |

### Controls

| Button | Action |
|---|---|
| D-pad | Move in 8 directions (the most a d-pad gives) |
| A | Fire the active weapon. **Auto-aim** picks the target (see below), so you can move freely, including backing away, while shooting |
| B | **Jump.** A second tap in mid-air **rolls** out of the jump toward the d-pad direction (or the facing direction) |
| A + B | **Class ability** (M4), when it's charged. When it isn't, the buttons do their normal jobs |
| SELECT | **A modifier, and the loadout.** **Tap** (acts on release): cycle to the next weapon. **Hold:** open the loadout menu / weapon wheel (later). **SELECT+B:** reload (built). **SELECT+A:** throw a grenade or drop a mine (reserved: the chord is caught and does nothing yet). While SELECT is down, A and B never reach the gun or the jump |
| START | Pause, map (M2) |

**A + B has to mean pressing them together, not "B while A is held".** Players hold A to keep firing, so
"B while holding A" must still be a jump. The rule:

- The ability triggers when A and B are both **newly pressed within a few frames** of each other (about 4, to be tuned).
- If B comes first, the jump starts immediately. If A arrives within that window, the ability cancels the jump's first frames, so jumps never feel delayed.
- If A comes first, the shot fires immediately, and B within the window turns it into the ability.
- If the ability isn't charged, the press counts as a normal jump and shot.
- The input code (M1) should support detecting presses within a window, and tests should cover each order of presses.

### Auto-aim

This is a casual game, so the player picks *when* to shoot and the game picks *what* to shoot at. Built in #5 (`src/aim.c`, `aim_score.s`, `shots.s`).

- Holding A fires at the **nearest enemy in line of sight**, with a bias toward the facing direction, so you tend to shoot what you're running at. Distance is octagonal (max + min/2). Enemies within 90 degrees of facing count as `aim_bias` (24 px) nearer. Range is `aim_range` (112 px).
- **Sticky targeting:** the target only changes when it dies, leaves line of sight, or another enemy is better by more than `aim_sticky` (16 px). This stops aim from jittering between two enemies.
- **Line of sight** samples the straight line at most 16 px apart. Walls and raised blocks block it; pits don't.
- **Cost control:** one enemy is rescored every other frame, round-robin, so each is rescored about 4 times a second. Scoring skips its turn when the frame is running late.
- A small **reticle sprite** (corner brackets, pulsing) marks the current target, so auto-aim is never a mystery.
- Aim isn't limited to 8 directions. Bullets use a 32-direction velocity table, so they fly straight at the target.
- With no target, shots go in the facing direction.
- Shots: 3 px/frame, one every 10 frames while A is held, up to 6 in flight. Each shot flies until its range runs out or it hits a wall. A hit takes 1 HP and flashes the enemy; at 0 HP the enemy is removed. No shooting while rolling or falling into a pit.
- The pause tuner has FIRE RATE, SHOT SPD, AIM RANGE and AIM BIAS.
- Still to tune: whether some weapons (shotguns, lobbed grenades) aim differently (M3 weapons).

### Jumping and height

Jumping makes the top-down world partly 3D, Link's Awakening style:

- The player has a height (`z`) as well as x/y. The sprite is drawn raised by `z` with a **shadow** on the ground, and jumping follows a short gravity arc.
- **Over enemies:** while high enough, ground enemies and ground-level bullets pass underneath. Flying enemies and "high" bullets can still hit.
- **Over holes:** pits are floor tiles you fall into if you land on them (lose some HP, respawn at the edge). Gaps up to about 2 blocks can be jumped.
- **Simulated platforms:** some blocks are **raised**. You can jump onto them and walk along them, and dropping off the edge lands you on the floor. Later: moving platforms over pits.
- Rolling out of a jump gives a burst of distance and a few invulnerability frames, so you can dodge in the air or extend a jump over a wider gap.

### Weapons

How weapons look and how each class feels. Decided: **16 aiming directions** (optimize if the CPU needs it), and **melee is a weapon class you can equip** (unlike Borderlands, where melee is a separate button).

**How they're drawn** (built for the pistol; `src/aim.c`, `tools/make-weapon-art.mjs`):
- The weapon is a separate sprite over the body. It points where auto-aim points: shots fly in 32 directions, the sprite shows the nearest of 16.
- The art is one right-facing drawing (a Sprite Studio template) rotated around its grip by the build tool, RotSprite style, for the right half of the circle and mirrored for the left (so the grip stays underneath). The tool also writes, per direction, where the weapon sits, where its muzzle is, which way it kicks, and whether it's drawn behind the body (aiming up).
- While shooting, the body faces the aim rather than the way it walks, so you can back away from something while firing at it.
- Each shot kicks the gun back (2 px, then 1 px) and shows a muzzle flash. Shots leave from the muzzle. The weapon is put away while rolling or falling.
- Sprites: the flash, the weapon (in front of or behind the body), the body, shadow and reticle have fixed slots (`src/oam.h`); 3-5 of the 10 per scanline around the player.

**How each class feels** (the same knobs, different amounts: fire mode, cadence, projectiles, kick, knockback, hitstop, movement while firing):

| Class | Fire mode | Projectiles | Kick and impact | Movement | Signature |
|---|---|---|---|---|---|
| Melee | Tap A to swing; a 3-hit combo in rhythm | None: a 24 px arc toward the target for a few frames | Big knockback, 3-frame hitstop on contact | Short lunge, slowed mid-swing | Swats enemy bullets back as your own |
| Handgun | Tap A fires at once; hold for a slower auto | 1 accurate bullet, medium speed | 1 px kick, small flash | Full speed | Rewards tapping |
| Shotgun | Slow (~40 frames), visible pump between shots | 5-7 pellets fanned ±22°, short range | Strong knockback, 2 px screen kick, pushes you back 2 px | Full speed | Point-blank power |
| Assault rifle | Tap (one shot per press), 3-round burst, or full auto (hold), by setting | A bullet every 6 frames on auto, 4 frames apart in a burst | 1 px kick | Full speed | The player picks how it fires (replaced the planned minigun) |
| Sniper | Hold A to steady (the reticle closes in ~0.6 s), fires at lock; a quick tap fires weak | Instant hit (the line-of-sight check) with a 2-frame tracer, pierces | Big screen kick, hitstop on hit | Slowed while steadying | 2x aim range, a crit on a full lock |

Built so far (`src/weapon.c`, firing in `src/aim.c`): pistol, shotgun, assault rifle, sniper and sword as stat presets (fire mode, fire rate, tap rate, pellets, spread, jitter, speed, range, aim range, damage, kick, shove, windup, slow), switched with SELECT and tunable on the pause panel. Fire modes: semi (tap fires at once), auto, rifle, charge (a critical at lock), and melee. The **assault rifle** (which replaced the minigun) fires by tap, 3-round burst or full auto: a setting (`rifle_mode`), on the pause panel for now and **on an options screen off the title later**. The minigun's spin-up (and walking slowly while firing) can come back as a heavy weapon class. The sword: each press swings (14 frames; the blade sprite sweeps ~135 degrees through its 16 directions around the aim), a 3-hit combo when chained (sweep, sweep back, a thrust for double damage), a lunge, knockback that never pushes an enemy into a wall or pit, hitstop on contact (the whole game freezes for HITSTOP frames, 3 by default), and enemy bullets in the swing turned into the player's shots. While a swing can hit, the blade is drawn with the Sprite Studio's motion-smear wings (its synthesizeSmearRightFromRight pattern, moved to hug the slimmer blade). Weapon art lives in the Color's second VRAM bank (about 200 of its 256 sprite tiles for 5 weapons and the smear): many more weapons means loading only the held weapon's frames. Not yet: the shotgun pump, the minigun barrel spin, the sniper's instant hit and pierce (it's a fast 6 px/frame shot for now), screen kick and knockback from bullets (with #13).

Next: the shared game-feel effects (#13): screen shake, knockback from shots, impact puffs, sound.

### Weapon loadout

- Up to **4 weapons**. Tapping SELECT cycles through them.
- Holding SELECT (about 12 frames) opens a **weapon wheel**: the 4 slots sit on the d-pad's
  up/right/down/left. Press a direction to highlight a slot, release SELECT to equip it.
- While the wheel is open the game pauses or drops to slow motion (to decide in playtests).
- **Deferred for now:** SELECT just cycles weapons. When the wheel is built, the world dims with the *faded* palette (one palette-register write, the same palette the scene fades use; the darkened one costs the same), the wheel is sprites on the second sprite palette at full contrast, and a scanline split keeps the HUD row bright for the weapon's name and stats.
- Tapping SELECT acts on release, so a tap and the start of a hold can't be confused.

### Health and shields (built)

`src/health.c` and the HUD in `src/hud.c` (issues #10, #11):
- **Numbers:**
  - Health starts at 60 points.
  - Enemies do 10 on touch and 8 per pea; a pit costs 10 and skips the shield.
  - The first shield: 40 points, recharging a point every 2 frames after 150 frames (2.5 s) without a hit.
- **The chest** (`src/chest.c`):
  - It sits in the arena's bottom-right and stays locked until the wave is cleared; then it glows gold.
  - Walking into it opens it: the shield pops out and lands in front.
  - Walking over the shield equips it, and the bar fills from empty, so you see it arrive.
  - The chest and shield art are placeholders, drawn in `tools/make-ui-art.mjs` (`artwork/ui/chest (1-2).png`, `shield (1).png`).
  - The chest's block is `BLK_PROP`: solid to the player and drawn as floor, with the chest as a sprite over it (the background can't change palette per block while it streams).
  - Max health, shield capacity, delay and rate are on the pause panel.
  - Health regen is a hook: `health_regen`, frames per point, 0 for now.
- **Hits:**
  - A hit the shield takes all of: 20 invulnerable frames, no knockback, no blink, and the player tinted shield blue for a moment.
  - A hit that breaks the shield: the overflow spills into health, and the player flashes white and blue.
  - A hit that reaches health: knockback and the 60-frame blink, as before.
- **HUD:**
  - The bottom row on the window: a 1 px ink line along the top, the shield bar (hanging from the line) over the health bar, then the held gun's icon (its right-facing art) and the rounds left in tall 8x16 digits. Switching weapons shows the name there for a second.
  - Every HUD tile is in VRAM bank 1 (bars, icons, the gun icon, tall digits, a copy of the font with the line), so the HUD uses none of the scene's tiles.
  - Bars are 1 px per point in whole 8 px cells, up to 88 px; past that they scale.
  - Before you have a shield, its row shows a dashed empty slot. With one, a cracked icon shows while it's drained.
  - The health bar pulses at a quarter health or less.
  - The bar tiles are in VRAM bank 1 (no scene tiles used), each with its own palette.
  - A bar redraws only when its value changes: a 13-byte copy in vblank.

### Loot

The Borderlands loop: kill or open → loot fountain → look → compare → equip. Notes on fitting it to a Game Boy (details in [docs/inspiration/borderlands-2.md](docs/inspiration/borderlands-2.md)):

- **Guns are a few packed bytes** rolled from a seed: manufacturer, rarity, element, part indices. Their stats come from those. The icon is assembled from pre-drawn part tiles; no runtime pixel work.
- **Manufacturers** (our own names, to be written with the story), one gimmick each:
  - thrown like a grenade when reloaded, and explodes;
  - burst fire;
  - semi-auto: fires as fast as you tap A, hits hard;
  - always elemental;
  - accuracy improves while firing;
  - fast fire rate with huge magazines;
  - slow explosive rounds.
- **Magazines and reloads (built, in M1):**
  - **When it reloads.** Automatically when a magazine empties, or with **SELECT+B** any time it isn't full. There's no idle reload (tried and dropped): a part-used magazine stays as it is until you reload it. A reload can't be cut short, except by switching weapons.
  - **Per weapon.** Each weapon keeps its own magazine, and switching weapons cancels a reload, as in Borderlands.
  - **On screen.** A bar over the player's head fills during the reload; it uses the reticle's sprites, since there's nothing to aim while you can't shoot. The HUD shows `AMMO n`.
  - **Base stats** follow typical Borderlands 2 values per gun type (approximate). Magazines are in the usual range; reloads are about 60% as long, because rooms and fights are smaller:

    | Type | Borderlands magazine | Borderlands reload | Ours: magazine | Ours: reload |
    |---|---|---|---|---|
    | Pistol | 6-21 (revolvers ~6) | ~1.5-2.5 s | 12 | 66 frames (1.1 s) |
    | Shotgun | 2-8 shells | ~2.5-3.5 s | 5 (a blast is one shell) | 100 frames (1.7 s) |
    | Assault rifle | 18-40 | ~2.5-3.5 s | 24 | 105 frames (1.75 s) |
    | Sniper | 3-8 | ~3-4 s | 4 | 120 frames (2 s) |
    | SMG (later) | 20-40 | ~2 s | | |
    | Launcher (later) | 1-6 | ~3-4.5 s | | |
    | Sword | none | none | 0 (never reloads) | |
  - **Loot later.** Generated guns roll their magazine and reload around these bases, and several manufacturer gimmicks hang off the reload (explodes or is thrown when reloaded). No reserve ammo for now: magazines are infinite.
- **Rarity:** the classic Borderlands colors (Game Boy Color), plus the height and sparkle of the light beam over a drop, the frame style of the item card, and a pickup sound.
- **Loot fountain:** chests and elite enemies pop items out in arcs, using the same height/shadow system as jumping.
- **Item card:** standing on an item shows its card in the window: name (with a joke prefix), manufacturer, rarity, and each stat with ▲/▼ against what you're holding. Picking up needs a button: proposal, a tap of SELECT while standing on an item takes it (swapping if the loadout is full); elsewhere SELECT keeps its normal job.
- **Shields are loot too:** capacity, recharge delay and rate, plus special effects (bursts when broken, damages enemies that touch you, absorbs bullets as ammo, boosts the next shot).
- **Elements and status:** Fire burns flesh over time, Shock strips shields, Acid eats armor, Slag marks enemies to take more damage. Shown by color (each element its own palette) plus a small effect sprite.
- **Budget:** shotguns and fast guns need a bigger shot pool than today's 6, with CPU budget tests to match.

### Open questions

- **Campaign details.** Develop the [story](docs/design/story.md) and [play loop](docs/design/play-loop.md): first expedition, death/return rules, persistent character leveling, collection rewards, and manufacturer identities. The CEO and ancient-network premise are established directions; names and detailed plot beats remain proposals.
- **Pickup button** (see Loot); a manual reload too, if a button frees up (reloads are automatic for now).

## How we work

- **Plan:** this file holds technical decisions and the milestone outline. [docs/design/](docs/design/README.md) holds campaign and story decisions, with a dated decision log. Keep the relevant documents aligned when a decision changes; distinguish proposals from agreed requirements.
- **Tasks:** [GitHub milestones](https://github.com/MrBendel/RogueGB/milestones) match the sections below, and each concrete task is an [issue](https://github.com/MrBendel/RogueGB/issues) with acceptance criteria. Issues are filed for the current and next milestone only; later ones get issues when we reach them.
- **Doing an issue:** branch, implement, add or update tests (`node tools/test.mjs`), and reference the issue in the commit (`Fixes #N`).
- **Verifying on a phone:** every push to `main` runs the GitHub Actions workflow (`.github/workflows/pages.yml`). It builds the ROM, runs the tests and deploys the playtest page to **https://mrbendel.github.io/RogueGB/**. The Claude-hosted playtest page (see CLAUDE.md) is the other option, and it collects bug reports.
- **Closing a milestone:** meet its "done when" list, do a playtest round, then close it on GitHub and mark it ✅ here.

## Milestones

See [the design work plan](docs/design/design-plan.md) for open campaign/system
questions and a proposed playable slice spanning M2–M6. Its scope counts are
proposals; it does not mark milestones or features complete.

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
- Damage model: health plus a shield layer (empty until you find a shield), hit flash, knockback, invulnerability frames, enemy bullets. A chest in the test arena holds the first shield
- HUD row along the bottom on the window layer: shield and health bars that grow with max values, space for the gun icon (M3) and ability meter (M4). Death → run over with the cause of death and kills
  - Built: the HUD (see "Health and shields"); the run-over screen names what killed you (a slime, a peashooter, a pea, a pit; "RUN ENDED" if ended from the pause panel) with time, kills and waves cleared.
- **Waves** (`src/waves.c`, built):
  - Wave 1 is the same five enemies every run.
  - Clearing a wave shows CLEAR! and starts a 4-second break. The next wave comes only while you're in the arena; the HUD shows WAVE n.
  - Wave n has 4 + n enemies (up to 8), 1 + n/2 of them peashooters, at arena spots at least 56 px from you.
  - **Difficulty ramp** (`waves.c`): each wave also toughens the enemies. Waves 6 and later use the last column. Wave 1 got a little meaner too (80 frames between shots instead of 90, slimes resting 26 instead of 30).

    | Wave | 1 | 2 | 3 | 4 | 5 | 6+ |
    |---|---|---|---|---|---|---|
    | Peashooter: frames between shots | 80 | 72 | 64 | 56 | 50 | 44 |
    | Pea speed (px/frame) | 1.5 | 1.6 | 1.75 | 1.9 | 2.0 | 2.0 |
    | 3-way spread | | | yes | yes | yes | yes |
    | Slime: rest between hops (frames) | 26 | 22 | 20 | 18 | 16 | 14 |
    | Slime hop speed (px/frame) | 0.75 | 0.8 | 0.9 | 0.95 | 1.0 | 1.0 |
    | Extra enemy HP | 0 | 0 | 1 | 1 | 2 | 2 |
  - **START WAVE** on the pause panel begins a run at a later wave (for testing).
- Jump on B: height (`z`), a shadow sprite, a gravity arc, and passing over ground enemies and bullets while airborne
- Roll out of a jump with a second B tap: a burst toward the d-pad direction, with invulnerability frames
- Pits and raised blocks in the test room: falling into pits, jumping across gaps, standing on raised blocks
- Game feel: hitstop, decaying screen shake, white hit flash via the second sprite palette, muzzle flash and impact puffs, eased knockback, input buffering for jump and roll, placeholder sound effects
  - Built:
    - hitstop, the hit flash, the muzzle flash, jump and roll buffering;
    - screen shake (`camera_shake`): up to 3 px, the SHAKE row on the pause panel, 0 = off;
    - placeholder sound effects (`src/sfx.c`), listed below.
  - Sound: register-level sounds on channels 1, 2 and 4, each with a priority; channel 3 is left for music. One observer a frame (`sfx_watch`) plays them from the game's counters, so the hot code makes no calls. The sounds:
    - each gun's shot, a sniper critical, the sword's swish;
    - hits, kills and deflects;
    - hurt, the shield's hit, break and recharge chime;
    - reload click and clack, the chest's arpeggio and the pickup;
    - jump, dash, a peashooter firing, death, and the title's start.
  - Not yet: impact puffs, knockback from bullets, an enemy death effect.
- Input: detect A+B pressed together within a few frames (the ability hook does nothing until M4), with tests for each order of presses
- Tuning values exposed as globals, so they can be adjusted live from the Watch panel

**Done when:** jumping, rolling and auto-aim feel good in playtests, a 60-second fight against both enemy types is fun to replay, holds 60 fps
with 6+ enemies and 10+ bullets on screen, and is covered by tests and replays.

### M2 · Generated layers (rooms and depth)
Expedition levels generated in game from the run's seed: authored chunks, a layer
layout with guarantees, and visual rulesets. Design and numbers:
[level-generation.md](docs/design/level-generation.md) (agreed 2026-10-12).

- Chunk format (5×4 blocks, wildcards, open edges, flips) and an authoring tool + converter (open: a Sprite Studio tab or Tiled)
- Layer generator: a seeded room grid with a guaranteed main path to the shaft down, optional branches, fixed landmarks (beacon, relic chamber, guardian arena), room types (entry, combat, connector, cache, beacon, guardian)
- Room assembly from chunks: 1–2 screen rooms, 2×2 set pieces; wildcards filled from the seed
- Visual rulesets per theme: autotiling, grouping rules for decorations, tileset and palettes; per-tile palette attributes streamed with the map
- Doors and transitions between rooms; doors lock until a combat room is cleared; cleared rooms remembered for the trip
- Spawn tables per room type and depth (replaces the arena's waves, which stay as a test room)
- Enemies outside the camera view paused or simplified, their sprites skipped
- Room features built on jumping: pits to cross, raised platforms, moving platforms
- Beacons end each layer: save point, way back up, later landing site
- The opening as a shortened layer on a fixed seed with fixed landmarks
- Minimap on the pause screen
- Built already: the scrolling camera with streaming, lead and clamping (rooms up to 32×32 blocks); the generator skeleton (chunks in `content/chunks`, layer layout with a guaranteed path, rooms from chunks with carved door paths, doors between rooms, combat rooms that stay cleared). See "Built so far" in level-generation.md

**Done when:** a 4-layer descent of 8–12 rooms per layer generates from a seed, the
same seed always gives the same descent, the harness checks thousands of seeds are
completable, and moving between rooms has no visible glitches.

### M3 · Towns and story systems
Authored towns on each planet, and the systems they share with the descent.

- Town maps (authored, not generated), interiors, and moving between town and descent
- NPCs: placement, simple states, talking
- Dialogue: a text box on the window layer with typewriter text, portraits (Echo, NPCs), A to advance, choices
- Interaction: talk, buy, pick up, open; resolve the button against firing, jumping, reloading and switching
- Trigger zones and contextual prompts (the tutorial's hints, Echo's lines)
- Shops (buy, sell), a currency and its pickups
- Bars: rumours and leads
- Missions: taken in town, tracked as campaign state, completed in the descent (the generator can be asked for mission content)
- Save data in SRAM (battery), with versioning and a checksum

**Done when:** you can walk a small town, talk to two NPCs, buy something, take a
mission, descend, complete it, come back, and the game saves and resumes.

### M4 · Guns and loot
The full Borderlands loot loop (see "Loot" above).

- Gun data and seeded generation: manufacturer, rarity, element, part indices → damage, fire rate, spread, projectile count, speed, magazine, reload; a joke name; an icon from part tiles
- Magazine and reload stats rolled per gun (the base magazines and automatic reloads are built), and the manufacturer gimmicks (including explode-on-reload)
- Chests and elite drops with a loot fountain; rarity beams; pickup and the item card with ▲/▼ stat compare
- 4-weapon loadout: tap SELECT to cycle, hold SELECT for the weapon wheel (d-pad picks a slot); a full loadout means dropping or swapping a weapon on pickup
- Shields as loot: capacity, recharge delay and rate, special effects
- Elements (Fire, Shock, Acid, plus a Slag-style "mark" debuff) against defense layers (flesh, shield, armor), with distinct bullet shapes and impact effects
- Gold drops and pickups
- "Badass" elite variants: ×2 HP, an extra attack, a guaranteed better drop

### M5 · Persistent character and expedition progression
Campaign scope revised 2026-10-12; settle the open rules in [play-loop.md](docs/design/play-loop.md) before implementation.

- Character/class selection at campaign start; develop the same character across expeditions
- Class abilities on A+B and distinct movement profiles; roster size, skill growth, and leveling remain open
- Persistent campaign objectives, discoveries, and destination unlocks
- Proposed ship hub for preparing expeditions, collections, and improvements
- Equipment, materials, and discovery collection/reward systems, with exact rules to be designed
- Death: corporate reconstruction returns the player to the ship with their loadout, charging a percentage of money. Fee details, items outside the loadout, and voluntary return remain open; Second Wind remains a candidate
- Character and campaign state in the save (the save system itself comes with M3)
- Gun-part/manufacturer unlocks are candidates to integrate with discovery rewards

### M6 · Planets, campaign, and bosses (content)
Early validation: standalone title-screen trials are built for Claimjaw (locked
charge/recovery) and Relay Warden (locked floor claims, repositioning, alternating
fan/ring attacks, relays, and one-time reinforcements).
The title's DOWN playtest submenu groups minibosses, bosses, individual enemy
trials (Slime/Peashooter), a mixed enemy group, and the movement lab.
Enemy trials also prototype Spore Husks, Ragebound, Contractors, and Bastion
Drones, plus a combined encounter; see [enemy families](docs/design/enemies.md).
Scrap Hounds add a separate pack trial with warned diagonal pounces.
The Collector gunship trial validates a 32×32 body, marked floor strikes, a
warned laser sweep, and exposed-core windows; see [gunship design](docs/design/gunship.md).
These families are not yet part of procedural campaign spawn tables.
See [boss design notes](docs/design/bosses.md). Campaign integration and rewards
remain open; these trials do not complete the content milestone.

Working content scope, expanded in [story.md](docs/design/story.md):

- Planet destinations with distinctive environments, enemies, collectibles, and recurring sites; four initial concepts, count not final
- Each planet's towns: NPCs, shops, bars and missions written for it (the systems come with M3)
- Repeatable expeditions with persistent landmarks and varied routes/encounters
- Deeper sites and return incentives tied to discoveries and character capabilities
- Bosses with readable patterns and special rooms suited to exploration and collecting
- Corporate encounters and recurring CEO communications that advance the conflict
- Ancient-network discoveries leading toward a final confrontation over its control; exact gates and ending open
- Optional harder expeditions after the campaign; the earlier fixed NG+ formula is no longer a requirement

### M7 · Sound and color
- Music and sound-effect driver (proposal: hUGEDriver), title, zone and boss themes
- A complete sound-effect set
- Color art pass: palettes for every asset, elements, enemy tiers and zones (the Color-only switch, double speed and the palette system come earlier, see "Hardware")
- Title screen art, polish pass on transitions and UI
- (Decided 2026-10-12: the HUD stays at the bottom.)

### M8 · Release
- Balance pass using recorded playtests
- Performance and timing checks in Emulicious/SameBoy and on real hardware
- Stress tests: worst-case sprite and CPU frames
- Credits, version number, release build and page
