# Skavengar

**Skavengar** is the game's chosen name. RogueGB remains the repository and build
identifier for now.

The campaign, story, and play-loop notes are in [docs/design/](docs/design/README.md).
Technical design decisions and implementation milestones are in [ROADMAP.md](ROADMAP.md).

### Combat playtests

Run `npm run play`. On the title, press **DOWN** to open **Playtests**. Choose
**Minibosses**, **Bosses**, **Enemies**, or **Movement Lab** with UP/DOWN and
**A/START**. Choose an encounter and press **A/START** to launch; **B** goes back.
Enemy trials include a Slime, a Peashooter, their mixed group, Spore Husks,
Ragebound, a Contractor, a Bastion Drone, a Faction Mix, and Scrap Hounds. The list scrolls.
See [enemy families](docs/design/enemies.md) for behaviors and counterplay. Normal shooting,
jumping, rolling, and weapon-switching controls apply. START pauses; SELECT while
paused ends the attempt. These are standalone prototypes with trial-only rewards.
See [boss designs and validation](docs/design/bosses.md).
The Bosses submenu also includes the [Collector Ship](docs/design/gunship.md),
a 32×32 gunship trial with marked bombs, a laser sweep, and an exposed core.

A Game Boy roguelite with run-and-gun shooting, built in C with
[GBDK-2020](https://gbdk.org/) 4.5.0. Right now it's the game shell: a title screen, a
**movement lab** (the player in a 2×2-screen room with pillars, pits, raised platforms
and a scrolling camera, with movement presets and live tuning) with an **arena** below it:
walk through the gap in the bottom wall to spawn slimes and peashooters. You have 6 HP;
hits, and falling into pits, cost 1. There's no attacking yet (auto-aim is next) with pause, and a run-over screen that loops back to the title.

### Movement lab controls

| Button | In play | Paused |
|---|---|---|
| D-pad | Move (8 directions) | UP/DOWN pick a value, LEFT/RIGHT change it |
| A | Fire at the auto-aim target (hold or tap, depending on the weapon) | |
| B | Jump; B again in mid-air dashes toward the d-pad direction (once per jump) | |
| SELECT | Tap: next weapon (pistol, shotgun, assault rifle, sniper, sword), on release. **SELECT+B:** reload. SELECT+A: reserved (grenade/mine) | End the run |
| START | Pause and open the tuning panel | Resume |

Weapons: the **pistol** fires as fast as you tap (holding fires slower); the **shotgun**
fires a fan of 5 pellets and shoves you back; the **assault rifle** fires by tap (one shot
per press), burst (3 rounds per press) or full auto (hold), set by the RIFLE row of the
pause panel; the **sniper** steadies while you
hold A (the reticle closes in) and fires a critical (double damage) when it locks; letting
go early fires a normal shot. The **sword** swings on each press of A: tap again as a swing
finishes to chain a 3-hit combo (a sweep, a sweep back, then a thrust that does double
damage). Each swing lunges you forward, knocks enemies back, freezes the action for a few
frames when it connects (hitstop), and swats enemy bullets back as your own shots; while it
can hit, the blade shows motion-smear wings (from the Sprite Studio).

The panel scrolls. MOVE picks the movement preset (SNAPPY, SMOOTH, FLOATY, CLASSIC).
Speed is in pixels per frame. ACCEL, DECEL and REVERSE are the frames it takes to reach top
speed, to stop, and to cancel full speed when pushing the other way. Any change shows the
preset as CUSTOM. JUMP is takeoff speed and GRAVITY the pull per frame (both in pixels per frame);
DASH SPD and DASH FR are the dash's speed and length. CAM LEAD is how many frames of
movement the camera looks ahead; LEAD SPD is the top speed of that look-ahead when you
change direction (pixels per frame: lower is smoother and lazier). It eases in and out. The
camera otherwise moves exactly with the player. The rest tune the weapon you're holding
(the panel's title names it): FIRE RATE (frames between shots while held), TAP RATE (when
tapping), PELLETS, SPREAD (32-direction steps between pellets), JITTER, SHOT SPD, RANGE,
DAMAGE, KICK, WINDUP (the sniper's steadying frames; the rifle's pause after a burst) and
AIM RANGE (for the sword: TAP RATE is
the combo window, RANGE its reach, KICK its knockback); AIM BIAS is how strongly auto-aim
prefers what you face, and HITSTOP how many frames a landed sword hit freezes the game.

## Code structure

| File | What it does |
|---|---|
| `src/main.c` | Boot and the main loop. Runs one scene update per frame and handles scene changes (fade out, `enter()` the new scene, fade in) |
| `src/state.h` | Scene IDs (`STATE_TITLE`, `STATE_PLAY`, `STATE_RUN_OVER`), `state_change()`, `wait_frame()` |
| `src/input.c` | Reads the joypad once per frame: `joy` (held), `joy_pressed`, `joy_released` |
| `src/gfx.c` | VRAM write queue and the vblank handler, `bkg_text`/`win_text`/`bkg_number`, framed boxes |
| `src/gfx_setup.c` | Banked: loading the font and frame tiles, clearing the screen, palette fades |
| `src/vram_queue.s` | Assembly: writes the queued VRAM updates during vblank |
| `src/title.c` | Title screen. START seeds the run's RNG and starts a run |
| `src/play.c` | Gameplay scene: the movement lab, HUD row and the pause/tuning panel |
| `src/player.c` | Player movement (8.8 fixed point, eased per axis), collision with wall sliding and corner nudging, presets |
| `src/sprite_slot.c` | Draws a 16×16 character from pre-rendered frames: two 8×16 sprites and a double-buffered 8-tile VRAM slot |
| `src/camera.c` | Camera: follows the player with a look-ahead (lead) that eases to its new place when you change direction, clamped to the room, at most 4 px a frame; streams one background row or column per frame |
| `src/camera_lead.s` | Assembly: the look-ahead's slide (ease in, cruise, ease out) and the camera's pixel position |
| `src/camera_setup.c` | Banked: placing the camera and drawing the whole view when a room starts |
| `src/actor.c` | Enemies and enemy bullets: pools, AI (slime chaser, peashooter shooter), contact and bullet damage, sprite allocation |
| `src/actor_draw.s` | Assembly: writes actors into the sprite table |
| `src/bullets.s` | Assembly: moves, collides and draws enemy bullets in one pass |
| `src/aim.c` | Banked: auto-aim (#5) and the held weapon. Picks a sticky target, fires on A in 32 directions, draws the reticle and the gun (16 directions, recoil, muzzle flash) |
| `src/weapon.c` | Banked: the weapons' stats and presets, loading their art, switching (SELECT). The sword's swing is in `aim.c` |
| `src/oam.h` | Which hardware sprites are the player's, the weapon's, the reticle's; actors get the rest |
| `src/weapon_<name>.h` | Generated by `tools/make-weapon-art.mjs`: where each weapon sits in each of its 16 directions |
| `src/aim_score.s` | Assembly: scores one enemy for auto-aim (distance, facing bias, line of sight), picks the target, draws the reticle |
| `src/shots.s` | Assembly: the player's shots move, hit walls or enemies, and draw in one pass; spawning a shot's pellets; the sword's hit-box scan |
| `src/room_fill.s` | Assembly: builds background rows/columns for the scrolling camera, and `room_block_at` |
| `src/room.c` | The current room's 16×16 block grid and collision queries |
| `src/run_over.c` | Run stats. START returns to the title |
| `src/run.h` | Per-run state: seed, play time, stats |

To add a scene: add a `STATE_*` ID and its `*_enter`/`*_update` to `state.h`, then add
it to both switches in `main.c`.

**Frame budget.** RogueGB is **Game Boy Color only** and runs the CPU at double speed
(`cpu_fast()` at boot), so a frame is 140,448 CPU cycles; the harness counts from the end of
vblank (9,120), which leaves 131,328 before the game drops a frame (`FRAME_LIMIT` in
`tests/helpers.mjs`). VRAM updates (new background rows and
columns, sprite animation frames) are queued during the frame and written by the vblank
interrupt (`gfx_strip`, `gfx_vram_copy`), because writing VRAM while the screen is drawing
waits on every byte. Keep per-frame code cheap:

- No 32-bit math or division: the CPU has no multiply, so they're long software routines
  (and they sit in bank 0). Split a 16-bit multiply into 8-bit halves (`diag_scale`), or
  cache it when its inputs rarely change (the camera lead, diagonal speeds).
- The hottest loops are in assembly (`*.s` in `src/`). Enemies think at 30 Hz (half of
  them each frame).
- Expensive checks are spread out. One terrain check per frame (`terrain_checks_left`):
  a slime's landing spot or auto-aim's line of sight, whoever asks first. Auto-aim scores
  one enemy every other frame, and skips its turn when the frame is running late (it reads
  the current scanline, `LY_REG`).
- The camera streams one strip per frame and moves at most 4 px per axis per frame, so the
  background always keeps up; anything faster forces a full, very slow redraw.

Tests guard it: `tests/camera.test.mjs` fails if a frame of walking, scrolling and jumping
uses more than half the frame (`emu.lastFrameCycles`); `tests/actors.test.mjs` caps an arena
fight at 52k (55k while shooting) and checks full pools (8 enemies, 20 bullets: median
~55k, at most 2% of frames over: past M1's target of 6+ enemies and 10+ bullets). `node harness/profile.mjs walk|arena|shoot|stress` shows
which functions the cycles go to in the worst frames (banked ones too), and how many
frames are over budget.

**ROM banks.** The CPU sees bank 0 (0x0000-0x3FFF) at all times and one switchable bank
at a time at 0x4000-0x7FFF. Bank 0 holds the hot paths: the main loop, vblank code, player,
actors, camera, collision, text drawing and the assembly loops. Cold code and data go in
switchable banks:

- C files that start with `#pragma bank 255` (the scenes, `room_lab.c`). Their entry points
  are declared `BANKED`, so calls switch banks automatically.
- Assets marked `bank: true` in `tools/assets.mjs` (all graphics). Bank-0 code can switch
  to `BANK(name)` with `SWITCH_ROM` and back; banked code must not (it would switch its
  own code out), so it uses the bank-0 helpers `banked_memcpy`, `banked_sprite_data` and
  `banked_bkg_data` (gfx.c). Small tables read every frame are copied to RAM when loaded. Animation frames copied to VRAM go
  through `gfx_vram_copy` with their bank, and the vblank handler switches it in.
- Don't pass pointers to banked data into a function in a *different* switchable bank:
  calling it hides the caller's bank. That's why text drawing stays in bank 0.

The build fails if anything fixed spills past 0x4000 (it would land in whichever bank is
switched in) or bank 0 is over 90% full; `tests/banks.test.mjs` checks what lives where.

Artwork is converted at build time. Small UI sprites the packs don't have (the aim
reticle, the player's shot, the muzzle flash) are drawn as ASCII art in
`tools/make-ui-art.mjs`, which writes them to `artwork/ui/`. Held weapons are drawn once,
pointing right, in `tools/make-weapon-art.mjs` (adapted from the Sprite Studio templates):
it rotates them into 16 directions (`artwork/weapons/`) and writes their placement table
(`src/weapon_<name>.h`). `tools/make-aim-table.mjs` writes the direction lookup auto-aim
uses. Rerun these after editing them; their output is committed. `tools/assets.mjs` turns the PNGs listed in its
`ASSETS` table into C arrays in `build/gen/`:

- **Background tiles** (font, frame): 2bpp tiles, at most 4 shades per image.
- **Sprites** (`kind: 'sprite'`): 16×16 animation frames for 8×16 sprite mode, for example
  `spr_player`. Identical and mirror-image columns are stored once, `mirror` builds an
  animation from another one flipped (left-facing walks come from right-facing ones), and
  each animation gets a `SPR_<NAME>_ANIM_<ANIM>` index. At most 3 colors plus transparent.

## Setup (once)

1. Install **Node.js 22 or newer** (the LTS installer from nodejs.org is fine).
2. Keep `gbdk-win64.zip` in `tools\`. The first build extracts it automatically
   into `tools\.gbdk-win64\`. An existing GBDK install also works: set `GBDK_HOME` or
   extract it to `tools\gbdk\`.

No `npm install` is needed. Everything the harness uses is already in the repo.

## Daily loop

| Command | What it does |
|---|---|
| `npm run play` | Builds, opens a local server at http://localhost:8080 and rebuilds and reloads the game whenever you save a file in `src\` |
| `npm run build` | Builds `build\roguegb.gb` (plus `.noi`/`.sym`/`.map` symbols) and prints ROM/RAM usage |
| `npm test` | Builds, then runs the headless tests in `tests\` |
| `npm run test:update` | Accepts the current screens as the new golden screenshots |
| `npm run vars` | Lists the game's global variables you can watch |
| `npm run run -- --input "0-60:RIGHT 30:A"` | Runs the ROM headlessly with scripted input (see below) |

`build\roguegb.gb` also runs in any emulator. Emulicious is the best choice for
source-level debugging, and it reads the `.sym` file.

## The playtest page

`npm run play` opens a page with:

- keyboard, gamepad and on-screen touch controls
- pause, frame-step, 0.25–4x speed, hold **Tab** to fast-forward, hold **R** to rewind
- save and load state
- **Watch**: live values of any global C variable, picked by name
- **Record**: records your input from the current moment and saves it to `replays\recordings\`
- **Report bug**: writes your note, a screenshot and the last 10–30 seconds of input to
  `replays\bugs\`, along with save states from the start and end of that clip

Every push to `main` also deploys the page to GitHub Pages at
**https://mrbendel.github.io/RogueGB/** (built and tested by `.github/workflows/pages.yml`).
Open it on a phone to test with the touch controls.

A hosted copy of the same page runs on claude.ai, so you can test from a phone or
share it with someone else. Reports filed there are saved with the page, and Claude
imports them with `tools/import-reports.mjs`.

## Replays reproduce exactly

The playtest page and the headless runner use the same emulator core
([Boytacean](https://github.com/joamag/boytacean), compiled to WebAssembly) and
apply input at frame boundaries. So a recording plays back exactly the same in tests:

```sh
node harness/cli.mjs run --replay replays/bugs/<file>.json --watch player_x,player_y --shot build/end.png
# ...
# reproduced: yes (matches the end state saved by the playtest page)
```

To turn a bug into a regression test:

```js
test('bullets never exceed 6 (bug from playtest)', () => {
  const emu = boot().replay('replays/bugs/2026-10-10T..._too-many-bullets.json');
  assert.ok(emu.v('bullets_active') <= 6);
});
```

A replay records which ROM it came from. After the game logic changes, an old
replay may play out differently. The runner warns when the ROM differs.

## Headless runner

```
node harness/cli.mjs run [rom] [--frames N] [--input SCRIPT | --replay FILE]
                         [--watch a,b:u16,c:i8] [--trace N] [--shot FILE]
                         [--shots-every N --out DIR] [--state-in F] [--state-out F]
                         [--record FILE] [--mode dmg|cgb] [--json]
```

Input scripts: `"30:START 31:- 60-180:RIGHT 90-92:RIGHT+A"`
- `a:BTN` holds the buttons from frame `a` onward
- `a-b:BTN` holds them for frames `a` to `b-1`
- `-` releases everything

Button names: `UP DOWN LEFT RIGHT A B START SELECT`.

It runs about 40–60x faster than realtime.

## Writing tests

The tests in `tests\*.test.mjs` use `node:test`. `boot()` gives a fresh emulator with these helpers:

- `v(name, type)`: read a global variable
- `hold('RIGHT+A', frames)`, `idle(frames)`, `tap('START')`: press buttons
- `play(script, frames)`: run an input script
- `replay(file)`: play back a recording or bug report
- `until(pred, maxFrames)`: run until a condition holds
- `shot(name)`: save a screenshot
- `matchGolden(emu, name)`: screenshot regression check against `tests/goldens.json`

Make game state you want to test a **global, non-static variable**. That gives it a
symbol, so tests and the watch panel can read it by name.

## Layout

```
src/              game source (C)
tools/            build.mjs, assets.mjs, test.mjs, serve.mjs, bundle.mjs, import-reports.mjs, GBDK archives
harness/          emu.mjs (shared core wrapper), node.mjs, cli.mjs, vendor/boytacean
web/              playtest page
tests/            node:test suites + goldens.json
replays/          recordings and bug reports from playtests
docs/gbdk/        GBDK reference docs (Markdown)
artwork/          source art (PNG); converted by tools/assets.mjs
build/            output (ignored), including build/gen/ generated tile sources
```

## Accuracy note

Boytacean passes the dmg-acid2 and cgb-acid2 rendering tests, but it is not
cycle-accurate. It's fine for logic, input and visual regression tests. Before
release, also check timing-sensitive things like raster effects, sound, and
running out of CPU time per frame in Emulicious or SameBoy, and on real hardware.
