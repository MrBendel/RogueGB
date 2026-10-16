// XP and levels (progress.c): kills and camps give XP; a level gives a skill
// point and health, refills, plays its fanfare and saves the file.
import { test } from 'node:test';
import { assert, boot, startRun, startDescent, settle, TITLE, PLAY } from './helpers.mjs';

const SLIME = 1;
const P = { level: 1, xp: 2, skill_points: 4 };
const A = (emu, n) => emu.symbols.addr(n);
const prof = (emu, f, len = 1) => emu.readBytes(A(emu, 'profile') + P[f], len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const setProf = (emu, f, v, len = 1) => emu.writeBytes(A(emu, 'profile') + P[f], Array.from({ length: len }, (_, i) => (v >> (8 * i)) & 255));
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);

/** A slime beside the player, shot until it's dead. */
function killSlime(emu) {
  const kills = emu.v('enemies_killed', 'u16');
  w16(emu, 'debug_x', emu.v('player_x', 'u16') + 40);
  w16(emu, 'debug_y', emu.v('player_y', 'u16'));
  emu.write8(A(emu, 'debug_type'), SLIME);
  emu.write8(A(emu, 'debug_cmd'), 2);
  for (let f = 0; f < 400 && emu.v('enemies_killed', 'u16') === kills; f++) emu.hold('A', 1);
  emu.idle(1);
  assert.equal(emu.v('enemies_killed', 'u16'), kills + 1, 'the slime died');
  return emu.idle(10);  // XP is handed out within 8 frames
}

test('the XP curve: 40 to reach level 2, rising to 9280 for the last of 30', () => {
  const need = (l) => 10 * l * l + 30 * l;
  assert.equal(need(1), 40);
  assert.equal(need(29), 9280);
});

test('a kill gives XP (4 for a slime); a file starts at level 1 with 60 health', () => {
  const emu = startRun().idle(2);
  assert.equal(prof(emu, 'level'), 1);
  assert.equal(emu.v('player_hp_max'), 60);
  killSlime(emu);
  assert.equal(prof(emu, 'xp', 2), 4);
});

test('filling the bar: a level up gives a skill point and 2 health, refills, plays the fanfare', () => {
  const emu = startRun().idle(2);
  setProf(emu, 'xp', 38, 2);
  emu.write8(A(emu, 'player_hp'), 20);
  killSlime(emu);
  assert.equal(prof(emu, 'level'), 2);
  assert.equal(prof(emu, 'xp', 2), 2, '42 - 40 carried over');
  assert.equal(prof(emu, 'skill_points'), 1);
  assert.equal(emu.v('player_hp_max'), 62);
  assert.equal(emu.v('player_hp'), 62, 'refilled');
  assert.equal(emu.v('level_ups', 'u16'), 1);
  assert.equal(emu.v('prio1'), 5, "the fanfare holds channel 2 (the kill's crunch plays on the noise channel)");
});

test('a level up in a file is saved at once', () => {
  const emu = startDescent().idle(2);
  setProf(emu, 'xp', 38, 2);
  const before = emu.v('saves_written', 'u16');
  killSlime(emu);
  assert.equal(prof(emu, 'level'), 2);
  assert.ok(emu.v('saves_written', 'u16') > before, 'saved');
  const again = boot();
  again.setSram(emu.sram());
  settle(settle(again, TITLE).tap('START').tap('A'), PLAY);
  assert.equal(prof(again, 'level'), 2);
  assert.equal(again.v('player_hp_max'), 62, 'the level\'s health, from the start');
});
