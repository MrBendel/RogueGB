// Tilesets: full-colour pixel art cut into background tiles for the Color.
//
// content/tilesets/<name>.json names pieces of a sheet (rectangles of 16x16
// blocks, see ice.json). The Color gives each 8x8 tile one palette of 4 colours,
// and a scene has `palettes` of them (6: the HUD keeps 6 and 7), so the art's
// colours are fitted:
//   1. Transparent pixels become the backdrop colour; colours are rounded to the
//      Color's 15 bits.
//   2. Each tile's colours (if more than 4: the 4 that keep it closest).
//   3. Palettes start as those sets (a set inside another shares it) and are
//      merged two at a time, the pair that loses least, until `palettes` are left.
//      A merged palette keeps 4 of the art's own colours (no blended ones).
//   4. Each tile takes the palette that draws it best; tiles that match another
//      (also flipped) are stored once.
// Writes build/gen/tiles_<name>.c/.h (banked):
//   tiles_<name>_tiles[]   2bpp, TILES_<NAME>_TILE_COUNT of them
//   tiles_<name>_pals[]    palettes * 4 colours (RGB555)
//   tiles_<name>_blocks[]  per block, 4 tiles (TL TR BL BR) as tile indices ...
//   tiles_<name>_attrs[]   ... and their attributes (palette, flips)
//   #define TS_<NAME>_<PIECE> its first block (row-major), _W and _H in blocks
// and build/tileset-<name>.png: each piece as drawn on the Color beside the art.
//
//   node tools/tileset.mjs [name]   convert and report (also run by build.mjs)
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { decodePng, GEN_DIR } from './assets.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DIR = path.join(ROOT, 'content', 'tilesets');
export const MAX_TILES = 176;  // background tiles 80-255 (gfx.h SCENE_TILES)
const MAX_BLOCKS = 64;         // room.h VIS_BLOCKS

// --- colours: 0xRRGGBB, rounded to 5 bits a channel ---
const round5 = (v) => ((v >> 3) << 3) | (v >> 5);
const pack = (r, g, b) => (round5(r) << 16) | (round5(g) << 8) | round5(b);
const R = (c) => c >> 16, G = (c) => (c >> 8) & 255, B = (c) => c & 255;
const lum = (c) => 0.299 * R(c) + 0.587 * G(c) + 0.114 * B(c);
// "Redmean": a cheap distance closer to how different colours look than plain RGB.
function dist(a, b) {
  const rm = (R(a) + R(b)) / 2, dr = R(a) - R(b), dg = G(a) - G(b), db = B(a) - B(b);
  return (2 + rm / 256) * dr * dr + 4 * dg * dg + (2 + (255 - rm) / 256) * db * db;
}

/** A histogram's error drawn with these colours (each pixel to its nearest). */
function error(hist, colours) {
  let e = 0;
  for (const [c, n] of hist) {
    let best = Infinity;
    for (const p of colours) best = Math.min(best, dist(c, p));
    e += best * n;
  }
  return e;
}

/** The 4 (or fewer) of a histogram's colours that draw it best. */
function choose4(hist) {
  const cs = [...hist.keys()];
  if (cs.length <= 4) return { colours: cs, err: 0 };
  // Exhaustive for small sets; otherwise start from the heaviest and improve by swaps.
  if (cs.length <= 14) {
    let best = null, bestErr = Infinity;
    for (let a = 0; a < cs.length; a++)
      for (let b = a + 1; b < cs.length; b++)
        for (let c = b + 1; c < cs.length; c++)
          for (let d = c + 1; d < cs.length; d++) {
            const e = error(hist, [cs[a], cs[b], cs[c], cs[d]]);
            if (e < bestErr) bestErr = e, best = [cs[a], cs[b], cs[c], cs[d]];
          }
    return { colours: best, err: bestErr };
  }
  let pick = [...cs].sort((x, y) => hist.get(y) - hist.get(x)).slice(0, 4), err = error(hist, pick);
  for (let improved = true; improved;) {
    improved = false;
    for (let i = 0; i < 4; i++)
      for (const c of cs) {
        if (pick.includes(c)) continue;
        const t = pick.slice();
        t[i] = c;
        const e = error(hist, t);
        if (e < err) err = e, pick = t, improved = true;
      }
  }
  return { colours: pick, err };
}

const addHist = (a, b) => {
  const m = new Map(a);
  for (const [c, n] of b) m.set(c, (m.get(c) || 0) + n);
  return m;
};

export function convert(name) {
  const def = JSON.parse(fs.readFileSync(path.join(DIR, `${name}.json`), 'utf8'));
  const img = decodePng(fs.readFileSync(path.join(ROOT, def.src)));
  const [br, bg, bb] = def.backdrop;
  const pixel = (x, y) => {
    const i = (y * img.width + x) * 4, a = img.rgba[i + 3] / 255;
    const mix = (v, w) => Math.round(v * a + w * (1 - a));
    return pack(mix(img.rgba[i], br), mix(img.rgba[i + 1], bg), mix(img.rgba[i + 2], bb));
  };
  // The pieces' blocks, and their tiles' pixels.
  const pieces = [], blocks = [], tiles = [];
  for (const [piece, [x, y, w, h]] of Object.entries(def.pieces)) {
    if (x % 8 || y % 8 || w % 16 || h % 16) throw new Error(`${name}.${piece}: x, y on the 8 px grid; w, h in 16s`);
    pieces.push({ piece, first: blocks.length, w: w / 16, h: h / 16, x, y });
    for (let by = 0; by < h / 16; by++)
      for (let bx = 0; bx < w / 16; bx++) {
        const q = [];
        for (let ty = 0; ty < 2; ty++)
          for (let tx = 0; tx < 2; tx++) {
            const ox = x + bx * 16 + tx * 8, oy = y + by * 16 + ty * 8, px = [];
            for (let yy = 0; yy < 8; yy++) for (let xx = 0; xx < 8; xx++) px.push(pixel(ox + xx, oy + yy));
            q.push(tiles.length);
            tiles.push(px);
          }
        blocks.push(q);
      }
  }
  if (blocks.length > MAX_BLOCKS) throw new Error(`${name}: ${blocks.length} blocks; a scene has ${MAX_BLOCKS}`);
  const hists = tiles.map((px) => px.reduce((m, c) => m.set(c, (m.get(c) || 0) + 1), new Map()));
  const art = new Set(tiles.flat());

  // Palettes: each tile's best 4 colours, merged down to def.palettes.
  let pals = [];
  for (const h of hists) {
    const { colours } = choose4(h);
    const key = new Set(colours);
    const home = pals.find((p) => [...key].every((c) => p.set.has(c)));
    if (home) home.hist = addHist(home.hist, h);
    else pals.push({ set: key, hist: h });
  }
  // Fold sets that fit in another without loss; then merge the cheapest pairs.
  pals.sort((a, b) => b.set.size - a.set.size);
  for (let i = pals.length - 1; i >= 0; i--) {
    const j = pals.findIndex((p, k) => k !== i && p.set.size + [...pals[i].set].filter((c) => !p.set.has(c)).length <= 4);
    if (j >= 0) {
      for (const c of pals[i].set) pals[j].set.add(c);
      pals[j].hist = addHist(pals[j].hist, pals[i].hist);
      pals.splice(i, 1);
    }
  }
  pals = pals.map((p) => ({ hist: p.hist, ...choose4(p.hist) }));
  while (pals.length > def.palettes) {
    let best = null;
    for (let i = 0; i < pals.length; i++)
      for (let j = i + 1; j < pals.length; j++) {
        const hist = addHist(pals[i].hist, pals[j].hist), m = choose4(hist);
        const cost = m.err - pals[i].err - pals[j].err;
        if (!best || cost < best.cost) best = { i, j, cost, merged: { hist, ...m } };
      }
    pals.splice(best.j, 1);
    pals.splice(best.i, 1, best.merged);
  }
  // Index 0 the lightest, 3 the darkest (the convention the rest of the art uses).
  const palettes = pals.map((p) => [...p.colours].sort((a, b) => lum(b) - lum(a)));
  while (palettes.some((p) => p.length < 4)) palettes.find((p) => p.length < 4).push(palettes[0][palettes[0].length - 1]);

  // Each tile: its best palette, as colour indices; stored once (flips included).
  const stored = [], index = new Map(), out = [];
  const flip = (idx, fx, fy) => {
    const r = new Array(64);
    for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) r[y * 8 + x] = idx[(fy ? 7 - y : y) * 8 + (fx ? 7 - x : x)];
    return r.join('');
  };
  let totalErr = 0;
  for (const px of tiles) {
    let best = null;
    palettes.forEach((p, k) => {
      const e = error(hists[tiles.indexOf(px)], p);
      if (!best || e < best.e) best = { k, e };
    });
    totalErr += best.e;
    const p = palettes[best.k];
    const idx = px.map((c) => p.reduce((bi, pc, i) => (dist(c, pc) < dist(c, p[bi]) ? i : bi), 0));
    let found = null;
    for (const [fx, fy] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
      const k = flip(idx, fx, fy);
      if (index.has(k)) { found = { t: index.get(k), fx, fy }; break; }
    }
    if (!found) {
      index.set(idx.join(''), stored.length);
      found = { t: stored.length, fx: 0, fy: 0 };
      stored.push(idx);
    }
    out.push({ ...found, pal: best.k, idx });
  }
  return { name, def, pieces, blocks, palettes, tiles: out, stored, art, totalErr, img, maps: buildMaps(def, pieces) };
}

const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');
const rgb555 = (c) => (R(c) >> 3) | ((G(c) >> 3) << 5) | ((B(c) >> 3) << 10);

const rowsOf = (d, a) => Array.from({ length: d.h }, (_, y) => '    ' + a.slice(y * d.w, (y + 1) * d.w).join(', ') + ',');

// --- maps: rows of legend characters -> each block's type (BLK_*) and look ---
export function buildMaps(def, pieces) {
  const byName = Object.fromEntries(pieces.map((p) => [p.piece, p]));
  const maps = {};
  for (const [map, rows] of Object.entries(def.maps || {})) {
    const h = rows.length, w = rows[0].length;
    if (w > 32 || h > 32 || rows.some((r) => r.length !== w)) throw new Error(`map ${map}: rows of one length, at most 32 x 32`);
    const at = (x, y) => (x >= 0 && y >= 0 && x < w && y < h ? rows[y][x] : null);
    const type = [], vis = [];
    for (let y = 0; y < h; y++)
      for (let x = 0; x < w; x++) {
        const ch = rows[y][x], l = def.legend[ch];
        if (!l) throw new Error(`map ${map}: '${ch}' at ${x},${y} isn't in the legend`);
        let p = byName[l.piece];
        if (!p) throw new Error(`legend '${ch}': no piece ${l.piece}`);
        // A variant now and then (by position, so the map looks the same every time).
        if (l.vary && (x * 7 + y * 13) % l.vary[1] === 0) p = byName[l.vary[0]];
        // Where this block sits in its piece: from the start of its run of this character.
        let x0 = x, y0 = y, x1 = x;
        while (at(x0 - 1, y) === ch) x0--;
        while (at(x, y0 - 1) === ch) y0--;
        while (at(x1 + 1, y) === ch) x1++;
        let bx = (x - x0) % p.w;
        if (l.stretch && p.w > 1) bx = x === x0 ? 0 : x === x1 ? p.w - 1 : Math.min(1, p.w - 1);
        const by = (y - y0) % p.h;
        type.push(l.solid ? 1 : 0);
        vis.push(p.first + by * p.w + bx);
      }
    maps[map] = { w, h, type, vis };
  }
  return maps;
}

function emit(ts) {
  const { name, blocks, palettes, tiles, stored, pieces } = ts;
  const NAME = name.toUpperCase(), id = `tiles_${name}`;
  const bytes = stored.map((idx) => {
    const t = [];
    for (let y = 0; y < 8; y++) {
      let lo = 0, hi = 0;
      for (let x = 0; x < 8; x++) {
        lo |= (idx[y * 8 + x] & 1) << (7 - x);
        hi |= (idx[y * 8 + x] >> 1) << (7 - x);
      }
      t.push(lo, hi);
    }
    return t;
  });
  const h = [
    `// Generated by tools/tileset.mjs from content/tilesets/${name}.json. Do not edit.`,
    `#ifndef GEN_${id.toUpperCase()}_H`, `#define GEN_${id.toUpperCase()}_H`,
    '#include <stdint.h>', '#include <gb/gb.h>', `BANKREF_EXTERN(${id})`,
    `#define TILES_${NAME}_TILE_COUNT ${stored.length}`,
    `#define TILES_${NAME}_BLOCKS ${blocks.length}`,
    `#define TILES_${NAME}_PALETTES ${palettes.length}`,
    ...pieces.flatMap((p) => [
      `#define TS_${NAME}_${p.piece.toUpperCase()} ${p.first}`,
      ...(p.w > 1 || p.h > 1 ? [`#define TS_${NAME}_${p.piece.toUpperCase()}_W ${p.w}`, `#define TS_${NAME}_${p.piece.toUpperCase()}_H ${p.h}`] : []),
    ]),
    `extern const uint8_t ${id}_tiles[];`, `extern const uint16_t ${id}_pals[];`,
    `extern const uint8_t ${id}_blocks[];`, `extern const uint8_t ${id}_attrs[];`,
    ...Object.entries(ts.maps).flatMap(([m, d]) => [
      `#define TS_${NAME}_MAP_${m.toUpperCase()}_W ${d.w}`, `#define TS_${NAME}_MAP_${m.toUpperCase()}_H ${d.h}`,
      `extern const uint8_t ${id}_${m}_type[];  // each block's BLK_* (row-major)`,
      `extern const uint8_t ${id}_${m}_vis[];   // ... and its look (a block of this tileset)`,
    ]),
    '#endif', '',
  ].join('\n');
  const c = [
    '#pragma bank 255',
    `// Generated by tools/tileset.mjs from content/tilesets/${name}.json. Do not edit.`,
    '#include <gb/gb.h>', `#include "${id}.h"`, `BANKREF(${id})`,
    `const uint8_t ${id}_tiles[] = {`, ...bytes.map((t) => '    ' + t.map(hex).join(', ') + ','), '};',
    `const uint16_t ${id}_pals[] = {`, ...palettes.map((p) => '    ' + p.map((c) => '0x' + rgb555(c).toString(16).padStart(4, '0')).join(', ') + ','), '};',
    `const uint8_t ${id}_blocks[] = {`, ...blocks.map((q) => '    ' + q.map((t) => tiles[t].t).join(', ') + ','), '};',
    `const uint8_t ${id}_attrs[] = {`,
    ...blocks.map((q) => '    ' + q.map((t) => hex(tiles[t].pal | (tiles[t].fx ? 0x20 : 0) | (tiles[t].fy ? 0x40 : 0))).join(', ') + ','),
    '};',
    ...Object.entries(ts.maps).flatMap(([m, d]) => [
      `const uint8_t ${id}_${m}_type[] = {`, ...rowsOf(d, d.type), '};',
      `const uint8_t ${id}_${m}_vis[] = {`, ...rowsOf(d, d.vis), '};',
    ]),
    '',
  ].join('\n');
  fs.mkdirSync(GEN_DIR, { recursive: true });
  fs.writeFileSync(path.join(GEN_DIR, `${id}.h`), h);
  fs.writeFileSync(path.join(GEN_DIR, `${id}.c`), c);
  return path.join(GEN_DIR, `${id}.c`);
}

/** The pieces as the Color draws them (left) beside the art (right), 3x. */
function preview(ts) {
  const { pieces, blocks, palettes, tiles, img, def } = ts;
  const gap = 8, S = 3, col = 2 * 48 + gap * 3;
  const rowsH = pieces.map((p) => p.h * 16 + gap);
  const W = Math.max(col * 2, gap + palettes.length * 56), H =rowsH.reduce((a, b) => a + b, gap) / 2 + 64;
  const rgb = Buffer.alloc(W * H * 3, 40);
  const put = (x, y, c) => {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    const i = (y * W + x) * 3;
    rgb[i] = R(c); rgb[i + 1] = G(c); rgb[i + 2] = B(c);
  };
  let cx = gap, cy = gap, colH = 0, n = 0;
  for (const p of pieces) {
    if (cy + p.h * 16 > H - 40) { cx += col; cy = gap; }
    for (let by = 0; by < p.h; by++)
      for (let bx = 0; bx < p.w; bx++) {
        const q = blocks[p.first + by * p.w + bx];
        q.forEach((t, k) => {
          const tile = tiles[t], pal = palettes[tile.pal];
          for (let y = 0; y < 8; y++)
            for (let x = 0; x < 8; x++) {
              const ox = bx * 16 + (k & 1) * 8 + x, oy = by * 16 + (k >> 1) * 8 + y;
              put(cx + ox, cy + oy, pal[tile.idx[y * 8 + x]]);
              const ai = ((p.y + oy) * img.width + p.x + ox) * 4, a = img.rgba[ai + 3];
              put(cx + 48 + gap + ox, cy + oy, a ? pack(img.rgba[ai], img.rgba[ai + 1], img.rgba[ai + 2]) : pack(...def.backdrop));
            }
        });
      }
    cy += p.h * 16 + gap;
    n++;
  }
  // The palettes along the bottom.
  palettes.forEach((p, k) => p.forEach((c, i) => {
    for (let y = 0; y < 12; y++) for (let x = 0; x < 12; x++) put(gap + k * 56 + i * 12 + x, H - 20 + y, c);
  }));
  const raw = Buffer.alloc((W * S * 3 + 1) * H * S);
  for (let y = 0; y < H * S; y++)
    for (let x = 0; x < W * S; x++) {
      const si = (Math.floor(y / S) * W + Math.floor(x / S)) * 3, di = y * (W * S * 3 + 1) + 1 + x * 3;
      raw[di] = rgb[si]; raw[di + 1] = rgb[si + 1]; raw[di + 2] = rgb[si + 2];
    }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type, 'ascii'), data]), crc = Buffer.alloc(4);
    crc.writeUInt32BE(zlib.crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W * S, 0); ihdr.writeUInt32BE(H * S, 4); ihdr[8] = 8; ihdr[9] = 2;
  const png = Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
  const file = path.join(ROOT, 'build', `tileset-${ts.name}.png`);
  fs.writeFileSync(file, png);
  return file;
}

/** Every tileset whose json or sheet is newer than its output. */
export function buildTilesets({ quiet = false } = {}) {
  if (!fs.existsSync(DIR)) return [];
  const self = fs.statSync(fileURLToPath(import.meta.url)).mtimeMs, out = [];
  for (const f of fs.readdirSync(DIR).filter((f) => f.endsWith('.json'))) {
    const name = f.slice(0, -5), c = path.join(GEN_DIR, `tiles_${name}.c`);
    out.push(c);
    const def = JSON.parse(fs.readFileSync(path.join(DIR, f), 'utf8'));
    const newest = Math.max(self, fs.statSync(path.join(DIR, f)).mtimeMs, fs.statSync(path.join(ROOT, def.src)).mtimeMs);
    if (fs.existsSync(c) && fs.statSync(c).mtimeMs > newest) continue;
    const ts = convert(name);
    if (ts.stored.length > MAX_TILES) throw new Error(`tileset ${name}: ${ts.stored.length} tiles; a scene has ${MAX_TILES} (cut pieces, or make more of them repeat)`);
    emit(ts);
    const png = preview(ts);
    if (!quiet) console.log(`[tileset] ${name}: ${ts.blocks.length} blocks, ${ts.stored.length}/${MAX_TILES} tiles, ${ts.art.size} colours in ${ts.palettes.length} palettes -> ${path.relative(ROOT, png)}`);
  }
  return out;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const only = process.argv[2];
  for (const f of fs.readdirSync(DIR).filter((f) => f.endsWith('.json'))) {
    const name = f.slice(0, -5);
    if (only && only !== name) continue;
    const ts = convert(name);
    emit(ts);
    const png = preview(ts);
    console.log(`${name}: ${ts.blocks.length}/${MAX_BLOCKS} blocks, ${ts.stored.length}/${MAX_TILES} tiles (of ${ts.tiles.length}), ${ts.art.size} colours -> ${ts.palettes.length} palettes`);
    ts.palettes.forEach((p, k) => console.log(`  palette ${k}: ${p.map((c) => '#' + c.toString(16).padStart(6, '0')).join(' ')}`));
    console.log(`  preview: ${path.relative(ROOT, png)}`);
  }
}
