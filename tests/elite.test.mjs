// Elites (elite.c): twice the health, a modifier, the bullets' hot colours, and a
// gun when one falls. A swift one thinks twice a step; a bomber bursts into peas.
import { test } from 'node:test';
import { assert, startRun, startDescent } from './helpers.mjs';

const SLIME = 1, DUELIST = 4, SWIFT = 1, BOMBER = 2, DROP_GUN = 2, SPR_PAL_BULLET = 4;
const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const r8 = (emu, name, i) => emu.read8(addr(emu, name) + i);

function spawn(emu, type, x, y) {
  w8(emu, 'debug_type', type);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  return emu.v('debug_slot');
}

test('a bomber elite bursts into eight peas when it falls, and leaves a gun', () => {
  const emu = startDescent().idle(2);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  const i = spawn(emu, SLIME, px + 48, py);
  w8(emu, 'actor_elite', BOMBER, i);
  emu.idle(2);
  const before = emu.v('bullets_fired', 'u16'), xp = emu.v('xp_pending', 'u16');
  w8(emu, 'actor_hp', 0, i);
  w8(emu, 'shots_killed', 1);  // (down: actor.c reaps it)
  emu.idle(1);
  assert.equal(r8(emu, 'actor_type', i), 0, 'down');
  assert.equal(emu.v('bullets_fired', 'u16') - before, 8, 'a ring of eight');
  const drops = Array.from(emu.readBytes(addr(emu, 'drop_kind'), 8));
  assert.ok(drops.includes(DROP_GUN), 'a gun on the floor');
  assert.equal(r8(emu, 'actor_elite', i), 0, 'the slot is plain again');
});

test('a swift elite thinks twice a step', () => {
  const tick = (elite) => {
    const emu = startRun().idle(2);
    const i = spawn(emu, DUELIST, emu.v('player_x', 'u16') + 200, emu.v('player_y', 'u16'));
    w8(emu, 'actor_elite', elite, i);
    w8(emu, 'actor_timer', 100, i);
    emu.idle(20);
    return 100 - r8(emu, 'actor_timer', i);
  };
  const plain = tick(0), swift = tick(SWIFT);
  assert.equal(swift, plain * 2, `timer ran ${swift} vs ${plain}`);
});

test('an elite in a camp: twice the health, its name on the HUD, the hot colours', () => {
  let found = null;
  for (let wait = 0; wait < 120 && !found; wait += 7) {
    const emu = startDescent(undefined, 4242 + wait);
    w8(emu, 'layer_depth', 7);  // (deep: an elite in every camp)
    const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
    const e = [1, 2, 4, 8].find((x) => links & x);
    const w = emu.v('room_w'), h = emu.v('room_h');
    const o = (emu.readBytes(addr(emu, 'level_open'), 4)[{ 1: 0, 2: 1, 4: 2, 8: 3 }[e]]) * 16;
    const at = { 1: [o, 2 * 16 - 8], 4: [o, h * 16 - 3 * 16 + 4], 8: [2 * 16, o - 4], 2: [w * 16 - 3 * 16, o - 4] }[e];
    w16(emu, 'player_x', at[0]);
    w16(emu, 'player_y', at[1]);
    const before = emu.v('maps_entered', 'u16');
    for (let f = 0; f < 120 && emu.v('maps_entered', 'u16') === before; f++) emu.hold({ 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e], 1);
    emu.idle(30);
    if (!emu.v('level_camps')) continue;
    w16(emu, 'player_x', emu.read16(addr(emu, 'camp_x')));
    w16(emu, 'player_y', emu.read16(addr(emu, 'camp_y')));
    emu.idle(3);
    for (let i = 0; i < 8; i++)
      if (r8(emu, 'actor_type', i) && r8(emu, 'actor_elite', i)) found = { emu, i };
  }
  assert.ok(found, 'a camp woke an elite');
  const { emu, i } = found;
  const type = r8(emu, 'actor_type', i), hp = r8(emu, 'actor_hp', i);
  const base = { 1: 3, 2: 4, 3: 5, 4: 6 }[type] + emu.v('enemy_hp_bonus');
  assert.equal(hp, base * 2 + 2, 'twice the health');
  assert.equal(r8(emu, 'actor_p0', i) & 7, SPR_PAL_BULLET, 'drawn hot');
  const name = String.fromCharCode(...emu.readBytes(addr(emu, 'hud_text'), 7));
  assert.ok(['SWIFT! ', 'BOMBER!'].includes(name), `named: "${name}"`);
});
