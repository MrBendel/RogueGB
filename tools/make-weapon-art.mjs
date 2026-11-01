// Builds each held weapon's 16 aiming directions from one right-facing drawing,
// plus the table the game needs to place it. Run it after editing a drawing:
//   node tools/make-weapon-art.mjs [--show]
//
// Writes, per weapon:
//   artwork/weapons/<name> (1..16).png   one 16x16 frame per direction (dir16 0 =
//                                        right, clockwise; y grows downward); the
//                                        sword's are its facings x poses (swingFrames())
//   src/weapon_<name>.h                  per-frame placement (see header())
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
  // The guns are drawn as solid parts, [x0, y0, x1, y1, shade] in a right-facing
  // 16x16 drawing (half-open; the outline goes around them, so leave a pixel for
  // it): shade 2 is the body (its top edge turns into the bright glint), 1 always
  // bright (a lens), 4 a dark detail. Each direction turns the parts, then outlines
  // and shades them (parted()), so every angle gets a closed outline and a glint
  // on the gun's top, which a turned drawing doesn't keep.
  // Blaster pistol: a slide, the grip under its back half, the trigger.
  pistol: {
    parts: [[3, 5, 12, 7, 2], [4, 7, 7, 10, 2], [8, 7, 9, 8, 4]],
    // (the muzzle 6 px ahead of the grip and 2 up, as the first pistol's: the same shots)
    // Seen from above (aiming up or down the screen): the slide, its rear sight, the bore.
    top: [[3, 5, 12, 8, 2], [4, 6, 5, 7, 4], [11, 6, 12, 7, 4]],
    grip: { x: 5.5, y: 8.5 },
    muzzle: { x: 11.5, y: 6.5 },
  },
  // Pump shotgun: a stock, a fat barrel, the grooved pump under it.
  shotgun: {
    parts: [[1, 6, 4, 9, 2], [4, 5, 15, 7, 2], [6, 7, 12, 9, 2], [8, 7, 9, 9, 4], [10, 7, 11, 9, 4]],
    // Seen from above (aiming up or down the screen): stock, barrel, the wider pump, the bore.
    top: [[1, 5, 4, 8, 2], [4, 5, 15, 7, 2], [6, 4, 12, 8, 2], [8, 4, 9, 8, 4], [10, 4, 11, 8, 4], [14, 5, 15, 7, 4]],
    grip: { x: 4.5, y: 7.5 },
    muzzle: { x: 15.5, y: 5.5 },
  },
  // Assault rifle: stock, receiver with a sight, thin barrel, magazine.
  rifle: {
    parts: [[0, 6, 3, 9, 2], [3, 5, 11, 8, 2], [11, 6, 15, 7, 2], [7, 8, 9, 11, 2], [6, 4, 9, 5, 2]],
    // Seen from above (aiming up or down the screen): stock, receiver with its sight rail, barrel, bore.
    top: [[0, 5, 3, 8, 2], [3, 5, 11, 8, 2], [11, 6, 15, 7, 2], [5, 6, 9, 7, 4], [14, 6, 15, 7, 4]],
    grip: { x: 5.5, y: 8.5 },
    muzzle: { x: 15.5, y: 6.5 },
  },
  // Sniper: a stock, a long thin barrel, the scope (its lens bright) over it.
  sniper: {
    parts: [[0, 8, 3, 11, 2], [3, 8, 9, 10, 2], [9, 8, 15, 9, 2], [5, 5, 11, 7, 2], [10, 5, 11, 7, 1], [5, 10, 7, 12, 2]],
    // Seen from above (aiming up or down the screen): stock, body, the wide scope (its lens), barrel, bore.
    top: [[0, 7, 3, 10, 2], [3, 7, 9, 10, 2], [9, 8, 15, 9, 2], [5, 6, 11, 11, 2], [10, 6, 11, 11, 1], [5, 6, 6, 11, 4], [14, 8, 15, 9, 4]],
    grip: { x: 5.5, y: 10.5 },
    muzzle: { x: 15.5, y: 8.5 },
  },
  // Energy sabre: a dark hilt, a crossguard, a two-row blade (its top edge the glint).
  sword: {
    parts: [[1, 7, 4, 9, 4], [4, 5, 5, 11, 2], [5, 7, 15, 9, 2]],
    grip: { x: 2.5, y: 8.5 },
    muzzle: { x: 15.5, y: 7.5 },
    // It doesn't aim: it swings the way you face (aim.c), so its 16 frames are
    // 4 facings (right, down, left, up) x 4 poses (swingFrames()). The swing's
    // frame has motion-smear wings (smeared()) around the blade's middle row.
    smear: { centre: 8 },
    swing: true,
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

/** The parts' outlined middle, on a pixel corner (what a gun turns around). */
function partsCentre(parts) {
  const x0 = Math.min(...parts.map((p) => p[0])) - 1, x1 = Math.max(...parts.map((p) => p[2])) + 1;
  const y0 = Math.min(...parts.map((p) => p[1])) - 1, y1 = Math.max(...parts.map((p) => p[3])) + 1;
  return { x: Math.round((x0 + x1) / 2), y: Math.round((y0 + y1) / 2) };
}

/** The parts turned clockwise by `deg` around c, placed at (8, 8): filled, then
 * outlined (4 neighbours), the body's lit edge bright. */
function parted(parts, c, deg) {
  const t = (deg * Math.PI) / 180, cos = Math.cos(t), sin = Math.sin(t);
  const fill = new Uint8Array(256);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const ox = x + 0.5 - 8, oy = y + 0.5 - 8;
    const lx = c.x + (ox * cos + oy * sin), ly = c.y + (-ox * sin + oy * cos);
    for (const [x0, y0, x1, y1, shade] of parts) if (lx >= x0 && lx < x1 && ly >= y0 && ly < y1) fill[y * 16 + x] = shade;
  }
  // The glint's side: the drawing's up or down, whichever turns toward the light
  // (from the upper left).
  let ux = sin, uy = -cos;
  if (ux + uy > 0.01) ux = -ux, uy = -uy;
  ux = Math.round(ux), uy = Math.round(uy);
  const at = (x, y) => (x >= 0 && x < 16 && y >= 0 && y < 16 ? fill[y * 16 + x] : 0);
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const v = fill[y * 16 + x];
    if (v === 4) out[y * 16 + x] = 3;
    else if (v === 1) out[y * 16 + x] = 1;
    else if (v) out[y * 16 + x] = at(x + ux, y + uy) ? 2 : 1;
    else if (at(x + 1, y) || at(x - 1, y) || at(x, y + 1) || at(x, y - 1)) out[y * 16 + x] = 3;
  }
  return out;
}

/** The parts as the right-facing drawing (in its own pixels: what a smear is added to). */
const partsDrawing = (parts) => parted(parts, { x: 8, y: 8 }, 0);

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
// Aiming steeply up or down the screen, a gun shows its top (weapon.top), not its side.
const TOP_DIRS = new Set([3, 4, 12, 13]);
const isLeft = (d) => d >= 5 && d <= 11;
const mirrorOf = (d) => (16 + 8 - d) % 16;

/** The 16 direction frames: rotate the right half, mirror it for the left half. */
function directions(weapon, c) {
  const frames = new Array(16);
  for (const d of RIGHT_HALF) {
    const parts = weapon.top && TOP_DIRS.has(d) ? weapon.top : weapon.parts;
    frames[d] = parts ? parted(parts, c, d * 22.5) : rotated(weapon, c, d * 22.5);
  }
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

// The sword's poses, per facing: at rest (by your side), the windup, the swing
// (smeared: the frame that can hit), the follow-through. Blade angles in degrees
// clockwise from right; left mirrors right. Facing up, it's behind the body.
// Frame 0 (right, at rest) is also its icon on the floor.
const FACINGS = [
  { hand: 0, poses: [67.5, -90, 0, 67.5] },     // right
  { hand: 90, poses: [112.5, 0, 90, 180] },     // down: across the front, right to left
  null,                                          // left: the right's, mirrored
  { hand: 270, poses: [112.5, 180, 270, 360], behind: 1 },  // up: over the head, left to right
];

/** The sword's 16 frames (facing x 4 + pose) and their placement rows. */
function swingFrames(weapon, c) {
  const drawing = partsDrawing(weapon.parts);
  const smear = smeared(drawing, weapon.smear.centre);
  const frames = new Array(16), rows = new Array(16);
  for (let f = 0; f < 4; f++) {
    const face = FACINGS[f === 2 ? 0 : f];
    const t = (face.hand * Math.PI) / 180;
    const hx = HAND.x + HAND.rx * Math.cos(t), hy = HAND.y + HAND.ry * Math.sin(t);
    face.poses.forEach((deg, p) => {
      let g = p === 2 ? rotated({ right: smear }, c, deg) : parted(weapon.parts, c, deg);
      const r = (deg * Math.PI) / 180, turn2 = (v) => ({ x: v.x * Math.cos(r) - v.y * Math.sin(r), y: v.x * Math.sin(r) + v.y * Math.cos(r) });
      const gr = turn2({ x: weapon.grip.x - c.x, y: weapon.grip.y - c.y }), tip = turn2({ x: weapon.muzzle.x - c.x, y: weapon.muzzle.y - c.y });
      let fx = hx - gr.x, fy = hy - gr.y;
      let row = { gx: Math.round(fx - 8), gy: Math.round(fy - 8), mx: Math.round(fx + tip.x), my: Math.round(fy + tip.y), kx: 0, ky: 0, behind: face.behind || 0 };
      if (f === 2) {  // mirrored about the player's middle
        g = mirrorX(g);
        row = { ...row, gx: -row.gx, mx: 16 - row.mx };
      }
      frames[f * 4 + p] = g;
      rows[f * 4 + p] = row;
    });
  }
  return { frames, rows };
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
  const c = weapon.parts ? partsCentre(weapon.parts) : centre(weapon.right);
  if (weapon.swing) {
    const { frames, rows } = swingFrames(weapon, c);
    frames.forEach((g, d) => fs.writeFileSync(path.join(OUT_ART, `${name} (${d + 1}).png`), png(g)));
    fs.writeFileSync(path.join(ROOT, 'src', `weapon_${name}.h`), header(name, rows));
    console.log(`[weapon-art] ${name}: 4 facings x 4 poses -> artwork/weapons/, src/weapon_${name}.h`);
    continue;
  }
  const frames = directions(weapon, c);
  frames.forEach((g, d) => fs.writeFileSync(path.join(OUT_ART, `${name} (${d + 1}).png`), png(g)));
  fs.writeFileSync(path.join(ROOT, 'src', `weapon_${name}.h`), header(name, placement(weapon, c)));
  console.log(`[weapon-art] ${name}: 16 directions -> artwork/weapons/, src/weapon_${name}.h`);
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
