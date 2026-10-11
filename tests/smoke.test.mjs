// Tests for the game shell: title -> play (with pause) -> run over -> title.
// Keep at least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import { boot, assert, matchGolden } from './helpers.mjs';

// Must match src/state.h.
const TITLE = 0, PLAY = 1, RUN_OVER = 2;

/** Run until the given state is active and its fade-in has finished. */
function settle(emu, state, what) {
  // frame_count > 0: before boot code runs, RAM reads as zeros (= TITLE, settled).
  return emu.until(
    (e) => e.v('frame_count', 'u16') > 0 && e.v('game_state') === state && !e.v('in_transition'),
    300,
    what,
  );
}

function bootToTitle() {
  return settle(boot(), TITLE, 'title screen');
}

function startRun(emu = bootToTitle()) {
  return settle(emu.tap('START'), PLAY, 'play state');
}

test('boots to the title screen and runs one loop per frame', () => {
  const emu = bootToTitle();
  const f0 = emu.v('frame_count', 'u16');
  emu.run(60);
  const fc = emu.v('frame_count', 'u16') - f0;
  // One game loop per frame: a slowdown or a hang shows up here.
  assert.ok(fc >= 59 && fc <= 61, `frame_count advanced ${fc} in 60 frames`);
});

test('title waits for START', () => {
  const emu = bootToTitle().hold('RIGHT+B+SELECT', 60).idle(5);
  assert.equal(emu.v('game_state'), TITLE);
});

test('START begins a run seeded by press timing', () => {
  const a = startRun(bootToTitle().idle(10));
  const b = startRun(bootToTitle().idle(37));
  assert.equal(a.v('run_seconds', 'u16'), 0);
  assert.notEqual(a.v('run_seed', 'u16'), b.v('run_seed', 'u16'));
  // Same timing, same seed: replays reproduce runs.
  assert.equal(startRun(bootToTitle().idle(10)).v('run_seed', 'u16'), a.v('run_seed', 'u16'));
});

test('the START that begins a run does not also pause it', () => {
  const emu = bootToTitle().hold('START', 1);
  settle(emu, PLAY, 'play state').hold('START', 10);
  assert.equal(emu.v('paused'), 0);
});

test('d-pad moves the player', () => {
  const emu = startRun().idle(2);
  const x0 = emu.v('player_x');
  emu.hold('RIGHT', 20);
  assert.equal(emu.v('player_x'), x0 + 20);
  emu.hold('LEFT', 10);
  assert.equal(emu.v('player_x'), x0 + 10);
});

test('player stays inside the room', () => {
  const emu = startRun().hold('LEFT+UP', 300);
  assert.equal(emu.v('player_x'), 16);
  assert.equal(emu.v('player_y'), 24);
});

test('holding A fires on a cooldown', () => {
  const emu = startRun().idle(2).hold('A', 40);
  // Cooldown is 8 frames, so 40 frames => 5 shots, max 6 bullets on screen.
  assert.equal(emu.v('bullets_fired', 'u16'), 5);
  assert.ok(emu.v('bullets_active') <= 6);
});

test('START pauses and resumes; the run clock stops while paused', () => {
  const emu = startRun().tap('START');
  assert.equal(emu.v('paused'), 1);
  const x0 = emu.v('player_x');
  const t0 = emu.v('run_seconds', 'u16') * 60 + emu.v('run_ticks');
  emu.hold('RIGHT+A', 120);
  assert.equal(emu.v('player_x'), x0);
  assert.equal(emu.v('bullets_fired', 'u16'), 0);
  assert.equal(emu.v('run_seconds', 'u16') * 60 + emu.v('run_ticks'), t0);
  emu.tap('START');
  assert.equal(emu.v('paused'), 0);
  emu.idle(130);
  assert.equal(emu.v('run_seconds', 'u16'), 2);
});

test('SELECT while paused ends the run; START returns to the title', () => {
  const emu = startRun().hold('A', 20).tap('START').tap('SELECT');
  settle(emu, RUN_OVER, 'run over screen');
  emu.tap('START');
  assert.equal(emu.v('next_state'), RUN_OVER, 'input is ignored for a moment');
  emu.idle(30).tap('START');
  settle(emu, TITLE, 'title screen');
  // And a new run starts fresh.
  startRun(emu);
  assert.equal(emu.v('bullets_fired', 'u16'), 0);
  assert.equal(emu.v('run_seconds', 'u16'), 0);
});

test('screen: title', () => {
  matchGolden(bootToTitle().idle(10), 'title');
});

test('screen: starting room', () => {
  matchGolden(startRun().idle(10), 'start-room');
});

test('screen: moving and shooting', () => {
  matchGolden(startRun().play('0-40:RIGHT 10-40:A', 40), 'shooting');
});

test('screen: paused', () => {
  matchGolden(startRun().idle(10).tap('START').idle(5), 'paused');
});

test('screen: run over', () => {
  const emu = startRun().hold('A', 41).idle(80).tap('START').tap('SELECT');
  matchGolden(settle(emu, RUN_OVER, 'run over screen'), 'run-over');
});
