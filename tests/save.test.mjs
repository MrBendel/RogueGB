// Save files (save.c): three slots in battery RAM, autosaved at map entrances,
// continued from the title, robust to a write cut short; the run's end settles
// the reconstruction fee.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, startDescent, TITLE, PLAY, RUN_OVER } from './helpers.mjs';

const OPENING_W = 4;
const SIDE = { 1: 0, 2: 1, 4: 2, 8: 3 };
const DIR = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' };
// Profile field offsets (save.h; SDCC packs structs).
const P = { level: 1, xp: 2, credits: 7, play_minutes: 11, best_depth: 14, guns: 18, gun_sel: 54, last_fee: 55, exp_active: 59, exp_seed: 60, exp_depth: 62, exp_map: 63 };

const A = (emu, n) => emu.symbols.addr(n);
const prof = (emu, field, len = 1) => {
  const b = emu.readBytes(A(emu, 'profile') + P[field], len);
  return b.reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
};
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);

/** Walk out through the first linked side of the current map into the next one. */
function nextMap(emu) {
  const links = emu.read8(A(emu, 'area_links') + emu.v('level_map'));
  const e = [1, 2, 4, 8].find((x) => links & x);
  const w = emu.v('room_w'), h = emu.v('room_h');
  const o = (emu.readBytes(A(emu, 'level_open'), 4)[SIDE[e]] + 1) * 16;
  const at = { 1: [o, 2 * 16 - 8], 4: [o, h * 16 - 3 * 16 + 4], 8: [2 * 16, o - 4], 2: [w * 16 - 3 * 16, o - 4] }[e];
  w16(emu, 'player_x', at[0]);
  w16(emu, 'player_y', at[1]);
  const before = emu.v('maps_entered', 'u16');
  for (let i = 0; i < 120 && emu.v('maps_entered', 'u16') === before; i++) emu.hold(DIR[e], 1);
  return settle(emu, PLAY);
}

/** A fresh emulator on these save files, at the title. */
const withSram = (sram) => {
  const emu = boot();
  emu.setSram(sram);
  return settle(emu, TITLE, 'title');
};

test('a new file: the files screen offers three, the first one is saved as soon as it starts', () => {
  const emu = bootToTitle().tap('START');
  assert.equal(emu.v('title_files'), 1);
  for (let s = 0; s < 3; s++) assert.equal(emu.read8(A(emu, 'slot_used') + s), 0, `slot ${s} is empty`);
  matchGolden(emu.idle(2), 'files-empty');
  settle(emu.tap('A').tap('A'), PLAY);  // the file, then its class
  assert.equal(emu.v('save_slot'), 0);
  assert.ok(emu.v('saves_written', 'u16') >= 1);
  const sram = emu.sram();
  assert.ok(sram[0] === 0x53 && sram[1] === 0x4b || sram[128] === 0x53 && sram[129] === 0x4b, 'the slot has a copy');
  assert.equal(prof(emu, 'level'), 1);
  assert.equal(emu.v('play_mode'), 6, 'a new character starts in the town');
  assert.equal(prof(emu, 'exp_active'), 0, 'no expedition yet');
});

test('continue: the expedition picks up at the map it was saved in, same seed and depth', () => {
  const emu = nextMap(startDescent());
  const map = emu.v('level_map'), seed = emu.v('area_seed_base', 'u16'), depth = emu.v('layer_depth');
  assert.equal(prof(emu, 'exp_map'), map, 'saved on entering the map');
  const again = withSram(emu.sram()).tap('START');
  assert.equal(again.read8(A(again, 'slot_used')), 1, 'the file is there');
  matchGolden(again.idle(2), 'files-one');
  settle(again.tap('A'), PLAY);
  assert.equal(again.v('level_map'), map);
  assert.equal(again.v('area_seed_base', 'u16'), seed);
  assert.equal(again.v('layer_depth'), depth);
});

test('a write cut short leaves the other copy: the file still loads', () => {
  const emu = nextMap(startDescent());
  const map = emu.v('level_map');
  const sram = emu.sram();
  // The newer copy (higher sequence number) loses its checksum: the older one, a map earlier, loads.
  const newer = (sram[128 + 3] - sram[3]) << 24 >> 24 > 0 ? 128 : 0;
  sram[newer + 8 + 5] ^= 0xff;
  const again = settle(withSram(sram).tap('START').tap('A'), PLAY);
  assert.equal(again.v('save_slot'), 0);
  assert.notEqual(again.v('level_map'), map, 'the older save: the map before');
  // Both copies broken: the slot reads as new.
  sram[(128 - newer) + 8 + 5] ^= 0xff;
  const fresh = withSram(sram).tap('START');  // (a new file there would ask for a class)
  assert.equal(fresh.read8(A(fresh, 'slot_used')), 0);
});

test('the end of a run: the corporation takes an eighth of your credits, the expedition is over, the file saved', () => {
  const emu = startDescent();
  emu.writeBytes(A(emu, 'profile') + P.credits, [800 & 255, 800 >> 8, 0, 0]);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 250]);  // hurt for 250: dead
  settle(emu.idle(2), RUN_OVER, 'run over');
  assert.equal(prof(emu, 'credits', 4), 700);
  assert.equal(prof(emu, 'last_fee', 4), 100);
  assert.equal(prof(emu, 'exp_active'), 0);
  const again = settle(withSram(emu.sram()).tap('START').tap('A'), PLAY);
  assert.equal(prof(again, 'credits', 4), 700, 'saved');
  assert.equal(again.v('layer_depth'), 1, 'a new expedition');
});

test('the lab and the yard save nothing', () => {
  const emu = settle(bootToTitle().tap('SELECT'), PLAY);
  assert.equal(emu.v('save_slot'), 0xff);
  assert.equal(emu.v('saves_written', 'u16'), 0);
});
