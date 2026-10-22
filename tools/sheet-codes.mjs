// Reading sprite sheets by their 8x8 tiles (the codes tileset pieces use: column
// then row in hex, "x"/"y" for flips; docs/design/lunarbase-sprites.md).
//
//   node tools/sheet-codes.mjs label <sheet.png> [out.png]
//       the sheet enlarged on its tile grid, each tile labelled with its code
//   node tools/sheet-codes.mjs map <sheet.png> <scene.png>
//       which sheet tile (and flip) each 8x8 tile of a scene is: how an example
//       scene is built from the sheet ("??" where it isn't a sheet tile: a decal)
//   node tools/sheet-codes.mjs compose <sheet.png> <codes.txt> <out.png> [decals.png]
//       a mockup from rows of codes ("..": empty; "+" joins a decal: "13+00")
//
// Output images go to build/ unless a path is given.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { decodePng } from './assets.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const load = (f) => decodePng(fs.readFileSync(f));

function encodePng(width, height, rgb) {
  const raw = Buffer.alloc((width * 3 + 1) * height);
  for (let y = 0; y < height; y++) rgb.copy(raw, y * (width * 3 + 1) + 1, y * width * 3, (y + 1) * width * 3);
  const chunk = (type, data) => {
    const len = Buffer.alloc(4), crc = Buffer.alloc(4), td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    len.writeUInt32BE(data.length);
    crc.writeUInt32BE(zlib.crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0); ihdr.writeUInt32BE(height, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// A 3x5 font for the labels (hex digits, x, y, ?).
const FONT = {
  0: '111101101101111', 1: '010110010010111', 2: '111001111100111', 3: '111001111001111', 4: '101101111001001',
  5: '111100111001111', 6: '111100111101111', 7: '111001010010010', 8: '111101111101111', 9: '111101111001111',
  a: '010101111101101', b: '110101110101110', c: '011100100100011', d: '110101101101110', e: '111100110100111',
  f: '111100110100100', x: '101101010101101', y: '101101010010010', '?': '111001011000010',
};

/** A tile's pixels (RGBA), flipped as asked, as a string key for matching. */
function tileKey(img, tx, ty, fx = false, fy = false) {
  let k = '';
  for (let y = 0; y < 8; y++)
    for (let x = 0; x < 8; x++) {
      const sx = tx * 8 + (fx ? 7 - x : x), sy = ty * 8 + (fy ? 7 - y : y), i = (sy * img.width + sx) * 4;
      k += img.rgba[i + 3] < 128 ? '-' : img.rgba.subarray(i, i + 3).join(',') + ';';
    }
  return k;
}
const code = (tx, ty, f = '') => tx.toString(16) + ty.toString(16) + f;

function label(sheetFile, out) {
  const img = load(sheetFile), S = 6, W = img.width * S, H = img.height * S, rgb = Buffer.alloc(W * H * 3);
  const set = (x, y, r, g, b) => { if (x >= 0 && y >= 0 && x < W && y < H) rgb.set([r, g, b], (y * W + x) * 3); };
  for (let y = 0; y < H; y++)
    for (let x = 0; x < W; x++) {
      const i = (Math.floor(y / S) * img.width + Math.floor(x / S)) * 4, a = img.rgba[i + 3] / 255;
      set(x, y, ...[0, 1, 2].map((c) => Math.round(img.rgba[i + c] * a + [96, 96, 112][c] * (1 - a))));
    }
  for (let ty = 0; ty < img.height / 8; ty++)
    for (let tx = 0; tx < img.width / 8; tx++) {
      const ox = tx * 8 * S, oy = ty * 8 * S;
      for (let k = 0; k < 8 * S; k++) { set(ox + k, oy, 255, 255, 0); set(ox, oy + k, 255, 255, 0); }
      const text = code(tx, ty);
      for (let x = 0; x < 17; x++) for (let y = 0; y < 13; y++) set(ox + 1 + x, oy + 1 + y, 0, 0, 0);
      [...text].forEach((ch, n) => [...FONT[ch]].forEach((bit, p) => {
        if (bit !== '1') return;
        const px = ox + 3 + n * 8 + (p % 3) * 2, py = oy + 3 + Math.floor(p / 3) * 2;
        set(px, py, 255, 90, 90); set(px + 1, py, 255, 90, 90); set(px, py + 1, 255, 90, 90); set(px + 1, py + 1, 255, 90, 90);
      }));
    }
  fs.writeFileSync(out, encodePng(W, H, rgb));
  console.log(`${path.relative(ROOT, out)}: ${img.width / 8} x ${img.height / 8} tiles, labelled`);
}

function map(sheetFile, sceneFile) {
  const sheet = load(sheetFile), scene = load(sceneFile), index = new Map();
  for (let ty = 0; ty < sheet.height / 8; ty++)
    for (let tx = 0; tx < sheet.width / 8; tx++)
      for (const [fx, fy, f] of [[0, 0, ''], [1, 0, 'x'], [0, 1, 'y'], [1, 1, 'xy']]) {
        const k = tileKey(sheet, tx, ty, fx, fy);
        if (!index.has(k)) index.set(k, code(tx, ty, f));
      }
  let miss = 0;
  const used = new Set();
  for (let ty = 0; ty < scene.height / 8; ty++) {
    const row = [];
    for (let tx = 0; tx < scene.width / 8; tx++) {
      const c = index.get(tileKey(scene, tx, ty));
      if (!c) miss++;
      else used.add(c.slice(0, 2));
      row.push((c || '??').padEnd(4));
    }
    console.log(row.join(' '));
  }
  console.log(`\n${miss} of ${(scene.width / 8) * (scene.height / 8)} tiles not in the sheet (decals, or edited); ${used.size} distinct sheet tiles`);
}

function compose(sheetFile, codesFile, out, decalsFile) {
  const sheet = load(sheetFile), decals = decalsFile ? load(decalsFile) : null;
  const rows = fs.readFileSync(codesFile, 'utf8').split(/\r?\n/).filter((r) => r.trim() && !r.startsWith('#')).map((r) => r.trim().split(/\s+/));
  const S = 4, W = Math.max(...rows.map((r) => r.length)) * 8, H = rows.length * 8, rgb = Buffer.alloc(W * S * H * S * 3);
  const put = (x, y, c) => { for (let a = 0; a < S; a++) for (let b = 0; b < S; b++) rgb.set(c, ((y * S + a) * W * S + x * S + b) * 3); };
  const draw = (img, c, ox, oy, under) => {
    const tx = parseInt(c[0], 16), ty = parseInt(c[1], 16), fx = c.slice(2).includes('x'), fy = c.slice(2).includes('y');
    for (let y = 0; y < 8; y++)
      for (let x = 0; x < 8; x++) {
        const i = ((ty * 8 + (fy ? 7 - y : y)) * img.width + tx * 8 + (fx ? 7 - x : x)) * 4;
        if (img.rgba[i + 3] > 127) put(ox + x, oy + y, img.rgba.subarray(i, i + 3));
        else if (under) put(ox + x, oy + y, [255, 0, 255]);
      }
  };
  rows.forEach((r, j) => r.forEach((c, i) => {
    if (c === '..') return;
    const [base, mark] = c.split('+');
    draw(sheet, base, i * 8, j * 8, true);
    if (mark && decals) draw(decals, mark, i * 8, j * 8, false);
  }));
  fs.writeFileSync(out, encodePng(W * S, H * S, rgb));
  console.log(`${path.relative(ROOT, out)}: ${W / 8} x ${H / 8} tiles`);
}

const [cmd, a, b, c, d] = process.argv.slice(2);
const outFile = (f, name) => (f ? path.resolve(f) : path.join(ROOT, 'build', name));
fs.mkdirSync(path.join(ROOT, 'build'), { recursive: true });
if (cmd === 'label' && a) label(a, outFile(b, `codes-${path.basename(a, '.png')}.png`));
else if (cmd === 'map' && a && b) map(a, b);
else if (cmd === 'compose' && a && b) compose(a, b, outFile(c, 'mockup.png'), d);
else {
  console.log('usage: node tools/sheet-codes.mjs label <sheet.png> [out.png]\n' +
    '       node tools/sheet-codes.mjs map <sheet.png> <scene.png>\n' +
    '       node tools/sheet-codes.mjs compose <sheet.png> <codes.txt> [out.png] [decals.png]');
  process.exit(cmd ? 1 : 0);
}
