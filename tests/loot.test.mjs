// Loot (loot.c, pickups.c): chests in the descent's camps open once the camp is
// cleared and drop credits and a gun; credits are taken by walking over them; a
// gun shows a card and A takes it into the loadout, which the file keeps.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, startDescent, TITLE, PLAY } from './helpers.mjs';

const SIDE = { 1: 0, 2: 1, 4: 2, 8: 3 };
const DIR = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' };
const GUN_SIZE = 9, GUNS = 18, GUN_SEL = 54, CREDITS = 7;  // Profile offsets (save.h)
const DROP_CREDITS = 1, DROP_GUN = 2, CARD_NONE = 0xff;

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const words = (emu, n, len) => Array.from({ length: len }, (_, i) => emu.read16(A(emu, n) + i * 2));
const credits = (emu) => emu.readBytes(A(emu, 'profile') + CREDITS, 4).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const gunType = (emu, s) => emu.read8(A(emu, 'profile') + GUNS + s * GUN_SIZE);

function leaveBy(emu, e) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const o = (emu.readBytes(A(emu, 'level_open'), 4)[SIDE[e]] + 1) * 16;
  const at = { 1: [o, 2 * 16 - 8], 4: [o, h * 16 - 3 * 16 + 4], 8: [2 * 16, o - 4], 2: [w * 16 - 3 * 16, o - 4] }[e];
  w16(emu, 'player_x', at[0]);
  w16(emu, 'player_y', at[1]);
  const before = emu.v('maps_entered', 'u16');
  for (let i = 0; i < 120 && emu.v('maps_entered', 'u16') === before; i++) emu.hold(DIR[e], 1);
  return settle(emu, PLAY).idle(20);  // (the map change's fade-in runs inside play: play waits for it)
}

/** A descent whose first neighbour map has a camp with a chest; we're in it, the camp cleared. */
function atCampChest() {
  for (let s = 0; s < 40; s++) {
    const emu = startDescent(bootToTitle(), 5000 + s * 13);
    const links = emu.read8(A(emu, 'area_links') + emu.v('level_map'));
    leaveBy(emu, [1, 2, 4, 8].find((x) => links & x));
    const chests = emu.v('level_chests');
    const camp = chests && emu.read8(A(emu, 'chest_camp'));
    if (!chests || camp === 0xff) continue;
    // Wake the camp (stand at it), then clear it.
    const cx = words(emu, 'camp_x', 2)[camp], cy = words(emu, 'camp_y', 2)[camp];
    w16(emu, 'player_x', cx - 8);
    w16(emu, 'player_y', cy - 12);
    emu.idle(3);
    for (let i = 0; i < 8; i++) emu.write8(A(emu, 'actor_type') + i, 0);
    emu.idle(3);
    return emu;
  }
  assert.fail('no camp chest beside the start map');
}

test("a camp's chest opens once the camp is cleared: credits and a gun drop in front of it", () => {
  const emu = atCampChest();
  const x = emu.read16(A(emu, 'chest_x')), y = emu.read16(A(emu, 'chest_y'));
  const before = credits(emu);
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y + 8);
  emu.idle(2);
  assert.equal(emu.v('chests_opened', 'u16'), 1);
  const kinds = Array.from(emu.readBytes(A(emu, 'drop_kind'), 6));
  assert.ok(kinds.includes(DROP_GUN), `a gun: ${kinds}`);
  // (the credits land within reach of the chest's front: often taken at once)
  assert.ok(kinds.includes(DROP_CREDITS) || credits(emu) > before, 'credits, on the floor or taken');
});

test('credits are taken by walking over them; standing on a gun shows its card; A takes it', () => {
  const emu = atCampChest();
  const x = emu.read16(A(emu, 'chest_x')), y = emu.read16(A(emu, 'chest_y'));
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y + 8);
  emu.idle(2);
  const kinds = Array.from(emu.readBytes(A(emu, 'drop_kind'), 6));
  const at = (k) => kinds.indexOf(k);
  const dx = words(emu, 'drop_x', 6), dy = words(emu, 'drop_y', 6);
  // Credits (unless they were taken already: they land within reach of the chest).
  const c = at(DROP_CREDITS);
  if (c >= 0) {
    w16(emu, 'player_x', dx[c]);
    w16(emu, 'player_y', dy[c] - 4);
    emu.idle(2);
  }
  assert.ok(credits(emu) > 0, 'credits picked up');
  // The gun: its card, then A.
  const g = at(DROP_GUN);
  w16(emu, 'player_x', dx[g]);
  w16(emu, 'player_y', dy[g] - 4);
  emu.idle(3);
  assert.equal(emu.v('card_drop'), g, 'the card shows');
  matchGolden(emu, 'loot-card');
  const fired = emu.v('shots_fired', 'u16');
  emu.tap('A').idle(2);
  assert.equal(emu.v('shots_fired', 'u16'), fired, 'A took it, no shot');
  assert.equal(emu.v('guns_taken', 'u16'), 1);
  assert.notEqual(gunType(emu, 1), 0xff, 'in the second slot');
  assert.equal(emu.read8(A(emu, 'profile') + GUN_SEL), 1, 'and held');
  // Kept by the file, the chest still open on a continue.
  const again = boot();
  again.setSram(emu.sram());
  settle(settle(again, TITLE).tap('START').tap('A'), PLAY);
  assert.equal(gunType(again, 1), gunType(emu, 1));
  assert.equal(again.v('chest_open_bits') & 1, 1, 'the chest stays open');
});

test('a full loadout swaps: the held gun drops where the new one lay', () => {
  const emu = atCampChest();
  // Fill the loadout with pistols.
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, [0, 0, 1, 0, 0, 0, 0, 0, 0]);
  const x = emu.read16(A(emu, 'chest_x')), y = emu.read16(A(emu, 'chest_y'));
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y + 8);
  emu.idle(2);
  const kinds = Array.from(emu.readBytes(A(emu, 'drop_kind'), 6));
  const g = kinds.indexOf(DROP_GUN);
  const r = emu.read16(A(emu, 'drop_value') + g * 2);
  const newType = emu.read8(A(emu, 'floor_gun') + r * GUN_SIZE);
  w16(emu, 'player_x', words(emu, 'drop_x', 6)[g]);
  w16(emu, 'player_y', words(emu, 'drop_y', 6)[g] - 4);
  emu.idle(3).tap('A').idle(2);
  assert.equal(gunType(emu, 0), newType, 'the held slot has the new gun');
  assert.equal(emu.read8(A(emu, 'drop_kind') + g), DROP_GUN, 'still a gun on the floor');
  assert.equal(emu.read8(A(emu, 'floor_gun') + r * GUN_SIZE), 0, 'the old pistol');
});

test('the lab has no loot: kills drop nothing there', () => {
  const emu = settle(bootToTitle().tap('SELECT'), PLAY);
  assert.equal(emu.v('pickups_on'), 0);
  assert.equal(emu.v('card_drop'), CARD_NONE);
});
