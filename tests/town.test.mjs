// The town (the hub): a file starts there; its lift goes down into the descent,
// an area's start map has one back up; a run that ends returns you there; Echo's
// first contact plays once per file.
import { test } from 'node:test';
import { assert, bootToTitle, settle, startDescent, takeLiftDown, PLAY, RUN_OVER, MODE_TOWN, MODE_DESCENT } from './helpers.mjs';

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const EXP_ACTIVE = 59, STORY = 88, CREDITS = 7;  // Profile offsets (save.h)
const prof = (emu, at, len = 1) => emu.readBytes(A(emu, 'profile') + at, len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);

const newFileInTown = () => settle(bootToTitle().tap('START').tap('A').tap('A'), PLAY);
/** Wait out a play -> play change (a lift). */
function arrive(emu, mode) {
  emu.until((e) => e.v('play_mode') === mode, 60, 'the mode');
  emu.until((e) => e.v('in_transition'), 60, 'the scene change');
  return settle(emu, PLAY);
}

test('a new file starts in the town; its lift goes down into a new expedition', () => {
  const emu = newFileInTown();
  assert.equal(emu.v('play_mode'), MODE_TOWN);
  takeLiftDown(emu);
  assert.equal(emu.v('play_mode'), MODE_DESCENT);
  assert.equal(emu.v('level_map'), emu.v('area_start'));
  assert.equal(prof(emu, EXP_ACTIVE), 1);
});

test("the start map's lift goes back up with everything: no fee, the expedition over", () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'profile') + CREDITS, [200, 0, 0, 0]);
  // The lift stands in the start map's middle; step beside it and press A.
  const w = emu.v('room_w'), h = emu.v('room_h');
  w16(emu, 'player_x', (w >> 1) * 16 - 16);
  w16(emu, 'player_y', (h >> 1) * 16 - 2);
  emu.idle(2);
  assert.notEqual(emu.v('talk_near'), 0xff, 'the lift is in reach');
  emu.tap('A');
  arrive(emu, MODE_TOWN);
  assert.equal(prof(emu, CREDITS, 4), 200, 'no fee');
  assert.equal(prof(emu, EXP_ACTIVE), 0);
});

test('a run that ends goes back to the town (after the fee)', () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 250]);  // hurt for 250
  settle(emu.idle(2), RUN_OVER).idle(40).tap('START');
  arrive(emu, MODE_TOWN);
  assert.equal(emu.v('save_slot'), 0, 'still the file');
});

test("Echo's first contact at the relay plays once per file", () => {
  const emu = newFileInTown();
  w16(emu, 'player_x', 8 * 16);
  w16(emu, 'player_y', 4 * 16);
  emu.idle(2);
  assert.equal(emu.v('dlg_active'), 1, 'she speaks');
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.equal(prof(emu, STORY) & 1, 1, 'remembered');
  // Down and back up: the town again, and she doesn't repeat herself.
  takeLiftDown(emu);
  const w = emu.v('room_w'), h = emu.v('room_h');
  w16(emu, 'player_x', (w >> 1) * 16 - 16);
  w16(emu, 'player_y', (h >> 1) * 16 - 2);
  emu.idle(2).tap('A');
  arrive(emu, MODE_TOWN);
  w16(emu, 'player_x', 8 * 16);
  w16(emu, 'player_y', 4 * 16);
  emu.idle(4);
  assert.equal(emu.v('dlg_active'), 0);
});

const PAGE_SHOP = 5, GUNS = 18, GUN_SIZE = 9, GUN_NONE = 0xff, PACK_SIZE = 89, PACK = 90;
const PISTOL = [0, 0, 1, 0, 0, 0, 0, 0, 0];
/** In town with credits, talking to Marro: his shop. */
function atShop(credits) {
  const emu = newFileInTown();
  emu.writeBytes(A(emu, 'zones_done'), [0xff]);
  emu.writeBytes(A(emu, 'profile') + CREDITS, [credits & 255, (credits >> 8) & 255, 0, 0]);
  w16(emu, 'player_x', 17 * 16);
  w16(emu, 'player_y', 10 * 16 - 4);
  emu.idle(4).tap('A').idle(4);
  return emu;
}
// Marro's price (loot.c gun_price): (15 + 5 level) doubled a rarity step.
const priceOf = (emu, i) => {
  const at = A(emu, 'shop_stock') + i * GUN_SIZE;
  const rarity = emu.read8(at + 1), level = emu.read8(at + 2);
  return (15 + 5 * level) << rarity;
};

test("Marro's shop: three guns at your level; A buys one into a free slot", () => {
  const emu = atShop(5000);
  assert.equal(emu.v('paused'), 1);
  assert.equal(emu.v('menu_page'), PAGE_SHOP);
  emu.shot('shop');
  const price = priceOf(emu, 0);
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'paid');
  assert.notEqual(emu.read8(A(emu, 'profile') + GUNS + GUN_SIZE), GUN_NONE, 'in the second slot');
  assert.equal(emu.v('shop_sold') & 1, 1, 'sold');
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'sold once');
  emu.tap('B').idle(2);
  assert.equal(emu.v('paused'), 0, 'B leaves');
});

test('not enough credits: nothing changes', () => {
  const emu = atShop(0);
  emu.tap('A').idle(2);
  assert.equal(emu.v('shop_sold'), 0);
  assert.equal(emu.v('guns_bought', 'u16'), 0);
});

test('a full loadout and backpack: buying trades in the held gun for a quarter of its price', () => {
  const emu = atShop(5000);
  // Three more pistols, the first one held; the backpack full of them too.
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, PISTOL);
  for (let s = 0; s < 4; s++) emu.writeBytes(A(emu, 'profile') + PACK + s * GUN_SIZE, PISTOL);
  const price = priceOf(emu, 0), held = (15 + 5 * 1) << 0;
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price + (held >> 2));
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), emu.read8(A(emu, 'shop_stock')), 'the held slot has it');
});

test('a full loadout: a bought gun goes into the backpack', () => {
  const emu = atShop(5000);
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, PISTOL);
  const price = priceOf(emu, 0), type = emu.read8(A(emu, 'shop_stock'));
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'no trade-in');
  assert.equal(emu.read8(A(emu, 'profile') + PACK), type, 'in the backpack');
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), 0, 'the pistol still held');
});

test('a bigger backpack from Marro: 4 slots, 8 for 300, 12 for 800, then no more', () => {
  const emu = atShop(1100);
  assert.equal(prof(emu, PACK_SIZE), 4, 'a new file starts with 4');
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);  // past the three guns
  assert.equal(emu.v('shop_sel'), 3, 'the upgrade row');
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 8);
  assert.equal(prof(emu, CREDITS, 4), 800);
  emu.shot('shop-upgrade');
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 12);
  assert.equal(prof(emu, CREDITS, 4), 0);
  for (let i = 0; i < 4; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 12, 'as big as it gets: the row is gone');
});

test("selling: RIGHT turns to the backpack's guns; A sells one for a quarter of its price", () => {
  const emu = atShop(0);
  emu.writeBytes(A(emu, 'profile') + PACK + GUN_SIZE, [1, 1, 3, 0, 0, 0, 0, 0, 0]);  // a green level-3 shotgun, second slot
  emu.tap('RIGHT').idle(2);
  assert.equal(emu.v('shop_mode'), 1);
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 0, 'an empty slot: nothing');
  emu.tap('DOWN').idle(2);
  emu.shot('shop-sell');
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), ((15 + 5 * 3) << 1) >> 2);
  assert.equal(emu.read8(A(emu, 'profile') + PACK + GUN_SIZE), GUN_NONE, 'gone');
  emu.tap('LEFT').idle(2);
  assert.equal(emu.v('shop_mode'), 0, 'back to buying');
});

test('a bigger backpack carries half again as much ammo on the next expedition', () => {
  const emu = atShop(300);
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4).tap('B').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 8);
  takeLiftDown(emu);
  assert.equal(prof(emu, 198), 108, 'pistol rounds: 72 and half again');
});
