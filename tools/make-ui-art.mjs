// Draws the small UI sprites the art packs don't have (the aim reticle, the
// player's shot, the muzzle flash, placeholder chest and shield) from ASCII art, as 16x16 RGBA PNGs in
// artwork/ui/. Run it after editing a drawing here; tools/assets.mjs converts the
// PNGs like any other art.
//   node tools/make-ui-art.mjs
//
// Characters: '.' transparent, 'w' white, 'g' gray, 'k' black. The asset
// converter maps colors by brightness, so these become shades 1, 2 and 3.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'artwork', 'ui');
const COLORS = { '.': [0, 0, 0, 0], w: [255, 255, 255, 255], g: [128, 128, 128, 255], k: [0, 0, 0, 255] };

/** Encode rows of ASCII art (16x16) as an RGBA PNG. */
function png(rows) {
  if (rows.length !== 16 || rows.some((r) => r.length !== 16)) throw new Error('art must be 16x16');
  const raw = Buffer.alloc((16 * 4 + 1) * 16);
  rows.forEach((row, y) => {
    raw[y * 65] = 0;
    [...row].forEach((ch, x) => {
      const c = COLORS[ch];
      if (!c) throw new Error(`unknown color '${ch}'`);
      raw.set(c, y * 65 + 1 + x * 4);
    });
  });
  const chunk = (type, data) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(zlib.crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(16, 0);
  ihdr.writeUInt32BE(16, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // RGBA
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0)),
  ]);
}

/** A 16x16 frame from its top-left 8x8 quarter, mirrored into the other three. */
function mirrored4(quarter) {
  const top = quarter.map((r) => r + [...r].reverse().join(''));
  return [...top, ...[...top].reverse()];
}

// Aim reticle: corner brackets (black with a white edge, visible on floor and
// walls) drawn around the target's 16x16 frame, pulsing 1 px inward.
const reticleOut = mirrored4([
  'wwwwww..',
  'wkkkkw..',
  'wkwwww..',
  'wkw.....',
  'wkw.....',
  'www.....',
  '........',
  '........',
]);
const reticleIn = mirrored4([
  '........',
  '.wwwwww.',
  '.wkkkkw.',
  '.wkwwww.',
  '.wkw....',
  '.wkw....',
  '.www....',
  '........',
]);

// Player shot: a 6x6 pellet in the middle 8 columns (cropped to one 8x16 sprite,
// like the enemy pea), rows 5-10 to match the bullet hitbox. The highlight turns.
const pad = (rows) => ['', '', '', '', ''].map(() => '.'.repeat(16))
  .concat(rows.map((r) => '....' + r + '....'))
  .concat(Array(16 - 5 - rows.length).fill('.'.repeat(16)));
const shotA = pad([
  '..kkkk..',
  '.kwwggk.',
  '.kwgggk.',
  '.kggggk.',
  '.kggggk.',
  '..kkkk..',
]);
const shotB = pad([
  '..kkkk..',
  '.kggggk.',
  '.kggggk.',
  '.kgggwk.',
  '.kggwwk.',
  '..kkkk..',
]);

// Muzzle flash: a burst centred on (8, 8), cropped to the middle 8 columns like the
// shot, so its sprite's (4, 8) is the muzzle. A big frame, then a small one.
const flashA = [
  '................', '................', '................', '................', '................',
  '........k.......',
  '......k.w.k.....',
  '.......www......',
  '.....kwwwwwk....',
  '.......www......',
  '......k.w.k.....',
  '........k.......',
  '................', '................', '................', '................',
];
const flashB = [
  '................', '................', '................', '................', '................',
  '................', '................',
  '........k.......',
  '.......kwk......',
  '........k.......',
  '................', '................', '................', '................', '................', '................',
];

// Placeholder props until real art: the arena's chest (closed, open) and the
// shield it holds. The chest is drawn with the enemy pea's palette (w = peach
// trim, g = orange wood, k = black), the shield with the weapons' gunmetal.
const chestClosed = [
  '................',
  '................',
  '..kkkkkkkkkkkk..',
  '.kggggggggggggk.',
  '.kgwggggggggwgk.',
  '.kwwwwwwwwwwwwk.',
  '.kkkkkkkkkkkkkk.',
  '.kgwggwkkwggwgk.',
  '.kgwggwkkwggwgk.',
  '.kgwgggwwgggwgk.',
  '.kgwggggggggwgk.',
  '.kwwwwwwwwwwwwk.',
  '.kgwggggggggwgk.',
  '.kkkkkkkkkkkkkk.',
  '................',
  '................',
];
const chestOpen = [
  '..kkkkkkkkkkkk..',
  '.kgwggggggggwgk.',
  '.kwwwwwwwwwwwwk.',
  '.kkkkkkkkkkkkkk.',
  '.kkkkkwkkkkkkkk.',
  '.kkwkkkkkkwkkkk.',
  '.kwwwwwwwwwwwwk.',
  '.kgwgggkkgggwgk.',
  '.kgwgggwwgggwgk.',
  '.kgwggggggggwgk.',
  '.kgwggggggggwgk.',
  '.kwwwwwwwwwwwwk.',
  '.kgwggggggggwgk.',
  '.kkkkkkkkkkkkkk.',
  '................',
  '................',
];
const shieldItem = [
  '................',
  '................',
  '...kkkkkkkkkk...',
  '..kwwwwwwwwggk..',
  '..kwgggggggggk..',
  '..kwggwwwwgggk..',
  '..kwggwkkwgggk..',
  '..kwggwkkwgggk..',
  '..kwggwwwwgggk..',
  '...kwgggggggk...',
  '...kwgggggggk...',
  '....kwgggggk....',
  '.....kwgggk.....',
  '......kggk......',
  '.......kk.......',
  '................',
];

fs.mkdirSync(OUT, { recursive: true });
const files = {
  'reticle (1).png': reticleOut, 'reticle (2).png': reticleIn, 'shot (1).png': shotA, 'shot (2).png': shotB,
  'flash (1).png': flashA, 'flash (2).png': flashB,
  'chest (1).png': chestClosed, 'chest (2).png': chestOpen, 'shield (1).png': shieldItem,
};
for (const [name, rows] of Object.entries(files)) {
  fs.writeFileSync(path.join(OUT, name), png(rows));
  console.log(`[ui-art] artwork/ui/${name}`);
}
