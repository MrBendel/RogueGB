// Shared helpers for ROM tests (node:test). Each test boots a fresh emulator.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {
  loadRom, saveScreenshot, readTyped, parseInputScript, replayInput, prepareReplay, maskFromNames, ROOT,
} from '../harness/node.mjs';

export { assert, ROOT };
const GOLDENS = path.join(ROOT, 'tests', 'goldens.json');
const OUT = path.join(ROOT, 'build', 'test-output');

/** Fresh emulator with convenience methods for tests. */
export function boot(opts = {}) {
  const emu = loadRom(opts.rom, opts);
  return Object.assign(emu, {
    /** Read a global C variable by name. type: u8|i8|u16|i16|bool */
    v(name, type = 'u8') {
      return readTyped(emu, emu.symbols.addr(name), type);
    },
    /** Hold buttons ("RIGHT+A", or [] for none) for n frames. */
    hold(buttons, n = 1) {
      const names = Array.isArray(buttons) ? buttons : String(buttons).split('+');
      emu.setButtons(maskFromNames(names));
      emu.run(n);
      return emu;
    },
    /** Release all buttons and wait n frames. */
    idle(n = 1) {
      emu.setButtons(0);
      emu.run(n);
      return emu;
    },
    /** Tap buttons for one frame, then release for one frame. */
    tap(buttons) {
      return emu.hold(buttons, 1).idle(1);
    },
    /** Play an input script like "0-60:RIGHT 30:A 31:-". */
    play(script, frames) {
      const input = replayInput({ events: parseInputScript(script) });
      const end = frames ?? parseInputScript(script).at(-1)[0] + 1;
      for (let f = 0; f < end; f++) {
        emu.setButtons(input(f));
        emu.step();
      }
      return emu;
    },
    /** Play a replay JSON file (from the playtest page) to the end. */
    replay(file) {
      const r = JSON.parse(fs.readFileSync(path.resolve(ROOT, file), 'utf8'));
      const input = prepareReplay(emu, r);
      for (let f = 0; f < r.length; f++) {
        emu.setButtons(input(f));
        emu.step();
      }
      return emu;
    },
    /** Run until predicate(emu) is true; fails after maxFrames. */
    until(pred, maxFrames = 600, what = 'condition') {
      for (let i = 0; i < maxFrames; i++) {
        if (pred(emu)) return emu;
        emu.step();
      }
      assert.fail(`timed out after ${maxFrames} frames waiting for ${what}`);
    },
    shot(name) {
      return saveScreenshot(emu, path.join(OUT, `${name}.png`));
    },
  });
}

/**
 * Screen regression check. Compares a hash of the current frame against
 * tests/goldens.json. Run with UPDATE_GOLDENS=1 to accept new output.
 * A PNG of the current frame is always written to build/test-output/.
 */
export function matchGolden(emu, name) {
  const goldens = fs.existsSync(GOLDENS) ? JSON.parse(fs.readFileSync(GOLDENS, 'utf8')) : {};
  const hash = emu.frameHash();
  const png = emu.shot(name);
  if (process.env.UPDATE_GOLDENS || !(name in goldens)) {
    goldens[name] = hash;
    fs.writeFileSync(GOLDENS, JSON.stringify(goldens, null, 2) + '\n');
    return;
  }
  assert.equal(hash, goldens[name], `screen "${name}" changed; see ${path.relative(ROOT, png)} (UPDATE_GOLDENS=1 to accept)`);
}

// Scene IDs, must match src/state.h.
export const TITLE = 0, PLAY = 1, RUN_OVER = 2;

/** Run until the given scene is active and its fade-in has finished. */
export function settle(emu, state, what = `state ${state}`) {
  // frame_count > 0: before boot code runs, RAM reads as zeros (= TITLE, settled).
  return emu.until(
    (e) => e.v('frame_count', 'u16') > 0 && e.v('game_state') === state && !e.v('in_transition'),
    300,
    what,
  );
}

export function bootToTitle() {
  return settle(boot(), TITLE, 'title screen');
}

/** Boot, press START on the title, and wait until gameplay is running. */
export function startRun(emu = bootToTitle()) {
  return settle(emu.tap('START'), PLAY, 'play state');
}
