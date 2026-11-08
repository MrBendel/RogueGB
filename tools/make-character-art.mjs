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
import { decodePng } from './assets.mjs';

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
  // The Shadow cloak: the user's own drawing (artwork/hooded/shadow cloak sheet.png),
  // a sheet of 16x16 frames, 4 across: rows walk down, up, left and right, then a
  // roll (right), and, when drawn, a roll down and a roll up.
  hooded: {
    palette: ['#b8c8f0', '#3c4878', '#0c1020'],  // shadow indigo
    sheet: 'artwork/hooded/shadow cloak sheet.png',
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
/** Every animation from a character's walk.{down,up,right}, hurt.{down,up,right} and ball (or its sheet). */
function frames(ch) {
  if (ch.sheet) return sheetFrames(ch);
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

// A hand-drawn sheet's frames: each is cut from its own spot (the frames aren't on
// an exact grid), its top at its row's top (so a step's bob stays), and placed
// across its 16 columns where its top half best overlays the row's first frame
// (centred), so the head doesn't wobble through a walk.
function cutSheet(file) {
  const img = decodePng(fs.readFileSync(path.join(ROOT, file)));
  const W = img.width, H = img.height, on = (x, y) => img.rgba[(y * W + x) * 4 + 3] > 127;
  const runs = (n, empty) => {
    const r = [];
    let s = -1;
    for (let i = 0; i <= n; i++) {
      const e = i === n || empty(i);
      if (!e && s < 0) s = i;
      if (e && s >= 0) { r.push([s, i - 1]); s = -1; }
    }
    return r;
  };
  const cols = runs(W, (x) => { for (let y = 0; y < H; y++) if (on(x, y)) return false; return true; });
  const rows = runs(H, (y) => { for (let x = 0; x < W; x++) if (on(x, y)) return false; return true; });
  // Shades by brightness: lightest 1, then 2, darkest 3.
  const lum = new Map();
  for (let i = 0; i < W * H; i++) if (img.rgba[i * 4 + 3] > 127) lum.set(img.rgba[i * 4] * 3 + img.rgba[i * 4 + 1] * 6 + img.rgba[i * 4 + 2], 0);
  const order = [...lum.keys()].sort((p, q) => q - p);
  if (order.length > 3) throw new Error(`${file}: ${order.length} colours, not 3`);
  const shade = (x, y) => (on(x, y) ? 1 + order.indexOf(img.rgba[(y * W + x) * 4] * 3 + img.rgba[(y * W + x) * 4 + 1] * 6 + img.rgba[(y * W + x) * 4 + 2]) : 0);
  return rows.map(([y0, y1]) => {
    if (y1 - y0 > 15) throw new Error(`${file}: a row ${y1 - y0 + 1} tall`);
    let first = null;
    return cols.map(([c0, c1]) => {
      // (this frame's own columns: a column of frames is as wide as its widest)
      let x0 = c1, x1 = c0;
      for (let y = y0; y <= y1; y++) for (let x = c0; x <= c1; x++) if (on(x, y)) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); }
      const w = x1 - x0 + 1;
      if (w > 16) throw new Error(`${file}: a frame ${w} wide`);
      const at = (ox) => Array.from({ length: 16 }, (_, y) => Array.from({ length: 16 }, (_, x) => {
        const sx = x0 + x - ox, sy = y0 + y;
        return sx >= x0 && sx <= x1 && sy <= y1 ? shade(sx, sy) : 0;
      }));
      let best = at((16 - w) >> 1);
      if (first) {
        let score = -1;
        for (let ox = 0; ox <= 16 - w; ox++) {
          const g = at(ox);
          let n = 0;
          for (let y = 0; y < 8; y++) for (let x = 0; x < 16; x++) if (g[y][x] && g[y][x] === first[y][x]) n++;
          if (n > score) { score = n; best = g; }
        }
      }
      first ??= best;
      return best;
    });
  });
}

function sheetFrames(ch) {
  const [down, up, left, right, roll, rollDown, rollUp] = cutSheet(ch.sheet);
  const out = { 'walk down': down, 'walk up': up, 'walk right': right, 'walk left': left };
  // Knocked back a pixel, away from the way you face (a standing frame).
  out['hurt down'] = [...down[0].slice(1), blank()];
  out['hurt up'] = [blank(), ...up[0].slice(0, 15)];
  out['hurt right'] = right[0].map((r) => [...r.slice(1), 0]);
  out.falling = [down[0], right[0], up[0], left[0]];
  out['roll right'] = roll;
  out['roll down'] = rollDown ?? roll;  // (rows 6 and 7 when drawn; till then the side roll)
  out['roll up'] = rollUp ?? roll;
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
  'walk left': (i) => `player walk left (${i + 1}).png`,
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
  const count = (f) => order.reduce((n, a) => n + (!f[a] ? 0 : isList(f[a]) ? f[a].length : 1), 0);
  const perRow = Math.max(...names.map((n) => count(all[n].f)));
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
    for (const a of order.filter((a) => f[a])) for (const g of isList(f[a]) ? f[a] : [f[a]]) { blit(g, pal, x, y0, S); x += 16 * S + PAD; }
    const y1 = y0 + 16 * S + PAD;
    x = PAD;
    for (const a of ['walk down', 'walk right', 'walk up', 'walk left'].filter((a) => f[a])) for (const g of f[a]) { blit(g, pal, x, y1, 1); x += 18; }
    x += 20;
    for (const a of ['walk down', 'walk right', 'walk up', 'walk left'].filter((a) => f[a])) for (const g of f[a]) { blit(g, pal, x, y1, 3); x += 16 * 3 + 2; }
  });
  const out = path.join(ROOT, 'build', 'characters.png');
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.writeFileSync(out, png(W, H, (x, y) => img[y][x]));
  console.log(`wrote ${out}`);
}
