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
export const MAX_TILES = 176;

// Art packs (bought, not ours to publish: never in the repository) are named
// "pack:<folder>/<file>" and found in the first of: $ROGUEGB_ART_PACKS, the
// artist's Google Drive on this PC, artwork/packs/ (gitignored). Missing: stand-ins.
const PACK_ROOTS = [process.env.ROGUEGB_ART_PACKS, 'G:/My Drive/Artwork/Assets', path.join(ROOT, 'artwork', 'packs')].filter(Boolean);
export function artPath(src) {
  if (!src.startsWith('pack:')) return path.join(ROOT, src);
  const rel = src.slice(5);
  for (const r of PACK_ROOTS) if (fs.existsSync(path.join(r, rel))) return path.join(r, rel);
  return path.join(PACK_ROOTS[PACK_ROOTS.length - 1], rel);  // (missing: the stand-in path reports it)
}  // background tiles 80-255 (gfx.h SCENE_TILES)
const MAX_BLOCKS = 128;        // room.h VIS_BLOCKS

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

// A stand-in for art that isn't here (a local-only sheet, docs/design/lunar-base.md),
// by what the theme uses a piece for, so maps still read without the real tiles:
// ground light with a speck, pits black, anything solid dark and outlined.
function placeholderPixel(piece, role) {
  let h = 0;
  for (const ch of piece) h = (h * 31 + ch.charCodeAt(0)) & 0xffff;
  if (role === 'floor') {
    const fill = pack(200, 196, 176), speck = pack(150, 146, 130);
    return (lx, ly) => ((lx & 15) === (h & 15) && (ly & 15) === ((h >> 4) & 15) ? speck : fill);
  }
  if (role === 'pit') return () => pack(16, 16, 16);
  const fill = h & 1 ? pack(110, 110, 120) : pack(90, 90, 100), edge = pack(170, 170, 180), ink = pack(30, 30, 30);
  return (lx, ly) => ((lx & 15) === 15 || (ly & 15) === 15 ? ink : (lx & 15) === 0 || (ly & 15) === 0 ? edge : fill);
}

// What a theme draws a piece as: 'floor', 'pit' or 'solid' (for stand-ins).
function pieceRoles(def) {
  const roles = {}, th = def.theme || {};
  const names = (r) => (Array.isArray(r) ? r : r && typeof r === 'object' ? Object.entries(r).filter(([k]) => !['style', 'w', 'h'].includes(k)).flatMap(([, v]) => (Array.isArray(v) ? v : [])) : []);
  for (const [t, r] of Object.entries(th))
    names(r).forEach((n, i) => {  // (a door list's doorway is floor; its jambs are wall)
      roles[n.split('.')[0]] ??= t === 'floor' || t === 'prop' || (t.startsWith('door') && !i) ? 'floor' : t === 'pit' ? 'pit' : 'solid';
    });
  for (const l of Object.values(def.legend || {})) roles[l.piece] ??= l.solid ? 'solid' : 'floor';  // (pieces only maps use)
  for (const [n, p] of Object.entries(def.pieces)) if (p.prefab) roles[n] ??= 'solid';  // (set pieces)
  return roles;
}

// A set piece ("prefab": rows of block types, one character a block: "." ground,
// "#" solid, "r" a roof you walk behind) is a piece the generator stamps whole
// (level.c: buildings become set pieces). Its "r" blocks are walk-behind.
const PREFAB_TYPE = { '.': 0, '#': 1, r: 0 };  // room.h BLK_*
function blockBehind(spec, b, w) {
  if (spec.prefab) return spec.prefab[Math.floor(b / w)][b % w] === 'r';
  return !!spec.behind;
}
function prefabsOf(def, pieces) {
  return pieces.filter((p) => def.pieces[p.piece].prefab).map((p) => {
    const rows = def.pieces[p.piece].prefab;
    if (rows.length !== p.h || rows.some((r) => r.length !== p.w || /[^.#r]/.test(r))) throw new Error(`prefab ${p.piece}: ${p.w} x ${p.h} blocks of . # r`);
    return { ...p, types: rows.join('').split('').map((c) => PREFAB_TYPE[c]) };
  });
}

export function convert(name) {
  const def = JSON.parse(fs.readFileSync(path.join(DIR, `${name}.json`), 'utf8'));
  const srcPath = artPath(def.src);
  // (ROGUEGB_STANDIN_ART=1: build as if local-only sheets were missing, to record
  // the stand-in screens the public build shows.)
  const placeholder = !fs.existsSync(srcPath) || (!!def.local && !!process.env.ROGUEGB_STANDIN_ART);
  if (placeholder && !def.local) throw new Error(`tileset ${name}: ${def.src} is missing`);
  const img = placeholder ? null : decodePng(fs.readFileSync(srcPath));
  // A second sheet of decals to stamp over pieces (optional; local like the main one).
  const decals = !placeholder && def.decals ? decodePng(fs.readFileSync(artPath(def.decals))) : null;
  // More sheets to stamp from, by name ("sheets": { "containers": "artwork/tiles/containers.png" }):
  // a piece's "stamp_from" picks one (art shared by every world, like the containers).
  const sheets = {};
  for (const [k, f] of Object.entries(def.sheets || {})) if (!placeholder) sheets[k] = decodePng(fs.readFileSync(artPath(f)));
  const [br, bg, bb] = def.backdrop;
  const sheetPixel = (x, y) => {
    const i = (y * img.width + x) * 4, a = img.rgba[i + 3] / 255;
    const mix = (v, w) => Math.round(v * a + w * (1 - a));
    return pack(mix(img.rgba[i], br), mix(img.rgba[i + 1], bg), mix(img.rgba[i + 2], bb));
  };
  // The pieces' blocks, and their tiles' pixels.
  const pieces = [], blocks = [], tiles = [], roles = pieceRoles(def);
  for (const [piece, spec] of Object.entries(def.pieces)) {
    // Each piece reads its pixels at piece-local (lx, ly). Three forms:
    // [x, y, w, h]: a rectangle of the sheet.
    // { rect: [x, y, w, h], overlay: [[sx, sy, w, h, dx, dy], ...] }: other parts of
    //   the sheet drawn over it (their transparent pixels let it show).
    // { tiles: ["00 10x", "20 00xy"] }: 8x8 sheet tiles by column and row (hex), each
    //   optionally flipped (x, y); rows of codes, 2 per block across and down.
    // A tiles piece can add "stamp": rows of codes like its tiles, from the "decals"
    // sheet (or "stamp_from": one of the tileset's "sheets"), drawn over it ("..": none).
    // Any form can add "behind": true: drawn over sprites, and walkable in a generated
    // map (a roof you walk behind).
    let w, h, pixel, x = 0, y = 0;
    if (spec.tiles) {
      const rows = spec.tiles.map((r) => r.trim().split(/\s+/));
      h = rows.length * 8;
      w = rows[0].length * 8;
      if (w % 16 || h % 16 || rows.some((r) => r.length * 8 !== w)) throw new Error(`${name}.${piece}: tiles rows of one length, an even number of tiles each way`);
      const codes = rows.map((r) => r.map((c) => {
        if (!/^[0-9a-f]{2}(x|y|xy)?$/i.test(c)) throw new Error(`${name}.${piece}: bad tile code ${c}`);
        return { tx: parseInt(c[0], 16), ty: parseInt(c[1], 16), fx: c.includes('x'), fy: c.slice(2).includes('y') };
      }));
      // "stamp": the same grid of decal codes ("..": none), from the "decals" sheet:
      // their opaque pixels are drawn over the tiles (a number on a roof, stripes).
      const stamp = spec.stamp && spec.stamp.map((r) => r.trim().split(/\s+/).map((c) => (c === '..' ? null : {
        tx: parseInt(c[0], 16), ty: parseInt(c[1], 16), fx: c.slice(2).includes('x'), fy: c.slice(2).includes('y'),
      })));
      const from = spec.stamp_from ? sheets[spec.stamp_from] : decals;
      if (stamp && !from && !placeholder) throw new Error(`${name}.${piece}: a stamp needs "decals" or a "sheets" entry (${spec.stamp_from || 'decals'})`);
      pixel = placeholder ? placeholderPixel(piece, roles[piece]) : (lx, ly) => {
        const ix = lx & 7, iy = ly & 7, d = stamp && stamp[ly >> 3] && stamp[ly >> 3][lx >> 3];
        if (d) {
          const i = ((d.ty * 8 + (d.fy ? 7 - iy : iy)) * from.width + d.tx * 8 + (d.fx ? 7 - ix : ix)) * 4;
          if (from.rgba[i + 3] > 127) return pack(from.rgba[i], from.rgba[i + 1], from.rgba[i + 2]);
        }
        const t = codes[ly >> 3][lx >> 3];
        return sheetPixel(t.tx * 8 + (t.fx ? 7 - ix : ix), t.ty * 8 + (t.fy ? 7 - iy : iy));
      };
    } else {
      [x, y, w, h] = Array.isArray(spec) ? spec : spec.rect;
      if (x % 8 || y % 8 || w % 16 || h % 16) throw new Error(`${name}.${piece}: x, y on the 8 px grid; w, h in 16s`);
      if (placeholder) {
        pixel = placeholderPixel(piece, roles[piece]);
      } else {
        const over = new Map();
        for (const [sx, sy, ow, oh, dx, dy] of spec.overlay || [])
          for (let yy = 0; yy < oh; yy++)
            for (let xx = 0; xx < ow; xx++)
              if (img.rgba[((sy + yy) * img.width + sx + xx) * 4 + 3]) over.set((dy + yy) * 4096 + dx + xx, [sx + xx, sy + yy]);
        pixel = (lx, ly) => {
          const o = over.get(ly * 4096 + lx);
          return o ? sheetPixel(o[0], o[1]) : sheetPixel(x + lx, y + ly);
        };
      }
    }
    pieces.push({ piece, first: blocks.length, w: w / 16, h: h / 16, pixel });
    for (let by = 0; by < h / 16; by++)
      for (let bx = 0; bx < w / 16; bx++) {
        const q = [];
        for (let ty = 0; ty < 2; ty++)
          for (let tx = 0; tx < 2; tx++) {
            const ox = bx * 16 + tx * 8, oy = by * 16 + ty * 8, px = [];
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
  return { name, def, pieces, blocks, palettes, tiles: out, stored, art, totalErr, img, placeholder, maps: buildMaps(def, pieces), theme: buildTheme(def, pieces) };
}

const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');
const rgb555 = (c) => (R(c) >> 3) | ((G(c) >> 3) << 5) | ((B(c) >> 3) << 10);

const chunk = (a, n = 32) => Array.from({ length: Math.ceil(a.length / n) }, (_, i) => '    ' + a.slice(i * n, (i + 1) * n).join(', ') + ',');
const rowsOf =(d, a) => Array.from({ length: d.h }, (_, y) => '    ' + a.slice(y * d.w, (y + 1) * d.w).join(', ') + ',');

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
        type.push(l.solid ? 1 : 0);
        // A block of another piece's run ("run": "R", a sign on a roof): itself, but
        // counted as that character, so the run around it still lines up.
        if (l.run) {
          vis.push(p.first);
          continue;
        }
        // Where this block sits in its piece: from the start of its run of this character.
        const same = (c) => c === ch || (c !== null && def.legend[c]?.run === ch);
        let x0 = x, y0 = y, x1 = x;
        while (same(at(x0 - 1, y))) x0--;
        while (same(at(x, y0 - 1))) y0--;
        while (same(at(x1 + 1, y))) x1++;
        let bx = (x - x0) % p.w;
        if (l.stretch && p.w > 1) bx = x === x0 ? 0 : x === x1 ? p.w - 1 : Math.min(1, p.w - 1);
        const by = (y - y0) % p.h;
        vis.push(p.first + by * p.w + bx);
      }
    maps[map] = { w, h, type, vis };
  }
  return maps;
}

// --- themes: how generated maps look (docs/design/level-generation.md, "Visuals") ---
//
// The generator places block types (room.h BLK_*); a theme says which of the
// tileset's blocks draws each one, from its neighbours. In the tileset file:
//   "theme": {
//     "floor": ["ground_a", "ground_a", "crater"],   a list: variants (repeats weigh)
//     "solid": { "style": "module", "roof_c": [...], "face_l": [...], ... },
//     "raised": "solid",                                 the same rules as another type
//   }
// Missing types draw as floor. Styles:
//   module: buildings seen from the front: roof rows (roof_tl roof_t roof_tr / roof_l
//     roof_c roof_r / roof_bl roof_b roof_br, the bottom row carrying the roof's lower
//     edge; roof_sl roof_s roof_sr for a roof only one row deep, both edges), then one
//     row of front (face_l face_c face_r) where the block below isn't solid. A lone
//     block, or a one-block-wide column, is a prop.
//   shade: { style: "shade", blocks, under }: under where the block above isn't the
//     same type (a wall's shadow on the floor below it), else blocks.
//   pattern: { style: "pattern", w, h, blocks }: blocks in a w x h grid (powers of two)
//     by map position, row by row (a floor of plates that must line up).
// A piece's blocks are named piece (its first) or piece.n (the nth, row-major).
// Compiled per type to 32 entries by neighbour mask: N 1, E 2, S 4, W 8, and 16
// for the block two below being the same type (the roof's bottom row knows it's
// the bottom).
// room.h's BLK_* in order, then "edge": solid blocks on the map's rim (the theme's
// "solid" rules unless it gives the edge its own).
export const BLOCK_TYPES = ['floor', 'solid', 'pit', 'raised', 'prop', 'stairs', 'deck', 'edge'];
const EDGE = BLOCK_TYPES.indexOf('edge');
const MODULE_FALLBACK = {
  roof_sl: 'roof_bl', roof_sr: 'roof_br', roof_s: 'roof_b',
  roof_tl: 'roof_l', roof_tr: 'roof_r', roof_t: 'roof_c', roof_bl: 'roof_l', roof_br: 'roof_r', roof_b: 'roof_c',
  roof_l: 'roof_c', roof_r: 'roof_c', face_l: 'face_c', face_r: 'face_c', column: 'prop', face_c: 'roof_c', roof_c: 'prop',
};

function moduleRole(m) {
  const N = m & 1, E = m & 2, S = m & 4, W = m & 8, S2 = m & 16;
  if (!E && !W) return !N && !S ? 'prop' : 'column';
  if (!S) return !W ? 'face_l' : !E ? 'face_r' : 'face_c';
  if (!S2) {
    // One roof row (the front below it): its top edge and its bottom edge in one block.
    if (!N) return !W ? 'roof_sl' : !E ? 'roof_sr' : 'roof_s';
    return !W ? 'roof_bl' : !E ? 'roof_br' : 'roof_b';
  }
  if (!N) return !W ? 'roof_tl' : !E ? 'roof_tr' : 'roof_t';
  return !W ? 'roof_l' : !E ? 'roof_r' : 'roof_c';
}

export function buildTheme(def, pieces) {
  if (!def.theme) return null;
  const byName = Object.fromEntries(pieces.map((p) => [p.piece, p]));
  const block = (ref) => {
    const [name, n] = ref.split('.');
    const p = byName[name];
    if (!p) throw new Error(`theme: no piece ${name}`);
    const i = n === undefined ? 0 : +n;
    if (i >= p.w * p.h) throw new Error(`theme: ${ref} is past the piece's ${p.w * p.h} blocks`);
    return p.first + i;
  };
  const rulesOf = (t, seen = []) => {
    let r = def.theme[t];
    if (r === undefined) r = t === 'floor' ? null : t === 'edge' ? 'solid' : 'floor';
    if (typeof r === 'string') {
      if (seen.includes(r)) throw new Error(`theme: ${t} refers back to itself`);
      return rulesOf(r, [...seen, t]);
    }
    if (r === null) throw new Error('theme: needs "floor"');
    return r;
  };
  const lists = [];  // [type * 32 + mask] -> block list
  const patternW = [], patternH = [];  // per type: a pattern by position (0: none)
  for (const t of BLOCK_TYPES) {
    const r = rulesOf(t);
    const pattern = !Array.isArray(r) && r.style === 'pattern';
    if (pattern) {
      const pw = r.w, ph = r.h, pow2 = (n) => n > 0 && (n & (n - 1)) === 0;
      if (!pow2(pw) || !pow2(ph) || r.blocks.length !== pw * ph) throw new Error(`theme ${t}: a pattern is w x h blocks (powers of two), row by row`);
    }
    patternW.push(pattern ? r.w : 0);
    patternH.push(pattern ? r.h : 0);
    for (let m = 0; m < 32; m++) {
      let list;
      if (Array.isArray(r)) {
        list = r;
      } else if (r.style === 'shade') {
        list = m & 1 ? r.blocks : r.under;  // (N: the block above is the same type)
      } else if (pattern) {
        list = r.blocks;  // (picked by position, not by the neighbours)
      } else if (r.style === 'module') {
        let role = moduleRole(m);
        while (!r[role]) {
          role = MODULE_FALLBACK[role];
          if (!role) throw new Error(`theme ${t}: a module style needs "prop" at least`);
        }
        list = r[role];
      } else {
        throw new Error(`theme ${t}: a list of pieces, or { style: "module" | "pattern" | "shade", ... }`);
      }
      lists.push(list.map(block));
    }
  }
  // Doorways: a one-block gap in the map's outer wall, by the side it's on (an
  // entrance room's doors): "door_n" "door_e" "door_s" "door_w": [the doorway, and
  // optionally the wall blocks either side of it (its jambs): left then right, or
  // above then below].
  for (const side of ['door_n', 'door_e', 'door_s', 'door_w']) lists.push((def.theme[side] || []).map(block));
  // A frame for the rim (an interior's walls): the look by where on the rim a block
  // is, over the edge rules: "frame": { "n", "s", "e", "w", "nw", "ne", "sw", "se" }.
  // A side with 2 or 4 blocks takes them in turn along it (to line up with a pattern).
  const FRAME = ['n', 'e', 's', 'w', 'nw', 'ne', 'sw', 'se'];
  for (const side of FRAME) {
    const l = (def.theme.frame || {})[side] || [];
    if (![0, 1, 2, 4].includes(l.length)) throw new Error(`theme frame.${side}: 1, 2 or 4 blocks`);
    lists.push(l.map(block));
  }
  // One flat list; each entry a start and a count into it (shared where equal).
  const flat = [], start = [], count = [], seenAt = new Map();
  for (const l of lists) {
    const key = l.join(',');
    if (!seenAt.has(key)) {
      seenAt.set(key, flat.length);
      flat.push(...l);
    }
    if (flat.length > 255) throw new Error('theme: more than 255 entries in all');
    start.push(seenAt.get(key));
    count.push(l.length);
  }
  const types = BLOCK_TYPES.length * 32;
  return {
    flat, start: start.slice(0, types), count: count.slice(0, types),
    doorStart: start.slice(types, types + 4), doorCount: count.slice(types, types + 4),
    frameStart: start.slice(types + 4), frameCount: count.slice(types + 4), patternW, patternH,
  };
}

// The generated loader and look-filler (in the tileset's own bank, beside its tables).
function themeSource(id, NAME) {
  return `
// Load this theme: its tiles, palettes and blocks (the display off, or in a fade).
void ${id}_load(void) BANKED {
    set_bkg_data(SCENE_TILES, TILES_${NAME}_TILE_COUNT, ${id}_tiles);
    memcpy(pal_base_bg, ${id}_pals, TILES_${NAME}_PALETTES * 4 * sizeof(uint16_t));
    for (uint8_t v = 0; v < VIS_BLOCKS; v++)
        for (uint8_t q = 0; q < 4; q++) {
            uint16_t b = v < TILES_${NAME}_BLOCKS ? (uint16_t)v * 4 + q : q;
            ROOM_TILE(v, q) = SCENE_TILES + ${id}_blocks[b];
            ROOM_ATTR(v, q) = ${id}_attrs[b];
        }
    room_attr_uniform = 0;
}

// A block's type for the rules: solid on the map's rim (and off the map) is the
// edge, its own kind, so a building against the rim keeps its own edges.
static uint8_t ${id}_type(uint8_t x, uint8_t y) {
    if (x >= room_w || y >= room_h) return ${EDGE};  // (off the left or top wraps to 255)
    uint8_t t = room_map[((uint16_t)y << 5) | x] & 0x7F;
    if (t >= ${EDGE}) return 0;
    if (t == BLK_SOLID && (!x || !y || x == room_w - 1 || y == room_h - 1)) return ${EDGE};
    return t;
}

// Each block's look from its type and its neighbours' (tools/tileset.mjs, themes).
void ${id}_apply(void) BANKED {
    for (uint8_t y = 0; y < room_h; y++) {
        for (uint8_t x = 0; x < room_w; x++) {
            uint16_t k = ((uint16_t)y << 5) | x;
            uint8_t t = ${id}_type(x, y), m = 0;
            if (${id}_type(x, y - 1) == t) m |= 1;
            if (${id}_type(x + 1, y) == t) m |= 2;
            if (${id}_type(x, y + 1) == t) m |= 4;
            if (${id}_type(x - 1, y) == t) m |= 8;
            if (${id}_type(x, y + 2) == t) m |= 16;
            uint8_t r = (t << 5) | m, n = ${id}_rule_count[r], v = 0;
            if (${id}_pattern_w[t]) {  // a pattern: by position
                v = (uint8_t)((y & (${id}_pattern_h[t] - 1)) * ${id}_pattern_w[t] + (x & (${id}_pattern_w[t] - 1)));
            } else if (n > 1) {
                // (A mixing hash: neighbours mustn't fall into rows or diagonals.)
                uint16_t z = (uint16_t)x * 0x9E37u ^ (uint16_t)y * 0x7F4Bu;
                z ^= z >> 7;
                z *= 0x2C1Bu;
                z ^= z >> 9;
                uint8_t h = (uint8_t)z;
                while (h >= n) h -= n;
                v = h;
            }
            room_vis[k] = ${id}_rule_list[${id}_rule_start[r] + v];
        }
    }
    // The frame: the rim's looks by where on it a block is (an interior's walls).
    for (uint8_t y = 0; y < room_h; y++) {
        for (uint8_t x = 0; x < room_w; x++) {
            uint8_t top = !y, bottom = y + 1 == room_h, left = !x, right = x + 1 == room_w, s;
            if (!(top | bottom | left | right)) continue;
            if (${id}_type(x, y) != ${EDGE}) continue;
            s = top ? (left ? 4 : right ? 5 : 0) : bottom ? (left ? 6 : right ? 7 : 2) : right ? 1 : 3;
            uint8_t n = ${id}_frame_count[s];
            if (n) room_vis[((uint16_t)y << 5) | x] = ${id}_rule_list[${id}_frame_start[s] + (((s == 1 || s == 3) ? y : x) & (n - 1))];
        }
    }
    // Doorways: a floor block on the rim with the rim solid to either side, drawn
    // with its side's look (if the theme has one).
    for (uint8_t y = 0; y < room_h; y++) {
        for (uint8_t x = 0; x < room_w; x++) {
            uint8_t s;
            if (!y) s = 0; else if (x + 1 == room_w) s = 1; else if (y + 1 == room_h) s = 2; else if (!x) s = 3; else continue;
            uint16_t k = ((uint16_t)y << 5) | x;
            if (room_map[k] != BLK_FLOOR || !${id}_door_count[s]) continue;
            uint8_t a, b;  // the blocks either side along the wall
            if (s & 1) a = ${id}_type(x, y - 1), b = ${id}_type(x, y + 1);
            else a = ${id}_type(x - 1, y), b = ${id}_type(x + 1, y);
            if (a != ${EDGE} || b != ${EDGE}) continue;
            uint8_t d = ${id}_door_start[s];
            room_vis[k] = ${id}_rule_list[d];
            if (${id}_door_count[s] < 3) continue;
            uint8_t step = s & 1 ? 32 : 1;  // the jambs: along the wall
            room_vis[k - step] = ${id}_rule_list[d + 1];
            room_vis[k + step] = ${id}_rule_list[d + 2];
        }
    }
    // Blocks you walk behind (a building's roof, drawn over sprites): floor to walk
    // on, once every look is chosen (the rules read the solid shapes). Not the rim.
    for (uint8_t y = 1; y + 1 < room_h; y++) {
        for (uint8_t x = 1; x + 1 < room_w; x++) {
            uint16_t k = ((uint16_t)y << 5) | x;
            if (room_map[k] == BLK_SOLID && ${id}_walk[room_vis[k]]) room_map[k] = BLK_FLOOR;
        }
    }
}
`;
}

function emit(ts) {
  const { name, blocks, palettes, tiles, stored, pieces } = ts;
  // Blocks of "behind" pieces: drawn over sprites (the attribute's priority bit), and
  // walkable in a generated map (the theme's apply).
  const behind = new Array(blocks.length).fill(0);
  for (const p of pieces) for (let b = 0; b < p.w * p.h; b++) if (blockBehind(ts.def.pieces[p.piece], b, p.w)) behind[p.first + b] = 1;
  const NAME = name.toUpperCase(), id = `tiles_${name}`, prefabs = prefabsOf(ts.def, pieces);
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
    `#define TILES_${NAME}_STAND_IN ${ts.placeholder ? 1 : 0}  // 1: the sheet wasn't here (a local-only pack): plain stand-in tiles`,
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
    ...(prefabs.length ? [
      `#define TILES_${NAME}_PREFABS ${prefabs.length}`,
      `#define TILES_${NAME}_PF_SIZES {${prefabs.map((p) => `${p.w}, ${p.h}`).join(', ')}}  // w, h in blocks, by preference`,
      `void ${id}_stamp(uint8_t pf, uint8_t x, uint8_t y) BANKED;  // a set piece's blocks and types, its top left at x, y`,
    ] : []),
    ...(ts.theme ? [
      `void ${id}_load(void) BANKED;   // tiles, palettes and blocks, for a generated map`,
      `void ${id}_apply(void) BANKED;  // each block's look from the theme's rules (after a map is built)`,
    ] : []),
    '#endif', '',
  ].join('\n');
  const c = [
    '#pragma bank 255',
    `// Generated by tools/tileset.mjs from content/tilesets/${name}.json. Do not edit.`,
    '#include <gb/gb.h>', '#include <string.h>', '#include "room.h"', '#include "gfx.h"', `#include "${id}.h"`, `BANKREF(${id})`,
    `const uint8_t ${id}_tiles[] = {`, ...bytes.map((t) => '    ' + t.map(hex).join(', ') + ','), '};',
    `const uint16_t ${id}_pals[] = {`, ...palettes.map((p) => '    ' + p.map((c) => '0x' + rgb555(c).toString(16).padStart(4, '0')).join(', ') + ','), '};',
    `const uint8_t ${id}_blocks[] = {`, ...blocks.map((q) => '    ' + q.map((t) => tiles[t].t).join(', ') + ','), '};',
    `const uint8_t ${id}_attrs[] = {`,
    ...blocks.map((q, b) => '    ' + q.map((t) => hex(tiles[t].pal | (tiles[t].fx ? 0x20 : 0) | (tiles[t].fy ? 0x40 : 0) | (behind[b] ? 0x80 : 0))).join(', ') + ','),
    '};',
    ...Object.entries(ts.maps).flatMap(([m, d]) => [
      `const uint8_t ${id}_${m}_type[] = {`, ...rowsOf(d, d.type), '};',
      `const uint8_t ${id}_${m}_vis[] = {`, ...rowsOf(d, d.vis), '};',
    ]),
    ...(prefabs.length ? [
      `static const uint8_t ${id}_pf_first[] = {${prefabs.map((p) => p.first).join(', ')}};`,
      `static const uint8_t ${id}_pf_w[] = {${prefabs.map((p) => p.w).join(', ')}};`,
      `static const uint16_t ${id}_pf_at[] = {${prefabs.map((p, i) => prefabs.slice(0, i).reduce((n, q) => n + q.types.length, 0)).join(', ')}};`,
      `static const uint8_t ${id}_pf_h[] = {${prefabs.map((p) => p.h).join(', ')}};`,
      `static const uint8_t ${id}_pf_types[] = {`, ...chunk(prefabs.flatMap((p) => p.types)), '};',
      `void ${id}_stamp(uint8_t pf, uint8_t x, uint8_t y) BANKED {`,
      `    const uint8_t *t = ${id}_pf_types + ${id}_pf_at[pf];`,
      `    uint8_t v = ${id}_pf_first[pf];`,
      `    for (uint8_t j = 0; j < ${id}_pf_h[pf]; j++) {`,
      `        uint16_t k = ((uint16_t)(y + j) << 5) | x;`,
      `        for (uint8_t i = 0; i < ${id}_pf_w[pf]; i++, k++) {`,
      `            room_map[k] = *t++;`,
      `            room_vis[k] = v++;`,
      `        }`,
      `    }`,
      `}`,
    ] : []),
    ...(ts.theme ? [
      `static const uint8_t ${id}_rule_start[] = {`, ...chunk(ts.theme.start), '};',
      `static const uint8_t ${id}_rule_count[] = {`, ...chunk(ts.theme.count), '};',
      `static const uint8_t ${id}_rule_list[] = {`, ...chunk(ts.theme.flat), '};',
      `static const uint8_t ${id}_walk[] = {`, ...chunk(behind), '};',
      `static const uint8_t ${id}_door_start[] = {${ts.theme.doorStart.join(', ')}};  // N E S W`,
      `static const uint8_t ${id}_door_count[] = {${ts.theme.doorCount.join(', ')}};`,
      `static const uint8_t ${id}_frame_start[] = {${ts.theme.frameStart.join(', ')}};  // N E S W, NW NE SW SE`,
      `static const uint8_t ${id}_frame_count[] = {${ts.theme.frameCount.join(', ')}};`,
      `static const uint8_t ${id}_pattern_w[] = {${ts.theme.patternW.join(', ')}};  // per block type (0: no pattern)`,
      `static const uint8_t ${id}_pattern_h[] = {${ts.theme.patternH.join(', ')}};`,
      themeSource(id, NAME),
    ] : []),
    '',
  ].join('\n');
  fs.mkdirSync(GEN_DIR, { recursive: true });
  fs.writeFileSync(path.join(GEN_DIR, `${id}.h`), h);
  fs.writeFileSync(path.join(GEN_DIR, `${id}.c`), c);
  return path.join(GEN_DIR, `${id}.c`);
}

/** For the web tools (web/world.html): every block in a row of 16x16 cells, as the
 * Color draws it, and atlas-<name>.json: each block's name and whether you walk
 * behind it. */
function writeAtlas(ts) {
  const { name, blocks, palettes, tiles, pieces, def } = ts;
  const W = blocks.length * 16, rgb = Buffer.alloc(W * 16 * 3);
  blocks.forEach((q, b) => q.forEach((t, k) => {
    const tile = tiles[t], pal = palettes[tile.pal];
    for (let y = 0; y < 8; y++)
      for (let x = 0; x < 8; x++) {
        const c = pal[tile.idx[y * 8 + x]], i = ((((k >> 1) * 8 + y) * W) + b * 16 + (k & 1) * 8 + x) * 3;
        rgb[i] = R(c); rgb[i + 1] = G(c); rgb[i + 2] = B(c);
      }
  }));
  const names = [], behind = [];
  for (const p of pieces)
    for (let b = 0; b < p.w * p.h; b++) {
      names[p.first + b] = p.w * p.h > 1 ? `${p.piece}.${b}` : p.piece;
      behind[p.first + b] = blockBehind(def.pieces[p.piece], b, p.w) ? 1 : 0;
    }
  const raw = Buffer.alloc((W * 3 + 1) * 16);
  for (let y = 0; y < 16; y++) rgb.copy(raw, y * (W * 3 + 1) + 1, y * W * 3, (y + 1) * W * 3);
  const chunkOf = (type, data) => {
    const len = Buffer.alloc(4), crc = Buffer.alloc(4), td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    len.writeUInt32BE(data.length);
    crc.writeUInt32BE(zlib.crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(16, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(path.join(GEN_DIR, `atlas-${name}.png`), Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunkOf('IHDR', ihdr), chunkOf('IDAT', zlib.deflateSync(raw)), chunkOf('IEND', Buffer.alloc(0))]));
  fs.writeFileSync(path.join(GEN_DIR, `atlas-${name}.json`), JSON.stringify({ name, standIn: ts.placeholder, names, behind }) + '\n');
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
              put(cx + 48 + gap + ox, cy + oy, p.pixel(ox, oy));
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
  const self = fs.statSync(fileURLToPath(import.meta.url)).mtimeMs, out = [], standIn = [];
  for (const f of fs.readdirSync(DIR).filter((f) => f.endsWith('.json'))) {
    const name = f.slice(0, -5), c = path.join(GEN_DIR, `tiles_${name}.c`), h = path.join(GEN_DIR, `tiles_${name}.h`);
    out.push(c);
    const def = JSON.parse(fs.readFileSync(path.join(DIR, f), 'utf8'));
    const src = artPath(def.src);
    const missing = !fs.existsSync(src) || (!!def.local && !!process.env.ROGUEGB_STANDIN_ART);
    if (missing) standIn.push(name);
    const newest = Math.max(self, fs.statSync(path.join(DIR, f)).mtimeMs, missing ? 0 : fs.statSync(src).mtimeMs);
    // (Rebuilt too when the art came or went since the last build.)
    const was = fs.existsSync(h) && /_STAND_IN 1/.test(fs.readFileSync(h, 'utf8'));
    if (fs.existsSync(c) && fs.statSync(c).mtimeMs > newest && was === missing) continue;
    const ts = convert(name);
    if (ts.stored.length > MAX_TILES) throw new Error(`tileset ${name}: ${ts.stored.length} tiles; a scene has ${MAX_TILES} (cut pieces, or make more of them repeat)`);
    emit(ts);
    writeAtlas(ts);
    const png = preview(ts);
    if (!quiet) console.log(`[tileset] ${name}: ${ts.blocks.length} blocks, ${ts.stored.length}/${MAX_TILES} tiles, ${ts.art.size} colours in ${ts.palettes.length} palettes${missing ? ' (stand-in art: the local-only sheet is missing)' : ''} -> ${path.relative(ROOT, png)}`);
  }
  // Which tilesets are stand-ins in this build (the tests keep separate screens for them).
  fs.writeFileSync(path.join(GEN_DIR, 'art-mode.json'), JSON.stringify({ standIn }) + '\n');
  return out;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const only = process.argv[2];
  for (const f of fs.readdirSync(DIR).filter((f) => f.endsWith('.json'))) {
    const name = f.slice(0, -5);
    if (only && only !== name) continue;
    const ts = convert(name);
    emit(ts);
    writeAtlas(ts);
    const png = preview(ts);
    console.log(`${name}: ${ts.blocks.length}/${MAX_BLOCKS} blocks, ${ts.stored.length}/${MAX_TILES} tiles (of ${ts.tiles.length}), ${ts.art.size} colours -> ${ts.palettes.length} palettes`);
    ts.palettes.forEach((p, k) => console.log(`  palette ${k}: ${p.map((c) => '#' + c.toString(16).padStart(6, '0')).join(' ')}`));
    console.log(`  preview: ${path.relative(ROOT, png)}`);
  }
}
