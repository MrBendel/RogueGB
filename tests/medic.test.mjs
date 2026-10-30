// The medic drone (medic.c): the descent's support. It hangs back from you and,
// every few seconds, glows and heals the squadmates near it, never past full.
import { test } from 'node:test';
import { assert, startRun } from './helpers.mjs';

const PEASHOOTER = 2, MEDIC = 6, GLOW = 1;
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

test('a medic glows, then heals a hurt squadmate near it, never past full', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const m = spawn(emu, MEDIC, px + 64, py);
  const s = spawn(emu, PEASHOOTER, px + 64, py + 20);
  const full = 4 + emu.v('enemy_hp_bonus');
  w8(emu, 'actor_hp', 1, s);
  w8(emu, 'actor_timer', 20, m);
  let glowed = false, healedAt = -1;
  for (let f = 0; f < 60 && healedAt < 0; f++) {
    emu.idle(1);
    if (r8(emu, 'actor_state', m) === GLOW && r8(emu, 'actor_flash', m)) glowed = true;
    if (r8(emu, 'actor_hp', s) > 1) healedAt = f;
  }
  assert.ok(glowed, 'warned first: glowing');
  assert.equal(r8(emu, 'actor_hp', s), 4 > full ? full : 4, 'patched up by 3');
  emu.shot('medic');
  for (let k = 0; k < 8; k++) w8(emu, 'player_hp', emu.v('player_hp_max')), emu.idle(50);  // (the turret's shooting)
  assert.equal(r8(emu, 'actor_hp', s), full, 'back to full, and no further');
});

test('a medic backs away from you', () => {
  const emu = startRun().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const m = spawn(emu, MEDIC, px + 24, py);
  const d0 = Math.abs(at(emu, 'actor_x', m) - px);
  emu.idle(30);
  assert.ok(Math.abs(at(emu, 'actor_x', m) - px) > d0 + 8, 'farther away');
});
