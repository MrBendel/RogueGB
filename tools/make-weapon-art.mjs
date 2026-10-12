// Builds each held weapon's 16 aiming directions from one right-facing drawing,
// plus the table the game needs to place it. Run it after editing a drawing:
//   node tools/make-weapon-art.mjs [--show]
//
// Writes, per weapon:
//   artwork/weapons/<name> (1..16).png   one 16x16 frame per direction (dir16 0 =
//                                        right, clockwise; y grows downward)
//   src/weapon_<name>.h                  per-direction placement (see header())
//
// Rotation is RotSprite-style, so small pixel art survives 22.5-degree steps:
// upscale 8x with Scale2x (three times), rotate with nearest-neighbour sampling,
// then sample each output pixel's centre. A weapon turns around the middle of its
// drawing (so even a 15-px rifle fits the 16x16 frame at every angle); the table
// then places each frame so the grip lands in the player's hand. The right half
// of the circle is rotated; the left half is mirrored from it so the grip stays
// underneath, and mirroring about x = 8 means the converter stores those columns
// once.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { parseAsciiGrid } from '../harness/sprite-gen.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT_ART = path.join(ROOT, 'artwork', 'weapons');

// Weapons, each drawn pointing right (shades 1-3, '.' transparent). `grip` is the
// pixel the hand holds; `muzzle` is where shots and the flash come out (for the
// sword, the tip). The pistol and shotgun are adapted from the Sprite Studio
// templates (small_pistol, big_shotgun in harness/sprite-gen.mjs); the rifle
// and sniper are new; the sword is a slimmer take on the Volt Sabre
// (melee_sword), so it keeps its shape at every swing angle.
const WEAPONS = {
  pistol: {
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
      '.......3333333..',
      '......32111112..',
      '......32233333..',
      '......3233......',
      '......333.......',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    grip: { x: 7.5, y: 9.5 },
    muzzle: { x: 13.5, y: 7.5 },
  },
  // Double-barrel: long and low, a stock behind the grip.
  shotgun: {
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '.....3333333333.',
      '..33311111111123',
      '.322222223333333',
      '.32233223.......',
      '..33..33........',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    grip: { x: 6.5, y: 8.5 },
    muzzle: { x: 15.5, y: 6.5 },
  },
  // Assault rifle: stock, receiver with a sight rail, magazine, long barrel.
  rifle: {
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '.......333......',
      '3...33322233333.',
      '3333311112111113',
      '3223333233333...',
      '.33..323........',
      '......33........',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    grip: { x: 6.5, y: 9.5 },
    muzzle: { x: 15.5, y: 7.5 },
  },
  // Long rifle with a scope on top.
  sniper: {
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '.....3333.......',
      '....311113......',
      '.33333333333333.',
      '3222221111111113',
      '.3333223333333..',
      '....3223........',
      '....333.........',
      '................',
      '................',
      '................',
      '................',
    ]),
    grip: { x: 5.5, y: 10.5 },
    muzzle: { x: 15.5, y: 8.5 },
  },
  // Sabre: pommel and grip, a crossguard, and a blade with a bright core.
  sword: {
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '....3...........',
      '...3233333333...',
      '3323121111111333',
      '...3233333333...',
      '....3...........',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    grip: { x: 1.5, y: 7.5 },
    muzzle: { x: 15.5, y: 7.5 },
    // Also drawn with motion-smear wings, shown while a swing can hit (see smeared()).
    smear: { centre: 7 },
  },
};

// Where the hand is, relative to the player's 16x16 frame, for each aim
// direction: around the body's middle (8, 10), 4 px out sideways, 3 px up/down.
const HAND = { x: 8, y: 10, rx: 4, ry: 3 };
// Directions drawn behind the body (pointing up and away from the camera).
const BEHIND = new Set([10, 11, 12, 13, 14]);
// Recoil kick per direction: 2 px back along the aim, rounded.
const KICK = 2;

const COLORS = [[0, 0, 0, 0], [255, 255, 255, 255], [128, 128, 128, 255], [0, 0, 0, 255]];

/** Scale2x (EPX) on a w x h grid of shade values. */
function scale2x(src, w, h) {
  const out = new Uint8Array(w * h * 4);
  const at = (x, y) => src[Math.min(h - 1, Math.max(0, y)) * w + Math.min(w - 1, Math.max(0, x))];
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const p = at(x, y), a = at(x, y - 1), b = at(x + 1, y), c = at(x - 1, y), d = at(x, y + 1);
      const o = (y * 2) * (w * 2) + x * 2;
      out[o] = c === a && c !== d && a !== b ? a : p;
      out[o + 1] = a === b && a !== c && b !== d ? b : p;
      out[o + w * 2] = d === c && d !== b && c !== a ? c : p;
      out[o + w * 2 + 1] = b === d && b !== a && d !== c ? d : p;
    }
  }
  return out;
}

/** The middle of the drawing's opaque pixels (rounded): what it turns around. */
function centre(grid) {
  let x0 = 16, x1 = -1, y0 = 16, y1 = -1;
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    if (!grid[y * 16 + x]) continue;
    x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y);
  }
  // On a pixel corner, like the frame's (8, 8), so 0, 90, 180 and 270 degrees
  // reproduce the drawing exactly (a half-pixel centre would resample it).
  return { x: Math.round((x0 + x1 + 1) / 2), y: Math.round((y0 + y1 + 1) / 2) };
}

/** The drawing rotated clockwise by `deg` around its centre, placed at (8, 8). */
function rotated(weapon, c, deg) {
  let big = weapon.right, size = 16;
  for (let i = 0; i < 3; i++) {
    big = scale2x(big, size, size);
    size *= 2;
  }
  const s = size / 16;  // 8
  const t = (deg * Math.PI) / 180, cos = Math.cos(t), sin = Math.sin(t);
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      // Output pixel centre relative to (8, 8), rotated back into the drawing.
      const ox = x + 0.5 - 8, oy = y + 0.5 - 8;
      const sx = c.x + (ox * cos + oy * sin);
      const sy = c.y + (-ox * sin + oy * cos);
      const bx = Math.floor(sx * s), by = Math.floor(sy * s);
      if (bx >= 0 && bx < size && by >= 0 && by < size) out[y * 16 + x] = big[by * size + bx];
    }
  }
  return out;
}

const mirrorX = (g) => {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) out[y * 16 + x] = g[y * 16 + 15 - x];
  return out;
};

// The Sprite Studio's smear: jagged energy wings flaring above and below the
// blade, the frame Oracle-style swings show at full extension. The wing pattern
// is synthesizeSmearRightFromRight's (harness/sprite-gen.mjs: [x, row, shade],
// rows 0-3 above, mirrored below); it was laid out for the thicker Volt Sabre, so
// here it's moved to hug a blade whose middle row is `centre` (the top wing ends
// on the row above the blade's top edge, the bottom one mirrors it).
const SMEAR_WING = [
  [6, 1, 2], [7, 1, 1], [8, 1, 3], [11, 0, 1],
  [7, 2, 3], [8, 2, 1], [9, 2, 2], [10, 2, 1], [11, 2, 3],
  [8, 3, 2], [9, 3, 3], [10, 3, 1], [11, 3, 2], [12, 3, 1], [13, 3, 3],
];
function smeared(grid, centre) {
  const out = grid.slice();
  // The wings' last row (3) goes just above the blade's top edge, centre - 1.
  const shift = centre - 1 - 1 - 3;
  for (const [x, y, c] of SMEAR_WING) {
    const top = y + shift, bottom = 2 * centre - top;
    for (const row of [top, bottom]) if (row >= 0 && row < 16 && !out[row * 16 + x]) out[row * 16 + x] = c;
  }
  return out;
}

const RIGHT_HALF = [0, 1, 2, 3, 4, 12, 13, 14, 15];
const isLeft = (d) => d >= 5 && d <= 11;
const mirrorOf = (d) => (16 + 8 - d) % 16;

/** The 16 direction frames: rotate the right half, mirror it for the left half. */
function directions(weapon, c) {
  const frames = new Array(16);
  for (const d of RIGHT_HALF) frames[d] = rotated(weapon, c, d * 22.5);
  for (let d = 0; d < 16; d++) if (isLeft(d)) frames[d] = mirrorX(frames[mirrorOf(d)]);
  return frames;
}

/** A vector from the drawing turned to direction d, as the frames are (mirrored on the left). */
function turn(v, d) {
  const left = isLeft(d);
  const t = ((left ? mirrorOf(d) : d) * 22.5 * Math.PI) / 180;
  const x = v.x * Math.cos(t) - v.y * Math.sin(t), y = v.x * Math.sin(t) + v.y * Math.cos(t);
  return { x: left ? -x : x, y };
}

function png(grid) {
  const raw = Buffer.alloc((16 * 4 + 1) * 16);
  for (let y = 0; y < 16; y++) {
    raw[y * 65] = 0;
    for (let x = 0; x < 16; x++) raw.set(COLORS[grid[y * 16 + x]], y * 65 + 1 + x * 4);
  }
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
  ihdr[8] = 8;
  ihdr[9] = 6;
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0)),
  ]);
}

/** Per-direction placement, relative to the player's 16x16 frame top-left. */
function placement(weapon, c) {
  const rows = [];
  for (let d = 0; d < 16; d++) {
    const t = (d * 22.5 * Math.PI) / 180, cos = Math.cos(t), sin = Math.sin(t);
    const hx = HAND.x + HAND.rx * cos, hy = HAND.y + HAND.ry * sin;
    // The frame goes where the grip lands in the hand; the muzzle turns with it.
    const g = turn({ x: weapon.grip.x - c.x, y: weapon.grip.y - c.y }, d);
    const m = turn({ x: weapon.muzzle.x - c.x, y: weapon.muzzle.y - c.y }, d);
    const fx = hx - g.x, fy = hy - g.y;  // the frame's centre (8, 8)
    rows.push({
      gx: Math.round(fx - 8), gy: Math.round(fy - 8),
      mx: Math.round(fx + m.x), my: Math.round(fy + m.y),
      kx: Math.round(-KICK * cos), ky: Math.round(-KICK * sin),
      behind: BEHIND.has(d) ? 1 : 0,
    });
  }
  return rows;
}

function header(name, rows) {
  const N = name.toUpperCase();
  const list = (key) => rows.map((r) => String(r[key]).padStart(3)).join(',');
  return `// Generated by tools/make-weapon-art.mjs from its drawing of the ${name}. Do not edit;
// edit the drawing there and run it again. Per aim direction (dir16, 0 = right,
// clockwise), relative to the player's 16x16 frame top-left:
//   GX/GY  the weapon frame's top-left     MX/MY  the muzzle (shots, flash)
//   KX/KY  the recoil kick (px)             BEHIND drawn behind the body
#ifndef WEAPON_${N}_H
#define WEAPON_${N}_H
#include <stdint.h>
static const int8_t weapon_${name}_place[7][16] = {
    {${list('gx')}},  // GX
    {${list('gy')}},  // GY
    {${list('mx')}},  // MX
    {${list('my')}},  // MY
    {${list('kx')}},  // KX
    {${list('ky')}},  // KY
    {${list('behind')}},  // BEHIND
};
#endif
`;
}

fs.mkdirSync(OUT_ART, { recursive: true });
for (const [name, weapon] of Object.entries(WEAPONS)) {
  const c = centre(weapon.right);
  const frames = directions(weapon, c);
  frames.forEach((g, d) => fs.writeFileSync(path.join(OUT_ART, `${name} (${d + 1}).png`), png(g)));
  fs.writeFileSync(path.join(ROOT, 'src', `weapon_${name}.h`), header(name, placement(weapon, c)));
  console.log(`[weapon-art] ${name}: 16 directions -> artwork/weapons/, src/weapon_${name}.h`);
  if (weapon.smear) {
    // Turned around the same centre as the plain drawing, so the placement table fits both.
    const sm = directions({ ...weapon, right: smeared(weapon.right, weapon.smear.centre) }, c);
    sm.forEach((g, d) => fs.writeFileSync(path.join(OUT_ART, `${name}_smear (${d + 1}).png`), png(g)));
    console.log(`[weapon-art] ${name}_smear: 16 directions -> artwork/weapons/`);
    if (process.argv.includes('--show')) {
      for (const d of [0, 2]) {
        console.log(`${name}_smear dir ${d}`);
        for (let y = 0; y < 16; y++) console.log('  ' + Array.from(sm[d].slice(y * 16, y * 16 + 16), (v) => '.123'[v]).join(''));
      }
    }
  }
  if (process.argv.includes('--show')) {
    frames.forEach((g, d) => {
      if (![0, 2, 4, 14].includes(d)) return;
      console.log(`${name} dir ${d}`);
      for (let y = 0; y < 16; y++) {
        const row = Array.from(g.slice(y * 16, y * 16 + 16), (v) => '.123'[v]).join('');
        if (row !== '................') console.log('  ' + row);
      }
    });
  }
}
