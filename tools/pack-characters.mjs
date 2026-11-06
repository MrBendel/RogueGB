// The classes' bodies from the 16x16-RPG-characters pack (G:\My Drive\Artwork\Assets),
// recoloured and tweaked, when the pack is on this PC. The pack is bought art and the
// repository is public, so its pixels are never committed: the build makes the frames
// into build/pack-art/<class>/ and uses them, and without the pack (CI, GitHub Pages)
// it uses the hand-drawn ones in artwork/<class>/ (tools/make-character-art.mjs).
//
// A pack sheet is 4 x 2 characters of 48x64: per character, rows face down, left,
// right and up, columns are step, stand, step. Walks here go stand, step, stand, step.
// The tweaks are drawn below as rows over the pack's own pixels.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { decodePng } from './assets.mjs';
import { artPath } from './tileset.mjs';

const ROOT = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const PACK = 'pack:16x16-RPG-characters/';

// The hunter's head from his portrait: a baseball cap for the scout's hood (a low
// crown lit on top, a flat dark bill out front, plain at the back), and goggles
// across the eyes over the scout's mask. Rows 0-7 of the head (one lower on the
// stepping frames).
const CAP = {
  down: ['................', '......33333.....', '.....3211223....', '....321122223...', '...33333333333..', '....322222223...',
    '...33111311133..', '....331131133...'],
  right: ['................', '....33333.......', '...3211223......', '...32122223.....', '...32222223333..', '...322322223....',
    '...322331113....', '..3223331113....'],
  up: ['................', '......33333.....', '.....3222223....', '....322222223...', '....322222223...', '....333333333...'],
};

export const PACK_CHARACTERS = {
  // The circled green scout, in a drabber green, with a baseball cap for the hood.
  hunter: {
    sheet: '04-scout.png', at: [2, 1],
    palette: ['#e0ecb8', '#5a6e36', '#141c10'],
    edit(g, dir, head) { CAP[dir].forEach((row, y) => put(g, head + y, row)); },
  },
  // The conjurer's hood, in purple; its eyes glow in the dark of it.
  hooded: {
    sheet: '06-conjurer.png', at: [2, 0],
    palette: ['#f0d8ff', '#6c4c9c', '#1c1030'],
  },
  // The white-haired one: the hair a metal dome, the face grey plate, one eye a lens.
  cyborg: {
    sheet: '01-generic.png', at: [3, 0],
    palette: ['#a8f0e8', '#7c8c84', '#18202a'],
    shades: { 'ffffff': 1, 'f0d0b0': 2, '3cbcfc': 2 },
    edit(g, dir, head) {
      const lens = { down: 10, right: 9 }[dir];
      if (lens) for (const y of [head + 6, head + 7]) g[y][lens] = 1;
    },
  },
};

const put = (g, y, row) => [...row].forEach((c, x) => { g[y][x] = c === '.' ? 0 : +c; });
const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16), 255];
const key = (r, g, b) => [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('');
const mirror = (g) => g.map((r) => [...r].reverse());
const blank = () => new Array(16).fill(0);

/** The 12 frames of a pack character as shade grids (0 clear, 1 light, 2 mid, 3 dark). */
function cut(img, ch) {
  const [cx, cy] = ch.at, px = (x, y) => (y * img.width + x) * 4;
  // Shades: near-black is the outline, then the commonest other colour is the mid.
  const count = new Map();
  for (let y = 0; y < 64; y++) for (let x = 0; x < 48; x++) {
    const i = px(cx * 48 + x, cy * 64 + y), d = img.rgba;
    if (d[i + 3] < 128) continue;
    const k = key(d[i], d[i + 1], d[i + 2]);
    count.set(k, (count.get(k) ?? 0) + 1);
  }
  const shades = { ...ch.shades };
  const others = [...count].filter(([k]) => !(k in shades) && parseInt(k.slice(0, 2), 16) + parseInt(k.slice(2, 4), 16) + parseInt(k.slice(4), 16) >= 80);
  others.sort((a, b) => b[1] - a[1]).forEach(([k], i) => { if (!(k in shades)) shades[k] = i ? 1 : 2; });
  const frames = {};
  ['down', 'left', 'right', 'up'].forEach((dir, row) => {
    frames[dir] = [0, 1, 2].map((col) => Array.from({ length: 16 }, (_, y) => Array.from({ length: 16 }, (_, x) => {
      const i = px(cx * 48 + col * 16 + x, cy * 64 + row * 16 + y), d = img.rgba;
      return d[i + 3] < 128 ? 0 : shades[key(d[i], d[i + 1], d[i + 2])] ?? 3;
    })));
  });
  return frames;
}

/** Every animation spr_<class> needs, as named 16x16 shade grids. */
function animations(name, ch, img) {
  const f = cut(img, ch);
  // The standing frame's head is a row higher than the steps'.
  for (const dir of ['down', 'right', 'up']) f[dir].forEach((g, col) => ch.edit?.(g, dir, col === 1 ? 0 : 1));
  const out = {};
  for (const dir of ['down', 'up', 'right']) {
    const [a, stand, b] = f[dir];
    out[`walk ${dir}`] = [stand, a, stand, b];
  }
  // Knocked back a pixel, away from the way you face.
  out['hurt down'] = [[...f.down[1].slice(1), blank()]];
  out['hurt up'] = [[blank(), ...f.up[1].slice(0, 15)]];
  out['hurt right'] = [f.right[1].map((r) => [...r.slice(1), 0])];
  out.falling = [f.down[1], f.right[1], f.up[1], mirror(f.right[1])];
  // The roll keeps the hand-drawn ball (artwork/<class>), in these colours.
  for (const dir of ['down', 'up', 'right']) {
    out[`roll ${dir}`] = [1, 2, 3, 4].map((n) => {
      const im = decodePng(fs.readFileSync(path.join(ROOT, 'artwork', name, `player roll ${dir} (${n}).png`)));
      const lums = [...new Set(Array.from({ length: 256 }, (_, i) => i).filter((i) => im.rgba[i * 4 + 3] >= 128)
        .map((i) => im.rgba[i * 4] * 3 + im.rgba[i * 4 + 1] * 6 + im.rgba[i * 4 + 2]))].sort((a, b) => b - a);
      return Array.from({ length: 16 }, (_, y) => Array.from({ length: 16 }, (_, x) => {
        const i = (y * 16 + x) * 4;
        return im.rgba[i + 3] < 128 ? 0 : 1 + lums.indexOf(im.rgba[i] * 3 + im.rgba[i + 1] * 6 + im.rgba[i + 2]);
      }));
    });
  }
  return out;
}

function png(w, h, pixel) {
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) raw.set(pixel(x, y), y * (w * 4 + 1) + 1 + x * 4);
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(zlib.crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

const file = (anim, i, list) => `player ${anim}${list.length > 1 || anim.startsWith('walk') ? ` (${i + 1})` : ''}.png`;

/**
 * The folder spr_<name>'s frames come from: build/pack-art/<name> made from the
 * pack when it's here (and ROGUEGB_STANDIN_ART isn't set), else artwork/<name>.
 */
export function characterDir(name) {
  const ch = PACK_CHARACTERS[name];
  const src = ch && artPath(PACK + ch.sheet);
  if (!ch || process.env.ROGUEGB_STANDIN_ART || process.env.REPO_CHARACTERS || !fs.existsSync(src)) return `artwork/${name}`;
  const rel = `build/pack-art/${name}`, dir = path.join(ROOT, rel);
  fs.mkdirSync(dir, { recursive: true });
  const pal = [[0, 0, 0, 0], ...ch.palette.map(hex)];
  for (const [anim, list] of Object.entries(animations(name, ch, decodePng(fs.readFileSync(src))))) {
    list.forEach((g, i) => {
      const out = path.join(dir, file(anim, i, list)), data = png(16, 16, (x, y) => pal[g[y][x]]);
      if (!fs.existsSync(out) || !fs.readFileSync(out).equals(data)) fs.writeFileSync(out, data);
    });
  }
  return rel;
}

// node tools/pack-characters.mjs: make the frames and a preview, build/pack-characters.png.
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const S = 6, names = Object.keys(PACK_CHARACTERS);
  const order = ['walk down', 'walk up', 'walk right', 'hurt down', 'hurt up', 'hurt right', 'falling', 'roll right'];
  const rows = names.map((n) => {
    const dir = path.join(ROOT, characterDir(n));
    return order.flatMap((a) => fs.readdirSync(dir).filter((f) => f.startsWith(`player ${a}`)).sort().map((f) => decodePng(fs.readFileSync(path.join(dir, f)))));
  });
  const W = Math.max(...rows.map((r) => r.length)) * (16 * S + 4) + 4, H = rows.length * (16 * S + 4) + 4;
  fs.writeFileSync(path.join(ROOT, 'build', 'pack-characters.png'), png(W, H, (x, y) => {
    const r = Math.floor((y - 4) / (16 * S + 4)), c = Math.floor((x - 4) / (16 * S + 4));
    const fx = Math.floor(((x - 4) % (16 * S + 4)) / S), fy = Math.floor(((y - 4) % (16 * S + 4)) / S);
    const im = rows[r]?.[c];
    if (!im || x < 4 || y < 4 || fx > 15 || fy > 15) return [0x30, 0x30, 0x38, 255];
    const i = (fy * 16 + fx) * 4;
    return im.rgba[i + 3] < 128 ? [0xd8, 0xd0, 0xb0, 255] : [...im.rgba.slice(i, i + 3), 255];
  }));
  console.log('wrote build/pack-characters.png');
}
