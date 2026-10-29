// The duelist (duelist.c): the descent's merc. It keeps to mid range sidestepping,
// warns (standing, flashing), fires a burst of three at where you were, reloads,
// and dashes aside once when shot while it strafes.
import { test } from 'node:test';
import { assert, startRun, FRAME_LIMIT } from './helpers.mjs';

const DUELIST = 4;
const STRAFE = 0, AIM = 1, BURST = 2, RELOAD = 3, DODGE = 4;
const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const at = (emu, name, i) => emu.read16(addr(emu, name) + i * 2);

function spawn(emu, x, y) {
  w8(emu, 'debug_type', DUELIST);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  return slot;
}

// One frame of a fight: toward the duelist while it's far, tapping fire (the
// pistol fires faster tapped). It keeps its distance, so standing still and
// holding fire mostly hits the walls.
function press(emu, i, f) {
  const dx = at(emu, 'actor_x', i) - emu.v('player_x', 'u16'), dy = at(emu, 'actor_y', i) - emu.v('player_y', 'u16');
  const keys = f & 4 ? [] : ['A'];
  if (Math.abs(dx) + Math.abs(dy) > 40) keys.push(Math.abs(dx) > Math.abs(dy) ? (dx > 0 ? 'RIGHT' : 'LEFT') : (dy > 0 ? 'DOWN' : 'UP'));
  emu.hold(keys, 1);
}

test('a duelist sidesteps at range, warns, fires a burst of three and reloads', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 64, py);
  const x0 = at(emu, 'actor_x', i), y0 = at(emu, 'actor_y', i);
  const seen = new Set();
  let flashed = false, moved = 0, fired = -1, before = emu.v('bullets_fired', 'u16');
  for (let f = 0; f < 400 && !(seen.has(RELOAD) && fired >= 0); f++) {
    emu.idle(1);
    const s = emu.read8(addr(emu, 'actor_state') + i);
    seen.add(s);
    if (s === STRAFE) moved = Math.max(moved, Math.abs(at(emu, 'actor_x', i) - x0) + Math.abs(at(emu, 'actor_y', i) - y0));
    if (s === AIM && emu.read8(addr(emu, 'actor_flash') + i)) flashed = true;
    if (s === RELOAD && fired < 0) fired = emu.v('bullets_fired', 'u16') - before;
  }
  assert.ok(moved >= 8, `sidestepped (${moved} px)`);
  assert.ok(seen.has(AIM) && flashed, 'warned first: standing, flashing');
  assert.equal(fired, 3, 'a burst of three');
  emu.shot('duelist');
});

test('shot while it strafes, a duelist dashes aside once', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 60, py);
  let dodged = 0;
  for (let f = 0; f < 600 && !dodged; f++) {
    press(emu, i, f);
    if (emu.read8(addr(emu, 'actor_state') + i) === DODGE) dodged = 1;
  }
  assert.ok(dodged, 'dashed aside');
});

test('a duelist goes down to gunfire', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, px + 60, py);
  for (let f = 0; f < 1500 && emu.v('actors_active'); f++) press(emu, i, f);
  assert.equal(emu.v('actors_active'), 0, 'down');
  assert.equal(emu.v('enemies_killed', 'u16'), 1);
});

test('eight duelists at once stay within the frame budget', () => {
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
