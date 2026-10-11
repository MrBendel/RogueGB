// Converts artwork PNGs into GBDK tile data at build time.
//
// Each entry in ASSETS becomes build/gen/<name>.c + <name>.h with
//   const uint8_t <name>_tiles[];   // 2bpp, 16 bytes per 8x8 tile
//   #define <NAME>_TILE_COUNT n
// Pixels map to the 4 DMG shades by brightness (lightest = 0, darkest = 3);
// transparent pixels become shade 0. Called by build.mjs; the generated files
// are only rewritten when a source PNG is newer.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const GEN_DIR = path.join(ROOT, 'build', 'gen');

export const ASSETS = [
  // ASCII 32..95: space, punctuation, digits, A-Z. GB Studio sheets start at
  // ASCII 32, 16 glyphs per row, so this is the first 4 rows.
  { name: 'font', src: 'artwork/Fonts for GB Studio/light/default caps thick/ascii.png', first: 0, count: 64 },
  // 3x3 dialog frame: corners, edges and fill, row-major.
  { name: 'frame', src: 'artwork/Fonts for GB Studio/light/frame.png' },
];

/** Minimal PNG decoder: 8-bit RGB/RGBA/gray/gray+alpha/indexed, non-interlaced. */
export function decodePng(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let pos = 8, width, height, depth, ctype, interlace, palette = null, trns = null;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      width = data.readUInt32BE(0); height = data.readUInt32BE(4);
      depth = data[8]; ctype = data[9]; interlace = data[12];
    } else if (type === 'PLTE') palette = data;
    else if (type === 'tRNS') trns = data;
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += 12 + len;
  }
  if (depth !== 8 || interlace) throw new Error(`unsupported PNG (depth ${depth}, interlace ${interlace})`);
  const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ctype];
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const px = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y++) {
    const f = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const v = raw[y * (stride + 1) + 1 + x];
      const a = x >= channels ? px[y * stride + x - channels] : 0;
      const b = y ? px[(y - 1) * stride + x] : 0;
      const c = x >= channels && y ? px[(y - 1) * stride + x - channels] : 0;
      let p;
      if (f === 0) p = 0;
      else if (f === 1) p = a;
      else if (f === 2) p = b;
      else if (f === 3) p = (a + b) >> 1;
      else {
        const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2 * c);
        p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      px[y * stride + x] = (v + p) & 0xff;
    }
  }
  const rgba = Buffer.alloc(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    const s = px.subarray(i * channels, i * channels + channels);
    let r, g, b, a = 255;
    if (ctype === 0) [r, g, b] = [s[0], s[0], s[0]];
    else if (ctype === 4) [r, g, b, a] = [s[0], s[0], s[0], s[1]];
    else if (ctype === 2) [r, g, b] = s;
    else if (ctype === 6) [r, g, b, a] = s;
    else {
      [r, g, b] = palette.subarray(s[0] * 3, s[0] * 3 + 3);
      if (trns && s[0] < trns.length) a = trns[s[0]];
    }
    rgba.set([r, g, b, a], i * 4);
  }
  return { width, height, rgba };
}

/** Slice an image into 8x8 tiles (row-major) and encode each as 2bpp. */
export function toTiles({ width, height, rgba }) {
  const lum = (i) => (rgba[i + 3] < 128 ? 255 : 0.299 * rgba[i] + 0.587 * rgba[i + 1] + 0.114 * rgba[i + 2]);
  // Rank the distinct brightness levels so any 4-color art maps cleanly.
  const levels = [...new Set(Array.from({ length: width * height }, (_, i) => Math.round(lum(i * 4))))].sort((a, b) => b - a);
  if (levels.length > 4) throw new Error(`image has ${levels.length} shades; expected at most 4`);
  const shadeOf = (v) => {
    if (levels.length === 2) return v === levels[0] ? 0 : 3;
    return levels.indexOf(v);
  };
  const tiles = [];
  for (let ty = 0; ty < height / 8; ty++) {
    for (let tx = 0; tx < width / 8; tx++) {
      const t = [];
      for (let y = 0; y < 8; y++) {
        let lo = 0, hi = 0;
        for (let x = 0; x < 8; x++) {
          const s = shadeOf(Math.round(lum(((ty * 8 + y) * width + tx * 8 + x) * 4)));
          lo |= (s & 1) << (7 - x);
          hi |= (s >> 1) << (7 - x);
        }
        t.push(lo, hi);
      }
      tiles.push(t);
    }
  }
  return tiles;
}

const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');

/** Regenerates stale assets. Returns the list of generated .c files. */
export function buildAssets({ quiet = false } = {}) {
  fs.mkdirSync(GEN_DIR, { recursive: true });
  const self = fs.statSync(fileURLToPath(import.meta.url)).mtimeMs;
  const out = [];
  for (const a of ASSETS) {
    const src = path.join(ROOT, a.src);
    const c = path.join(GEN_DIR, `${a.name}.c`);
    const h = path.join(GEN_DIR, `${a.name}.h`);
    out.push(c);
    const genTime = fs.existsSync(c) && fs.existsSync(h) ? fs.statSync(c).mtimeMs : 0;
    if (genTime > Math.max(fs.statSync(src).mtimeMs, self)) continue;
    if (!quiet) console.log(`[assets] ${a.src} -> build/gen/${a.name}.c`);
    const all = toTiles(decodePng(fs.readFileSync(src)));
    const tiles = all.slice(a.first ?? 0, (a.first ?? 0) + (a.count ?? all.length));
    const NAME = a.name.toUpperCase();
    fs.writeFileSync(h, [
      `// Generated by tools/assets.mjs from ${a.src}. Do not edit.`,
      `#ifndef GEN_${NAME}_H`, `#define GEN_${NAME}_H`, '#include <stdint.h>',
      `#define ${NAME}_TILE_COUNT ${tiles.length}`,
      `extern const uint8_t ${a.name}_tiles[];`, '#endif', '',
    ].join('\n'));
    fs.writeFileSync(c, [
      `// Generated by tools/assets.mjs from ${a.src}. Do not edit.`,
      `#include "${a.name}.h"`,
      `const uint8_t ${a.name}_tiles[] = {`,
      ...tiles.map((t) => '    ' + t.map(hex).join(', ') + ','),
      '};', '',
    ].join('\n'));
  }
  return out;
}
