// The playable characters' body frames, drawn by hand as pixel grids and
// expanded into the animation set spr_player-style assets need. Run it after
// editing a drawing:
//   node tools/make-character-art.mjs [--show]
//
// Writes artwork/<name>/player walk down (1..4).png etc. (the same names as
// artwork/spacemarine) and, with --show, build/characters.png: every frame at
// 6x plus the walk cycles at 1x and 3x on a floor colour.
//
// The style is the user's Monk.png (the hooded wanderer): no outline; a rim of
// the light shade along the top-left edges (the light comes from there); dark only
// for shadow, the face's void and the legs; leaning, asymmetric silhouettes.
// Each character is drawn frame by frame: four walk frames per facing (left is
// right mirrored, by the converter), and one hurt frame per facing, shown knocked
// back a pixel. Falling spins through the four facings; the roll turns one
// curled-up `ball` drawing a quarter at a time.
//
// Shades: . transparent, 1 light, 2 mid, 3 dark (the outline). The converter
// keeps the art's own colours as the palette, lightest first.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const ROOT = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');

// The hooded wanderer, from the user's Monk.png (G:\My Drive\Artwork): no outline,
// a rim of light along the top-left edges, dark only for the face's void, the
// shadowed side and the legs. Its frames 1-2 (nearly facing you) are the walk
// down (not mirrored for the other step: the light comes from the left), frames
// 3-6 the walk right. The back view is frames 1-2 with the hood closed and a fold.
const MONK_1 = [
  '.....11111......',
  '...12222221.....',
  '...12222222.....',
  '....1233333.....',
  '....12313313....',
  '..2.12313313....',
  '...2.233333.....',
  '....3222222.....',
  '...2.2223222....',
  '.....2212322....',
  '....222212322...',
  '..22221111122...',
  '...22333333322..',
  '....3333333332..',
  '......33.33.....',
  '......3..3......',
];
const MONK_2 = [
  '....11111.......',
  '...12222222.....',
  '...122222222....',
  '....12333333....',
  '....12313313....',
  '...212313313....',
  '....22333333....',
  '...2.3222222....',
  '.....22222222...',
  '....222213222...',
  '..22222331222...',
  '...2221111222...',
  '.....3333322....',
  '......33332.....',
  '......3..3......',
  '................',
];
const MONK_RIGHT = [
  [
    '........11......',
    '.....2222221....',
    '.....22222221...',
    '......22222332..',
    '......22331331..',
    '...2..32331331..',
    '....2332333333..',
    '...2.32222222...',
    '....222222222...',
    '....12222332....',
    '...12222213.....',
    '..222222333.....',
    '..222223122.....',
    '.......333......',
    '.......33.......',
    '......3.........',
  ],
  [
    '........1111....',
    '.....22222221...',
    '.....222222221..',
    '......22332233..',
    '...2..22331331..',
    '....2.22331331..',
    '.....232333333..',
    '....2.2222222...',
    '......2222222...',
    '......222233....',
    '...122222233....',
    '..2222223322....',
    '...222231222....',
    '......333333....',
    '......33..3.....',
    '......3.........',
  ],
  [
    '.....1111.......',
    '......222111....',
    '......2222221...',
    '......22222221..',
    '......22332233..',
    '....2.22331331..',
    '.....222331331..',
    '....2.32233333..',
    '.....33222212...',
    '...111222223....',
    '..2222222233....',
    '..2222223312....',
    '...22221122.....',
    '.....333333.....',
    '.....333.333....',
    '...........3....',
  ],
  [
    '......22211.....',
    '....22222222....',
    '....22222222....',
    '.....2223322....',
    '......223133....',
    '...2..223133....',
    '....23223333....',
    '...2.3222222....',
    '.....2222222....',
    '...122222332....',
    '..1122222331....',
    '..222223332.....',
    '..222231122.....',
    '....3333333.....',
    '....33...33.....',
    '...3......3.....',
  ],
];
const MONK_BACK_1 = [
  '.....11111......',
  '...12222221.....',
  '...12222222.....',
  '....1222232.....',
  '....12222322....',
  '..2.12222322....',
  '...2.222232.....',
  '....3333333.....',
  '...2.2222222....',
  '.....2222222....',
  '....222222222...',
  '..22222222222...',
  '...22333333322..',
  '....3333333332..',
  '......33.33.....',
  '......3..3......',
];
const MONK_BACK_2 = [
  '....11111.......',
  '...12222222.....',
  '...122222222....',
  '....12222232....',
  '....12222232....',
  '...212222232....',
  '....22222232....',
  '...2.3333333....',
  '.....22222222...',
  '....222222222...',
  '..22222222222...',
  '...22222222222..',
  '.....3333322....',
  '......33332.....',
  '......3..3......',
  '................',
];

// The bounty hunter in the same style: a field cap with a long brim, the face in
// its shadow, a bandana, hunched shoulders, a pack with a bedroll, dark trousers.
// Each view is a body (rows 0-12) over legs; on the passing steps the body bobs
// down a pixel and the legs are two rows.
const body = (top, bob, legs) => (bob ? ['................', ...top] : top).concat(legs);
const HUNTER_SIDE = [
  '................',
  '.....1111.......',
  '....122222......',
  '...12222222.....',
  '...1222222222...',
  '...22333333.....',
  '...2311311......',
  '..1222222.......',
  '.112222222......',
  '.1222223332.....',
  '.122223222211...',
  '..222222333.....',
  '..2233332.......',
];
const HUNTER_FRONT = [
  '................',
  '.....11111......',
  '....1222222.....',
  '...122222222....',
  '..12222222222...',
  '...21311312.....',
  '...21311312.....',
  '...22222222.....',
  '..11222222223...',
  '..12222222232...',
  '..122222222322..',
  '..1222112223....',
  '...223333322....',
];
const HUNTER_BACK = [
  '................',
  '.....11111......',
  '....1222222.....',
  '...122222222....',
  '...122222222....',
  '...233333332....',
  '...23333333.....',
  '....211112......',
  '..11222222223...',
  '..12211111232...',
  '..12232223232...',
  '..1223333223....',
  '...223333322....',
];
const FRONT_LEGS = ['...333..333.....', '...33....33.....', '..333....333....'];
const FRONT_STEP = ['...333..333.....', '...33....333....'];
// The cyborg gunner: bald, a metal plate over one temple and glowing eyes, a
// dark high-collared coat, metal shins.
const CYBORG_SIDE = [
  '................',
  '.....1111.......',
  '....122222......',
  '...1222222......',
  '...12222331.....',
  '...12232313.....',
  '...22222223.....',
  '....2333........',
  '..113333333.....',
  '.1333333333.....',
  '.133333332221...',
  '..333333333.....',
  '..33333333......',
];
const CYBORG_FRONT = [
  '................',
  '.....1111.......',
  '....1222222.....',
  '...122222222....',
  '...133223322....',
  '...231223122....',
  '...22222232.....',
  '....2233322.....',
  '..11333333333...',
  '..13333333333...',
  '..133333333322..',
  '..1333221333....',
  '...333333333....',
];
const CYBORG_BACK = [
  '................',
  '.....1111.......',
  '....1222222.....',
  '...122222222....',
  '...122222232....',
  '...122222232....',
  '...22222223.....',
  '....222232......',
  '..11333333333...',
  '..13333333333...',
  '..133333333332..',
  '..1333333333....',
  '...333333333....',
];
const SHIN_LEGS = ['...323..323.....', '...22....22.....', '..333....333....'];
const SHIN_STEP = ['...323..323.....', '...22....333....'];
const flip = (rows) => rows.map((r) => [...r].reverse().join(''));
const swap = (rows, swaps) => rows.map((r, y) => swaps[y] ?? r);

const CHARACTERS = {
  hunter: {
    palette: ['#f0d8a8', '#5f7a3a', '#141c10'],
    walk: {
      down: [body(HUNTER_FRONT, 0, FRONT_LEGS), body(HUNTER_FRONT, 1, FRONT_STEP), body(HUNTER_FRONT, 0, FRONT_LEGS), body(HUNTER_FRONT, 1, flip(FRONT_STEP))],
      up: [body(HUNTER_BACK, 0, FRONT_LEGS), body(HUNTER_BACK, 1, FRONT_STEP), body(HUNTER_BACK, 0, FRONT_LEGS), body(HUNTER_BACK, 1, flip(FRONT_STEP))],
      right: [
        body(HUNTER_SIDE, 0, ['...333..333.....', '..333....333....', '..33......333...']),
        body(HUNTER_SIDE, 1, ['....3333........', '....3333........']),
        body(HUNTER_SIDE, 0, ['...333.333......', '...33...333.....', '..333....33.....']),
        body(HUNTER_SIDE, 1, ['....333.........', '...3333.........']),
      ],
    },
    // Eyes shut.
    hurt: {
      down: body(swap(HUNTER_FRONT, { 5: '...21111112.....', 6: '...23333332.....' }), 0, FRONT_LEGS),
      up: body(HUNTER_BACK, 0, FRONT_LEGS),
      right: body(swap(HUNTER_SIDE, { 6: '...2311333......' }), 0, ['...333..333.....', '..333....333....', '..33......333...']),
    },
    ball: [
      '................',
      '................',
      '.....11111......',
      '....1222222.....',
      '...122222222....',
      '..12222222222...',
      '..23333333332...',
      '..1231131132....',
      '..1222222222....',
      '..1222111122....',
      '...233333332....',
      '....3333333.....',
      '................',
      '................',
      '................',
      '................',
    ],
  },
  hooded: {
    palette: ['#e0f8d0', '#346856', '#081820'],
    walk: {
      down: [MONK_1, MONK_2, MONK_1, MONK_2],
      up: [MONK_BACK_1, MONK_BACK_2, MONK_BACK_1, MONK_BACK_2],
      right: MONK_RIGHT,
    },
    // Eyes shut.
    hurt: {
      down: swap(MONK_1, { 4: '....12333333....', 5: '..2.12333333....' }),
      up: MONK_BACK_1,
      right: swap(MONK_RIGHT[0], { 4: '......22333333..', 5: '...2..32333333..' }),
    },
    ball: [
      '................',
      '................',
      '....111111......',
      '...12222221.....',
      '..1222222222....',
      '..1223333322....',
      '..1231331322....',
      '..1223333322....',
      '..1222222222....',
      '..2222111222....',
      '...333333333....',
      '....3333333.....',
      '................',
      '................',
      '................',
      '................',
    ],
  },
  cyborg: {
    palette: ['#a8f0e8', '#7c8c84', '#18202a'],
    walk: {
      down: [body(CYBORG_FRONT, 0, SHIN_LEGS), body(CYBORG_FRONT, 1, SHIN_STEP), body(CYBORG_FRONT, 0, SHIN_LEGS), body(CYBORG_FRONT, 1, flip(SHIN_STEP))],
      up: [body(CYBORG_BACK, 0, SHIN_LEGS), body(CYBORG_BACK, 1, SHIN_STEP), body(CYBORG_BACK, 0, SHIN_LEGS), body(CYBORG_BACK, 1, flip(SHIN_STEP))],
      right: [
        body(CYBORG_SIDE, 0, ['...323..323.....', '..32......22....', '..33......333...']),
        body(CYBORG_SIDE, 1, ['....3223........', '....3333........']),
        body(CYBORG_SIDE, 0, ['...323.323......', '...22...22......', '..333....33.....']),
        body(CYBORG_SIDE, 1, ['....323.........', '...3333.........']),
      ],
    },
    // Eyes out.
    hurt: {
      down: body(swap(CYBORG_FRONT, { 5: '...233223322....' }), 0, SHIN_LEGS),
      up: body(CYBORG_BACK, 0, SHIN_LEGS),
      right: body(swap(CYBORG_SIDE, { 5: '...12232333.....' }), 0, ['...323..323.....', '..32......22....', '..33......333...']),
    },
    ball: [
      '................',
      '................',
      '.....1111.......',
      '....1222222.....',
      '...122222222....',
      '...133223322....',
      '..1122222222....',
      '..13333333333...',
      '..13333333333...',
      '..1333333333....',
      '...333333333....',
      '....3333333.....',
      '................',
      '................',
      '................',
      '................',
    ],
  },
};

const SHADE = { '.': 0, 1: 1, 2: 2, 3: 3 };

function grid(list) {
  if (list.length !== 16) throw new Error(`${list.length} rows, not 16`);
  return list.map((r, i) => {
    if (r.length !== 16) throw new Error(`row ${i} "${r}" is ${r.length} wide`);
    return [...r].map((c) => {
      if (!(c in SHADE)) throw new Error(`row ${i}: bad shade "${c}"`);
      return SHADE[c];
    });
  });
}

const blank = () => new Array(16).fill(0);
const mirror = (g) => g.map((r) => [...r].reverse());
const rotate = (g) => g.map((r, y) => r.map((_, x) => g[15 - x][y]));  // a quarter clockwise
/** Every animation from a character's walk.{down,up,right}, hurt.{down,up,right} and ball. */
function frames(ch) {
  const out = {};
  for (const d of ['down', 'up', 'right']) out[`walk ${d}`] = ch.walk[d].map(grid);
  // Knocked back a pixel, away from the way you face.
  const [hd, hu, hr] = [ch.hurt.down, ch.hurt.up, ch.hurt.right].map(grid);
  out['hurt down'] = [...hd.slice(1), blank()];
  out['hurt up'] = [blank(), ...hu.slice(0, 15)];
  out['hurt right'] = hr.map((r) => [...r.slice(1), 0]);
  const r1 = out['walk right'][0];
  out.falling = [out['walk down'][0], r1, out['walk up'][0], mirror(r1)];
  const ball = grid(ch.ball);
  const turns = [ball, rotate(ball), rotate(rotate(ball)), rotate(rotate(rotate(ball)))];
  out['roll right'] = turns;
  out['roll down'] = turns;
  out['roll up'] = [turns[0], turns[3], turns[2], turns[1]];
  return out;
}

const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16), 255];

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
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0)),
  ]);
}

const FILES = {
  'walk down': (i) => `player walk down (${i + 1}).png`,
  'walk up': (i) => `player walk up (${i + 1}).png`,
  'walk right': (i) => `player walk right (${i + 1}).png`,
  'hurt down': () => 'player hurt down.png',
  'hurt up': () => 'player hurt up.png',
  'hurt right': () => 'player hurt right.png',
  falling: (i) => `player falling (${i + 1}).png`,
  'roll down': (i) => `player roll down (${i + 1}).png`,
  'roll up': (i) => `player roll up (${i + 1}).png`,
  'roll right': (i) => `player roll right (${i + 1}).png`,
};
const isList = (f) => Array.isArray(f[0][0]);

const all = {};
for (const [name, ch] of Object.entries(CHARACTERS)) {
  const pal = [[0, 0, 0, 0], ...ch.palette.map(hex)];
  const f = frames(ch);
  all[name] = { pal, f };
  const dir = path.join(ROOT, 'artwork', name);
  fs.mkdirSync(dir, { recursive: true });
  for (const [anim, list] of Object.entries(f)) {
    (isList(list) ? list : [list]).forEach((g, i) => fs.writeFileSync(path.join(dir, FILES[anim](i)), png(16, 16, (x, y) => pal[g[y][x]])));
  }
}

if (process.argv.includes('--show')) {
  // Per character: a row of every frame at 6x, then the walks at 1x and 3x.
  const S = 6, PAD = 4, FLOOR = [0xd8, 0xd0, 0xb0, 255], BG = [0x30, 0x30, 0x38, 255];
  const names = Object.keys(all), order = Object.keys(FILES);
  const perRow = order.reduce((n, a) => n + (isList(all[names[0]].f[a]) ? all[names[0]].f[a].length : 1), 0);
  const W = perRow * (16 * S + PAD) + PAD, rowH = 16 * S + PAD * 3 + 16 * 3 + PAD;
  const H = names.length * rowH;
  const img = Array.from({ length: H }, () => Array.from({ length: W }, () => BG));
  const blit = (g, pal, ox, oy, s) => {
    for (let y = 0; y < 16 * s; y++) for (let x = 0; x < 16 * s; x++) {
      const v = g[Math.floor(y / s)][Math.floor(x / s)];
      img[oy + y][ox + x] = v ? pal[v] : FLOOR;
    }
  };
  names.forEach((name, ci) => {
    const { pal, f } = all[name];
    const y0 = ci * rowH + PAD;
    let x = PAD;
    for (const a of order) for (const g of isList(f[a]) ? f[a] : [f[a]]) { blit(g, pal, x, y0, S); x += 16 * S + PAD; }
    const y1 = y0 + 16 * S + PAD;
    x = PAD;
    for (const a of ['walk down', 'walk right', 'walk up']) for (const g of f[a]) { blit(g, pal, x, y1, 1); x += 18; }
    x += 20;
    for (const a of ['walk down', 'walk right', 'walk up']) for (const g of f[a]) { blit(g, pal, x, y1, 3); x += 16 * 3 + 2; }
  });
  const out = path.join(ROOT, 'build', 'characters.png');
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.writeFileSync(out, png(W, H, (x, y) => img[y][x]));
  console.log(`wrote ${out}`);
}
