// The playable characters' body frames, drawn by hand as pixel grids and
// expanded into the animation set spr_player-style assets need. Run it after
// editing a drawing:
//   node tools/make-character-art.mjs [--show]
//
// Writes artwork/<name>/player walk down (1..4).png etc. (the same names as
// artwork/spacemarine) and, with --show, build/characters.png: every frame at
// 6x plus the walk cycles at 1x and 3x on a floor colour.
//
// The style is the user's Characters.png (G:\My Drive\Artwork): a dark outline,
// the light shade on the top-left edges, compact silhouettes. Each class's side
// view is the user's drawing; its front and back views are drawn to match.
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

// The bounty hunter, from the user's Characters.png (G:\My Drive\Artwork): a dark
// outline, a field cap with a long brim, goggles over a face mask, arms held
// forward (the game draws the gun). The side view is the user's drawing (rows
// 0-12 here, the legs below are its stride); the front and back views, the other
// steps, the hurt frames and the ball are drawn to match. Each view is a body
// (rows 0-12) over legs; on the passing steps the body bobs down a pixel and the
// legs are two rows.
const body = (top, bob, legs) => (bob ? ['................', ...top] : top).concat(legs);
const HUNTER_SIDE = [
  '......321223....',
  '.....322223333..',
  '.....3332211113.',
  '.....333333333..',
  '....33221313....',
  '...333321123....',
  '..31123332333...',
  '..311333333323..',
  '..321121122123..',
  '..32233233333...',
  '...3333333......',
  '...33323333.....',
  '...322233333....',
];
const HUNTER_STRIDE = ['..3222..3333....', '..3333...3223...', '..23333..33333..'];
const HUNTER_PASS = ['...32223333.....', '..333333333.....'];
const HUNTER_STRIDE_2 = ['..3333..3222....', '..3223...3333...', '.33333...33332..'];
const HUNTER_PASS_2 = ['...33333222.....', '..333333333.....'];
const HUNTER_FRONT = [
  '.....333333.....',
  '....32211223....',
  '...3222222223...',
  '..333333333333..',
  '...3311331133...',
  '...3322222233...',
  '....33222233....',
  '..333322223333..',
  '.32213333331223.',
  '.32133222233123.',
  '.32233322333223.',
  '..333332233333..',
  '...3333333333...',
];
const HUNTER_BACK = [
  '.....333333.....',
  '....32211223....',
  '...3222222223...',
  '...3322222233...',
  '...3332222333...',
  '....33333333....',
  '....33333333....',
  '..333322223333..',
  '.32232111123223.',
  '.32232222223223.',
  '.32232222223223.',
  '..333233332333..',
  '...3333333333...',
];
const HUNTER_LEGS = ['...3322..2233...', '...3333..3333...', '..33333..33333..'];
const HUNTER_STEP = ['...3322..2233...', '...3333...33333.'];
// The cyborg, from the user's Characters.png: a bare metal head with a lens eye,
// a dark coat, long jointed legs. The side view is the user's drawing; the rest
// are drawn to match.
const CYBORG_SIDE = [
  '.......1112.....',
  '......212112....',
  '......3313213...',
  '......3313313...',
  '......3321113...',
  '.....3333212....',
  '....321333333...',
  '....3213313313..',
  '....32211113213.',
  '.....3332233333.',
  '.....3333332....',
  '.....3233322....',
  '....3333.3213...',
];
const CYBORG_STRIDE = ['...33233..323...', '...3223...323...', '...3223...3223..'];
const CYBORG_PASS = ['.....323323.....', '....3223223.....'];
const CYBORG_STRIDE_2 = ['...323...33233..', '...323...3223...', '..3223....3223..'];
const CYBORG_PASS_2 = ['.....323323.....', '.....3223223....'];
const CYBORG_FRONT = [
  '......3333......',
  '.....311123.....',
  '....31111123....',
  '....31331323....',
  '....31111123....',
  '.....3322233....',
  '...3333223333...',
  '..322333333223..',
  '..321333333123..',
  '..321333333123..',
  '..322332233223..',
  '...3333333333...',
  '....33333333....',
];
const CYBORG_BACK = [
  '......3333......',
  '.....311123.....',
  '....31111123....',
  '....31111223....',
  '....31122223....',
  '.....3322233....',
  '...3333333333...',
  '..322333333223..',
  '..321333333123..',
  '..321332233123..',
  '..322333333223..',
  '...3333333333...',
  '....33333333....',
];
const CYBORG_LEGS = ['....323..323....', '....323..323....', '...3223..3223...'];
const CYBORG_STEP = ['....323..323....', '....323...3223..'];
// The hooded one, from the user's Characters.png: a deep hood with two eyes in
// its dark, a long robe. The side view is the user's drawing.
const HOODED_SIDE = [
  '.....3212111....',
  '.....32222221...',
  '.....32222222...',
  '.....32233333...',
  '.....32231331...',
  '...2313233333...',
  '....232233333...',
  '....233222122...',
  '....2.32233221..',
  '......32233221..',
  '.....1222323221.',
  '...122222222322.',
  '...322223333322.',
];
const HOODED_STRIDE = ['....32233333322.', '......3332323...', '.....332...333..'];
const HOODED_PASS = ['....32233333322.', '.......3333.....'];
const HOODED_STRIDE_2 = ['....32233333322.', '......3323333...', '......333..332..'];
const HOODED_PASS_2 = ['....32233333322.', '......33333.....'];
const HOODED_FRONT = [
  '.....333333.....',
  '....32111123....',
  '...3211222223...',
  '...3213333323...',
  '...3231331323...',
  '...3223333223...',
  '....32222223....',
  '...3221222223...',
  '..322122223223..',
  '..321222223223..',
  '..322222232223..',
  '..322222232223..',
  '..333333333333..',
];
const HOODED_BACK = [
  '.....333333.....',
  '....32111123....',
  '...3211222223...',
  '...3212222223...',
  '...3212222223...',
  '...3222222223...',
  '....32222223....',
  '...3222222223...',
  '..322222222223..',
  '..321222232223..',
  '..322222232223..',
  '..322222232223..',
  '..333333333333..',
];
const HOODED_LEGS = ['....333..333....', '....33....33....', '...333....333...'];
const HOODED_STEP = ['....333..333....', '....33....333...'];
const flip = (rows) => rows.map((r) => [...r].reverse().join(''));
const swap = (rows, swaps) => rows.map((r, y) => swaps[y] ?? r);

const CHARACTERS = {
  hunter: {
    palette: ['#f0d8a8', '#5f7a3a', '#141c10'],
    walk: {
      down: [body(HUNTER_FRONT, 0, HUNTER_LEGS), body(HUNTER_FRONT, 1, HUNTER_STEP), body(HUNTER_FRONT, 0, HUNTER_LEGS), body(HUNTER_FRONT, 1, flip(HUNTER_STEP))],
      up: [body(HUNTER_BACK, 0, HUNTER_LEGS), body(HUNTER_BACK, 1, HUNTER_STEP), body(HUNTER_BACK, 0, HUNTER_LEGS), body(HUNTER_BACK, 1, flip(HUNTER_STEP))],
      right: [
        body(HUNTER_SIDE, 0, HUNTER_STRIDE),
        body(HUNTER_SIDE, 1, HUNTER_PASS),
        body(HUNTER_SIDE, 0, HUNTER_STRIDE_2),
        body(HUNTER_SIDE, 1, HUNTER_PASS_2),
      ],
    },
    // Goggles dark.
    hurt: {
      down: body(swap(HUNTER_FRONT, { 4: '...3333333333...' }), 0, HUNTER_LEGS),
      up: body(HUNTER_BACK, 0, HUNTER_LEGS),
      right: body(swap(HUNTER_SIDE, { 4: '....33223333....' }), 0, HUNTER_STRIDE),
    },
    ball: [
      '................',
      '................',
      '.....333333.....',
      '....32211223....',
      '...3222222223...',
      '..333333333333..',
      '..331133113323..',
      '..322222222223..',
      '..321333333123..',
      '..322333333223..',
      '...3322332233...',
      '....33333333....',
      '................',
      '................',
      '................',
      '................',
    ],
  },
  hooded: {
    palette: ['#e0f8d0', '#346856', '#081820'],
    walk: {
      down: [body(HOODED_FRONT, 0, HOODED_LEGS), body(HOODED_FRONT, 1, HOODED_STEP), body(HOODED_FRONT, 0, HOODED_LEGS), body(HOODED_FRONT, 1, flip(HOODED_STEP))],
      up: [body(HOODED_BACK, 0, HOODED_LEGS), body(HOODED_BACK, 1, HOODED_STEP), body(HOODED_BACK, 0, HOODED_LEGS), body(HOODED_BACK, 1, flip(HOODED_STEP))],
      right: [
        body(HOODED_SIDE, 0, HOODED_STRIDE),
        body(HOODED_SIDE, 1, HOODED_PASS),
        body(HOODED_SIDE, 0, HOODED_STRIDE_2),
        body(HOODED_SIDE, 1, HOODED_PASS_2),
      ],
    },
    // Eyes shut.
    hurt: {
      down: body(swap(HOODED_FRONT, { 4: '...3233333323...' }), 0, HOODED_LEGS),
      up: body(HOODED_BACK, 0, HOODED_LEGS),
      right: body(swap(HOODED_SIDE, { 4: '.....32233333...' }), 0, HOODED_STRIDE),
    },
    ball: [
      '................',
      '................',
      '.....333333.....',
      '....32111123....',
      '...3211222223...',
      '...3213333323...',
      '...3231331323...',
      '..322333333223..',
      '..321222222223..',
      '..322222232223..',
      '...3333333333...',
      '....33333333....',
      '................',
      '................',
      '................',
      '................',
    ],
  },
  cyborg: {
    palette: ['#a8f0e8', '#7c8c84', '#18202a'],
    walk: {
      down: [body(CYBORG_FRONT, 0, CYBORG_LEGS), body(CYBORG_FRONT, 1, CYBORG_STEP), body(CYBORG_FRONT, 0, CYBORG_LEGS), body(CYBORG_FRONT, 1, flip(CYBORG_STEP))],
      up: [body(CYBORG_BACK, 0, CYBORG_LEGS), body(CYBORG_BACK, 1, CYBORG_STEP), body(CYBORG_BACK, 0, CYBORG_LEGS), body(CYBORG_BACK, 1, flip(CYBORG_STEP))],
      right: [
        body(CYBORG_SIDE, 0, CYBORG_STRIDE),
        body(CYBORG_SIDE, 1, CYBORG_PASS),
        body(CYBORG_SIDE, 0, CYBORG_STRIDE_2),
        body(CYBORG_SIDE, 1, CYBORG_PASS_2),
      ],
    },
    // The lens out.
    hurt: {
      down: body(swap(CYBORG_FRONT, { 3: '....31111123....' }), 0, CYBORG_LEGS),
      up: body(CYBORG_BACK, 0, CYBORG_LEGS),
      right: body(swap(CYBORG_SIDE, { 2: '......3313333...', 3: '......3313333...' }), 0, CYBORG_STRIDE),
    },
    ball: [
      '................',
      '................',
      '......3333......',
      '.....311123.....',
      '....31111123....',
      '....31331323....',
      '...3333223333...',
      '..322333333223..',
      '..321333333123..',
      '..322333333223..',
      '...3333333333...',
      '....33333333....',
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
