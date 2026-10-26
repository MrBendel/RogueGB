// RogueGB chunk editor (web/chunks.html): paint the chunks the level generator
// builds maps from (content/chunks/*.json), and preview what the generator makes
// of them. The preview runs the real generator: the built ROM, booted once, with
// the edited chunk tables patched into a copy of it and the booted state loaded
// back, so every edit shows up in a few milliseconds without a rebuild.
// Saving writes the files through the dev server (tools/serve.mjs), which rebuilds.
import initCore, * as B from '../harness/vendor/boytacean/boytacean.js';
import { Emu, Symbols, BTN, toBase64 } from '../harness/emu.mjs';
import {
  CHUNK_W, CHUNK_H, CHUNK_CELLS, LEGEND, TAGS, ANY, parseChunkDoc, checkChunkSet, encodeChunkTables, cellsToRows, tagNames,
} from './chunk-format.js';

const $ = (id) => document.getElementById(id);
const CHARS = Object.keys(LEGEND);  // cell code -> character
const TOOL_NAMES = ['floor', 'wall', 'pit', 'raised', 'maybe wall', 'maybe pit', 'maybe raised', 'enemy', 'chest'];
const TOOL_HELP = [
  'Floor.', 'Wall: blocks everything.', 'Pit: you fall in.', 'Raised: a platform; a wall from the floor.',
  'Wall or floor, a coin flip each time the map is built.', 'Pit or floor, at random.', 'Raised or floor, at random.',
  'An enemy slot (camp chunks only; floor elsewhere).', "The camp's or cache's chest (camp and cache chunks only).",
];
// Block colours (the preview's map and the thumbnails).
const COL = { floor: '#cfd8dc', wall: '#3a4048', pit: '#0e1114', raised: '#9a8466', enemy: '#d23c2c', chest: '#d9a52b' };
const CELL_COL = [COL.floor, COL.wall, COL.pit, COL.raised, '#7c858d', '#5f676e', '#b6a68f', COL.floor, COL.floor];
const BLK_COL = [COL.floor, COL.wall, COL.pit, COL.raised, COL.chest];  // BLK_FLOOR..BLK_PROP
const ROLE_NAMES = ['open', 'rock', 'camp', 'cache', 'start/exit'];
const AREA_W = 4, AREA_H = 3, CELLS = 12, MAP_EXIT = 3;
const DEPTH1 = 7919;  // descent.c: an area's seed is the run seed + depth * 7919

// ---------------------------------------------------------------------------
// The model: every chunk file, each { file, doc, chunks: [{ name, tags, weight, cells }] }.
let files = [];
let fileIdx = 0;
let sel = 0;
let tool = 1;
let filter = 0;  // tag bits, 0 = all
let dirty = new Set();
const undo = [], redo = [];

const cur = () => files[fileIdx];
const chunk = () => cur().chunks[sel];
const allChunks = () => files.flatMap((f, t) => f.chunks.map((c) => ({ ...c, theme: t })));

function snapshot() { return JSON.stringify({ fileIdx, sel, chunks: files.map((f) => f.chunks) }); }
function restore(s) {
  const v = JSON.parse(s);
  v.chunks.forEach((c, i) => { files[i].chunks = c; dirty.add(i); });
  fileIdx = v.fileIdx;
  sel = Math.min(v.sel, cur().chunks.length - 1);
}
/** Call before a change: remembers the state for undo. */
function checkpoint() {
  undo.push(snapshot());
  if (undo.length > 200) undo.shift();
  redo.length = 0;
}
function changed() {
  dirty.add(fileIdx);
  renderAll();
  schedulePreview();
}

// ---------------------------------------------------------------------------
// Loading and saving
let canSave = true;
async function loadFiles() {
  const r = await fetch('/api/chunks', { cache: 'no-store' }).catch(() => null);
  if (!r?.ok) throw new Error('No chunk files: open this page from the dev server (npm run play).');
  files = (await r.json()).map(({ file, doc }) => ({ file, doc, chunks: parseChunkDoc(doc, file).chunks }));
  if (!files.length) throw new Error('content/chunks/ has no .json files');
}

async function save() {
  const errs = checkChunkSet(allChunks());
  if (errs.length && !confirm(`The generator can't use these chunks yet:\n\n${errs.join('\n')}\n\nSave anyway? (The build will fail until it's fixed.)`)) return;
  try {
    for (const i of [...dirty]) {
      const f = files[i];
      const doc = { ...f.doc, chunks: f.chunks.map((c) => ({ name: c.name, tags: tagNames(c.tags), weight: c.weight, rows: cellsToRows(c.cells) })) };
      const r = await fetch('/api/chunks/' + encodeURIComponent(f.file), {
        method: 'PUT', headers: { 'content-type': 'application/json' }, body: JSON.stringify(doc),
      });
      if (!r.ok) throw new Error(await r.text());
      f.doc = doc;
      dirty.delete(i);
    }
    pill('saved · the dev server is rebuilding the ROM', 'ok');
  } catch (e) {
    canSave = false;
    pill('save failed: ' + e.message, 'err');
  }
  renderHeader();
}

function pill(text, kind = '') {
  const p = $('ce-pill');
  p.textContent = text;
  p.className = 'pill ' + kind;
}

// ---------------------------------------------------------------------------
// Rendering: header, list, editor
function renderHeader() {
  $('ce-file').innerHTML = files.map((f, i) => `<option value="${i}">${f.doc.theme || f.file}${dirty.has(i) ? ' •' : ''}</option>`).join('');
  $('ce-file').value = fileIdx;
  $('ce-save').disabled = !dirty.size;
  $('ce-undo').disabled = !undo.length;
  $('ce-redo').disabled = !redo.length;
  if (dirty.size) pill('unsaved changes', '');
}

function drawCells(canvas, cells, flip = 0) {
  const g = canvas.getContext('2d');
  for (let r = 0; r < CHUNK_H; r++) {
    for (let c = 0; c < CHUNK_W; c++) {
      const sr = flip & 2 ? CHUNK_H - 1 - r : r, sc = flip & 1 ? CHUNK_W - 1 - c : c;
      const v = cells[sr * CHUNK_W + sc];
      g.fillStyle = v === 7 ? COL.enemy : v === 8 ? COL.chest : CELL_COL[v];
      g.fillRect(c, r, 1, 1);
    }
  }
}

function lint(c) {
  const out = [];
  const has = (v) => c.cells.includes(v);
  const camp = c.tags & TAGS.camp, cache = c.tags & TAGS.cache;
  if (camp && !has(7)) out.push('A camp chunk needs enemy slots (e).');
  if ((camp || cache) && !has(8)) out.push(`A ${camp ? 'camp' : 'cache'} chunk needs a chest slot (*).`);
  if (!camp && has(7)) out.push('Enemy slots only count in camp chunks; here they are floor.');
  if (!camp && !cache && has(8)) out.push('Chest slots only count in camp and cache chunks; here they are floor.');
  if (c.cells.filter((v) => v === 7).length > 5) out.push('A camp holds 5 enemies; slots after the 5th are floor.');
  if (!c.tags) out.push('No tags: the generator never picks it.');
  if (c.cells.every((v) => v !== 0 && v < 7) && c.tags & (TAGS.open | TAGS.start | TAGS.exit)) out.push('No floor at all in a walkable role: expect carving.');
  return out;
}

function renderList() {
  $('ce-filter').innerHTML = [['all', 0], ...Object.entries(TAGS)]
    .map(([n, b]) => `<button class="chip ${filter === b ? 'on' : ''}" data-tag="${b}">${n}</button>`).join('');
  const list = $('ce-list');
  list.innerHTML = '';
  cur().chunks.forEach((c, i) => {
    if (filter && !(c.tags & filter)) return;
    const el = document.createElement('div');
    el.className = 'ce-item' + (i === sel ? ' sel' : '');
    el.dataset.i = i;
    const cv = document.createElement('canvas');
    cv.width = CHUNK_W;
    cv.height = CHUNK_H;
    drawCells(cv, c.cells);
    const warn = lint(c).length ? ' <span class="warn" title="see the warnings under the grid">⚠</span>' : '';
    el.append(cv);
    el.insertAdjacentHTML('beforeend', `<div style="min-width:0"><div class="nm">${esc(c.name)}${warn}</div>
      <div class="meta">${tagNames(c.tags).join(' ') || 'no tags'} · ×${c.weight}${usage ? ` · ${usagePct(i)}` : ''}</div></div>`);
    list.append(el);
  });
}

function renderEditor() {
  const c = chunk();
  if (document.activeElement !== $('ce-name')) $('ce-name').value = c.name;
  if (document.activeElement !== $('ce-weight')) $('ce-weight').value = c.weight;
  $('ce-tags').innerHTML = Object.entries(TAGS)
    .map(([n, b]) => `<button class="chip ${c.tags & b ? 'on' : ''}" data-bit="${b}" title="${TAG_HELP[n]}">${n}</button>`).join('');
  const grid = $('ce-grid');
  if (!grid.children.length) {
    for (let k = 0; k < CHUNK_CELLS; k++) {
      const d = document.createElement('div');
      d.dataset.k = k;
      grid.append(d);
    }
  }
  [...grid.children].forEach((d, k) => {
    const v = c.cells[k];
    d.className = `cell c${v}`;
    d.innerHTML = `<span>${CHARS[v]}</span>`;
  });
  for (let f = 0; f < 4; f++) drawCells($('ce-f' + f), c.cells, f);
  $('ce-lint').innerHTML = lint(c).map((t) => `<div>${t}</div>`).join('');
  [...$('ce-tools').children].forEach((b, v) => b.classList.toggle('on', v === tool));
}
const TAG_HELP = {
  open: 'Most of the ground in a map',
  rock: 'Solid clusters that shape the map',
  camp: 'Enemies (e) around a chest (*)',
  cache: 'A hidden chest (*)',
  start: "The middle of the area's first map",
  exit: 'The middle of the last map, around the shaft down',
};

function renderTools() {
  $('ce-tools').innerHTML = TOOL_NAMES.map((n, v) =>
    `<button title="${TOOL_HELP[v]}"><span class="sw cell c${v}"><span>${v >= 4 && v <= 6 ? '?' : ''}</span></span>${n}<kbd>${CHARS[v]}</kbd></button>`).join('');
}

function renderAll() {
  renderHeader();
  renderList();
  renderEditor();
}

const esc = (s) => String(s).replace(/[&<>"]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));

// ---------------------------------------------------------------------------
// Editing
let painting = false;
let lastK = -1;
// Paint cell k, and the cells on a line from the last one painted (a fast drag
// skips cells between pointer events).
function paintTo(k, v) {
  if (k == null) return;
  const cells = chunk().cells;
  const from = lastK < 0 ? k : lastK;
  const x0 = from % CHUNK_W, y0 = Math.floor(from / CHUNK_W), x1 = k % CHUNK_W, y1 = Math.floor(k / CHUNK_W);
  const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0));
  let any = false;
  for (let t = 0; t <= n; t++) {
    const x = Math.round(x0 + ((x1 - x0) * t) / (n || 1)), y = Math.round(y0 + ((y1 - y0) * t) / (n || 1));
    if (cells[y * CHUNK_W + x] !== v) { cells[y * CHUNK_W + x] = v; any = true; }
  }
  lastK = k;
  if (any) changed();
}
const cellAt = (el) => { const k = el?.closest?.('.cell')?.dataset.k; return k == null ? null : +k; };
$('ce-grid').addEventListener('pointerdown', (e) => {
  e.preventDefault();
  checkpoint();
  painting = e.button === 2 ? 0 : tool;
  lastK = -1;
  $('ce-grid').setPointerCapture(e.pointerId);
  paintTo(cellAt(e.target), painting);
});
$('ce-grid').addEventListener('pointermove', (e) => {
  if (painting === false) return;
  paintTo(cellAt(document.elementFromPoint(e.clientX, e.clientY)), painting);
});
addEventListener('pointerup', () => { painting = false; });
$('ce-grid').addEventListener('contextmenu', (e) => e.preventDefault());

$('ce-tools').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  tool = [...$('ce-tools').children].indexOf(b);
  renderEditor();
});
$('ce-tags').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  checkpoint();
  chunk().tags ^= +b.dataset.bit;
  changed();
});
$('ce-name').addEventListener('change', () => {
  checkpoint();
  chunk().name = $('ce-name').value.trim() || 'chunk';
  changed();
});
$('ce-weight').addEventListener('change', () => {
  checkpoint();
  chunk().weight = Math.max(1, Math.min(15, Math.round(+$('ce-weight').value) || 1));
  changed();
});
$('ce-list').addEventListener('click', (e) => {
  const it = e.target.closest('.ce-item');
  if (!it) return;
  sel = +it.dataset.i;
  renderAll();
  drawPreview();
});
$('ce-filter').addEventListener('click', (e) => {
  const b = e.target.closest('button');
  if (!b) return;
  filter = +b.dataset.tag;
  renderList();
});
$('ce-file').addEventListener('change', () => {
  fileIdx = +$('ce-file').value;
  sel = 0;
  renderAll();
  drawPreview();
});
$('ce-new').addEventListener('click', () => {
  checkpoint();
  const tags = filter || TAGS.open;
  cur().chunks.splice(sel + 1, 0, { name: uniqueName('new'), tags, weight: 2, cells: new Array(CHUNK_CELLS).fill(0) });
  sel++;
  changed();
});
$('ce-dup').addEventListener('click', () => {
  checkpoint();
  const c = chunk();
  cur().chunks.splice(sel + 1, 0, { ...c, name: uniqueName(c.name), cells: [...c.cells] });
  sel++;
  changed();
});
$('ce-del').addEventListener('click', () => {
  if (cur().chunks.length <= 1) return;
  checkpoint();
  cur().chunks.splice(sel, 1);
  sel = Math.min(sel, cur().chunks.length - 1);
  changed();
});
function uniqueName(base) {
  base = base.replace(/-\d+$/, '');
  const names = new Set(allChunks().map((c) => c.name));
  for (let n = 2; ; n++) if (!names.has(`${base}-${n}`)) return `${base}-${n}`;
}
function doUndo(from, to) {
  if (!from.length) return;
  to.push(snapshot());
  restore(from.pop());
  renderAll();
  schedulePreview();
}
$('ce-undo').addEventListener('click', () => doUndo(undo, redo));
$('ce-redo').addEventListener('click', () => doUndo(redo, undo));
$('ce-save').addEventListener('click', save);

addEventListener('keydown', (e) => {
  if (e.target.matches('input, select, textarea')) return;
  const k = e.key.toLowerCase();
  if ((e.ctrlKey || e.metaKey) && k === 's') { e.preventDefault(); save(); return; }
  if ((e.ctrlKey || e.metaKey) && k === 'z') { e.preventDefault(); doUndo(e.shiftKey ? redo : undo, e.shiftKey ? undo : redo); return; }
  if ((e.ctrlKey || e.metaKey) && k === 'y') { e.preventDefault(); doUndo(redo, undo); return; }
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (e.key in LEGEND) { tool = LEGEND[e.key]; renderEditor(); return; }
  if (k === 'r') roll();
  if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
    e.preventDefault();
    const vis = cur().chunks.map((c, i) => i).filter((i) => !filter || cur().chunks[i].tags & filter);
    const at = vis.indexOf(sel);
    sel = vis[Math.max(0, Math.min(vis.length - 1, at + (e.key === 'ArrowDown' ? 1 : -1)))] ?? sel;
    renderAll();
    drawPreview();
  }
});
addEventListener('beforeunload', (e) => { if (dirty.size) e.preventDefault(); });

// ---------------------------------------------------------------------------
// The generator, in the emulator
let baseRom = null, symbols = null, bootState = null, emu = null;

function romOffset(name) {
  const s = symbols.get(name);
  if (!s) throw new Error(`the ROM has no ${name}: rebuild it (npm run build)`);
  return s.bank ? s.bank * 0x4000 + (s.addr - 0x4000) : s.addr;
}

/** A copy of the built ROM with these chunks (and a seed to start runs on) patched in. */
function patchRom(chunks, seed = 0) {
  const rom = baseRom.slice();
  const t = encodeChunkTables(chunks);
  rom[romOffset('_chunk_count')] = t.count;
  rom.set(t.tags, romOffset('_chunk_tags'));
  rom.set(t.weight, romOffset('_chunk_weight'));
  rom.set(t.theme, romOffset('_chunk_theme'));
  rom.set(t.cells, romOffset('_chunk_cells'));
  const s = romOffset('_tool_seed');
  rom[s] = seed & 255;
  rom[s + 1] = seed >> 8;
  return rom;
}

async function bootEmu() {
  const [r, s] = await Promise.all([fetch('/build/roguegb.gb', { cache: 'no-store' }), fetch('/build/roguegb.noi', { cache: 'no-store' })]);
  if (!r.ok || !s.ok) throw new Error('No ROM yet: run "npm run build" (or fix the build errors).');
  baseRom = new Uint8Array(await r.arrayBuffer());
  symbols = new Symbols(await s.text());
  romOffset('_chunk_cells');
  romOffset('_tool_seed');
  // Boot to the test lab (SELECT on the title): the debug commands run in play.
  const e = new Emu(B, baseRom);
  e.symbols = symbols;
  const v = (n) => e.read8(symbols.addr(n));
  const settled = (state) => (e.read8(symbols.addr('frame_count')) || e.read8(symbols.addr('frame_count') + 1)) && v('game_state') === state && !v('in_transition');
  let n = 0;
  while (!settled(0)) { e.run(1); if (++n > 2000) throw new Error('the ROM never reached the title'); }
  e.setButtons(BTN.SELECT); e.run(1); e.setButtons(0); e.run(1);
  while (!settled(1)) { e.run(1); if (++n > 3000) throw new Error('the ROM never reached play'); }
  e.run(2);
  bootState = e.saveState();
  e.gb.free();
}

/** A fresh emulator on the edited chunks, in the booted state. */
function loadChunksIntoEmu(chunks) {
  emu?.gb?.free();
  emu = new Emu(B, patchRom(chunks));
  emu.symbols = symbols;
  emu.loadState(bootState);
}

const A = (n) => symbols.addr(n);
const bytes = (n, len) => Array.from(emu.readBytes(A(n), len));
const words = (n, len) => Array.from({ length: len }, (_, i) => emu.read16(A(n) + i * 2));

/** Generate the area for a run seed and build map i (debug command 5 in play.c). */
function generate(seed, i) {
  const area = (seed + DEPTH1) & 0xffff;
  // debug_cmd, debug_type, debug_x sit together: one write for all three.
  if (A('debug_type') === A('debug_cmd') + 1 && A('debug_x') === A('debug_cmd') + 2) {
    emu.writeBytes(A('debug_cmd'), [5, i, area & 255, area >> 8]);
  } else {
    emu.writeBytes(A('debug_x'), [area & 255, area >> 8]);
    emu.write8(A('debug_type'), i);
    emu.write8(A('debug_cmd'), 5);
  }
  emu.setButtons(0);
  emu.run(1);
}

function readArea() {
  return {
    type: bytes('area_type', CELLS), links: bytes('area_links', CELLS),
    cw: bytes('area_cw', CELLS), ch: bytes('area_ch', CELLS), start: emu.read8(A('area_start')), exit: emu.read8(A('area_exit')),
  };
}
function readMap() {
  const w = emu.read8(A('room_w')), h = emu.read8(A('room_h'));
  const camps = emu.read8(A('level_camps')), chests = emu.read8(A('level_chests'));
  const slots = bytes('camp_slots', 2), sx = words('camp_slot_x', 10), sy = words('camp_slot_y', 10);
  return {
    w, h, map: Array.from(emu.readBytes(0xd400, 1024)), open: bytes('level_open', 4),
    picks: bytes('level_picks', 48), roles: bytes('roles', 48), carved: emu.read8(A('level_carved')),
    camps: Array.from({ length: camps }, (_, c) => Array.from({ length: slots[c] }, (_, s) => [sx[c * 5 + s] >> 4, sy[c * 5 + s] >> 4])),
    chests: Array.from({ length: chests }, (_, c) => [emu.read16(A('chest_x') + c * 2) >> 4, emu.read16(A('chest_y') + c * 2) >> 4]),
  };
}

/** The whole area for a seed: its layout and every map, built. */
function buildArea(seed) {
  generate(seed, 0);
  const area = readArea();
  area.maps = [];
  for (let i = 0; i < CELLS; i++) {
    if (!area.type[i]) continue;
    generate(seed, i);
    area.maps[i] = readMap();
  }
  return area;
}

// ---------------------------------------------------------------------------
// Preview
let seed = 1;
let area = null;
let mapSel = -1;
let previewTimer = null;
let chunkBase = 0;  // index of this file's first chunk among all chunks (picks are global indices)

function schedulePreview() {
  clearTimeout(previewTimer);
  previewTimer = setTimeout(refreshPreview, 60);
}

function refreshPreview() {
  if (!bootState) return;
  const chunks = allChunks();
  const errs = checkChunkSet(chunks);
  if (errs.length) {
    $('ce-hover').innerHTML = `<span style="color:var(--rec)">The generator can't run: ${errs.map(esc).join('; ')}</span>`;
    return;
  }
  try {
    loadChunksIntoEmu(chunks);
    area = buildArea(seed);
    if (mapSel < 0 || !area.type[mapSel]) mapSel = area.start;
    drawPreview();
  } catch (e) {
    $('ce-hover').textContent = 'Preview failed: ' + e.message;
    console.error(e);
  }
}

function roll() {
  seed = 1 + Math.floor(Math.random() * 65535);
  $('ce-seed').value = seed;
  mapSel = -1;
  refreshPreview();
}

const SLOT = 104, BPX = 3;  // the area view: a slot per map, 3 px per block
function drawPreview() {
  if (!area) return;
  chunkBase = files.slice(0, fileIdx).reduce((s, f) => s + f.chunks.length, 0);
  const mine = chunkBase + sel;
  const cv = $('ce-area');
  cv.width = AREA_W * SLOT;
  cv.height = AREA_H * SLOT;
  const g = cv.getContext('2d');
  g.clearRect(0, 0, cv.width, cv.height);
  const centre = (i) => [(i % AREA_W) * SLOT + SLOT / 2, Math.floor(i / AREA_W) * SLOT + SLOT / 2];
  // Links first, under the maps.
  g.strokeStyle = '#8a9299';
  g.lineWidth = 6;
  for (let i = 0; i < CELLS; i++) {
    if (!area.type[i]) continue;
    const [x, y] = centre(i);
    if (area.links[i] & 2) { g.beginPath(); g.moveTo(x, y); g.lineTo(x + SLOT, y); g.stroke(); }
    if (area.links[i] & 4) { g.beginPath(); g.moveTo(x, y); g.lineTo(x, y + SLOT); g.stroke(); }
  }
  for (let i = 0; i < CELLS; i++) {
    const m = area.maps[i];
    if (!m) continue;
    const [cx, cy] = centre(i);
    const ox = Math.round(cx - (m.w * BPX) / 2), oy = Math.round(cy - (m.h * BPX) / 2);
    drawBlocks(g, m, ox, oy, BPX, i);
    if (m.picks.slice(0, 64).some((k, c) => k === mine && c % 8 < m.w / CHUNK_W && c >> 3 < m.h / CHUNK_H)) outlinePicks(g, m, ox, oy, BPX, mine, 1);
    g.strokeStyle = i === mapSel ? '#34c77b' : 'transparent';
    g.lineWidth = 2;
    g.strokeRect(ox - 2, oy - 2, m.w * BPX + 4, m.h * BPX + 4);
    g.fillStyle = '#fff';
    g.font = 'bold 11px system-ui';
    const label = i === area.start ? 'start' : area.type[i] === MAP_EXIT ? 'exit' : '';
    if (label) g.fillText(label, ox + 2, oy + 11);
  }
  drawMap();
}

function drawBlocks(g, m, ox, oy, s, i) {
  for (let y = 0; y < m.h; y++) {
    for (let x = 0; x < m.w; x++) {
      g.fillStyle = BLK_COL[m.map[(y << 5) | x] & 0x7f] || '#f0f';
      g.fillRect(ox + x * s, oy + y * s, s, s);
    }
  }
  const dot = (x, y, col, r) => { g.fillStyle = col; g.fillRect(ox + x * s + s / 2 - r, oy + y * s + s / 2 - r, r * 2, r * 2); };
  const r = Math.max(1.5, s * 0.35);
  for (const camp of m.camps) for (const [x, y] of camp) dot(x, y, COL.enemy, r);
  for (const [x, y] of m.chests) dot(x, y, COL.chest, r * 1.2);
  // The start's mark, or the exit's shaft (descent.c digs 2x2 pits there).
  const mx = (m.w >> 1) - 1, my = (m.h >> 1) - 1;
  if (i === area.start) dot(mx + 0.5, my + 0.5, '#5b8cff', r * 1.6);
  if (area.type[i] === MAP_EXIT) { g.fillStyle = '#b04cff'; g.fillRect(ox + mx * s, oy + my * s, s * 2, s * 2); }
}

function outlinePicks(g, m, ox, oy, s, k, lw) {
  g.strokeStyle = '#34c77b';
  g.lineWidth = lw;
  for (let cy = 0; cy < m.h / CHUNK_H; cy++) {
    for (let cx = 0; cx < m.w / CHUNK_W; cx++) {
      if (m.picks[cy * 8 + cx] !== k) continue;
      g.strokeRect(ox + cx * CHUNK_W * s + lw / 2, oy + cy * CHUNK_H * s + lw / 2, CHUNK_W * s - lw, CHUNK_H * s - lw);
    }
  }
}

const MPX = 14;  // the selected map: 14 px per block
function drawMap() {
  const m = area.maps[mapSel];
  const cv = $('ce-map');
  if (!m) { cv.width = cv.height = 0; return; }
  cv.width = m.w * MPX;
  cv.height = m.h * MPX;
  const g = cv.getContext('2d');
  drawBlocks(g, m, 0, 0, MPX, mapSel);
  // Chunk boundaries.
  g.strokeStyle = 'rgba(90, 120, 140, 0.45)';
  g.lineWidth = 1;
  for (let x = CHUNK_W; x < m.w; x += CHUNK_W) { g.beginPath(); g.moveTo(x * MPX + 0.5, 0); g.lineTo(x * MPX + 0.5, cv.height); g.stroke(); }
  for (let y = CHUNK_H; y < m.h; y += CHUNK_H) { g.beginPath(); g.moveTo(0, y * MPX + 0.5); g.lineTo(cv.width, y * MPX + 0.5); g.stroke(); }
  outlinePicks(g, m, 0, 0, MPX, chunkBase + sel, 3);
  const camps = m.camps.length, chests = m.chests.length;
  $('ce-hover').innerHTML = `Map ${mapSel}: ${m.w}×${m.h} blocks, ${camps} camp${camps === 1 ? '' : 's'}, ${chests} chest${chests === 1 ? '' : 's'}`
    + (m.carved ? ' · <span style="color:var(--rec)">carved a path</span>' : '') + ' · hover a chunk to name it, click to edit it';
}

function chunkAtMap(e) {
  const m = area?.maps[mapSel];
  if (!m) return null;
  const r = $('ce-map').getBoundingClientRect();
  const bx = Math.floor(((e.clientX - r.left) / r.width) * m.w), by = Math.floor(((e.clientY - r.top) / r.height) * m.h);
  const cx = Math.floor(bx / CHUNK_W), cy = Math.floor(by / CHUNK_H);
  if (cx < 0 || cy < 0 || cx >= m.w / CHUNK_W || cy >= m.h / CHUNK_H) return null;
  return { k: m.picks[cy * 8 + cx], role: m.roles[cy * 8 + cx] };
}
$('ce-map').addEventListener('mousemove', (e) => {
  const at = chunkAtMap(e);
  if (!at) return;
  const c = allChunks()[at.k];
  $('ce-hover').textContent = `${c?.name ?? '?'} (${files[c?.theme]?.doc.theme ?? ''}) as ${ROLE_NAMES[at.role] ?? at.role}`;
});
$('ce-map').addEventListener('mouseleave', () => area && drawMap());
$('ce-map').addEventListener('click', (e) => {
  const at = chunkAtMap(e);
  if (!at) return;
  let k = at.k;
  fileIdx = files.findIndex((f) => (k < f.chunks.length ? true : ((k -= f.chunks.length), false)));
  sel = k;
  filter = 0;
  renderAll();
  drawPreview();
});
$('ce-area').addEventListener('click', (e) => {
  if (!area) return;
  const r = $('ce-area').getBoundingClientRect();
  const i = Math.floor(((e.clientY - r.top) / r.height) * AREA_H) * AREA_W + Math.floor(((e.clientX - r.left) / r.width) * AREA_W);
  if (area.type[i]) { mapSel = i; drawPreview(); }
});
$('ce-seed').addEventListener('change', () => {
  seed = Math.max(1, Math.min(65535, Math.round(+$('ce-seed').value) || 1));
  $('ce-seed').value = seed;
  mapSel = -1;
  refreshPreview();
});
$('ce-roll').addEventListener('click', roll);
$('ce-play').addEventListener('click', () => {
  try {
    localStorage.setItem('rgb.draftRom', JSON.stringify(toBase64(patchRom(allChunks(), seed))));
  } catch (e) {
    pill("couldn't hand the ROM to the playtest page: " + e.message, 'err');
    return;
  }
  window.open('index.html?draft', 'roguegb-draft');
});

// ---------------------------------------------------------------------------
// Survey: many areas, and how often each chunk ends up in them
let usage = null;  // { picks: count per chunk, cells, maps, carved, camps, chests, areas }
const usagePct = (i) => {
  const n = usage.picks[chunkBase + i] || 0;
  return n ? `${((100 * n) / usage.cells).toFixed(1)}%` : '<span class="warn">unused</span>';
};

async function survey() {
  if (!bootState) return;
  const btn = $('ce-run-survey');
  btn.disabled = true;
  const chunks = allChunks();
  loadChunksIntoEmu(chunks);
  const u = { picks: new Array(chunks.length).fill(0), cells: 0, maps: 0, carved: 0, camps: 0, chests: 0, areas: 0 };
  const N = 100;
  for (let n = 0; n < N; n++) {
    const s = 1 + ((n * 7919 + 12345) % 65535);
    const a = buildArea(s);
    u.areas++;
    for (const m of a.maps) {
      if (!m) continue;
      u.maps++;
      u.carved += m.carved;
      u.camps += m.camps.length;
      u.chests += m.chests.length;
      for (let cy = 0; cy < m.h / CHUNK_H; cy++) for (let cx = 0; cx < m.w / CHUNK_W; cx++) { u.picks[m.picks[cy * 8 + cx]]++; u.cells++; }
    }
    if (n % 10 === 9) { btn.textContent = `📊 ${n + 1}/${N}…`; await new Promise((r) => setTimeout(r)); }
  }
  usage = u;
  btn.textContent = '📊 Survey 100 areas';
  btn.disabled = false;
  refreshPreview();  // the survey left the emulator on its last area
  renderList();
  renderSurvey(chunks);
}

function renderSurvey(chunks) {
  const u = usage;
  const max = Math.max(...u.picks);
  const rows = chunks.map((c, k) => [c, k]).sort((a, b) => u.picks[b[1]] - u.picks[a[1]]).map(([c, k]) =>
    `<tr><td>${esc(c.name)}</td><td class="muted">${tagNames(c.tags).join(' ')}</td><td class="n">${u.picks[k] ? ((100 * u.picks[k]) / u.cells).toFixed(1) + '%' : '<span style="color:var(--rec)">unused</span>'}</td>
     <td style="width:35%"><div class="bar" style="width:${max ? (100 * u.picks[k]) / max : 0}%"></div></td></tr>`).join('');
  $('ce-survey').innerHTML = `<p><strong>${u.areas} areas, ${u.maps} maps</strong> · ${(u.maps / u.areas).toFixed(1)} maps an area ·
    ${(u.camps / u.maps).toFixed(2)} camps and ${(u.chests / u.maps).toFixed(2)} chests a map ·
    <span style="${u.carved ? 'color:var(--rec)' : ''}">${((100 * u.carved) / u.maps).toFixed(0)}% of maps carved a path</span></p>
    <table>${rows}</table>`;
}
$('ce-run-survey').addEventListener('click', survey);

// ---------------------------------------------------------------------------
// Start
renderTools();
(async () => {
  try {
    await loadFiles();
    renderAll();
    pill('booting the ROM…');
    await initCore();
    await bootEmu();
    pill('ready', 'ok');
    $('ce-seed').value = seed;
    refreshPreview();
  } catch (e) {
    pill(e.message, 'err');
    console.error(e);
  }
})();

// The dev server announces new builds: the preview follows the saved chunks.
try {
  const es = new EventSource('/events');
  es.onmessage = async (e) => {
    const ev = JSON.parse(e.data);
    if (ev.type !== 'build' || !ev.ok || !bootState) return;
    await bootEmu();
    pill(dirty.size ? 'ROM rebuilt · unsaved changes' : 'ROM rebuilt', 'ok');
    refreshPreview();
  };
} catch { /* no dev server */ }
