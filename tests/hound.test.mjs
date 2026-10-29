// The hound (hound.c): the descent's flanker. It circles you at mid range, warns
// (crouched, flashing), lunges at where you were, and rests after it.
import { test } from 'node:test';
import { assert, startRun, FRAME_LIMIT } from './helpers.mjs';

const HOUND = 3;
const PROWL = 0, CROUCH = 1, LUNGE = 2, REST = 3;
const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const at = (emu, name, i) => emu.read16(addr(emu, name) + i * 2);

function spawn(emu, x, y) {
  w8(emu, 'debug_type', HOUND);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  return slot;
}

test('a hound closes in, circles, warns, lunges at you and rests after', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 80, py);
  const seen = new Set(), dist = () => Math.abs(at(emu, 'actor_x', i) - emu.v('player_x', 'u16')) + Math.abs(at(emu, 'actor_y', i) - emu.v('player_y', 'u16'));
  let flashed = false, lungeFrom = null, lunged = 0, closest = 999;
  for (let f = 0; f < 400 && !(seen.has(REST) && lunged); f++) {
    emu.idle(1);
    const s = emu.read8(addr(emu, 'actor_state') + i);
    seen.add(s);
    if (s === PROWL) closest = Math.min(closest, dist());
    if (s === CROUCH && emu.read8(addr(emu, 'actor_flash') + i)) flashed = true;
    if (s === LUNGE && !lungeFrom) lungeFrom = [at(emu, 'actor_x', i), at(emu, 'actor_y', i)];
    if (s === REST && lungeFrom && !lunged) lunged = Math.abs(at(emu, 'actor_x', i) - lungeFrom[0]) + Math.abs(at(emu, 'actor_y', i) - lungeFrom[1]);
  }
  assert.ok(closest < 80, `closed in (to ${closest} px)`);
  assert.ok(seen.has(CROUCH) && flashed, 'warned first: crouched, flashing');
  assert.ok(lunged >= 20, `lunged ${lunged} px`);
  assert.ok(seen.has(REST), 'rested after');
  emu.shot('hound');
});

test('a hound goes down to gunfire', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  spawn(emu, px + 60, py);
  for (let f = 0; f < 900 && emu.v('actors_active'); f++) emu.hold('A', 1);
  assert.equal(emu.v('actors_active'), 0, 'down');
  assert.equal(emu.v('enemies_killed', 'u16'), 1);
});

test('eight hounds at once stay within the frame budget', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  for (let k = 0; k < 8; k++) spawn(emu, px + (k & 3) * 20 - 30, py + (k >> 2) * 60 - 40);
  let worst = 0;
  for (let f = 0; f < 300; f++) {
    emu.hold(f & 64 ? 'LEFT' : 'RIGHT', 1);
    worst = Math.max(worst, emu.lastFrameCycles);
  }
  assert.ok(worst < FRAME_LIMIT, `worst frame ${worst} of ${FRAME_LIMIT}`);
});
