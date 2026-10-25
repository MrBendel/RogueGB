// RogueGB world viewer (web/world.html): a seed's whole generated area, every map
// drawn the way the game draws it. Nothing here re-implements the generator: the
// built ROM runs in the page (as in the chunk editor), is asked to build each map
// (debug command 5 in play.c), and the maps are read out of its memory, with the
// looks the theme chose (room_vis) drawn from the tileset's block atlas
// (build/gen/atlas-<tileset>.png, written by tools/tileset.mjs). Chunk weights can
// be tweaked live (patched into a copy of the ROM) and saved through the dev server.
// Without one (GitHub Pages) it reads world-data/ beside the page (tools/world-data.mjs),
// and saving downloads the chunk file instead.
import initCore, * as B from '../harness/vendor/boytacean/boytacean.js';
import { Emu, Symbols, BTN } from '../harness/emu.mjs';
import { parseChunkDoc, encodeChunkTables, checkChunkSet, tagNames, cellsToRows } from './chunk-format.js';

const $ = (id) => document.getElementById(id);
const AREA_W = 4, AREA_H = 3, CELLS = AREA_W * AREA_H;
const MAP_START = 1, MAP_EXIT = 3;
const DEPTH_STEP = 7919;            // descent.c: an area's seed is the run seed + depth * 7919
const ROOM_MAP = 0xd400, ROOM_VIS = 0xd700;  // room.h
const BLK = 16, GAP = 3;            // px a block; blocks between maps in the area view
const EDGES = [1, 2, 4, 8];         // N E S W (level.h EDGE_*), the order of level_open
const TYPE_COL = ['#cfd8dc', '#3a4048', '#0e1114', '#9a8466', '#d9a52b', '#8aa0aa', '#aaa088'];
const ROLE_NAMES = ['open', 'rock', 'camp', 'cache', 'mark'];
const ROLE_COL = ['rgba(0,0,0,0)', 'rgba(80,120,255,.18)', 'rgba(255,60,40,.18)', 'rgba(255,200,40,.22)', 'rgba(120,255,120,.2)'];
// A chunk theme and the tilesets that draw it: its maps, and the entrance room (indoors).
const THEME_TILESET = { lunar: { main: 'lunar_ext', start: 'lunar_int' } };
const PROPS = new Set(['crate', 'crate_stack', 'boulder']);

let files = [];        // chunk files: { file, doc, chunks, saved (weights as loaded) }
let baseRom = null, symbols = null, bootState = null, emu = null;
let atlases = {};      // { main, start }: each { img, names, behind, standIn }
const atlasOf = (m) => (m.isStart && atlases.start) || atlases.main || null;
let area = null, sel = -1, themeIdx = 0;
let devServer = false;  // the dev server (npm run play): live files, saving; else world-data/
const mapCanvas = new Map();  // cache: map index -> canvas at 1 px per px

// ---------------------------------------------------------------------------
// Files and the ROM
async function loadFiles() {
  let r = await fetch('/api/chunks', { cache: 'no-store' }).catch(() => null);
  devServer = !!r?.ok;
  if (!devServer) r = await fetch('world-data/chunks.json', { cache: 'no-store' }).catch(() => null);
  if (!r?.ok) throw new Error('No chunk files: open this page from the dev server (npm run play), or the published site.');
  files = (await r.json()).map(({ file, doc }) => {
    const chunks = parseChunkDoc(doc, file).chunks;
    return { file, doc, chunks, saved: chunks.map((c) => c.weight) };
  });
}
const allChunks = () => files.flatMap((f, t) => f.chunks.map((c) => ({ ...c, theme: t })));

function romOffset(name) {
  const s = symbols.get(name);
  if (!s) throw new Error(`the ROM has no ${name}: rebuild it (npm run build)`);
  return s.bank ? s.bank * 0x4000 + (s.addr - 0x4000) : s.addr;
}

function patchRom() {
  const rom = baseRom.slice(), t = encodeChunkTables(allChunks());
  rom[romOffset('_chunk_count')] = t.count;
  rom.set(t.tags, romOffset('_chunk_tags'));
  rom.set(t.weight, romOffset('_chunk_weight'));
  rom.set(t.theme, romOffset('_chunk_theme'));
  rom.set(t.cells, romOffset('_chunk_cells'));
  return rom;
}

async function bootRom() {
  const at = devServer ? '/build/' : 'world-data/';
  const [r, s] = await Promise.all([fetch(at + 'roguegb.gb', { cache: 'no-store' }), fetch(at + 'roguegb.noi', { cache: 'no-store' })]);
  if (!r.ok || !s.ok) throw new Error('No ROM yet: run "npm run build" (or fix the build errors).');
  baseRom = new Uint8Array(await r.arrayBuffer());
  symbols = new Symbols(await s.text());
  romOffset('_chunk_cells');
  if (!symbols.get('_area_theme')) throw new Error('the ROM has no area_theme: rebuild it (npm run build)');
  // Boot to the test lab (SELECT on the title): debug commands run in play.
  const e = new Emu(B, baseRom);
  e.symbols = symbols;
  const v = (n) => e.read8(symbols.addr(n));
  const settled = (st) => (e.read8(symbols.addr('frame_count')) || e.read8(symbols.addr('frame_count') + 1)) && v('game_state') === st && !v('in_transition');
  let n = 0;
  while (!settled(0)) { e.run(1); if (++n > 2000) throw new Error('the ROM never reached the title'); }
  e.setButtons(BTN.SELECT); e.run(1); e.setButtons(0); e.run(1);
  while (!settled(1)) { e.run(1); if (++n > 3000) throw new Error('the ROM never reached play'); }
  e.run(2);
  bootState = e.saveState();
  e.gb.free();
  reloadEmu();
}

/** A fresh emulator on the current chunk weights, in the booted state. */
function reloadEmu() {
  emu?.gb?.free();
  emu = new Emu(B, patchRom());
  emu.symbols = symbols;
  emu.loadState(bootState);
}

async function loadAtlas() {
  atlases = {};
  const sets = THEME_TILESET[files[themeIdx].doc.theme];
  $('wv-artnote').textContent = sets ? '' : '(no tileset for this theme: block types only)';
  if (!sets) return;
  const at = devServer ? '/build/gen/' : 'world-data/';
  for (const [role, ts] of Object.entries(sets)) {
    const [j, img] = await Promise.all([
      fetch(`${at}atlas-${ts}.json`, { cache: 'no-store' }).then((r) => (r.ok ? r.json() : null)),
      new Promise((res) => { const i = new Image(); i.onload = () => res(i); i.onerror = () => res(null); i.src = `${at}atlas-${ts}.png?${Date.now()}`; }),
    ]);
    if (j && img) atlases[role] = { img, ...j };
  }
  if (!atlases.main) { $('wv-artnote').textContent = '(no atlas: rebuild)'; return; }
  $('wv-artnote').textContent = atlases.main.standIn ? '(stand-in art: the pack isn\'t on this machine)' : '';
}

// ---------------------------------------------------------------------------
// The generator
const A = (n) => symbols.addr(n);
const bytes = (n, len) => Array.from(emu.readBytes(A(n), len));
const words = (n, len) => Array.from({ length: len }, (_, i) => emu.read16(A(n) + i * 2));

function generate(areaSeed, i) {
  emu.write8(A('area_theme'), themeIdx);
  emu.write8(A('chunk_tables_loaded'), 0);  // re-read the tables (and the theme filter)
  emu.writeBytes(A('debug_x'), [areaSeed & 255, areaSeed >> 8]);
  emu.write8(A('debug_type'), i);
  emu.write8(A('debug_cmd'), 5);
  emu.setButtons(0);
  emu.run(1);
}

function readMap() {
  const w = emu.read8(A('room_w')), h = emu.read8(A('room_h'));
  const camps = emu.read8(A('level_camps')), chests = emu.read8(A('level_chests'));
  const slots = bytes('camp_slots', 2), sx = words('camp_slot_x', 10), sy = words('camp_slot_y', 10);
  return {
    w, h, map: Array.from(emu.readBytes(ROOM_MAP, 1024)), vis: Array.from(emu.readBytes(ROOM_VIS, 1024)),
    open: bytes('level_open', 4), picks: bytes('level_picks', 48), roles: bytes('roles', 48), carved: emu.read8(A('level_carved')),
    camps: Array.from({ length: camps }, (_, c) => Array.from({ length: slots[c] }, (_, s) => [sx[c * 5 + s], sy[c * 5 + s]])),
    chests: Array.from({ length: chests }, (_, c) => [emu.read16(A('chest_x') + c * 2), emu.read16(A('chest_y') + c * 2)]),
  };
}

/** A whole area: its layout, and every map built. */
function buildArea(areaSeed) {
  generate(areaSeed, 0);
  const a = {
    seed: areaSeed, type: bytes('area_type', CELLS), links: bytes('area_links', CELLS),
    start: emu.read8(A('area_start')), exit: emu.read8(A('area_exit')), maps: [],
  };
  for (let i = 0; i < CELLS; i++) {
    if (!a.type[i]) continue;
    generate(areaSeed, i);
    a.maps[i] = readMap();
    a.maps[i].isStart = i === a.start;  // (drawn with the entrance room's tileset)
  }
  return a;
}

/** What a map holds, for the info line and the survey. */
function mapStats(m) {
  let buildings = 0, props = 0, solid = 0;
  for (let y = 0; y < m.h; y++)
    for (let x = 0; x < m.w; x++) {
      const k = y * 32 + x, name = atlasOf(m)?.names[m.vis[k]] || '';
      if (/^roof_(bl|sl)/.test(name)) buildings++;   // one bottom-left corner a building
      if (PROPS.has(name)) props++;
      if ((m.map[k] & 0x7f) === 1 && x && y && x < m.w - 1 && y < m.h - 1) solid++;
    }
  const enemies = m.camps.reduce((s, c) => s + c.length, 0);
  return { buildings, props, solid, enemies, camps: m.camps.length, chests: m.chests.length, carved: m.carved };
}

// ---------------------------------------------------------------------------
// Drawing
const opt = (id) => $(id).checked;

/** A map at full size (16 px a block), cached. */
function drawMap(i) {
  const key = i + (opt('wv-art') ? 'a' : 't');
  if (mapCanvas.has(key)) return mapCanvas.get(key);
  const m = area.maps[i], c = document.createElement('canvas'), atlas = atlasOf(m);
  c.width = m.w * BLK; c.height = m.h * BLK;
  const g = c.getContext('2d');
  for (let y = 0; y < m.h; y++)
    for (let x = 0; x < m.w; x++) {
      const k = y * 32 + x, v = m.vis[k];
      if (opt('wv-art') && atlas && v < atlas.names.length) {
        g.drawImage(atlas.img, v * BLK, 0, BLK, BLK, x * BLK, y * BLK, BLK, BLK);
      } else {
        g.fillStyle = TYPE_COL[m.map[k] & 0x7f] || '#f0f';
        g.fillRect(x * BLK, y * BLK, BLK, BLK);
      }
    }
  mapCanvas.set(key, c);
  return c;
}

/** Enemies, chests, the chunk grid and roles on a map drawn at (ox, oy), scale s. */
function overlays(g, m, ox, oy, s, detail) {
  if (opt('wv-grid') || detail) {
    g.strokeStyle = 'rgba(255,255,255,.35)';
    g.lineWidth = 1;
    for (let cy = 0; cy * 4 < m.h; cy++)
      for (let cx = 0; cx * 5 < m.w; cx++) {
        const role = m.roles[cy * 8 + cx];
        g.fillStyle = ROLE_COL[role] || 'transparent';
        g.fillRect(ox + cx * 5 * BLK * s, oy + cy * 4 * BLK * s, 5 * BLK * s, 4 * BLK * s);
        g.strokeRect(ox + cx * 5 * BLK * s + 0.5, oy + cy * 4 * BLK * s + 0.5, 5 * BLK * s, 4 * BLK * s);
      }
  }
  if (opt('wv-enemies')) {
    g.fillStyle = '#ff3b2f';
    for (const camp of m.camps) for (const [x, y] of camp) {
      g.beginPath(); g.arc(ox + (x + 8) * s, oy + (y + 8) * s, Math.max(2, 5 * s), 0, 7); g.fill();
    }
  }
  if (opt('wv-chests')) {
    g.fillStyle = '#ffd23a'; g.strokeStyle = '#000';
    for (const [x, y] of m.chests) {
      g.fillRect(ox + (x + 3) * s, oy + (y + 3) * s, 10 * s + 1, 10 * s + 1);
      g.strokeRect(ox + (x + 3) * s, oy + (y + 3) * s, 10 * s + 1, 10 * s + 1);
    }
  }
}

function layout() {
  let cw = 0, ch = 0;
  for (const m of area.maps) if (m) { cw = Math.max(cw, m.w); ch = Math.max(ch, m.h); }
  return { cw: cw + GAP, ch: ch + GAP };
}

function drawArea() {
  const s = +$('wv-zoom').value, { cw, ch } = layout(), c = $('wv-area'), g = c.getContext('2d');
  c.width = Math.ceil(AREA_W * cw * BLK * s); c.height = Math.ceil(AREA_H * ch * BLK * s);
  g.imageSmoothingEnabled = false;
  g.fillStyle = '#101210'; g.fillRect(0, 0, c.width, c.height);
  const at = (i) => {
    const m = area.maps[i];
    const ox = ((i % AREA_W) * cw + (cw - m.w) / 2) * BLK * s, oy = (Math.floor(i / AREA_W) * ch + (ch - m.h) / 2) * BLK * s;
    return [ox, oy];
  };
  // Links: a line from each opening to the neighbour's.
  if (opt('wv-links')) {
    g.strokeStyle = '#7fd46a'; g.lineWidth = Math.max(2, 6 * s);
    for (let i = 0; i < CELLS; i++) {
      const m = area.maps[i];
      if (!m) continue;
      for (const [side, dx] of [[1, 1], [2, AREA_W]]) {  // east and south: each link once
        if (!(area.links[i] & EDGES[side]) || !area.maps[i + dx]) continue;
        const n = area.maps[i + dx], [ax, ay] = at(i), [bx, by] = at(i + dx), mid = 2;
        g.beginPath();
        if (side === 1) {
          g.moveTo(ax + m.w * BLK * s, ay + (m.open[1] + mid) * BLK * s);
          g.lineTo(bx, by + (n.open[3] + mid) * BLK * s);
        } else {
          g.moveTo(ax + (m.open[2] + mid) * BLK * s, ay + m.h * BLK * s);
          g.lineTo(bx + (n.open[0] + mid) * BLK * s, by);
        }
        g.stroke();
      }
    }
  }
  for (let i = 0; i < CELLS; i++) {
    const m = area.maps[i];
    if (!m) continue;
    const [ox, oy] = at(i);
    g.drawImage(drawMap(i), ox, oy, m.w * BLK * s, m.h * BLK * s);
    overlays(g, m, ox, oy, s, false);
    // The map's name: start, exit, carved.
    const label = `${i}${i === area.start ? ' START' : ''}${i === area.exit ? ' EXIT' : ''}${m.carved ? ' · carved' : ''}`;
    g.font = `${Math.max(10, 28 * s)}px system-ui, sans-serif`;
    g.fillStyle = 'rgba(0,0,0,.65)';
    g.fillRect(ox, oy - Math.max(12, 32 * s), g.measureText(label).width + 8, Math.max(12, 32 * s));
    g.fillStyle = m.carved ? '#e0a040' : i === area.start || i === area.exit ? '#7fd46a' : '#ddd';
    g.fillText(label, ox + 4, oy - Math.max(3, 8 * s));
    if (i === sel) {
      g.strokeStyle = '#ffd23a'; g.lineWidth = 3;
      g.strokeRect(ox - 2, oy - 2, m.w * BLK * s + 4, m.h * BLK * s + 4);
    }
  }
  const maps = area.maps.filter(Boolean), st = maps.map(mapStats);
  const sum = (k) => st.reduce((a, x) => a + x[k], 0);
  $('wv-areainfo').textContent = `· area seed ${area.seed} · ${maps.length} maps · ${sum('buildings')} buildings · ${sum('props')} props · ${sum('camps')} camps (${sum('enemies')} enemies) · ${sum('chests')} chests${sum('carved') ? ` · ${sum('carved')} carved` : ''}`;
}

function drawDetail() {
  const c = $('wv-map'), g = c.getContext('2d');
  if (sel < 0 || !area.maps[sel]) { c.width = c.height = 0; $('wv-mapinfo').textContent = '· click one above'; $('wv-picks').textContent = ''; return; }
  const m = area.maps[sel], s = 1;
  c.width = m.w * BLK; c.height = m.h * BLK;
  g.imageSmoothingEnabled = false;
  g.drawImage(drawMap(sel), 0, 0);
  overlays(g, m, 0, 0, s, true);
  // Each chunk cell: the chunk the generator picked.
  const names = allChunks().map((x) => x.name);
  g.font = '11px system-ui, sans-serif';
  for (let cy = 0; cy * 4 < m.h; cy++)
    for (let cx = 0; cx * 5 < m.w; cx++) {
      const k = m.picks[cy * 8 + cx], label = `${names[k] ?? k} (${ROLE_NAMES[m.roles[cy * 8 + cx]] ?? '?'})`;
      g.fillStyle = 'rgba(0,0,0,.6)';
      g.fillRect(cx * 80 + 2, cy * 64 + 2, g.measureText(label).width + 6, 14);
      g.fillStyle = '#fff';
      g.fillText(label, cx * 80 + 5, cy * 64 + 13);
    }
  const st = mapStats(m);
  $('wv-mapinfo').textContent = `· map ${sel} · ${m.w} x ${m.h} blocks · ${st.buildings} buildings · ${st.props} props · ${st.camps} camps · ${st.chests} chests${m.carved ? ' · paths carved' : ''}`;
  $('wv-picks').textContent = 'Chunk cells: the chunk picked (its role). Blue rock, red camp, yellow cache, green the start or exit mark.';
}

// ---------------------------------------------------------------------------
// The page
const seedVal = () => Math.max(0, Math.min(65535, +$('wv-seed').value | 0));
const areaSeed = () => (seedVal() + (+$('wv-depth').value) * DEPTH_STEP) & 0xffff;

function refresh() {
  if (!emu) return;
  const t0 = performance.now();
  mapCanvas.clear();
  area = buildArea(areaSeed());
  if (!area.maps[sel]) sel = area.start;
  drawArea();
  drawDetail();
  pill(`built in ${Math.round(performance.now() - t0)} ms`, 'ok');
}
const redraw = () => { mapCanvas.clear(); drawArea(); drawDetail(); };

function pill(text, kind = '') {
  const p = $('wv-pill');
  p.textContent = text;
  p.className = 'pill ' + kind;
}

$('wv-area').addEventListener('click', (e) => {
  const c = $('wv-area'), r = c.getBoundingClientRect(), s = +$('wv-zoom').value, { cw, ch } = layout();
  const px = (e.clientX - r.left) * (c.width / r.width) / (BLK * s), py = (e.clientY - r.top) * (c.height / r.height) / (BLK * s);
  const i = Math.floor(py / ch) * AREA_W + Math.floor(px / cw);
  if (area?.maps[i]) { sel = i; drawArea(); drawDetail(); }
});
for (const id of ['wv-art', 'wv-enemies', 'wv-chests', 'wv-grid', 'wv-links']) $(id).addEventListener('change', redraw);
$('wv-zoom').addEventListener('change', drawArea);
$('wv-seed').addEventListener('change', refresh);
$('wv-depth').addEventListener('change', refresh);
$('wv-prev').addEventListener('click', () => { $('wv-seed').value = Math.max(0, seedVal() - 1); refresh(); });
$('wv-next').addEventListener('click', () => { $('wv-seed').value = Math.min(65535, seedVal() + 1); refresh(); });
$('wv-rand').addEventListener('click', () => { $('wv-seed').value = Math.floor(Math.random() * 65536); refresh(); });
$('wv-theme').addEventListener('change', async () => { themeIdx = +$('wv-theme').value; await loadAtlas(); renderWeights(); refresh(); });
addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if (e.key === 'ArrowRight') $('wv-next').click();
  if (e.key === 'ArrowLeft') $('wv-prev').click();
});

// --- the survey: a run of seeds ---
$('wv-survey-run').addEventListener('click', async () => {
  const n = Math.max(5, Math.min(500, +$('wv-survey-n').value | 0)), from = seedVal(), depth = +$('wv-depth').value;
  const rows = [];
  pill('surveying…');
  for (let k = 0; k < n; k++) {
    const seed = (from + k) & 0xffff, a = buildArea((seed + depth * DEPTH_STEP) & 0xffff);
    const st = a.maps.filter(Boolean).map(mapStats);
    const sum = (key) => st.reduce((x, y) => x + y[key], 0);
    rows.push({ seed, maps: st.length, buildings: sum('buildings'), props: sum('props'), camps: sum('camps'), enemies: sum('enemies'), chests: sum('chests'), carved: sum('carved') });
    if (k % 10 === 9) { pill(`surveying… ${k + 1} of ${n}`); await new Promise((r) => setTimeout(r)); }
  }
  const avg = (key) => (rows.reduce((x, y) => x + y[key], 0) / rows.length).toFixed(1);
  const range = (key) => `${Math.min(...rows.map((r) => r[key]))}-${Math.max(...rows.map((r) => r[key]))}`;
  const keys = ['maps', 'buildings', 'props', 'camps', 'enemies', 'chests', 'carved'];
  const carved = rows.filter((r) => r.carved), sparse = [...rows].sort((a, b) => a.buildings - b.buildings).slice(0, 5);
  const link = (r) => `<a class="seed" data-seed="${r.seed}">${r.seed}</a>`;
  $('wv-survey').innerHTML = `<table><tr><th></th><th class="n">avg</th><th class="n">range</th></tr>
    ${keys.map((k) => `<tr><td>${k} an area</td><td class="n">${avg(k)}</td><td class="n">${range(k)}</td></tr>`).join('')}</table>
    <p>${carved.length ? `<span class="warn">${carved.length} of ${n} areas needed paths carved</span>: ${carved.slice(0, 12).map(link).join(', ')}` : 'No area needed paths carved.'}</p>
    <p>Fewest buildings: ${sparse.map((r) => `${link(r)} (${r.buildings})`).join(', ')}</p>`;
  for (const a of $('wv-survey').querySelectorAll('a.seed')) a.addEventListener('click', () => { $('wv-seed').value = a.dataset.seed; refresh(); });
  refresh();
});

// --- chunk weights ---
function renderWeights() {
  const f = files[themeIdx];
  $('wv-wfile').textContent = `· content/chunks/${f.file}`;
  const byTag = {};
  f.chunks.forEach((c, k) => { for (const t of tagNames(c.tags)) (byTag[t] ||= []).push(k); });
  $('wv-weights').innerHTML = Object.entries(byTag).map(([tag, ks]) => `<p class="muted" style="margin:6px 0 2px">${tag}</p><table>${ks.map((k) => {
    const c = f.chunks[k];
    return `<tr><td>${c.name}</td><td class="n"><input type="number" min="1" max="15" data-k="${k}" value="${c.weight}" style="width:56px"></td></tr>`;
  }).join('')}</table>`).join('');
  for (const inp of $('wv-weights').querySelectorAll('input')) inp.addEventListener('change', () => {
    const k = +inp.dataset.k, v = Math.max(1, Math.min(15, +inp.value | 0));
    for (const other of $('wv-weights').querySelectorAll(`input[data-k="${k}"]`)) other.value = v;  // (a chunk listed under several tags)
    f.chunks[k].weight = v;
    weightsChanged();
  });
  weightsChanged(false);
}

function weightsChanged(rebuild = true) {
  const f = files[themeIdx], dirty = f.chunks.some((c, k) => c.weight !== f.saved[k]);
  $('wv-save').disabled = $('wv-revert').disabled = !dirty;
  if (!rebuild) return;
  const errs = checkChunkSet(allChunks());
  if (errs.length) { pill(errs[0], 'err'); return; }
  reloadEmu();
  refresh();
  if (dirty) pill('weights changed (not saved)', '');
}

$('wv-revert').addEventListener('click', () => {
  const f = files[themeIdx];
  f.chunks.forEach((c, k) => { c.weight = f.saved[k]; });
  renderWeights();
  reloadEmu();
  refresh();
});
$('wv-save').addEventListener('click', async () => {
  const f = files[themeIdx];
  const doc = { ...f.doc, chunks: f.chunks.map((c) => ({ name: c.name, tags: tagNames(c.tags), weight: c.weight, rows: cellsToRows(c.cells) })) };
  if (!devServer) {  // (a static site can't write files: hand it over instead)
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([JSON.stringify(doc, null, 2) + '\n'], { type: 'application/json' }));
    a.download = f.file;
    a.click();
    URL.revokeObjectURL(a.href);
    pill(`downloaded ${f.file}: put it in content/chunks/`, 'ok');
    return;
  }
  try {
    const r = await fetch('/api/chunks/' + encodeURIComponent(f.file), { method: 'PUT', headers: { 'content-type': 'application/json' }, body: JSON.stringify(doc) });
    if (!r.ok) throw new Error(await r.text());
    f.doc = doc;
    f.saved = f.chunks.map((c) => c.weight);
    weightsChanged(false);
    pill('saved · the dev server is rebuilding the ROM', 'ok');
  } catch (e) {
    pill('save failed: ' + e.message, 'err');
  }
});

// --- start ---
(async () => {
  try {
    await loadFiles();
    if (!devServer) $('wv-save').textContent = '⬇ Download chunk file';
    files.forEach((f, i) => $('wv-theme').add(new Option(f.doc.theme || f.file, i)));
    themeIdx = Math.max(0, files.findIndex((f) => f.doc.theme === 'lunar'));
    $('wv-theme').value = themeIdx;
    for (let d = 1; d <= 9; d++) $('wv-depth').add(new Option(d, d));
    if (innerWidth < 700) $('wv-zoom').value = '0.25';  // (a phone: more of the area at once)
    const q = new URLSearchParams(location.search);
    if (q.get('seed')) $('wv-seed').value = q.get('seed');
    pill('booting the ROM…');
    await initCore();
    await loadAtlas();
    await bootRom();
    renderWeights();
    refresh();
  } catch (e) {
    pill(e.message, 'err');
    console.error(e);
  }
})();

// The dev server announces new builds: follow them (a saved chunk file, a theme change).
if (location.hostname === 'localhost' || location.hostname === '127.0.0.1') try {
  const es = new EventSource('/events');
  es.onmessage = async (e) => {
    const ev = JSON.parse(e.data);
    if (ev.type !== 'build' || !ev.ok || !bootState) return;
    await loadFiles();
    await loadAtlas();
    await bootRom();
    renderWeights();
    refresh();
    pill('ROM rebuilt', 'ok');
  };
} catch { /* no dev server */ }
