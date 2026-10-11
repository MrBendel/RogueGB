# RogueGB

A Game Boy roguelite with run-and-gun shooting, built in C with
[GBDK-2020](https://gbdk.org/) 4.5.0. Right now `src/main.c` is a small test scene
(a ship that moves and shoots). Its only job is to prove the build and test pipeline works.

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
tools/            build.mjs, test.mjs, serve.mjs, bundle.mjs, import-reports.mjs, GBDK archives
harness/          emu.mjs (shared core wrapper), node.mjs, cli.mjs, vendor/boytacean
web/              playtest page
tests/            node:test suites + goldens.json
replays/          recordings and bug reports from playtests
docs/gbdk/        GBDK reference docs (Markdown)
build/            output (ignored)
```

## Accuracy note

Boytacean passes the dmg-acid2 and cgb-acid2 rendering tests, but it is not
cycle-accurate. It's fine for logic, input and visual regression tests. Before
release, also check timing-sensitive things like raster effects, sound, and
running out of CPU time per frame in Emulicious or SameBoy, and on real hardware.
