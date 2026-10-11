# RogueGB

The plan, the design decisions and the milestones are in [ROADMAP.md](ROADMAP.md).

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
| B | Jump; B again in mid-air dashes toward the d-pad direction (once per jump) | |
| SELECT | Next movement preset (SNAPPY, SMOOTH, FLOATY, CLASSIC) | End the run |
| START | Pause and open the tuning panel | Resume |

Speed is in pixels per frame. ACCEL, DECEL and REVERSE are the frames it takes to reach top
speed, to stop, and to cancel full speed when pushing the other way. Any change shows the
preset as CUSTOM. JUMP is takeoff speed and GRAVITY the pull per frame (both in pixels per frame);
DASH SPD and DASH FR are the dash's speed and length. CAM LEAD is how many frames of
movement the camera looks ahead; CAM EASE is how quickly it catches up (0 = locked to the
player, higher = smoother and lazier).

## Code structure

| File | What it does |
|---|---|
| `src/main.c` | Boot and the main loop. Runs one scene update per frame and handles scene changes (fade out, `enter()` the new scene, fade in) |
| `src/state.h` | Scene IDs (`STATE_TITLE`, `STATE_PLAY`, `STATE_RUN_OVER`), `state_change()`, `wait_frame()` |
| `src/input.c` | Reads the joypad once per frame: `joy` (held), `joy_pressed`, `joy_released` |
| `src/gfx.c` | Font and frame tiles, `bkg_text`/`win_text`/`bkg_number`, framed boxes, palette fades |
| `src/title.c` | Title screen. START seeds the run's RNG and starts a run |
| `src/play.c` | Gameplay scene: the movement lab, HUD row and the pause/tuning panel |
| `src/player.c` | Player movement (8.8 fixed point, eased per axis), collision with wall sliding and corner nudging, presets |
| `src/sprite_slot.c` | Draws a 16×16 character from pre-rendered frames: two 8×16 sprites and a double-buffered 8-tile VRAM slot |
| `src/camera.c` | Camera: eased follow with lead, clamped to the room; streams background columns and rows as it scrolls |
| `src/actor.c` | Enemies and enemy bullets: pools, AI (slime chaser, peashooter shooter), contact and bullet damage, sprite allocation |
| `src/actor_draw.s` | Assembly: writes actors and bullets into the sprite table, moves bullets |
| `src/room_fill.s` | Assembly: builds background rows/columns for the scrolling camera |
| `src/room.c` | The current room's 16×16 block grid and collision queries |
| `src/run_over.c` | Run stats. START returns to the title |
| `src/run.h` | Per-run state: seed, play time, stats |

To add a scene: add a `STATE_*` ID and its `*_enter`/`*_update` to `state.h`, then add
it to both switches in `main.c`.

**Frame budget.** A frame is 70,224 CPU cycles. VRAM updates (new background rows and
columns, sprite animation frames) are queued during the frame and written by the vblank
interrupt (`gfx_strip`, `gfx_vram_copy`), because writing VRAM while the screen is drawing
waits on every byte. Avoid 32-bit math in per-frame code: the CPU has no multiply, so a
32-bit multiply is a long software routine. `tests/camera.test.mjs` fails if any frame of
walking, scrolling and jumping uses more than half the frame (`emu.lastFrameCycles`), and
`tests/actors.test.mjs` fails if an arena fight ever exceeds a frame. The hottest loops are
in assembly (`*.s` in `src/`), and enemies think at 30 Hz (half of them each frame).
`harness/profile.mjs` profiles which functions the CPU time goes to.

Artwork is converted at build time. `tools/assets.mjs` turns the PNGs listed in its
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
