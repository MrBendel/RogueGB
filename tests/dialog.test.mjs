// The dialogue box (dialog.c) and its scripts (tools/dialogue.mjs).
import { test } from 'node:test';
import { assert, matchGolden, bootToTitle, startRun, settle, PLAY } from './helpers.mjs';
import { loadDialogue, wrap, DLG_WRAP_PX } from '../tools/dialogue.mjs';

const w16 = (emu, n, v) => emu.writeBytes(emu.symbols.addr(n), [v & 255, v >> 8]);
const yard = () => settle(bootToTitle().tap('B'), PLAY, 'the ice yard');
/** Into the yard, then up to Echo's relay: its zone plays her first contact. */
const yardTalk = () => {
  const emu = yard();
  assert.equal(emu.v('dlg_active'), 0, 'nobody talks at the start');
  w16(emu, 'player_x', 8 * 16);
  w16(emu, 'player_y', 4 * 16);
  emu.idle(2);
  return emu;
};
const closeAll = (emu) => {
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.equal(emu.v('dlg_active'), 0);
  return emu;
};
const ids = () => loadDialogue().scripts.map((s) => s.id);

test('every page fits the box: lines within the width, at most 2 a page', () => {
  for (const s of loadDialogue().scripts) {
    for (const p of s.pages) assert.ok(p.lines.length >= 1 && p.lines.length <= 2, `${s.id}: ${p.lines.length} lines`);
  }
  const long = wrap('The quick brown fox jumps over the lazy dog, and then it does it all over again, twice.');
  assert.ok(long.length >= 2, 'a long speech takes more than one page');
  assert.throws(() => wrap('naïve'), /no 'ï'/);
  assert.ok(DLG_WRAP_PX <= 144);
});

test("walking up to the relay starts Echo's first contact: typed out a letter every 2 frames, the world waiting", () => {
  const emu = yardTalk();
  assert.equal(emu.v('dlg_active'), 1);
  const x = emu.v('player_x', 'u16');
  const pos = emu.v('dlg_pos');
  emu.hold('RIGHT', 20);
  assert.equal(emu.v('player_x', 'u16'), x, 'the player stays put');
  const typed = emu.v('dlg_pos') - pos;
  assert.ok(typed >= 9 && typed <= 11, `${typed} letters in 20 frames`);
  matchGolden(emu, 'dialog-typing');
});

test('A finishes the page at once, then turns it; after the last page the box closes and play resumes', () => {
  const emu = yardTalk();
  emu.tap('A');
  assert.equal(emu.v('dlg_waiting'), 1, 'the whole page is out');
  assert.equal(emu.v('dlg_pos'), emu.v('dlg_len'));
  const page = emu.v('dlg_page_i');
  emu.tap('A');
  assert.equal(emu.v('dlg_page_i'), page + 1, 'the next page');
  assert.equal(emu.v('dlg_waiting'), 0);
  let taps = 0;
  while (emu.v('dlg_active') && taps < 40) { emu.tap('A'); taps++; }
  assert.equal(emu.v('dlg_active'), 0, 'closed');
  const x = emu.v('player_x', 'u16');
  emu.hold('RIGHT', 20);
  assert.ok(emu.v('player_x', 'u16') > x, 'the player moves again');
});

test('a script opened from play (debug command 6), its speaker named', () => {
  const emu = startRun().idle(2);
  const script = ids().indexOf('service_receipt');
  emu.writeBytes(emu.symbols.addr('debug_cmd'), [6, script]);
  emu.idle(1);
  assert.equal(emu.v('dlg_active'), 1);
  emu.tap('A');
  matchGolden(emu.idle(20), 'dialog-receipt');
});

test('beside a talker an A bubble shows; A talks instead of firing, and each talker has its script', () => {
  const emu = closeAll(yardTalk());
  assert.equal(emu.v('talk_near'), 0, 'the relay is in reach');
  const fired = emu.v('shots_fired', 'u16');
  emu.tap('A');
  assert.equal(emu.v('dlg_active'), 1, 'A opened its script');
  assert.equal(emu.v('dlg_page_i'), loadDialogue().scripts.slice(0, ids().indexOf('voice_echo')).reduce((n, s) => n + s.pages.length, 0));
  closeAll(emu).idle(4);
  assert.equal(emu.v('shots_fired', 'u16'), fired, 'no shot fired');
  assert.equal(emu.v('dlg_active'), 0, 'the zone plays once: walking around it again starts nothing');
  // The scrap broker at block (18, 10): stand just left of him.
  w16(emu, 'player_x', 17 * 16);
  w16(emu, 'player_y', 10 * 16 - 4);
  emu.idle(60);
  assert.equal(emu.v('talk_near'), 3);
  matchGolden(emu, 'talk-bubble');
  emu.tap('A');
  assert.equal(emu.v('dlg_who'), loadDialogue().speakers.findIndex((p) => p.key === 'marro'));
});
