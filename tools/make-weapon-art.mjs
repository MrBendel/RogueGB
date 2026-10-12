// Builds a held weapon's 16 aiming directions from one right-facing drawing
// (a Sprite Studio template in harness/sprite-gen.mjs), plus the table the game
// needs to place it. Run it after editing a weapon's drawing:
//   node tools/make-weapon-art.mjs
//
// Writes, per weapon:
//   artwork/weapons/<name> (1..16).png   one 16x16 frame per direction (dir16 0 =
//                                        right, clockwise; y grows downward)
//   src/weapon_<name>.h                  per-direction placement (see below)
//
// Rotation is RotSprite-style, so small pixel art survives 22.5-degree steps:
// upscale 8x with Scale2x (three times), rotate with nearest-neighbour sampling,
// then sample each output pixel's centre. The gun turns around its grip (the
// "pivot"), which sits at the frame's point (8, 8). The right half of the circle
// is rotated; the left half is mirrored from it so the grip stays underneath,
// and mirroring about x = 8 means the converter stores those columns once.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import { parseAsciiGrid } from '../harness/sprite-gen.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT_ART = path.join(ROOT, 'artwork', 'weapons');

// Weapons. `right` is the drawing pointing right (shades 1-3, '.' transparent);
// `pivot` is the pixel the hand holds; `muzzle` is where shots and the flash come
// out. Adapted from the Sprite Studio's sidearm (WEAPON_TEMPLATES.small_pistol):
// the barrel is a pixel shorter so every direction fits within 7 px of the grip.
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
    pivot: { x: 7.5, y: 9.5 },   // the grip, between the two 3s of the handle
    muzzle: { x: 13.5, y: 7.5 }, // just past the barrel's end
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

/** The drawing rotated clockwise by `deg` around its pivot, pivot at (8, 8). */
function rotated(weapon, deg) {
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
      // Output pixel centre relative to the pivot, rotated back into the drawing.
      const ox = x + 0.5 - 8, oy = y + 0.5 - 8;
      const sx = weapon.pivot.x + (ox * cos + oy * sin);
      const sy = weapon.pivot.y + (-ox * sin + oy * cos);
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

/** The 16 direction frames: rotate the right half, mirror it for the left half. */
function directions(weapon) {
  const frames = new Array(16);
  for (const d of [0, 1, 2, 3, 4, 12, 13, 14, 15]) frames[d] = rotated(weapon, d * 22.5);
  for (const d of [5, 6, 7, 8, 9, 10, 11]) frames[d] = mirrorX(frames[(16 + 8 - d) % 16]);
  return frames;
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
function placement(weapon) {
  const rows = [];
  for (let d = 0; d < 16; d++) {
    const t = (d * 22.5 * Math.PI) / 180, cos = Math.cos(t), sin = Math.sin(t);
    const hx = Math.round(HAND.x + HAND.rx * cos), hy = Math.round(HAND.y + HAND.ry * sin);
    // The muzzle, rotated with the gun (mirrored on the left half, like the art).
    let mx = weapon.muzzle.x - weapon.pivot.x, my = weapon.muzzle.y - weapon.pivot.y;
    const left = d >= 5 && d <= 11;
    const rt = left ? ((8 - d) * 22.5 * Math.PI) / 180 : t;
    let rx = mx * Math.cos(rt) - my * Math.sin(rt), ry = mx * Math.sin(rt) + my * Math.cos(rt);
    if (left) rx = -rx;
    rows.push({
      gx: hx - 8, gy: hy - 8,  // the frame's top-left (the pivot is at 8, 8)
      mx: Math.round(hx + rx), my: Math.round(hy + ry),
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
static const int8_t weapon_${name}_gx[16] = {${list('gx')}};
static const int8_t weapon_${name}_gy[16] = {${list('gy')}};
static const int8_t weapon_${name}_mx[16] = {${list('mx')}};
static const int8_t weapon_${name}_my[16] = {${list('my')}};
static const int8_t weapon_${name}_kx[16] = {${list('kx')}};
static const int8_t weapon_${name}_ky[16] = {${list('ky')}};
static const uint8_t weapon_${name}_behind[16] = {${list('behind')}};
#endif
`;
}

fs.mkdirSync(OUT_ART, { recursive: true });
for (const [name, weapon] of Object.entries(WEAPONS)) {
  const frames = directions(weapon);
  frames.forEach((g, d) => fs.writeFileSync(path.join(OUT_ART, `${name} (${d + 1}).png`), png(g)));
  fs.writeFileSync(path.join(ROOT, 'src', `weapon_${name}.h`), header(name, placement(weapon)));
  console.log(`[weapon-art] ${name}: 16 directions -> artwork/weapons/, src/weapon_${name}.h`);
  if (process.argv.includes('--show')) {
    frames.forEach((g, d) => {
      console.log(`dir ${d}`);
      for (let y = 0; y < 16; y++) console.log('  ' + Array.from(g.slice(y * 16, y * 16 + 16), (v) => '.123'[v]).join(''));
    });
  }
}
