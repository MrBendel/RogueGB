// The loader (tank.c): the descent's tank. It rolls slowly at you, and shots don't
// push it; close in it stops and flashes, slams a ring of eight peas, and stands
// recovering.
import { test } from 'node:test';
import { assert, startRun } from './helpers.mjs';

const TANK = 5;
const ROLL = 0, WINDUP = 1, RECOVER = 2;
const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const at = (emu, name, i) => emu.read16(addr(emu, name) + i * 2);

function spawn(emu, x, y) {
  w8(emu, 'debug_type', TANK);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  return slot;
}

test('a loader rolls slowly at you, and shots do not push it', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 72, py);
  assert.equal(emu.read8(addr(emu, 'actor_hp') + i), 14 + emu.v('enemy_hp_bonus'), 'tough');
  const x0 = at(emu, 'actor_x', i);
  emu.idle(40);
  const rolled = x0 - at(emu, 'actor_x', i);
  assert.ok(rolled >= 16 && rolled <= 22, `half a px a frame (${rolled} px in 40)`);
  // Shoot it: it keeps coming instead of being shoved back.
  let back = 0;
  for (let f = 0; f < 30; f++) {
    const x = at(emu, 'actor_x', i);
    emu.hold(f & 2 ? [] : ['A'], 1);
    if (at(emu, 'actor_x', i) > x) back++;
  }
  assert.ok(emu.read8(addr(emu, 'actor_hp') + i) < 14 + emu.v('enemy_hp_bonus'), 'hit');
  assert.equal(back, 0, 'never pushed back');
});

test('close in, a loader warns, slams a ring of eight, then recovers', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 24, py);
  w8(emu, 'actor_timer', 0, i);
  let flashed = false, fired = -1, state;
  const before = emu.v('bullets_fired', 'u16');
  for (let f = 0; f < 120 && fired < 0; f++) {
    emu.idle(1);
    state = emu.read8(addr(emu, 'actor_state') + i);
    if (state === WINDUP && emu.read8(addr(emu, 'actor_flash') + i)) flashed = true;
    if (state === RECOVER) fired = emu.v('bullets_fired', 'u16') - before;
  }
  assert.ok(flashed, 'warned first');
  assert.equal(fired, 8, 'a ring of eight');
  emu.shot('tank');
  emu.idle(50);
  assert.equal(emu.read8(addr(emu, 'actor_state') + i), ROLL, 'rolling again');
});
