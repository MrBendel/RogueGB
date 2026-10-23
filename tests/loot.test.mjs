// Loot (loot.c, pickups.c): chests in the descent's camps open once the camp is
// cleared and drop credits and a gun; credits are taken by walking over them; a
// gun shows a card and A takes it into the loadout, which the file keeps.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, startDescent, TITLE, PLAY } from './helpers.mjs';

const SIDE = { 1: 0, 2: 1, 4: 2, 8: 3 };
const DIR = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' };
const GUN_SIZE = 9, GUNS = 18, GUN_SEL = 54, CREDITS = 7, PACK_SIZE = 89, PACK = 90;  // Profile offsets (save.h)
const DROP_CREDITS = 1, DROP_GUN = 2, CARD_NONE = 0xff;

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const words = (emu, n, len) => Array.from({ length: len }, (_, i) => emu.read16(A(emu, n) + i * 2));
const credits = (emu) => emu.readBytes(A(emu, 'profile') + CREDITS, 4).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const gunType = (emu, s) => emu.read8(A(emu, 'profile') + GUNS + s * GUN_SIZE);
const PISTOL = [0, 0, 1, 0, 0, 0, 0, 0, 0];
/** Pistols in every free loadout slot (pack too: the whole backpack). */
function fillUp(emu, pack = true) {
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, PISTOL);
  if (!pack) return;
  const n = emu.read8(A(emu, 'profile') + PACK_SIZE);
  for (let s = 0; s < n; s++) emu.writeBytes(A(emu, 'profile') + PACK + s * GUN_SIZE, PISTOL);
}

function leaveBy(emu, e) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const inset = emu.v('level_map') === emu.v('area_start') ? 0 : 1;  // (an entrance room's doorways are one block)
  const o = (emu.readBytes(A(emu, 'level_open'), 4)[SIDE[e]] + inset) * 16;
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
  fillUp(emu);
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

test('swapping on a gun never flashes the window over the play area (it switches between frames)', () => {
  const emu = atCampChest();
  fillUp(emu);
  const x = emu.read16(A(emu, 'chest_x')), y = emu.read16(A(emu, 'chest_y'));
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y + 8);
  emu.idle(2);
  const g = Array.from(emu.readBytes(A(emu, 'drop_kind'), 6)).indexOf(DROP_GUN);
  w16(emu, 'player_x', words(emu, 'drop_x', 6)[g]);
  w16(emu, 'player_y', words(emu, 'drop_y', 6)[g] - 4);
  emu.idle(4);
  // A solid band of one color across screen lines 100-125 is the window drawn over
  // the play area (the floor's dots and blocks always vary).
  const solid = () => {
    const fb = emu.frameBuffer();
    const colors = new Set();
    for (let y = 100; y < 126; y++) for (let x = 0; x < 160; x += 3) colors.add(fb.slice((y * 160 + x) * 3, (y * 160 + x) * 3 + 3).join());
    return colors.size === 1;
  };
  for (let n = 0; n < 6; n++) {
    emu.hold('A', 1);
    for (let f = 0; f < 8; f++) {
      assert.ok(!solid(), `swap ${n}, frame ${f}: the window over the play area`);
      emu.idle(1);
    }
  }
  assert.ok(emu.v('guns_taken', 'u16') >= 3, 'swapped');
  assert.notEqual(emu.v('card_drop'), CARD_NONE, 'the card stays up (the old gun lies here)');
});

test('a full loadout, room in the backpack: A stows the gun there and leaves the hands alone', () => {
  const emu = atCampChest();
  fillUp(emu, false);
  const x = emu.read16(A(emu, 'chest_x')), y = emu.read16(A(emu, 'chest_y'));
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y + 8);
  emu.idle(2);
  const g = Array.from(emu.readBytes(A(emu, 'drop_kind'), 6)).indexOf(DROP_GUN);
  const r = emu.read16(A(emu, 'drop_value') + g * 2);
  const newType = emu.read8(A(emu, 'floor_gun') + r * GUN_SIZE);
  w16(emu, 'player_x', words(emu, 'drop_x', 6)[g]);
  w16(emu, 'player_y', words(emu, 'drop_y', 6)[g] - 4);
  emu.idle(3).tap('A').idle(2);
  assert.equal(emu.read8(A(emu, 'profile') + PACK), newType, 'in the backpack');
  assert.equal(gunType(emu, 0), 0, 'still the pistol in hand');
  assert.equal(emu.read8(A(emu, 'drop_kind') + g), 0, 'nothing left on the floor');
  assert.equal(emu.v('card_drop'), CARD_NONE, 'the card goes');
});

// --- ammo: a reserve per gun type on an expedition (loot.h) ---
const AMMO = 198, WEAPON_SWORD = 4, DROP_AMMO = 4;
const reserve = (emu, t) => emu.read8(A(emu, 'profile') + AMMO + t);
const mag = (emu) => emu.read8(A(emu, 'weapon_ammo') + emu.v('weapon_cur'));

test('an expedition starts with full reserves; a reload draws from them, and only what is there', () => {
  const emu = startDescent().idle(4);
  assert.deepEqual([0, 1, 2, 3].map((t) => reserve(emu, t)), [72, 24, 120, 12]);
  emu.writeBytes(A(emu, 'profile') + AMMO, [5]);
  emu.writeBytes(A(emu, 'weapon_ammo') + emu.v('weapon_cur'), [1]);
  emu.tap('A').idle(120);  // the last round, then the reload
  assert.equal(mag(emu), 5, 'the reserve went into the magazine');
  assert.equal(reserve(emu, 0), 0);
});

test('every gun dry: A says so and takes up the sword; picking up rounds puts the gun back in hand', () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'profile') + AMMO, [0]);
  emu.writeBytes(A(emu, 'weapon_ammo') + emu.v('weapon_cur'), [0]);
  emu.tap('A').idle(2);
  assert.equal(emu.v('melee_fallback'), 1);
  assert.equal(emu.v('weapon_cur'), WEAPON_SWORD);
  // Pistol rounds at the player's feet.
  emu.writeBytes(A(emu, 'drop_kind'), [DROP_AMMO]);
  emu.writeBytes(A(emu, 'drop_value'), [0, 0]);
  emu.writeBytes(A(emu, 'drop_x'), [emu.read8(A(emu, 'player_x')), emu.read8(A(emu, 'player_x') + 1)]);
  emu.writeBytes(A(emu, 'drop_y'), [emu.read8(A(emu, 'player_y')), emu.read8(A(emu, 'player_y') + 1)]);
  emu.writeBytes(A(emu, 'pickups_on'), [1]);
  emu.idle(3);
  assert.equal(emu.read8(A(emu, 'drop_kind')), 0, 'taken');
  assert.equal(emu.v('melee_fallback'), 0);
  assert.equal(emu.v('weapon_cur'), 0, 'the pistol again');
  emu.idle(120);
  assert.equal(mag(emu), 12, 'reloaded from the pickup');
  assert.equal(reserve(emu, 0), 0);
});

test('a dry gun with another loaded: A switches to that one, not the sword', () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'profile') + GUNS + GUN_SIZE, [1, 0, 1, 0, 0, 0, 0, 0, 0]);  // a shotgun, slot 2
  emu.writeBytes(A(emu, 'slot_ammo') + 1, [255]);
  emu.writeBytes(A(emu, 'profile') + AMMO, [0]);
  emu.writeBytes(A(emu, 'weapon_ammo') + emu.v('weapon_cur'), [0]);
  emu.tap('A').idle(2);
  assert.equal(emu.v('melee_fallback'), 0);
  assert.equal(emu.v('weapon_cur'), 1, 'the shotgun');
});

test("the lab's ammo isn't limited", () => {
  const emu = settle(bootToTitle().tap('SELECT'), PLAY).idle(4);
  const before = emu.v('reloads', 'u16');
  emu.writeBytes(A(emu, 'weapon_ammo') + emu.v('weapon_cur'), [1]);
  emu.tap('A').idle(120);
  assert.ok(emu.v('reloads', 'u16') > before);
  assert.ok(mag(emu) > 1, 'a full magazine');
});
