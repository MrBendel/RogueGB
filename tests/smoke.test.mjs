// Tests for the game shell: title -> play (with pause) -> run over -> title.
// Movement and collision are in movement.test.mjs.
// Keep at least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import { assert, matchGolden, settle, bootToTitle, startRun, TITLE, PLAY, RUN_OVER } from './helpers.mjs';

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

test('START pauses and resumes; the run clock stops while paused', () => {
  const emu = startRun().tap('START');
  assert.equal(emu.v('paused'), 1);
  const x0 = emu.v('player_x', 'u16');
  const y0 = emu.v('player_y', 'u16');
  const t0 = emu.v('run_seconds', 'u16') * 60 + emu.v('run_ticks');
  emu.hold('DOWN+A', 120); // d-pad drives the tuning panel while paused, not the player
  assert.equal(emu.v('player_x', 'u16'), x0);
  assert.equal(emu.v('player_y', 'u16'), y0);
  assert.equal(emu.v('run_seconds', 'u16') * 60 + emu.v('run_ticks'), t0);
  emu.tap('START');
  assert.equal(emu.v('paused'), 0);
  emu.idle(130);
  assert.equal(emu.v('run_seconds', 'u16'), 2);
});

test('SELECT while paused ends the run; START returns to the title', () => {
  const emu = startRun().hold('RIGHT', 20).tap('START').idle(3).tap('SELECT');
  settle(emu, RUN_OVER, 'run over screen');
  emu.tap('START');
  assert.equal(emu.v('next_state'), RUN_OVER, 'input is ignored for a moment');
  emu.idle(30).tap('START');
  settle(emu, TITLE, 'title screen');
  // And a new run starts fresh, back at the start position.
  startRun(emu);
  assert.equal(emu.v('run_seconds', 'u16'), 0);
  assert.equal(emu.v('player_x', 'u16') >> 8, 72);
  assert.equal(emu.v('player_vx', 'i16'), 0);
});

test('screen: title', () => {
  matchGolden(bootToTitle().idle(10), 'title');
});

test('screen: starting room', () => {
  matchGolden(startRun().idle(10), 'start-room');
});

test('screen: paused', () => {
  matchGolden(startRun().idle(10).tap('START').idle(5), 'paused');
});

test('screen: run over', () => {
  const emu = startRun().hold('RIGHT', 30).idle(100).tap('START').idle(3).tap('SELECT');
  matchGolden(settle(emu, RUN_OVER, 'run over screen'), 'run-over');
});
