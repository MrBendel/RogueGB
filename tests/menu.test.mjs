// The pause menu (menu.c): character, skills and options pages (tuning only in
// the lab), options saved on the cartridge, and save-and-quit for a file.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, startRun, startDescent, TITLE, PLAY } from './helpers.mjs';

const PAGE_CHARACTER = 0, PAGE_SKILLS = 1, PAGE_OPTIONS = 2, PAGE_TUNE = 3;
const A = (emu, n) => emu.symbols.addr(n);
const setting = (emu, i) => emu.read8(A(emu, 'settings') + i);  // text_speed, shake, sound

test('a file pauses on the character page; B turns to skills, options, and back (no tuning)', () => {
  const emu = startDescent().idle(2).tap('START').idle(2);
  assert.equal(emu.v('menu_page'), PAGE_CHARACTER);
  matchGolden(emu, 'pause-character');
  const pages = [];
  for (let i = 0; i < 3; i++) pages.push(emu.tap('B').idle(2).v('menu_page'));
  assert.deepEqual(pages, [PAGE_SKILLS, PAGE_OPTIONS, PAGE_CHARACTER]);
});

test('the lab pauses on tuning, and has all four pages', () => {
  const emu = startRun().idle(2).tap('START').idle(2);
  assert.equal(emu.v('menu_page'), PAGE_TUNE);
  const pages = [];
  for (let i = 0; i < 4; i++) pages.push(emu.tap('B').idle(2).v('menu_page'));
  assert.deepEqual(pages, [PAGE_CHARACTER, PAGE_SKILLS, PAGE_OPTIONS, PAGE_TUNE]);
});

test('options change with LEFT/RIGHT and are kept on the cartridge', () => {
  const emu = startDescent().idle(2).tap('START').tap('B').tap('B').idle(2);
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

// (The lab pauses on tuning: options are three pages on.)
const labOptions = () => startRun().idle(2).tap('START').tap('B').tap('B').tap('B').idle(2);

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
