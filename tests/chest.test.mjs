// The arena's chest and the first shield (chest.c, issue #10).
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const LOCKED = 0, READY = 1, OPEN = 2, TAKEN = 3;
const CHEST_X = 17 * 16, CHEST_Y = 21 * 16;  // chest.h
const ACTOR_MAX = 8, BULLET_MAX = 20, BULLET_SIZE = 13;
const SHIELD_CAP = 40;

const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v) => emu.writeBytes(addr(emu, name), [v & 0xff, (v >> 8) & 0xff]);
const px = (emu) => emu.v('player_x', 'u16');
const py = (emu) => emu.v('player_y', 'u16');

/** Put the player at (x, y) and let the camera catch up. */
function teleport(emu, x, y) {
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y);
  w16(emu, 'player_vx', 0);
  w16(emu, 'player_vy', 0);
  return emu.idle(60);
}

/** In the arena with the wave spawned, then every enemy and bullet removed. */
function waveCleared() {
  const emu = startRun().idle(2);
  teleport(emu, 9 * 16, 17 * 16);  // in the arena: the wave spawns
  assert.equal(emu.v('wave_spawned'), 1);
  assert.equal(emu.v('chest_state'), LOCKED, 'locked while enemies are about');
  for (let i = 0; i < ACTOR_MAX; i++) w8(emu, 'actor_type', 0, i);
  w8(emu, 'actor_count', 0);
  for (let i = 0; i < BULLET_MAX; i++) emu.write8(addr(emu, 'bullets') + i * BULLET_SIZE, 0);
  emu.idle(2);
  return emu;
}

/** Beside the chest (left of it, feet level with its middle). */
const besideChest = (emu) => teleport(emu, CHEST_X - 20, CHEST_Y - 4);

test('the chest is locked until the wave is cleared, then it can be opened', () => {
  const emu = waveCleared();
  assert.equal(emu.v('chest_state'), READY);
});

test('the chest is solid: you walk into it, not through it (open or not)', () => {
  const emu = besideChest(waveCleared());
  emu.hold('RIGHT', 40);
  assert.equal(py(emu), CHEST_Y - 4, 'level with it');
  assert.ok(px(emu) + 3 + 10 <= CHEST_X, `stopped at the chest (x ${px(emu)})`);
});

test('walking into the chest opens it; the shield pops out and lands in front', () => {
  const emu = besideChest(waveCleared());
  emu.hold('RIGHT', 20);
  assert.equal(emu.v('chest_state'), OPEN);
  const zs = [];
  for (let f = 0; f < 60 && !emu.v('item_landed'); f++) zs.push(emu.idle(1).v('item_z', 'i16'));
  assert.ok(Math.max(...zs) > 4 * 256, 'it pops up');
  assert.equal(emu.v('item_landed'), 1, 'and lands');
  assert.equal(emu.v('item_x', 'u16'), CHEST_X);
  assert.ok(emu.v('item_y', 'u16') > CHEST_Y + 8, 'in front of the chest');
  assert.equal(emu.v('shield_cap'), 0, 'not yours until you pick it up');
});

test('walking over the shield equips it: the bar fills from empty', () => {
  const emu = besideChest(waveCleared());
  emu.hold('RIGHT', 20);
  emu.until((e) => e.v('item_landed'), 80, 'the shield to land');
  // Step down past the chest, then right onto the shield.
  emu.hold('DOWN', 20);
  for (let f = 0; f < 60 && emu.v('chest_state') !== TAKEN; f++) emu.hold('RIGHT', 1);
  assert.equal(emu.v('chest_state'), TAKEN, 'picked up');
  assert.equal(emu.v('shield_cap'), SHIELD_CAP);
  assert.ok(emu.v('shield_hp') < 10, 'starts nearly empty');
  emu.idle(SHIELD_CAP * 2 + 10);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP, 'charged up');
  assert.equal(emu.v('hud_shield_px'), SHIELD_CAP);
});

test('the chest glows while it can be opened', () => {
  const emu = besideChest(waveCleared());
  const SPR_PAL_BULLET = 4, SPR_PAL_SHOT = 5, PROP_OAM = 11;
  const pals = new Set();
  for (let f = 0; f < 40; f++) pals.add(emu.idle(1).readBytes(0xc000 + PROP_OAM * 4 + 3, 1)[0] & 7);
  assert.deepEqual([...pals].sort(), [SPR_PAL_BULLET, SPR_PAL_SHOT]);
});

test('screen: the chest ready to open (glowing)', () => {
  const emu = besideChest(waveCleared());
  matchGolden(emu.until((e) => (e.v('chest_tick') & 31) === 20, 64, 'mid-glow').idle(2), 'chest-ready');
});

test('screen: the shield out of the chest', () => {
  const emu = besideChest(waveCleared());
  emu.hold('RIGHT', 20);
  emu.until((e) => e.v('item_landed'), 80, 'the shield to land');
  matchGolden(emu.until((e) => (e.v('chest_tick') & 32) === 0, 64, 'the bob down').idle(2), 'chest-open');
});
