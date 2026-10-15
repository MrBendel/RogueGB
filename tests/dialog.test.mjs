// The dialogue box (dialog.c) and its scripts (tools/dialogue.mjs).
import { test } from 'node:test';
import { assert, matchGolden, bootToTitle, startRun, settle, PLAY } from './helpers.mjs';
import { loadDialogue, wrap, DLG_WRAP_PX } from '../tools/dialogue.mjs';

const yardTalk = () => settle(bootToTitle().tap('B'), PLAY, 'the ice yard');
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

test('the yard opens with a conversation: typed out a letter every 2 frames, the world waiting', () => {
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
