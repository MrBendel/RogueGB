// The pause menu (menu.c): character, gear, skills and options pages (tuning only in
// the lab), options saved on the cartridge, and save-and-quit for a file.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, startRun, startDescent, TITLE, PLAY } from './helpers.mjs';

const PAGE_CHARACTER = 0, PAGE_GEAR = 1, PAGE_SKILLS = 2, PAGE_OPTIONS = 3, PAGE_TUNE = 4;
const A = (emu, n) => emu.symbols.addr(n);
const setting = (emu, i) => emu.read8(A(emu, 'settings') + i);  // text_speed, shake, sound
/** B n times (a page takes a few frames to draw: a tap during it is missed). */
const turn = (emu, n) => { for (let i = 0; i < n; i++) emu.tap('B').idle(4); return emu; };

test('a file pauses on the character page; B turns to gear, skills, options, and back (no tuning)', () => {
  const emu = startDescent().idle(2).tap('START').idle(2);
  assert.equal(emu.v('menu_page'), PAGE_CHARACTER);
  matchGolden(emu, 'pause-character');
  const pages = [];
  for (let i = 0; i < 4; i++) pages.push(emu.tap('B').idle(2).v('menu_page'));
  assert.deepEqual(pages, [PAGE_GEAR, PAGE_SKILLS, PAGE_OPTIONS, PAGE_CHARACTER]);
});

test('the lab pauses on tuning, and has all five pages', () => {
  const emu = startRun().idle(2).tap('START').idle(2);
  assert.equal(emu.v('menu_page'), PAGE_TUNE);
  const pages = [];
  for (let i = 0; i < 5; i++) pages.push(emu.tap('B').idle(2).v('menu_page'));
  assert.deepEqual(pages, [PAGE_CHARACTER, PAGE_GEAR, PAGE_SKILLS, PAGE_OPTIONS, PAGE_TUNE]);
});

test('options change with LEFT/RIGHT and are kept on the cartridge', () => {
  const emu = turn(startDescent().idle(2).tap('START').idle(4), 3);
  assert.equal(emu.v('menu_page'), PAGE_OPTIONS);
  emu.tap('RIGHT');                 // text: fast
  emu.tap('DOWN').tap('RIGHT');     // shake: off
  emu.tap('DOWN').tap('LEFT');      // sound: low
  assert.deepEqual([0, 1, 2].map((i) => setting(emu, i)), [2, 0, 1]);
  matchGolden(emu.idle(2), 'pause-options');
  const again = boot();
  again.setSram(emu.sram());
  settle(again, TITLE);
  assert.deepEqual([0, 1, 2].map((i) => setting(again, i)), [2, 0, 1], 'loaded at the title');
});

// (The lab pauses on tuning: options are four pages on.)
const labOptions = () => turn(startRun().idle(2).tap('START').idle(4), 4);

test('text speed: instant puts a whole page out at once', () => {
  const emu = labOptions();
  assert.equal(emu.v('menu_page'), PAGE_OPTIONS);
  emu.tap('LEFT');  // normal -> slow
  emu.tap('LEFT');  // slow -> instant (it wraps)
  assert.equal(setting(emu, 0), 3);
  emu.tap('START').idle(2);
  emu.writeBytes(A(emu, 'debug_cmd'), [6, 0]);  // a script
  emu.idle(3);  // (the box opens between frames)
  assert.equal(emu.v('dlg_pos'), emu.v('dlg_len'), 'the page is out');
});

test('screen shake off: hits and kills leave the view still', () => {
  const emu = labOptions();
  emu.tap('DOWN').tap('RIGHT');  // shake: off
  emu.tap('START').idle(2);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 5, 0, 0, 0, 0]);  // hurt for 5
  emu.idle(1);
  assert.equal(emu.v('cam_shake'), 0);
});

test('save and quit: SELECT in the pause menu goes to the title; the file continues where it was', () => {
  const emu = startDescent().idle(2);
  const map = emu.v('level_map'), seed = emu.v('area_seed_base', 'u16');
  emu.tap('START').idle(2).tap('SELECT');
  settle(emu, TITLE, 'the title');
  const again = settle(emu.tap('START').tap('A'), PLAY);
  assert.equal(again.v('level_map'), map);
  assert.equal(again.v('area_seed_base', 'u16'), seed);
  assert.equal(again.v('run_seconds', 'u16') >= 0, true);
});

// The gear page: the loadout's 4 rows, then the backpack's.
const GUNS = 18, GUN_SIZE = 9, GUN_SEL = 54, PACK = 90;
const gearPage = () => startDescent().idle(2).tap('START').tap('B').idle(2);

test('gear: A picks a gun up, A on a backpack slot swaps them; the held gun changes with it', () => {
  const emu = startDescent().idle(2);
  emu.writeBytes(A(emu, 'profile') + PACK + GUN_SIZE, [1, 2, 4, 0, 0, 0, 0, 0, 0]);  // a blue shotgun, backpack slot 2
  emu.tap('START').tap('B').idle(2);
  assert.equal(emu.v('menu_page'), PAGE_GEAR);
  emu.tap('A').idle(2);
  assert.equal(emu.v('gear_pick'), 0, 'the pistol picked up');
  for (let i = 0; i < 5; i++) emu.tap('DOWN');
  emu.idle(2);
  matchGolden(emu, 'pause-gear');
  emu.tap('A').idle(2);
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), 1, 'the shotgun in the loadout');
  assert.equal(emu.read8(A(emu, 'profile') + PACK + GUN_SIZE), 0, 'the pistol in the backpack');
  assert.equal(emu.v('gear_pick'), 0xff);
  assert.equal(emu.v('weapon_cur'), 1, 'held: the shotgun');
});

test("gear: B puts a picked-up gun back (doesn't turn the page); the last gun can't leave the loadout", () => {
  const emu = gearPage();
  emu.tap('A').idle(2).tap('B').idle(2);
  assert.equal(emu.v('gear_pick'), 0xff);
  assert.equal(emu.v('menu_page'), PAGE_GEAR, 'still the gear page');
  emu.tap('A');
  for (let i = 0; i < 4; i++) emu.tap('DOWN');
  emu.tap('A').idle(2);  // onto an empty backpack slot
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), 0, 'still in the loadout');
  assert.equal(emu.read8(A(emu, 'profile') + PACK), 0xff);
});

test('gear: the backpack list scrolls', () => {
  const emu = startDescent().idle(2);
  emu.writeBytes(A(emu, 'profile') + 89, [12]);
  for (let s = 0; s < 12; s++) emu.writeBytes(A(emu, 'profile') + PACK + s * GUN_SIZE, s === 11 ? [2, 4, 9, 0, 0, 0, 0, 0, 0] : [0xff]);
  emu.tap('START').tap('B').idle(2);
  for (let i = 0; i < 15; i++) emu.tap('DOWN');
  emu.idle(2);
  assert.equal(emu.v('gear_sel'), 15);
  assert.equal(emu.v('gear_top'), 8);
  matchGolden(emu, 'pause-gear-scrolled');
});

test('the menu keeps the palettes it borrows in far RAM (work RAM bank 2) and puts them back', () => {
  const emu = startRun().idle(30);
  const pal = () => Array.from(emu.readBytes(emu.symbols.addr('pal_base_bg'), 48));
  const before = pal();
  const near = () => Array.from(emu.readBytes(0xd000, 48));  // bank 1's: our own globals
  const nearBefore = near();
  emu.tap('START').idle(20);
  assert.equal(emu.v('paused'), 1);
  assert.deepEqual(near(), nearBefore, "bank 1's 0xD000 untouched: the copy went to bank 2");
  assert.notDeepEqual(pal(), before, "the menu's inks while it's open");
  assert.equal(emu.v('far_n'), 48, 'copied out through far RAM');
  emu.tap('START').idle(20);
  assert.equal(emu.v('paused'), 0);
  assert.deepEqual(pal(), before, 'the palettes back as they were');
  // Bank 1 is back in the window (SVBK): the room map still reads as before.
  assert.equal(emu.read8(0xff70) & 7, 1, 'SVBK: bank 1');
});
