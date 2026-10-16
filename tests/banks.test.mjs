// ROM banking (#17): what lives in bank 0 (always visible at 0x0000-0x3FFF) and
// what lives in switchable banks (shown one at a time at 0x4000-0x7FFF).
import { test } from 'node:test';
import fs from 'node:fs';
import path from 'node:path';
import { assert, ROOT, startRun } from './helpers.mjs';
import { checkBank0, BANK0_LIMIT } from '../tools/build.mjs';

const NOI = path.join(ROOT, 'build', 'roguegb.noi');
const syms = new Map([...fs.readFileSync(NOI, 'utf8').matchAll(/^DEF (\S+) 0x([0-9A-F]+)/gim)].map((m) => [m[1], parseInt(m[2], 16)]));
// Banked symbols carry their bank above bit 16 (bank 1 at 0x4000 is 0x14000).
const bankOf = (name) => {
  const a = syms.get('_' + name);
  assert.ok(a !== undefined, `symbol ${name} exists`);
  return a >> 16;
};

test('bank 0 holds all fixed code and data, with room to spare', () => {
  const r = checkBank0(NOI);
  assert.ok(r.ok, r.error);
  assert.ok(r.used <= 0x4000 * BANK0_LIMIT, `bank 0 used ${r.used} bytes`);
});

test('global RAM sections stay below the fixed room map', () => {
  // Where the map starts (src/room.h; the work RAM layout is described there).
  const mapAt = parseInt(fs.readFileSync(path.join(ROOT, 'src', 'room.h'), 'utf8').match(/#define ROOM_MAP_ADDR (0x[0-9A-Fa-f]+)/)[1], 16);
  for (const section of ['DATA','BSS','INITIALIZED']) {
    const start=syms.get(`s__${section}`), size=syms.get(`l__${section}`);
    assert.ok(Number.isInteger(start) && Number.isInteger(size),`${section} layout exists`);
    assert.ok(start+size<=mapAt,`${section} ends at 0x${(start+size).toString(16)} and overlaps room_map at 0x${mapAt.toString(16)}`);
  }
});

test('cold code and once-per-scene data live in switchable banks', () => {
  for (const name of [
    'title_enter', 'title_update', 'play_enter', 'play_update', 'run_over_enter', 'run_over_update',
    'run_start', 'room_load_lab', 'gfx_init', 'gfx_clear', 'fade_in', 'fade_out', 'camera_init', 'bkg_frame', 'win_frame', 'actors_init', 'actor_spawn', 'player_init', 'move_apply_preset', 'move_recalc',
    'font_tiles', 'frame_tiles', 'spr_player_tiles', 'spr_slime_tiles', 'spr_peashooter_tiles', 'spr_pea_tiles',
  ]) {
    assert.ok(bankOf(name) >= 1, `${name} is banked`);
  }
});

test('hot paths, vblank code and text drawing stay in bank 0', () => {
  for (const name of [
    'main', 'wait_frame', 'gfx_vbl', 'gfx_commit', 'vram_queue_write',
    'player_update', 'player_draw', 'actors_update', 'actors_emit', 'bullets_update',
    'camera_update', 'room_block_at', 'room_draw_row', 'room_draw_column', 'room_fill_row', 'room_fill_column',
    // Banked code passes strings from its own bank to these; if they were in another
    // switchable bank, calling them would hide the caller's strings.
    'bkg_text', 'win_text',
  ]) {
    assert.equal(bankOf(name), 0, `${name} is in bank 0`);
    assert.ok(syms.get('_' + name) < 0x4000, `${name} is below 0x4000`);
  }
});

test('the game runs across banks: banked scenes call bank-0 code every frame', () => {
  // play_update is banked and calls into bank 0 each frame; a full run through the
  // lab, scrolling and jumping, exercises those calls and the bank switches around
  // asset loading (gfx_init, actors_init, room_load_lab).
  const emu = startRun().idle(2).hold('LEFT', 80).hold('DOWN+B', 200).hold('RIGHT', 200);
  assert.equal(emu.v('game_state'), 1, 'still playing');
  // At vsync, banked calls have returned and restored the caller's bank. Once
  // autobanking places play_update elsewhere, its bank need not remain active.
  // Observe the function entry itself, bounded to a few frames of instructions.
  const entry = syms.get('_play_update') & 0xffff;
  let entered = false;
  for (let cycles = 0; cycles < 300000;) {
    const r = emu.gb.registers(), pc = r.pc;
    r.free();
    if (pc === entry) {
      emu._snap = null;
      if (emu.read8(syms.get('__current_bank')) === bankOf('play_update')) {
        entered = true;
        break;
      }
    }
    cycles += emu.gb.clock();
  }
  assert.ok(entered, 'entered play_update with its ROM bank selected');
});

test('banked code never switches banks itself (it would swap its own code out)', () => {
  // A #pragma bank file that calls SWITCH_ROM runs fine only while the code and
  // the data happen to share a bank; once the ROM grows, it crashes. Banked code
  // uses the bank-0 helpers instead (banked_memcpy, banked_sprite_data, ...).
  const src = path.join(ROOT, 'src');
  for (const f of fs.readdirSync(src).filter((n) => n.endsWith('.c'))) {
    const text = fs.readFileSync(path.join(src, f), 'utf8');
    if (!/^#pragma bank/m.test(text)) continue;
    assert.ok(!/SWITCH_ROM\s*\(/.test(text), `${f} is banked and calls SWITCH_ROM`);
  }
});
