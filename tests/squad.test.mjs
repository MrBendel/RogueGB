// The camp kinds made from the enemy trials (squad.c): the spider lunges after a
// warning, the bulwark's shield stops shots from in front only, and the rift
// stalker can't be shot while it shimmers.
import { test } from 'node:test';
import { assert, startRun } from './helpers.mjs';

const SPIDER = 7, BULWARK = 8, STALKER = 9;
const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const r8 = (emu, name, i) => emu.read8(addr(emu, name) + i);
const at = (emu, name, i) => emu.read16(addr(emu, name) + i * 2);

function spawn(emu, type, x, y) {
  w8(emu, 'debug_type', type);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  return slot;
}

// A player shot at (x, y) moving (vx, vy) px/frame 8.8 (the Bullet record: actor.h).
function shoot(emu, x, y, vx, vy) {
  const b = (v) => [v & 255, (v >> 8) & 255, v < 0 ? 255 : 0];
  emu.writeBytes(addr(emu, 'shots'), [60, ...b(vx), 0, x & 255, x >> 8, ...b(vy), 0, y & 255, y >> 8]);
  w8(emu, 'shot_count', 1);
  w8(emu, 'shot_damage', 2);
}

test('a spider flashes, then lunges at you', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const s = spawn(emu, SPIDER, px + 36, py);
  w8(emu, 'actor_timer', 2, s);
  let warned = false;
  for (let f = 0; f < 40 && r8(emu, 'actor_state', s) !== 2; f++) {
    emu.idle(1);
    if (r8(emu, 'actor_flash', s)) warned = true;
  }
  assert.ok(warned, 'flashed first');
  assert.equal(r8(emu, 'actor_state', s), 2, 'lunging');
  const d0 = Math.abs(at(emu, 'actor_x', s) - px);
  emu.idle(6);
  assert.ok(Math.abs(at(emu, 'actor_x', s) - px) < d0 - 8, 'closed in fast');
});

test("a bulwark's shield stops shots from in front, not from behind", () => {
  for (const front of [true, false]) {
    const emu = startRun().idle(2);
    const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
    const b = spawn(emu, BULWARK, px + 56, py);
    w8(emu, 'actor_timer', 200, b);
    emu.idle(2);
    assert.equal(r8(emu, 'actor_try', b), 1, 'facing you (left)');
    const hp = r8(emu, 'actor_hp', b), bx = at(emu, 'actor_x', b), by = at(emu, 'actor_y', b);
    if (front) shoot(emu, bx - 30, by, 3 * 256, 0);
    else shoot(emu, bx + 34, by, -3 * 256, 0);
    emu.idle(16);
    if (front) {
      assert.equal(r8(emu, 'actor_hp', b), hp, 'blocked');
      assert.equal(emu.v('squad_blocks', 'u16'), 1);
    } else {
      assert.ok(r8(emu, 'actor_hp', b) < hp, 'hit in the back');
    }
  }
});

test('a rift stalker is shot through while it shimmers, and hurt once it has dashed', () => {
  for (const solid of [false, true]) {
    const emu = startRun().idle(2);
    const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
    const s = spawn(emu, STALKER, px + 56, py);
    w8(emu, 'actor_timer', 200, s);
    if (solid) w8(emu, 'actor_state', 3, s);  // (recovering)
    emu.idle(2);
    assert.equal(r8(emu, 'actor_guard', s), solid ? 0 : 1);
    const hp = r8(emu, 'actor_hp', s), sx = at(emu, 'actor_x', s), sy = at(emu, 'actor_y', s);
    w8(emu, 'actor_timer', 200, s);
    shoot(emu, sx - 24, sy, 3 * 256, 0);
    emu.idle(12);
    if (solid) assert.ok(r8(emu, 'actor_hp', s) < hp, 'hit');
    else assert.equal(r8(emu, 'actor_hp', s), hp, 'passed through');
  }
});
