// Classes and skill trees (skills.c, menu.c): a new file picks a class; the pause
// menu's skills page spends skill points; learned skills change the guns and health.
import { test } from 'node:test';
import { assert, matchGolden, boot, bootToTitle, settle, TITLE, PLAY } from './helpers.mjs';

const P = { cls: 0, level: 1, skill_points: 4, skills: 5 };
const W = { mag: 15, reload: 16 };  // WeaponStats offsets (weapon.h)
const A = (emu, n) => emu.symbols.addr(n);
const prof = (emu, f, len = 1) => emu.readBytes(A(emu, 'profile') + P[f], len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const setProf = (emu, f, v) => emu.write8(A(emu, 'profile') + P[f], v);

/**
 * A new file of class `cls`, playing. The picker offers the classes someone plays
 * (1 Volt-Weaver, 2 Ballistician, 3 Ghost-Stalker; RIGHT steps through them).
 */
function newFile(cls = 1) {
  const emu = bootToTitle().tap('START').tap('A');
  assert.equal(emu.v('title_files'), 2, 'an empty file asks for a class');
  assert.equal(emu.v('title_class'), 1, 'the first class with a character');
  for (let i = 1; i < cls; i++) emu.tap('RIGHT');
  return settle(emu.tap('A'), PLAY).idle(4);
}
/** A new file made an Ironclad (no character plays it, so the picker skips it; its skills are the tested ones). */
function ironclad() {
  const emu = newFile();
  setProf(emu, 'cls', 0);
  return emu;
}
/** Pause, on the skills page (LEFT/RIGHT turn the pause menu's pages). */
const skillsPage = (emu) => emu.tap('START').idle(2).tap('RIGHT').idle(4).tap('RIGHT').idle(4);  // (in town: past the gear page)

test('a new file asks for a class; it is saved with the file', () => {
  const emu = newFile(2);
  assert.equal(prof(emu, 'cls'), 2, 'the Ballistician');
  const again = boot();
  again.setSram(emu.sram());
  settle(settle(again, TITLE).tap('START').tap('A'), PLAY);
  assert.equal(prof(again, 'cls'), 2);
});

test("each file plays as its class's character, and the files say who", () => {
  const CHAR_HUNTER = 2, CHAR_HOODED = 3, CHAR_CYBORG = 4;
  for (const [cls, who] of [[1, CHAR_HOODED], [2, CHAR_CYBORG], [3, CHAR_HUNTER]]) {
    const emu = newFile(cls);
    assert.equal(emu.v('player_character'), who);
    const again = boot();
    again.setSram(emu.sram());
    settle(settle(again, TITLE).tap('START').tap('A'), PLAY);
    assert.equal(again.v('player_character'), who, 'opening the file');
  }
  const emu = newFile(3);
  const files = boot();
  files.setSram(emu.sram());
  matchGolden(settle(files, TITLE).tap('START').idle(2), 'files-with-characters');
});

test('SELECT on a file asks to erase it: B keeps it, A erases it', () => {
  const emu = newFile(3);
  const t = boot();
  t.setSram(emu.sram());
  settle(t, TITLE).tap('START').idle(2);
  assert.equal(t.read8(A(t, 'slot_used')), 1);
  t.tap('SELECT').idle(2);
  assert.equal(t.v('title_files'), 3, 'asking');
  matchGolden(t, 'erase-file-prompt');
  t.tap('B').idle(2);
  assert.equal(t.v('title_files'), 1);
  assert.equal(t.read8(A(t, 'slot_used')), 1, 'kept');
  t.tap('SELECT').idle(2).tap('A').idle(2);
  assert.equal(t.v('title_files'), 1);
  assert.equal(t.read8(A(t, 'slot_used')), 0, 'erased');
  t.tap('SELECT').idle(2);
  assert.equal(t.v('title_files'), 1, 'an empty file has nothing to erase');
  t.tap('A');
  assert.equal(t.v('title_files'), 2, 'and starts a new one');
});

test('the class picker', () => {
  matchGolden(bootToTitle().tap('START').tap('A').idle(2), 'class-picker');
});

test('a skill point buys a first-tier skill; the next one in its branch opens; tier 3 stays locked', () => {
  const emu = ironclad();
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
  const emu = ironclad();
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
  const emu = ironclad();
  setProf(emu, 'skill_points', 1);
  matchGolden(emu.tap('START').idle(4), 'profile-page');
});
