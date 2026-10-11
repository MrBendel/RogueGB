// Smoke tests for the harness test scene. Replace these as the real game
// takes shape; keep at least "boots" and one golden screen per major screen.
import { test } from 'node:test';
import { boot, assert, matchGolden } from './helpers.mjs';

test('boots and runs the main loop', () => {
  const emu = boot();
  emu.run(60);
  const fc = emu.v('frame_count', 'u16');
  // One game loop per frame: a slowdown or a hang shows up here.
  assert.ok(fc >= 58 && fc <= 61, `frame_count=${fc} after 60 frames`);
});

test('d-pad moves the player', () => {
  const emu = boot().idle(5);
  const x0 = emu.v('player_x');
  emu.hold('RIGHT', 20);
  assert.equal(emu.v('player_x'), x0 + 20);
  emu.hold('LEFT', 10);
  assert.equal(emu.v('player_x'), x0 + 10);
});

test('player stays inside the room', () => {
  const emu = boot().hold('LEFT+UP', 300);
  assert.equal(emu.v('player_x'), 16);
  assert.equal(emu.v('player_y'), 24);
});

test('holding A fires on a cooldown', () => {
  const emu = boot().idle(5).hold('A', 40);
  // Cooldown is 8 frames, so 40 frames => 5 shots, max 6 bullets on screen.
  assert.equal(emu.v('bullets_fired', 'u16'), 5);
  assert.ok(emu.v('bullets_active') <= 6);
});

test('screen: starting room', () => {
  const emu = boot().idle(10);
  matchGolden(emu, 'start-room');
});

test('screen: moving and shooting', () => {
  const emu = boot().play('0-40:RIGHT 10-40:A', 40);
  matchGolden(emu, 'shooting');
});
