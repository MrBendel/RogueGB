// Procedural Game Boy (DMG 2bpp, 16x16 / 8x16) character animation & weapon
// synthesizer. Shared between the browser Sprite Studio and Node build tools.
//
// Shade values in a 16x16 grid (Uint8Array of length 256):
//   0 = transparent
//   1 = light   (#c4cfa1 / #e0f8d0)
//   2 = medium  (#8b956d / #88c070)
//   3 = dark    (#1f2420 / #081820)

export const S_FLIPX = 0x20;
export const S_FLIPY = 0x40;

export const DMG_PALETTES = {
  dmg_olive: [
    [0, 0, 0, 0],
    [0xe0, 0xf0, 0xc8, 255],
    [0x8b, 0x95, 0x6d, 255],
    [0x1f, 0x24, 0x20, 255],
  ],
  dmg_pocket: [
    [0, 0, 0, 0],
    [0xf4, 0xf3, 0xee, 255],
    [0x97, 0x9d, 0x95, 255],
    [0x1f, 0x24, 0x20, 255],
  ],
  dmg_classic: [
    [0, 0, 0, 0],
    [0x9b, 0xbc, 0x0f, 255],
    [0x30, 0x62, 0x30, 255],
    [0x0f, 0x38, 0x0f, 255],
  ],
};

/** Parse a 16x16 ASCII art template ('.' or '0' = 0, '1' = 1, '2' = 2, '3' = 3). */
export function parseAsciiGrid(rows) {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    const row = rows[y] || '';
    for (let x = 0; x < 16; x++) {
      const ch = row[x] || '.';
      out[y * 16 + x] = ch === '1' ? 1 : ch === '2' ? 2 : ch === '3' ? 3 : 0;
    }
  }
  return out;
}

export function gridToAscii(grid) {
  const rows = [];
  for (let y = 0; y < 16; y++) {
    let s = '';
    for (let x = 0; x < 16; x++) {
      const v = grid[y * 16 + x];
      s += v === 0 ? '.' : String(v);
    }
    rows.push(s);
  }
  return rows;
}

export function cloneGrid(g) {
  return new Uint8Array(g);
}

/** Mirror a 16x16 grid horizontally. */
export function flipGridX(g) {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = g[y * 16 + (15 - x)];
    }
  }
  return out;
}

/** Mirror a 16x16 grid vertically. */
export function flipGridY(g) {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = g[(15 - y) * 16 + x];
    }
  }
  return out;
}

/** Rotate a 16x16 grid 90 degrees clockwise. */
export function rotateGrid90(g) {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = g[(15 - x) * 16 + y];
    }
  }
  return out;
}

/** Quantize an arbitrary RGBA image buffer into a 16x16 4-shade DMG grid (0..3). */
export function quantizeRgbaToGrid16(rgba, width, height) {
  // Resample to 16x16 using nearest-neighbor if dimensions differ.
  const sample = new Uint8Array(16 * 16 * 4);
  for (let y = 0; y < 16; y++) {
    const sy = Math.min(height - 1, Math.floor((y + 0.5) * height / 16));
    for (let x = 0; x < 16; x++) {
      const sx = Math.min(width - 1, Math.floor((x + 0.5) * width / 16));
      const si = (sy * width + sx) * 4;
      const di = (y * 16 + x) * 4;
      sample[di] = rgba[si];
      sample[di + 1] = rgba[si + 1];
      sample[di + 2] = rgba[si + 2];
      sample[di + 3] = rgba[si + 3];
    }
  }
  const lumOf = (r, g, b) => 0.299 * r + 0.587 * g + 0.114 * b;
  const lumMap = new Map();
  const pixelLums = new Float32Array(256);
  for (let i = 0; i < 256; i++) {
    const a = sample[i * 4 + 3];
    if (a < 128) {
      pixelLums[i] = -1;
      continue;
    }
    const r = sample[i * 4], g = sample[i * 4 + 1], b = sample[i * 4 + 2];
    const l = lumOf(r, g, b);
    pixelLums[i] = l;
    lumMap.set((r << 16) | (g << 8) | b, l);
  }
  const out = new Uint8Array(256);
  if (lumMap.size === 0) return out;

  if (lumMap.size <= 3) {
    const ranked = [...lumMap.entries()].sort((a, b) => b[1] - a[1]).map(([k]) => k);
    const colorIdx = new Map(ranked.map((k, idx) => [k, ranked.length === 1 ? 3 : ranked.length === 2 ? (idx === 0 ? 1 : 3) : idx + 1]));
    for (let i = 0; i < 256; i++) {
      if (pixelLums[i] < 0) continue;
      const k = (sample[i * 4] << 16) | (sample[i * 4 + 1] << 8) | sample[i * 4 + 2];
      out[i] = colorIdx.get(k) || 3;
    }
    return out;
  }

  // More than 3 opaque colors: quantize luminance by tertiles into 1 (light), 2 (mid), 3 (dark).
  const validLums = [...pixelLums].filter((l) => l >= 0).sort((a, b) => a - b);
  const t1 = validLums[Math.floor(validLums.length * 0.34)];
  const t2 = validLums[Math.floor(validLums.length * 0.68)];
  for (let i = 0; i < 256; i++) {
    const l = pixelLums[i];
    if (l < 0) out[i] = 0;
    else if (l <= t1) out[i] = 3;
    else if (l <= t2) out[i] = 2;
    else out[i] = 1;
  }
  return out;
}

/** Analyze a 16x16 character sprite's bounding box and anatomy splits (head, torso, legs). */
export function analyzeAnatomy(grid) {
  let topY = 16, botY = -1, leftX = 16, rightX = -1;
  const rowWidths = new Uint8Array(16);
  for (let y = 0; y < 16; y++) {
    let rMin = 16, rMax = -1;
    for (let x = 0; x < 16; x++) {
      if (grid[y * 16 + x] !== 0) {
        if (y < topY) topY = y;
        if (y > botY) botY = y;
        if (x < leftX) leftX = x;
        if (x > rightX) rightX = x;
        if (x < rMin) rMin = x;
        if (x > rMax) rMax = x;
      }
    }
    rowWidths[y] = rMax >= rMin ? rMax - rMin + 1 : 0;
  }
  if (botY < topY) return { topY: 1, botY: 14, leftX: 2, rightX: 13, centerX: 7.5, headEnd: 9, legStart: 13 };
  const h = botY - topY + 1;
  const headEnd = Math.min(botY - 3, Math.max(topY + 4, topY + Math.round(h * 0.6)));
  const legStart = Math.max(headEnd + 2, botY - 2);
  const centerX = (leftX + rightX) / 2;
  return { topY, botY, leftX, rightX, centerX, headEnd, legStart, rowWidths };
}

/** Synthesize a back-facing (UP) 16x16 sprite from a front-facing (DOWN) sprite. */
export function synthesizeUpFromDown(downGrid) {
  const out = cloneGrid(downGrid);
  const { topY, headEnd, legStart, leftX, rightX } = analyzeAnatomy(downGrid);
  // Find dominant mid/hair shade in the top half of the head.
  let count1 = 0, count2 = 0;
  for (let y = topY; y <= headEnd; y++) {
    for (let x = leftX; x <= rightX; x++) {
      const v = downGrid[y * 16 + x];
      if (v === 1) count1++;
      if (v === 2) count2++;
    }
  }
  const backShade = count2 >= count1 ? 2 : 1;
  // Replace interior face features (eyes/mouth) in rows topY+2 .. headEnd with back-of-head shading.
  for (let y = topY + 2; y <= headEnd; y++) {
    let rMin = 16, rMax = -1;
    for (let x = 0; x < 16; x++) {
      if (downGrid[y * 16 + x] !== 0) {
        if (x < rMin) rMin = x;
        if (x > rMax) rMax = x;
      }
    }
    for (let x = rMin + 1; x <= rMax - 1; x++) {
      if (out[y * 16 + x] === 0) continue;
      if (y === headEnd) out[y * 16 + x] = 3; // collar/hairline shadow
      else if (y === topY + 2 && backShade === 2) out[y * 16 + x] = 1; // crown highlight
      else out[y * 16 + x] = backShade;
    }
  }
  // Clean torso interior to look like the back of a tunic/cloak.
  for (let y = headEnd + 1; y < legStart; y++) {
    for (let x = leftX + 2; x <= rightX - 2; x++) {
      if (out[y * 16 + x] !== 0 && out[y * 16 + x] !== 3) {
        out[y * 16 + x] = y === legStart - 1 ? 3 : 2;
      }
    }
  }
  return out;
}

/** Synthesize a right-facing (RIGHT) profile 16x16 sprite from a front-facing (DOWN) sprite. */
export function synthesizeRightFromDown(downGrid) {
  const out = new Uint8Array(256);
  const { topY, headEnd, legStart, botY } = analyzeAnatomy(downGrid);
  for (let y = topY; y <= botY; y++) {
    const row = [];
    for (let x = 0; x < 16; x++) {
      if (downGrid[y * 16 + x] !== 0) row.push({ x, v: downGrid[y * 16 + x] });
    }
    if (!row.length) continue;
    const rMin = row[0].x;
    const rMax = row[row.length - 1].x;
    const w = rMax - rMin + 1;
    // Narrow profile slightly (~1px narrower on each side if wide) and shift feature bias +1px right.
    const newW = w > 6 ? w - 2 : w;
    const startX = Math.round(8 - newW / 2);
    for (let i = 0; i < newW; i++) {
      // Sample with a slight shift toward the right side of the front sprite so the eye lands near the right edge.
      const srcIdx = Math.min(row.length - 1, Math.max(0, Math.floor((i / Math.max(1, newW - 1)) * (row.length - 1))));
      const dx = startX + i;
      if (dx >= 0 && dx < 16) {
        let v = row[srcIdx].v;
        if (i === 0 || i === newW - 1) v = 3; // crisp outer profile outline
        out[y * 16 + dx] = v;
      }
    }
    // Add side-facing eye dot on the right side of the face.
    if (y === headEnd - 2 || y === headEnd - 1) {
      const eyeX = Math.min(14, startX + newW - 2);
      const cheekX = Math.min(14, startX + newW - 3);
      if (eyeX >= 0) out[y * 16 + eyeX] = 3;
      if (cheekX >= 0) out[y * 16 + cheekX] = 1;
    }
    // Draw centered side-arm on the torso.
    if (y > headEnd && y < legStart) {
      const armX = Math.max(1, Math.min(14, Math.round(startX + newW * 0.45)));
      out[y * 16 + armX] = y === legStart - 1 ? 1 : 2;
      if (armX - 1 >= startX) out[y * 16 + armX - 1] = 3;
    }
  }
  return out;
}

/** Create a 4-frame walk/run cycle from a single standing directional frame. */
export function synthesizeRunFrames(baseGrid, dir = 'down') {
  const { topY, headEnd, legStart, botY, centerX } = analyzeAnatomy(baseGrid);
  const f0 = cloneGrid(baseGrid);

  // Helper to shift upper body by (dx, dy) and customize left/right legs.
  const makeStride = (bodyDy, leftLegDy, rightLegDy, armSwing) => {
    const g = new Uint8Array(256);
    // Draw head + torso shifted by bodyDy.
    for (let y = topY; y < legStart; y++) {
      const ny = y + bodyDy;
      if (ny < 0 || ny >= 16) continue;
      for (let x = 0; x < 16; x++) {
        let sx = x;
        // Subtle arm swing on torso rows.
        if (y > headEnd && armSwing) {
          if (dir === 'right' || dir === 'left') {
            // Shift mid-torso arm pixel horizontally by armSwing
            if (x >= 6 && x <= 10) sx = Math.max(0, Math.min(15, x - armSwing));
          }
        }
        const v = baseGrid[y * 16 + sx];
        if (v !== 0) g[ny * 16 + x] = v;
      }
    }
    // Draw legs/feet shifted independently on left vs right side of centerX.
    for (let y = legStart; y <= botY; y++) {
      for (let x = 0; x < 16; x++) {
        const v = baseGrid[y * 16 + x];
        if (v === 0) continue;
        const isLeftHalf = x < centerX;
        const dy = isLeftHalf ? leftLegDy : rightLegDy;
        const dx = (dir === 'right' || dir === 'left') ? (isLeftHalf ? armSwing : -armSwing) : 0;
        const ny = Math.max(0, Math.min(15, y + dy));
        const nx = Math.max(0, Math.min(15, x + dx));
        g[ny * 16 + nx] = v;
      }
    }
    return g;
  };

  // Frame 1: Stride A (left leg up, right leg down, torso bobs up or leans).
  const f1 = makeStride(0, -1, 0, 1);
  // Frame 2: Passing contact (upper body squashes down 1px).
  const f2 = makeStride(1, 0, 0, 0);
  // Frame 3: Stride B (right leg up, left leg down, opposite arm swing).
  const f3 = makeStride(0, 0, -1, -1);

  return [f0, f1, f2, f3];
}

/** Synthesize a 2-frame idle animation (standing + 1px breathing bob). */
export function synthesizeIdleFrames(baseGrid) {
  const { topY, legStart, botY } = analyzeAnatomy(baseGrid);
  const f0 = cloneGrid(baseGrid);
  const f1 = new Uint8Array(256);
  // Plant feet first.
  for (let y = legStart; y <= botY; y++) {
    for (let x = 0; x < 16; x++) {
      if (baseGrid[y * 16 + x] !== 0) f1[y * 16 + x] = baseGrid[y * 16 + x];
    }
  }
  // Shift head + torso down by 1px for breathing/ready bob.
  for (let y = topY; y < legStart; y++) {
    const ny = Math.min(15, y + 1);
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) f1[ny * 16 + x] = v;
    }
  }
  return [f0, f1];
}

/** Synthesize a 3-frame jump sequence (takeoff squash, airborne tuck, fall stretch). */
export function synthesizeJumpFrames(baseGrid) {
  const { topY, headEnd, legStart, botY } = analyzeAnatomy(baseGrid);

  // Frame 0: Crouch / takeoff squash (head+torso down 1px, feet planted).
  const [f0, fCrouch] = synthesizeIdleFrames(baseGrid);

  // Frame 1: Airborne tuck (whole body up 1px, legs tucked up 2px).
  const fAir = new Uint8Array(256);
  for (let y = topY; y < legStart; y++) {
    const ny = Math.max(0, y - 1);
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) fAir[ny * 16 + x] = v;
    }
  }
  for (let y = legStart; y <= botY; y++) {
    const ny = Math.max(0, y - 2);
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) fAir[ny * 16 + x] = v;
    }
  }

  // Frame 2: Fall / descend (head up 1px, arms raised 1px, feet extended).
  const fFall = new Uint8Array(256);
  for (let y = topY; y <= headEnd; y++) {
    const ny = Math.max(0, y - 1);
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) fFall[ny * 16 + x] = v;
    }
  }
  for (let y = headEnd + 1; y <= botY; y++) {
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) fFall[y * 16 + x] = v;
    }
  }
  return [fCrouch, fAir, fFall];
}

/** Synthesize a 4-frame roll/dash tumble (crouched ball rotated 0, 90, 180, 270 deg). */
export function synthesizeRollFrames(baseGrid) {
  const { topY, headEnd, legStart, botY } = analyzeAnatomy(baseGrid);
  // Build a compact tucked ball pose centered in the 16x16 frame.
  const ball = new Uint8Array(256);
  for (let y = topY; y <= headEnd; y++) {
    const ny = Math.min(14, y + 2);
    for (let x = 2; x < 14; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) ball[ny * 16 + x] = v;
    }
  }
  for (let y = headEnd + 1; y <= botY; y++) {
    const ny = Math.max(2, y - 2);
    for (let x = 2; x < 14; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0 && ball[ny * 16 + x] === 0) ball[ny * 16 + x] = v;
    }
  }
  // Round off extreme corners (rows 0..1, 14..15, cols 0..1, 14..15) so rotation looks like a somersault.
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const d = Math.hypot(x - 7.5, y - 7.5);
      if (d > 6.8) ball[y * 16 + x] = 0;
    }
  }
  const r90 = rotateGrid90(ball);
  const r180 = rotateGrid90(r90);
  const r270 = rotateGrid90(r180);
  return [ball, r90, r180, r270];
}

/** Synthesize a 1-frame hurt/knockback pose. */
export function synthesizeHurtFrame(baseGrid) {
  const out = new Uint8Array(256);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v === 0) continue;
      // Slight backward lean + eyes/face flash.
      const nx = Math.max(0, Math.min(15, x + (y < 9 ? -1 : 0)));
      out[y * 16 + nx] = v === 2 && y < 8 ? 1 : v;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Built-in Weapon Templates (16x16 grids + directional grip/anchor transforms)
// Each weapon has:
//   - right: horizontal pose (facing right)
//   - up: vertical pose (facing up)
//   - down: vertical pose (facing down)
//   - swing (for melee): diagonal slash frame
//   - fire (for guns): recoil + muzzle flash frame

export const WEAPON_TEMPLATES = {
  melee_sword: {
    id: 'melee_sword',
    category: 'melee',
    name: 'Knight Broadsword',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '......33........',
      '......313.......',
      '....33323333333.',
      '...3233111111113',
      '...323322222223.',
      '....3331333333..',
      '......33........',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    swing_right: parseAsciiGrid([
      '.........111....',
      '...........111..',
      '.......33....11.',
      '.......313....11',
      '......33233...11',
      '.....3231233..11',
      '.....33331233.11',
      '........3312331.',
      '.........33123..',
      '..........333...',
      '............11..',
      '..........111...',
      '........11......',
      '................',
      '................',
      '................',
    ]),
  },
  melee_club: {
    id: 'melee_club',
    category: 'melee',
    name: 'Spiked Club',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '.........3..3...',
      '........313313..',
      '....33332212213.',
      '...3222322222223',
      '...333333222223.',
      '........313313..',
      '.........3..3...',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    swing_right: parseAsciiGrid([
      '........1111....',
      '...........111..',
      '..........3..11.',
      '........3313..11',
      '.......32223..11',
      '.....33321223.11',
      '....322322213.1.',
      '....33333223....',
      '........333.....',
      '...........11...',
      '.........11.....',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
  },
  small_pistol: {
    id: 'small_pistol',
    category: 'small_gun',
    name: 'Rogue Sidearm (Small Gun)',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '................',
      '.....3333333....',
      '....321111123...',
      '....322333333...',
      '....3233........',
      '....333.........',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    fire_right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '...........1.1..',
      '..........111...',
      '....33333331111.',
      '...321111123111.',
      '...32233333311..',
      '...3233....1.1..',
      '...333..........',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
  },
  small_wand: {
    id: 'small_wand',
    category: 'small_gun',
    name: 'Arcane Blaster (Small Gun)',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '.........333....',
      '....333331113...',
      '...3222231213...',
      '....333331113...',
      '.........333....',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    fire_right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '...........1....',
      '........33311...',
      '...333331113111.',
      '..32222312131211',
      '...333331113111.',
      '........33311...',
      '...........1....',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
  },
  big_shotgun: {
    id: 'big_shotgun',
    category: 'big_gun',
    name: 'Boomstick (Big Gun)',
    right: parseAsciiGrid([
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
      '................',
    ]),
    fire_right: parseAsciiGrid([
      '................',
      '................',
      '............1...',
      '...........111..',
      '....333333331111',
      '.333111111112121',
      '3222222233333111',
      '32233223...111..',
      '.33..33.....1...',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
  },
  big_launcher: {
    id: 'big_launcher',
    category: 'big_gun',
    name: 'Heavy Cannon (Big Gun)',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '...33.......333.',
      '..32233333331123',
      '..31111111112223',
      '..32222222222223',
      '..3333223322333.',
      '......33..33....',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    fire_right: parseAsciiGrid([
      '................',
      '................',
      '.............1..',
      '..33.......3311.',
      '.322333333311211',
      '.311111111122211',
      '.322222222222211',
      '.33332233223311.',
      '.....33..33..1..',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
  },
};

/** Build directional weapon frames (down, up, right, left) for ready + active/attack states. */
export function buildWeaponDirectionalFrames(weaponTemplate) {
  const rightReady = weaponTemplate.right;
  const rightActive = weaponTemplate.swing_right || weaponTemplate.fire_right || rightReady;
  const leftReady = flipGridX(rightReady);
  const leftActive = flipGridX(rightActive);
  // Up is rotated 270 deg CW (90 deg CCW); Down is rotated 90 deg CW.
  const downReady = rotateGrid90(rightReady);
  const downActive = rotateGrid90(rightActive);
  const upReady = rotateGrid90(rotateGrid90(downReady));
  const upActive = rotateGrid90(rotateGrid90(downActive));
  return {
    down: [downReady, downActive],
    up: [upReady, upActive],
    right: [rightReady, rightActive],
    left: [leftReady, leftActive],
  };
}

/**
 * Compute the default weapon overlay offset (dx, dy) and draw order (`behind`)
 * relative to the character's 16x16 top-left for a given direction, animation,
 * frame index, and weapon category ('melee' | 'small_gun' | 'big_gun').
 */
export function defaultWeaponAnchor(dir, animKind, frameIdx, weaponCategory = 'small_gun', activeAttack = false) {
  const bob = (animKind === 'idle' && frameIdx === 1) || (animKind === 'run' && frameIdx === 2) ? 1 : 0;
  const jumpDy = animKind === 'jump' ? (frameIdx === 0 ? 1 : -1) : 0;
  const recoil = activeAttack ? (weaponCategory === 'big_gun' ? -2 : weaponCategory === 'small_gun' ? -1 : 2) : 0;
  const baseDy = bob + jumpDy;

  if (dir === 'right') {
    const dx = (weaponCategory === 'big_gun' ? 3 : 5) + recoil;
    return { dx, dy: 2 + baseDy, behind: false };
  }
  if (dir === 'left') {
    const dx = -(weaponCategory === 'big_gun' ? 3 : 5) - recoil;
    return { dx, dy: 2 + baseDy, behind: false };
  }
  if (dir === 'up') {
    const dy = -4 - recoil + baseDy;
    return { dx: 2, dy, behind: true };
  }
  // down
  const dy = 4 + recoil + baseDy;
  return { dx: -2, dy, behind: false };
}

/** Composite two 16x16 grids with an offset (dx, dy) and behind flag for previewing. */
export function compositeGrids(bodyGrid, weaponGrid, { dx = 0, dy = 0, behind = false } = {}) {
  const out = new Uint8Array(256);
  const draw = (src, ox, oy) => {
    for (let y = 0; y < 16; y++) {
      const ny = y + oy;
      if (ny < 0 || ny >= 16) continue;
      for (let x = 0; x < 16; x++) {
        const nx = x + ox;
        if (nx < 0 || nx >= 16) continue;
        const v = src[y * 16 + x];
        if (v !== 0) out[ny * 16 + nx] = v;
      }
    }
  };
  if (behind) {
    if (weaponGrid) draw(weaponGrid, dx, dy);
    draw(bodyGrid, 0, 0);
  } else {
    draw(bodyGrid, 0, 0);
    if (weaponGrid) draw(weaponGrid, dx, dy);
  }
  return out;
}

/**
 * Generate a complete character animation rig from:
 *   - `down` (required 16x16 grid)
 *   - `up` (optional 16x16 grid — synthesized from `down` if omitted)
 *   - `right` (optional 16x16 grid — synthesized from `down` if omitted)
 *   - optional walk overrides `walk_down`, `walk_up`, `walk_right`
 */
export function generateCharacterRig({ name = 'hero', down, up = null, right = null, walk_down = null, walk_up = null, walk_right = null }) {
  const baseDown = cloneGrid(down);
  const baseUp = up ? cloneGrid(up) : synthesizeUpFromDown(baseDown);
  const baseRight = right ? cloneGrid(right) : synthesizeRightFromDown(baseDown);
  const baseLeft = flipGridX(baseRight);

  const runDown = walk_down?.length === 4 ? walk_down.map(cloneGrid) : synthesizeRunFrames(baseDown, 'down');
  const runUp = walk_up?.length === 4 ? walk_up.map(cloneGrid) : synthesizeRunFrames(baseUp, 'up');
  const runRight = walk_right?.length === 4 ? walk_right.map(cloneGrid) : synthesizeRunFrames(baseRight, 'right');
  const runLeft = runRight.map(flipGridX);

  const idleDown = synthesizeIdleFrames(baseDown);
  const idleUp = synthesizeIdleFrames(baseUp);
  const idleRight = synthesizeIdleFrames(baseRight);
  const idleLeft = idleRight.map(flipGridX);

  const jumpDown = synthesizeJumpFrames(baseDown);
  const jumpUp = synthesizeJumpFrames(baseUp);
  const jumpRight = synthesizeJumpFrames(baseRight);
  const jumpLeft = jumpRight.map(flipGridX);

  const rollDown = synthesizeRollFrames(baseDown);
  const rollUp = synthesizeRollFrames(baseUp);
  const rollRight = synthesizeRollFrames(baseRight);
  const rollLeft = rollRight.map(flipGridX);

  const hurtDown = [synthesizeHurtFrame(baseDown)];
  const hurtUp = [synthesizeHurtFrame(baseUp)];
  const hurtRight = [synthesizeHurtFrame(baseRight)];
  const hurtLeft = hurtRight.map(flipGridX);

  return {
    name,
    bases: { down: baseDown, up: baseUp, right: baseRight, left: baseLeft },
    anims: {
      idle_down: idleDown,
      idle_up: idleUp,
      idle_right: idleRight,
      idle_left: idleLeft,
      run_down: runDown,
      run_up: runUp,
      run_right: runRight,
      run_left: runLeft,
      jump_down: jumpDown,
      jump_up: jumpUp,
      jump_right: jumpRight,
      jump_left: jumpLeft,
      roll_down: rollDown,
      roll_up: rollUp,
      roll_right: rollRight,
      roll_left: rollLeft,
      hurt_down: hurtDown,
      hurt_up: hurtUp,
      hurt_right: hurtRight,
      hurt_left: hurtLeft,
    },
  };
}

// ---------------------------------------------------------------------------
// GBDK 8x16 Column Compiler (deduplicates columns + hardware X/Y flips)

const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');
const reverseBits = (b) => {
  let r = 0;
  for (let i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i;
  return r;
};
const flipRows = (bytes) =>
  Array.from({ length: 16 }, (_, r) => bytes.slice((15 - r) * 2, (15 - r) * 2 + 2)).flat();

export function encodeColumn8x16(grid, x0) {
  const out = [];
  for (let y = 0; y < 16; y++) {
    let lo = 0, hi = 0;
    for (let x = 0; x < 8; x++) {
      const c = grid[y * 16 + x0 + x] & 3;
      lo |= (c & 1) << (7 - x);
      hi |= (c >> 1) << (7 - x);
    }
    out.push(lo, hi);
  }
  return out;
}

/**
 * Compile a character rig into deduplicated GBDK 8x16 C arrays & header.
 */
export function compileRigToGbdk(rig, cName = 'spr_custom') {
  const columns = [];
  const index = new Map();
  const addColumn = (bytes) => {
    const variants = [
      [bytes, 0],
      [bytes.map(reverseBits), S_FLIPX],
      [flipRows(bytes), S_FLIPY],
      [flipRows(bytes).map(reverseBits), S_FLIPX | S_FLIPY],
    ];
    for (const [v, flip] of variants) {
      const hit = index.get(v.join(','));
      if (hit) return [hit[0], hit[1] ^ flip];
    }
    columns.push(bytes);
    const ref = [columns.length - 1, 0];
    index.set(bytes.join(','), ref);
    return ref;
  };

  const frames = [];
  const anims = {};
  const animNames = Object.keys(rig.anims);
  for (const name of animNames) {
    const grids = rig.anims[name];
    anims[name] = [frames.length, grids.length];
    for (const g of grids) {
      frames.push([...addColumn(encodeColumn8x16(g, 0)), ...addColumn(encodeColumn8x16(g, 8))]);
    }
  }

  const NAME = cName.toUpperCase();
  const romBytes = columns.length * 32 + frames.length * 4 + animNames.length * 2;
  const header = [
    `// Generated by RogueGB Sprite Studio (${rig.name}).`,
    `#ifndef GEN_${NAME}_H`,
    `#define GEN_${NAME}_H`,
    '#include <stdint.h>',
    `#define ${NAME}_COLUMN_COUNT ${columns.length}`,
    `#define ${NAME}_FRAME_COUNT ${frames.length}`,
    `#define ${NAME}_ANIM_COUNT ${animNames.length}`,
    ...animNames.map((n, i) => `#define ${NAME}_ANIM_${n.toUpperCase()} ${i}`),
    `extern const uint8_t ${cName}_tiles[];`,
    `extern const uint8_t ${cName}_frames[];`,
    `extern const uint8_t ${cName}_anims[];`,
    '#endif',
    '',
  ].join('\n');

  const source = [
    `// Generated by RogueGB Sprite Studio (${rig.name}).`,
    `#include "${cName}.h"`,
    `const uint8_t ${cName}_tiles[] = {`,
    ...columns.map((t) => '    ' + t.map(hex).join(', ') + ','),
    '};',
    `const uint8_t ${cName}_frames[] = {`,
    ...frames.map((f) => '    ' + f.map(hex).join(', ') + ','),
    '};',
    `const uint8_t ${cName}_anims[] = {`,
    ...animNames.map((n) => `    ${anims[n][0]}, ${anims[n][1]}, // ${n}`),
    '};',
    '',
  ].join('\n');

  return {
    header,
    source,
    stats: {
      columns: columns.length,
      frames: frames.length,
      anims: animNames.length,
      romBytes,
    },
  };
}

// ---------------------------------------------------------------------------
// Built-in Character Presets (16x16 4-shade DMG grids)
export const CHARACTER_PRESETS = {
  adventurer: {
    id: 'adventurer',
    name: 'Adventurer (Default Hero)',
    mode: '3pose',
    down: parseAsciiGrid([
      '....33333333....',
      '...3222222223...',
      '..321111111123..',
      '..321112211123..',
      '..333333333333..',
      '..311111111113..',
      '..313111111313..',
      '..313111111313..',
      '...3111331113...',
      '....33333333....',
      '...3223223223...',
      '..312233332213..',
      '..333222222333..',
      '....333..333....',
      '....333..333....',
      '................',
    ]),
    up: parseAsciiGrid([
      '....33333333....',
      '...3222222223...',
      '..321111111123..',
      '..322222222223..',
      '..322222222223..',
      '..332222222233..',
      '..313333333313..',
      '...3222222223...',
      '....33333333....',
      '...3222222223...',
      '..322322223223..',
      '..313333333313..',
      '...3322222233...',
      '....333..333....',
      '....333..333....',
      '................',
    ]),
    right: parseAsciiGrid([
      '....3333333.....',
      '...322222223....',
      '..32111111123...',
      '..322221111233..',
      '..33333333333...',
      '...322111113....',
      '...311111313....',
      '...321111313....',
      '....3311113.....',
      '.....33333......',
      '....3221223.....',
      '....3221223.....',
      '....3311333.....',
      '.....33.33......',
      '.....33.33......',
      '................',
    ]),
  },
  knight_guard: {
    id: 'knight_guard',
    name: 'Armored Guard (Single-Sprite Synth)',
    mode: '1pose',
    down: parseAsciiGrid([
      '.....333333.....',
      '....31111113....',
      '...3211331123...',
      '..322223322223..',
      '..333333333333..',
      '..323133331323..',
      '..323132231323..',
      '..322223322223..',
      '...3333333333...',
      '...3211331123...',
      '..322112211223..',
      '..313223322313..',
      '..333222222333..',
      '....323..323....',
      '....333..333....',
      '................',
    ]),
  },
  cyber_monk: {
    id: 'cyber_monk',
    name: 'Mystic Monk (Single-Sprite Synth)',
    mode: '1pose',
    down: parseAsciiGrid([
      '.....333333.....',
      '...3311111133...',
      '..311111111113..',
      '..311113311113..',
      '..311111111113..',
      '..313111111313..',
      '..313111111313..',
      '...3111221113...',
      '....33333333....',
      '...3221221223...',
      '..322212212223..',
      '..312322223213..',
      '...3322332233...',
      '....323..323....',
      '....333..333....',
      '................',
    ]),
  },
  shadow_nocturne: {
    id: 'shadow_nocturne',
    name: 'Hooded Rogue (Single-Sprite Synth)',
    mode: '1pose',
    down: parseAsciiGrid([
      '.....333333.....',
      '....33222233....',
      '...3222222223...',
      '..322333333223..',
      '..323333333323..',
      '..323113311323..',
      '..323333333323..',
      '...3233223323...',
      '....33333333....',
      '...3322332233...',
      '..323222222323..',
      '..313233332313..',
      '...3322..2233...',
      '....333..333....',
      '....33....33....',
      '................',
    ]),
  },
};
