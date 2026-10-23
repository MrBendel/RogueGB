// The chunk tables (tools/chunks.mjs) and what the chunk editor (web/chunks.js)
// relies on: it patches edited chunks and a seed into a copy of the built ROM,
// and loads a booted save state into it, to preview the real generator.
import { test } from 'node:test';
import fs from 'node:fs';
import path from 'node:path';
import * as B from '../harness/vendor/boytacean/boytacean.js';
import { Emu } from '../harness/emu.mjs';
import { assert, ROOT, boot, settle, startRun, startDescent, TITLE } from './helpers.mjs';
import { loadChunks } from '../tools/chunks.mjs';
import { CHUNK_CELLS, encodeChunkTables, parseChunkDoc, checkChunkSet, cellsToRows, LEGEND } from '../web/chunk-format.js';

const ROM = path.join(ROOT, 'build', 'roguegb.gb');
const offset = (emu, name) => {
  const s = emu.symbols.get(name);
  return s.bank ? s.bank * 0x4000 + (s.addr - 0x4000) : s.addr;
};
function patch(emu, chunks, seed = 0) {
  const rom = new Uint8Array(fs.readFileSync(ROM));
  const t = encodeChunkTables(chunks);
  rom[offset(emu, '_chunk_count')] = t.count;
  rom.set(t.tags, offset(emu, '_chunk_tags'));
  rom.set(t.weight, offset(emu, '_chunk_weight'));
  rom.set(t.theme, offset(emu, '_chunk_theme'));
  rom.set(t.cells, offset(emu, '_chunk_cells'));
  rom[offset(emu, '_tool_seed')] = seed & 255;
  rom[offset(emu, '_tool_seed') + 1] = seed >> 8;
  return rom;
}
function build(emu, seed) {
  const run = (map) => {
    emu.writeBytes(emu.symbols.addr('debug_cmd'), [5, map, seed & 255, seed >> 8]);
    emu.setButtons(0);
    emu.run(1);
  };
  run(0);
  run(emu.read8(emu.symbols.addr('area_exit')));  // (4 x 4 chunks: the entrance room is only 2 x 2)
  return Array.from(emu.readBytes(0xd400, 1024));
}

test('the ROM holds the chunk tables where the editor patches them', () => {
  const emu = boot();
  const { chunks } = loadChunks();
  const t = encodeChunkTables(chunks);
  const rom = fs.readFileSync(ROM);
  assert.equal(rom[offset(emu, '_chunk_count')], chunks.length);
  assert.deepEqual([...rom.subarray(offset(emu, '_chunk_tags'), offset(emu, '_chunk_tags') + t.tags.length)], [...t.tags]);
  assert.deepEqual([...rom.subarray(offset(emu, '_chunk_cells'), offset(emu, '_chunk_cells') + t.cells.length)], [...t.cells]);
  assert.equal(rom[offset(emu, '_tool_seed')] | rom[offset(emu, '_tool_seed') + 1], 0, 'runs start on a random seed');
  // debug_cmd, debug_type and debug_x are written in one go.
  const a = (n) => emu.symbols.addr(n);
  assert.equal(a('debug_type'), a('debug_cmd') + 1);
  assert.equal(a('debug_x'), a('debug_cmd') + 2);
});

test('patched chunks in a booted state build what a fresh boot of that ROM builds', () => {
  const base = startRun().idle(2);
  const state = base.saveState();
  // Every chunk floor except a pit in the middle: unmistakable.
  const { chunks } = loadChunks();
  const edited = chunks.map((c) => ({ ...c, cells: c.cells.map((v, k) => (k === 7 ? LEGEND.o : v === LEGEND['#'] ? 0 : v)) }));
  const rom = patch(base, edited);
  const swapped = new Emu(B, rom);
  swapped.symbols = base.symbols;
  swapped.loadState(state);
  swapped.write8(swapped.symbols.addr('chunk_tables_loaded'), 0);
  const tmp = path.join(ROOT, 'build', 'test-output', 'chunks-patched.gb');
  fs.mkdirSync(path.dirname(tmp), { recursive: true });
  fs.writeFileSync(tmp, rom);
  const e = boot({ rom: tmp });
  e.symbols = base.symbols;
  const fresh = startRun(settle(e, TITLE)).idle(2);
  const seed = 2024;
  const a = build(swapped, seed), b = build(fresh, seed), orig = build(base, seed);
  assert.deepEqual(a, b);
  assert.notDeepEqual(a, orig, 'the edit changed the map');
  assert.ok(a.filter((v) => v === 2).length > 4, 'pits from the edited chunks');
});

test('a seed patched into the ROM starts runs on it', () => {
  const base = boot();
  const tmp = path.join(ROOT, 'build', 'test-output', 'chunks-seeded.gb');
  fs.writeFileSync(tmp, patch(base, loadChunks().chunks, 4242));
  const e = boot({ rom: tmp });
  e.symbols = base.symbols;
  startDescent(settle(e, TITLE), 777);  // the ROM's seed wins over the test's
  assert.equal(e.v('run_seed', 'u16'), 4242);
});

test('chunk files round-trip through the editor format', () => {
  const dir = path.join(ROOT, 'content', 'chunks');
  for (const f of fs.readdirSync(dir).filter((x) => x.endsWith('.json'))) {
    const doc = JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8'));
    const { chunks } = parseChunkDoc(doc, f);
    chunks.forEach((c, i) => assert.deepEqual(cellsToRows(c.cells), doc.chunks[i].rows));
    assert.equal(chunks[0].cells.length, CHUNK_CELLS);
  }
  assert.deepEqual(checkChunkSet(loadChunks().chunks), []);
  assert.match(checkChunkSet([]).join(), /no chunks/);
});
