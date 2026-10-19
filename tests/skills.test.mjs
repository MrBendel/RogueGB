// Classes and skill trees (skills.c, menu.c): a new file picks a class; the pause
// menu's skills page spends skill points; learned skills change the guns and health.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, TITLE, PLAY } from './helpers.mjs';

const P = { cls: 0, level: 1, skill_points: 4, skills: 5 };
const W = { mag: 15, reload: 16 };  // WeaponStats offsets (weapon.h)
const A = (emu, n) => emu.symbols.addr(n);
const prof = (emu, f, len = 1) => emu.readBytes(A(emu, 'profile') + P[f], len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const setProf = (emu, f, v) => emu.write8(A(emu, 'profile') + P[f], v);

/** A new file of class `cls` (the picker: RIGHT steps through them), playing. */
function newFile(cls = 0) {
  const emu = bootToTitle().tap('START').tap('A');
  assert.equal(emu.v('title_files'), 2, 'an empty file asks for a class');
  for (let i = 0; i < cls; i++) emu.tap('RIGHT');
  return settle(emu.tap('A'), PLAY).idle(4);
}
/** Pause, on the skills page (B turns the pause menu's pages). */
const skillsPage = (emu) => emu.tap('START').idle(2).tap('B').idle(4).tap('B').idle(4);  // (past the gear page)

test('a new file asks for a class; it is saved with the file', () => {
  const emu = newFile(2);
  assert.equal(prof(emu, 'cls'), 2, 'the Ballistician');
  const again = boot();
  again.setSram(emu.sram());
  settle(settle(again, TITLE).tap('START').tap('A'), PLAY);
  assert.equal(prof(again, 'cls'), 2);
});

test('the class picker', () => {
  matchGolden(bootToTitle().tap('START').tap('A').idle(2), 'class-picker');
});

test('a skill point buys a first-tier skill; the next one in its branch opens; tier 3 stays locked', () => {
  const emu = newFile();
  setProf(emu, 'skill_points', 2);
  skillsPage(emu);
  assert.equal(emu.v('menu_page'), 2);
  emu.tap('A');  // Plated Mesh (branch 0, tier 0)
  assert.equal(prof(emu, 'skills', 2), 1);
  assert.equal(prof(emu, 'skill_points'), 1);
  emu.tap('DOWN').tap('DOWN').tap('A');  // tier 2: needs tier 1
  assert.equal(prof(emu, 'skills', 2), 1, 'locked');
  emu.tap('UP').tap('A');  // tier 1, now open
  assert.equal(prof(emu, 'skills', 2), 3);
  assert.equal(prof(emu, 'skill_points'), 0);
  emu.tap('DOWN').tap('A');
  assert.equal(prof(emu, 'skills', 2), 3, 'no points left');
});

test('skills change the game: Plated Mesh +15 health, Expanded Drums +2 rounds, Hair-Pin Draw halves reloads', () => {
  const emu = newFile();  // the Ironclad
  const hp = emu.v('player_hp_max'), mag = emu.read8(A(emu, 'wpn') + W.mag), reload = emu.read8(A(emu, 'wpn') + W.reload);
  setProf(emu, 'skill_points', 3);
  skillsPage(emu);
  emu.tap('A');                    // Plated Mesh
  emu.tap('RIGHT').tap('A');       // Hair-Pin Draw
  emu.tap('RIGHT').tap('A');       // Expanded Drums
  assert.equal(emu.v('player_hp_max'), hp + 15);
  assert.equal(emu.read8(A(emu, 'wpn') + W.mag), mag + 2);
  assert.equal(emu.read8(A(emu, 'wpn') + W.reload), reload >> 1);
  matchGolden(emu.idle(2), 'skills-page');
  emu.tap('START').idle(4);
  assert.equal(emu.v('paused'), 0);
  // Switching guns keeps the skills on top (and the stored stats stay the gun's own).
  emu.tap('SELECT').idle(2).tap('SELECT').idle(2).tap('SELECT').idle(2).tap('SELECT').idle(2).tap('SELECT').idle(2);
  assert.equal(emu.read8(A(emu, 'wpn') + W.mag), mag + 2, 'back on the pistol, still +2');
});

test('the profile page', () => {
  const emu = newFile();
  setProf(emu, 'skill_points', 1);
  matchGolden(emu.tap('START').idle(4), 'profile-page');
});
