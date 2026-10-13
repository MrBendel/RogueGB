# RogueGB roadmap

A top-down roguelite shooter for the Game Boy: Rogue Legacy's run and heir structure
with Borderlands-style guns. See [docs/inspiration/](docs/inspiration/README.md) for
the design notes behind it.

## Decisions so far

| Topic | Decision |
|---|---|
| View | **Top-down**, at the scale of the Capcom-era (Oracle) Zelda games: 16×16 characters drawn as 2 hardware sprites in 8×16 mode, and 16×16 map blocks. Matches the `artwork/8bitadventure` pack |
| Rooms | **Mixed sizes.** Most rooms are one screen (10×8 blocks) and slide to the next one, Zelda style. Some are bigger (2×1, 1×2, 2×2 screens) with a camera that follows the player, for arenas and bosses. Room data allows any size from the start |
| Graphics | **Pre-rendered.** Nothing is drawn or combined pixel by pixel at runtime. Characters' bodies are 2 sprites; right-facing frames are mirrored for left. All frames live in ROM; VRAM holds only the frames on screen, swapped in when the animation changes (4 tiles = 64 bytes). **The held weapon is its own sprite (1-2 sprites) over the body**, pre-drawn in 16 aiming directions by `tools/make-weapon-art.mjs` (see "Weapons" below). Any combining or rotating of art happens in the build tools, not in the game. Cheap runtime effects: sprite flips (sprites only; DMG background tiles can't flip, so mirrored background art is pre-rendered by the build), palette changes, sprites stacked by the hardware where the line budget allows |
| Movement | Eased 8-way movement with per-axis acceleration, deceleration and reversal. **Default: SNAPPY at 1 px/frame** (5 frames to top speed, 4 to stop, 3 to turn around), chosen in the movement lab with the real player sprite (2 px/frame felt right with a placeholder box, but too fast with the character art). **Each character has a movement profile** (top speed + those three frame counts) from a data table, so heir classes, traits and enemies can move differently |
| Performance | A frame is 140,448 CPU cycles at the Color's double speed (131,328 after vblank; it was 70,224 / 65,664 at normal speed). VRAM updates are queued and written in vblank; per-frame code avoids 32-bit math and division; the hottest loops are hand-written assembly; enemies think at 30 Hz; the camera streams one strip per frame and moves at most 4 px a frame. Tests guard the budget: walking under half a frame, an arena fight under 48k, full pools (8 enemies + 20 bullets) at most 1% of frames over (#26). `harness/profile.mjs` shows where cycles go |
| Dynamic content | Only **data** is generated at runtime: floor layouts, room picks and mirroring, spawns, gun stats. It runs during fades, so it has no per-frame budget |
| HUD | One block row (16 px) on the window layer. **Bottom of the screen for M1**, because the hardware does that for free. A top HUD like Zelda's needs a scanline interrupt; it's an optional M6 polish item |
| Hardware | **Game Boy Color only** (decided 2026-10, replacing "DMG first"): the ROM header is CGB-only, the CPU runs at double speed, and the Color's extra hardware is ours to use: 8 background + 8 sprite palettes, a second VRAM bank (512 sprite tiles), per-tile palettes and flips for the background, HDMA. The game won't run on an original Game Boy. Color arrives in steps: grey palettes matching the old look first (done), per-asset palettes (done: each sprite keeps its art's colors, `src/palettes.c` picks the room, HUD and tool-drawn art), the second VRAM bank (done: all weapon art, so bank 0's sprite tiles end at 52 of 256), then the dimmed weapon wheel, and color for elements, rarity and zones |
| Health and shields | **Borderlands-style layers.** A shield takes damage first and recharges after a delay without hits. Health only regenerates with a perk (class, trait or Manor). **You start without a shield** and earn the first one from a chest. Falling into a pit skips the shield. Shown as **bars, not hearts**: they grow as max values grow (Rogue Legacy style), and exact numbers are on the pause screen. How the character levels up (in-run XP, the Manor, or both) is decided once the story is outlined (see Open questions) |
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
  - The first shield (the chest's, once the chest exists): 40 points, recharging a point every 2 frames after 150 frames (2.5 s) without a hit.
  - Max health, shield capacity, delay and rate are on the pause panel.
  - Health regen is a hook: `health_regen`, frames per point, 0 for now.
- **Hits:**
  - A hit the shield takes all of: 20 invulnerable frames, no knockback, no blink, and the player tinted shield blue for a moment.
  - A hit that breaks the shield: the overflow spills into health, and the player flashes white and blue.
  - A hit that reaches health: knockback and the 60-frame blink, as before.
- **HUD:**
  - The bottom row on the window: the shield bar over the health bar, then the gun's name.
  - Bars are 1 px per point in whole 8 px cells, up to 88 px; past that they scale.
  - The shield row is empty until you have one, and shows a cracked icon while empty.
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
  - **When it reloads.** Every button is taken, so reloading is automatic, at two moments:
    - when a magazine empties;
    - after `RELOAD_IDLE` (60) frames without firing while the magazine isn't full.

    Pressing A during that idle reload cancels it and fires what's left.
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

- **Story and progression.** Map out the story first. It decides how the character grows (in-run levels, the Manor, or both), where shields, chests and special guns show up, and the manufacturer names.
- **Pickup button** (see Loot); a manual reload too, if a button frees up (reloads are automatic for now).

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
- Damage model: health plus a shield layer (empty until you find a shield), hit flash, knockback, invulnerability frames, enemy bullets. A chest in the test arena holds the first shield
- HUD row along the bottom on the window layer: shield and health bars that grow with max values, space for the gun icon (M3) and ability meter (M4). Death → run over with the cause of death and kills
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
- Camera that follows the player in rooms larger than one screen: streaming map columns and rows into the 32×32 background buffer, clamping at room edges, and a look-ahead (lead) in the direction of travel that eases in and out when the direction changes, with a capped top speed (a plain eased camera lurched on quick turns; a constant-rate slide stopped dead)
- Enemies outside the camera view are paused or simplified, and their sprites are skipped
- Seeded floor generator: start, combat, special and boss slots, guaranteed to connect, deterministic for a given seed. Variety from data: rooms mirrored horizontally or vertically (via a block mirror table; on the Color, background tiles can also flip in hardware), per-room layout variants (pits, platforms, cover), random spawn and decoration picks
- Doors lock until the room is cleared; cleared rooms are remembered for the run
- Spawn tables per room
- Room features built on jumping: rooms with pits to cross, raised-platform layouts, moving platforms
- Minimap on the pause screen
- Floor exit → next floor; the run-over screen shows the floor reached

**Done when:** a run is 3 floors of about 8 rooms, the same seed always gives the
same layout (with a test for it), and moving between rooms has no visible glitches.

### M3 · Guns and loot
The full Borderlands loot loop (see "Loot" above).

- Gun data and seeded generation: manufacturer, rarity, element, part indices → damage, fire rate, spread, projectile count, speed, magazine, reload; a joke name; an icon from part tiles
- Magazine and reload stats rolled per gun (the base magazines and automatic reloads are built), and the manufacturer gimmicks (including explode-on-reload)
- Chests and elite drops with a loot fountain; rarity beams; pickup and the item card with ▲/▼ stat compare
- 4-weapon loadout: tap SELECT to cycle, hold SELECT for the weapon wheel (d-pad picks a slot); a full loadout means dropping or swapping a weapon on pickup
- Shields as loot: capacity, recharge delay and rate, special effects
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
- Color art pass: palettes for every asset, elements, enemy tiers and zones (the Color-only switch, double speed and the palette system come earlier, see "Hardware")
- Title screen art, polish pass on transitions and UI
- Optional: move the HUD to the top of the screen (scanline interrupt switches the window off below it)

### M7 · Release
- Balance pass using recorded playtests
- Performance and timing checks in Emulicious/SameBoy and on real hardware
- Stress tests: worst-case sprite and CPU frames
- Credits, version number, release build and page
