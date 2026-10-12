// Procedural Game Boy & Game Boy Color (DMG/CGB 2bpp, 16x16 / 8x16) character
// animation & Oracle-style weapon swipe synthesizer.
// Shared between the browser Sprite Studio and Node build tools.
//
// Shade values in a 16x16 grid (Uint8Array of length 256):
//   0 = transparent
//   1 = light / highlight / skin
//   2 = medium / primary color
//   3 = dark / outline

export const S_FLIPX = 0x20;
export const S_FLIPY = 0x40;

// Game Boy Color (CGB) & DMG 4-color sprite palettes:
//   index 0: transparent [0,0,0,0]
//   index 1: light shade (skin / highlight)
//   index 2: mid shade   (tunic / armor / blade)
//   index 3: dark shade  (outline, usually black or deep shadow)
export const GBC_PALETTES = {
  gbc_hero_red: {
    name: 'GBC Hero Red (Crimson Tunic)',
    colors: [[0, 0, 0, 0], [0xff, 0xd8, 0xa0, 255], [0xe8, 0x20, 0x10, 255], [0x10, 0x10, 0x18, 255]],
  },
  gbc_oracle_green: {
    name: 'GBC Oracle Green (Link Tunic)',
    colors: [[0, 0, 0, 0], [0xff, 0xd8, 0x98, 255], [0x20, 0xa8, 0x28, 255], [0x10, 0x18, 0x10, 255]],
  },
  gbc_guard_blue: {
    name: 'GBC Royal Blue (Sapphire Armor)',
    colors: [[0, 0, 0, 0], [0xff, 0xd8, 0xa0, 255], [0x18, 0x80, 0xe0, 255], [0x08, 0x10, 0x20, 255]],
  },
  gbc_monk_gold: {
    name: 'GBC Solar Gold (Monk / Club)',
    colors: [[0, 0, 0, 0], [0xff, 0xe8, 0x78, 255], [0xd8, 0x78, 0x18, 255], [0x18, 0x10, 0x08, 255]],
  },
  gbc_nocturne_purple: {
    name: 'GBC Shadow Violet (Rogue Cloak)',
    colors: [[0, 0, 0, 0], [0xf8, 0xd8, 0xf8, 255], [0x90, 0x38, 0xd0, 255], [0x18, 0x08, 0x28, 255]],
  },
  gbc_volt_blade: {
    name: 'GBC Volt Cyan (Electric Blade)',
    colors: [[0, 0, 0, 0], [0xff, 0xff, 0xe0, 255], [0x18, 0xd8, 0xff, 255], [0x10, 0x18, 0x48, 255]],
  },
  gbc_steel_cyan: {
    name: 'GBC Sky Steel (Blade & Ice)',
    colors: [[0, 0, 0, 0], [0xf8, 0xff, 0xff, 255], [0x38, 0xa8, 0xf8, 255], [0x10, 0x20, 0x38, 255]],
  },
  gbc_gunmetal: {
    name: 'GBC Gunmetal & Brass (Firearms)',
    colors: [[0, 0, 0, 0], [0xff, 0xe0, 0x70, 255], [0x78, 0x88, 0x98, 255], [0x10, 0x14, 0x18, 255]],
  },
  dmg_olive: {
    name: 'DMG Olive LCD (Monochrome)',
    colors: [[0, 0, 0, 0], [0xe0, 0xf0, 0xc8, 255], [0x8b, 0x95, 0x6d, 255], [0x1f, 0x24, 0x20, 255]],
  },
  dmg_classic: {
    name: 'DMG Classic Green (Monochrome)',
    colors: [[0, 0, 0, 0], [0x9b, 0xbc, 0x0f, 255], [0x30, 0x62, 0x30, 255], [0x0f, 0x38, 0x0f, 255]],
  },
};

export const DMG_PALETTES = {
  dmg_olive: GBC_PALETTES.dmg_olive.colors,
  dmg_pocket: [[0, 0, 0, 0], [0xf4, 0xf3, 0xee, 255], [0x97, 0x9d, 0x95, 255], [0x1f, 0x24, 0x20, 255]],
  dmg_classic: GBC_PALETTES.dmg_classic.colors,
};

/** Snap an 8-bit RGB triplet to 15-bit Game Boy Color RGB555 integer. */
export function rgbToCgb555(r, g, b) {
  return ((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10);
}

/** Snap an 8-bit [r,g,b,a] color to 5-bit-per-channel GBC display RGB. */
export function snapColorToGbc([r, g, b, a = 255]) {
  const s = (v) => ((v >> 3) << 3) | (v >> 5);
  return [s(r), s(g), s(b), a];
}

/** Format a 4-color palette array as a GBDK C `palette_color_t` array initializer. */
export function formatGbcPaletteC(palColors, cName = 'spr_hero_pal') {
  const entries = [1, 2, 3].map((i) => {
    const [r, g, b] = palColors[i] || [0, 0, 0];
    return `RGB8(${r}, ${g}, ${b})`;
  });
  return `const palette_color_t ${cName}[4] = { RGB8(0, 0, 0), ${entries.join(', ')} };`;
}

/** Infer sprite height (16 or 24) from a 16-wide grid buffer. */
export function gridHeight(g) {
  return Math.round((g?.length || 256) / 16);
}

/** Parse a 16x16 or 16x24 ASCII art template ('.' or '0' = 0, '1' = 1, '2' = 2, '3' = 3). */
export function parseAsciiGrid(rows) {
  const H = rows.length > 16 ? 24 : 16;
  const out = new Uint8Array(16 * H);
  for (let y = 0; y < H; y++) {
    const row = rows[y] || '';
    for (let x = 0; x < 16; x++) {
      const ch = row[x] || '.';
      out[y * 16 + x] = ch === '1' ? 1 : ch === '2' ? 2 : ch === '3' ? 3 : 0;
    }
  }
  return out;
}

export function gridToAscii(grid) {
  const H = gridHeight(grid);
  const rows = [];
  for (let y = 0; y < H; y++) {
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

/** Mirror a 16xH grid horizontally. */
export function flipGridX(g) {
  const H = gridHeight(g);
  const out = new Uint8Array(16 * H);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = g[y * 16 + (15 - x)];
    }
  }
  return out;
}

/** Mirror a 16xH grid vertically. */
export function flipGridY(g) {
  const H = gridHeight(g);
  const out = new Uint8Array(16 * H);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = g[(H - 1 - y) * 16 + x];
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

/** Shift a 16xH grid by (dx, dy) pixels without wrapping. */
export function shiftGrid(g, dx, dy) {
  const H = gridHeight(g);
  const out = new Uint8Array(16 * H);
  for (let y = 0; y < H; y++) {
    const sy = y - dy;
    if (sy < 0 || sy >= H) continue;
    for (let x = 0; x < 16; x++) {
      const sx = x - dx;
      if (sx < 0 || sx >= 16) continue;
      out[y * 16 + x] = g[sy * 16 + sx];
    }
  }
  return out;
}

/** Flood-fill a contiguous region in a 16xH grid with `newShade`. */
export function floodFillGrid(g, startX, startY, newShade) {
  const H = gridHeight(g);
  if (startX < 0 || startX >= 16 || startY < 0 || startY >= H) return g;
  const target = g[startY * 16 + startX];
  if (target === newShade) return g;
  const stack = [[startX, startY]];
  while (stack.length) {
    const [x, y] = stack.pop();
    if (x < 0 || x >= 16 || y < 0 || y >= H) continue;
    if (g[y * 16 + x] !== target) continue;
    g[y * 16 + x] = newShade;
    stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
  }
  return g;
}

/**
 * Convert a character grid between 16x16 and 16x24.
 * - `stretch = false`: Pads/crops from the top (keeps feet anchored at the bottom).
 * - `stretch = true`:  Proportionally scales the character's vertical anatomy to fill the new height.
 */
export function resizeGridHeight(grid, targetH = 24, stretch = false) {
  const srcH = gridHeight(grid);
  if (srcH === targetH) return cloneGrid(grid);
  const out = new Uint8Array(16 * targetH);
  if (!stretch) {
    const yOff = targetH - srcH; // e.g., +8 when 16->24, -8 when 24->16
    for (let y = 0; y < targetH; y++) {
      const sy = y - yOff;
      if (sy < 0 || sy >= srcH) continue;
      for (let x = 0; x < 16; x++) out[y * 16 + x] = grid[sy * 16 + x];
    }
    return out;
  }
  const { topY, botY } = analyzeAnatomy(grid);
  const spanSrc = Math.max(1, botY - topY + 1);
  const dstTop = targetH === 24 ? 1 : 1;
  const dstBot = targetH === 24 ? 22 : 14;
  const spanDst = dstBot - dstTop + 1;
  for (let y = dstTop; y <= dstBot; y++) {
    const t = (y - dstTop) / Math.max(1, spanDst - 1);
    const sy = Math.min(botY, Math.max(topY, Math.round(topY + t * (spanSrc - 1))));
    for (let x = 0; x < 16; x++) {
      out[y * 16 + x] = grid[sy * 16 + x];
    }
  }
  return out;
}

/**
 * Extract both the 16xH 2bpp grid (`targetH` = 16 or 24) AND the 4-color Game Boy Color
 * palette from an RGBA image buffer (ranking opaque colors from lightest=1 to darkest=3).
 */
export function extractGridAndPalette(rgba, width, height, targetH = 16) {
  const H = targetH === 24 ? 24 : 16;
  const totalPx = 16 * H;
  const sample = new Uint8Array(totalPx * 4);
  for (let y = 0; y < H; y++) {
    const sy = Math.min(height - 1, Math.floor((y + 0.5) * height / H));
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
  const colorCounts = new Map();
  const pixelLums = new Float32Array(totalPx);
  for (let i = 0; i < totalPx; i++) {
    const a = sample[i * 4 + 3];
    if (a < 128) {
      pixelLums[i] = -1;
      continue;
    }
    const r = sample[i * 4], g = sample[i * 4 + 1], b = sample[i * 4 + 2];
    const l = lumOf(r, g, b);
    pixelLums[i] = l;
    const key = (r << 16) | (g << 8) | b;
    const prev = colorCounts.get(key);
    if (prev) prev.count++;
    else colorCounts.set(key, { r, g, b, lum: l, count: 1 });
  }
  const grid = new Uint8Array(totalPx);
  const defaultPal = GBC_PALETTES.gbc_hero_red.colors.map((c) => [...c]);
  if (colorCounts.size === 0) return { grid, palette: defaultPal };

  if (colorCounts.size <= 3) {
    const ranked = [...colorCounts.entries()].sort((a, b) => b[1].lum - a[1].lum);
    const colorIdx = new Map(
      ranked.map(([k], idx) => [k, ranked.length === 1 ? 3 : ranked.length === 2 ? (idx === 0 ? 1 : 3) : idx + 1]),
    );
    const palette = [[0, 0, 0, 0], [...defaultPal[1]], [...defaultPal[2]], [...defaultPal[3]]];
    for (const [k, info] of ranked) {
      const s = colorIdx.get(k);
      palette[s] = snapColorToGbc([info.r, info.g, info.b, 255]);
    }
    for (let i = 0; i < totalPx; i++) {
      if (pixelLums[i] < 0) continue;
      const k = (sample[i * 4] << 16) | (sample[i * 4 + 1] << 8) | sample[i * 4 + 2];
      grid[i] = colorIdx.get(k) || 3;
    }
    return { grid, palette };
  }

  const validLums = [...pixelLums].filter((l) => l >= 0).sort((a, b) => a - b);
  const t1 = validLums[Math.floor(validLums.length * 0.34)];
  const t2 = validLums[Math.floor(validLums.length * 0.68)];
  const sums = [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
  for (let i = 0; i < totalPx; i++) {
    const l = pixelLums[i];
    if (l < 0) continue;
    const s = l <= t1 ? 3 : l <= t2 ? 2 : 1;
    grid[i] = s;
    sums[s][0] += sample[i * 4];
    sums[s][1] += sample[i * 4 + 1];
    sums[s][2] += sample[i * 4 + 2];
    sums[s][3]++;
  }
  const palette = [[0, 0, 0, 0], [...defaultPal[1]], [...defaultPal[2]], [...defaultPal[3]]];
  for (let s = 1; s <= 3; s++) {
    const n = sums[s][3];
    if (n > 0) {
      palette[s] = snapColorToGbc([
        Math.round(sums[s][0] / n),
        Math.round(sums[s][1] / n),
        Math.round(sums[s][2] / n),
        255,
      ]);
    }
  }
  return { grid, palette };
}

/** Quantize an arbitrary RGBA image buffer into a 16xH 4-shade grid (0..3). */
export function quantizeRgbaToGrid16(rgba, width, height, targetH = 16) {
  return extractGridAndPalette(rgba, width, height, targetH).grid;
}

/** Analyze a 16xH character sprite's bounding box and anatomy splits (head, torso, legs). */
export function analyzeAnatomy(grid) {
  const H = gridHeight(grid);
  let topY = H, botY = -1, leftX = 16, rightX = -1;
  const rowWidths = new Uint8Array(H);
  for (let y = 0; y < H; y++) {
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
  if (botY < topY) {
    return {
      H,
      topY: 1,
      botY: H - 2,
      leftX: 2,
      rightX: 13,
      centerX: 7.5,
      headEnd: Math.round(H * 0.5),
      legStart: H - 4,
      rowWidths,
    };
  }
  const h = botY - topY + 1;
  const headRatio = H > 16 ? 0.45 : 0.6;
  const legRows = H > 16 ? 5 : 2;
  const headEnd = Math.min(botY - 3, Math.max(topY + 4, topY + Math.round(h * headRatio)));
  const legStart = Math.max(headEnd + 2, botY - legRows);
  const centerX = (leftX + rightX) / 2;
  return { H, topY, botY, leftX, rightX, centerX, headEnd, legStart, rowWidths };
}

/** Synthesize a back-facing (UP) 16xH sprite from a front-facing (DOWN) sprite. */
export function synthesizeUpFromDown(downGrid) {
  const out = cloneGrid(downGrid);
  const { topY, headEnd, legStart, leftX, rightX } = analyzeAnatomy(downGrid);
  let count1 = 0, count2 = 0;
  for (let y = topY; y <= headEnd; y++) {
    for (let x = leftX; x <= rightX; x++) {
      const v = downGrid[y * 16 + x];
      if (v === 1) count1++;
      if (v === 2) count2++;
    }
  }
  const backShade = count2 >= count1 ? 2 : 1;
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
      if (y === headEnd) out[y * 16 + x] = 3;
      else if (y === topY + 2 && backShade === 2) out[y * 16 + x] = 1;
      else out[y * 16 + x] = backShade;
    }
  }
  for (let y = headEnd + 1; y < legStart; y++) {
    for (let x = leftX + 2; x <= rightX - 2; x++) {
      if (out[y * 16 + x] !== 0 && out[y * 16 + x] !== 3) {
        out[y * 16 + x] = y === legStart - 1 ? 3 : 2;
      }
    }
  }
  return out;
}

/** Synthesize a right-facing (RIGHT) profile 16xH sprite from a front-facing (DOWN) sprite. */
export function synthesizeRightFromDown(downGrid) {
  const { H, topY, headEnd, legStart, botY } = analyzeAnatomy(downGrid);
  const out = new Uint8Array(16 * H);
  for (let y = topY; y <= botY; y++) {
    const row = [];
    for (let x = 0; x < 16; x++) {
      if (downGrid[y * 16 + x] !== 0) row.push({ x, v: downGrid[y * 16 + x] });
    }
    if (!row.length) continue;
    const rMin = row[0].x;
    const rMax = row[row.length - 1].x;
    const w = rMax - rMin + 1;
    const newW = w > 6 ? w - 2 : w;
    const startX = Math.round(8 - newW / 2);
    for (let i = 0; i < newW; i++) {
      const srcIdx = Math.min(row.length - 1, Math.max(0, Math.floor((i / Math.max(1, newW - 1)) * (row.length - 1))));
      const dx = startX + i;
      if (dx >= 0 && dx < 16) {
        let v = row[srcIdx].v;
        if (i === 0 || i === newW - 1) v = 3;
        out[y * 16 + dx] = v;
      }
    }
    if (y === headEnd - 2 || y === headEnd - 1) {
      const eyeX = Math.min(14, startX + newW - 2);
      const cheekX = Math.min(14, startX + newW - 3);
      if (eyeX >= 0) out[y * 16 + eyeX] = 3;
      if (cheekX >= 0) out[y * 16 + cheekX] = 1;
    }
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
  const { H, topY, headEnd, legStart, botY, centerX } = analyzeAnatomy(baseGrid);
  const f0 = cloneGrid(baseGrid);

  const makeStride = (bodyDy, leftLegDy, rightLegDy, armSwing) => {
    const g = new Uint8Array(16 * H);
    for (let y = topY; y < legStart; y++) {
      const ny = y + bodyDy;
      if (ny < 0 || ny >= H) continue;
      for (let x = 0; x < 16; x++) {
        let sx = x;
        if (y > headEnd && armSwing && (dir === 'right' || dir === 'left')) {
          if (x >= 6 && x <= 10) sx = Math.max(0, Math.min(15, x - armSwing));
        }
        const v = baseGrid[y * 16 + sx];
        if (v !== 0) g[ny * 16 + x] = v;
      }
    }
    for (let y = legStart; y <= botY; y++) {
      for (let x = 0; x < 16; x++) {
        const v = baseGrid[y * 16 + x];
        if (v === 0) continue;
        const isLeftHalf = x < centerX;
        const dy = isLeftHalf ? leftLegDy : rightLegDy;
        const dx = (dir === 'right' || dir === 'left') ? (isLeftHalf ? armSwing : -armSwing) : 0;
        const ny = Math.max(0, Math.min(H - 1, y + dy));
        const nx = Math.max(0, Math.min(15, x + dx));
        g[ny * 16 + nx] = v;
      }
    }
    return g;
  };

  const strideAmt = H > 16 ? -2 : -1;
  const f1 = makeStride(0, strideAmt, 0, 1);
  const f2 = makeStride(1, 0, 0, 0);
  const f3 = makeStride(0, 0, strideAmt, -1);
  return [f0, f1, f2, f3];
}

/** Synthesize a 2-frame idle animation (standing + 1px breathing bob). */
export function synthesizeIdleFrames(baseGrid) {
  const { H, topY, legStart, botY } = analyzeAnatomy(baseGrid);
  const f0 = cloneGrid(baseGrid);
  const f1 = new Uint8Array(16 * H);
  for (let y = legStart; y <= botY; y++) {
    for (let x = 0; x < 16; x++) {
      if (baseGrid[y * 16 + x] !== 0) f1[y * 16 + x] = baseGrid[y * 16 + x];
    }
  }
  for (let y = topY; y < legStart; y++) {
    const ny = Math.min(H - 1, y + 1);
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v !== 0) f1[ny * 16 + x] = v;
    }
  }
  return [f0, f1];
}

/** Synthesize a 3-frame jump sequence (takeoff squash, airborne tuck, fall stretch). */
export function synthesizeJumpFrames(baseGrid) {
  const { H, topY, headEnd, legStart, botY } = analyzeAnatomy(baseGrid);
  const [, fCrouch] = synthesizeIdleFrames(baseGrid);

  const fAir = new Uint8Array(16 * H);
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

  const fFall = new Uint8Array(16 * H);
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

/**
 * Synthesize a 4-frame dodge-roll / dash sequence for 16x16 or 16x24 sprites:
 * - For `down` and `up`: Enter-the-Gungeon-style vertical dive, tucked vertical
 *   somersault/dash (around the horizontal X-axis, never sideways!), high-speed
 *   vertical streak with motion lines, and crouched landing slide with dust puffs.
 * - For `right` (and `left` via flipGridX): horizontal forward tumble with dash trails.
 */
export function synthesizeRollFrames(baseGrid, dir = 'down') {
  const { H, topY, headEnd, legStart, botY, leftX, rightX } = analyzeAnatomy(baseGrid);
  const yPad = H - 16; // 0 for 16x16, 8 for 16x24

  if (dir === 'down' || dir === 'up') {
    const isDown = dir === 'down';
    const sign = isDown ? 1 : -1;

    // Frame 0: Dive / Launch into the vertical dash (body pitches forward, trailing dash ticks)
    const f0 = new Uint8Array(16 * H);
    for (let y = topY; y <= botY; y++) {
      const isLegs = y >= legStart;
      const dy = isLegs ? -sign : sign * 2;
      const ny = Math.max(0, Math.min(H - 1, y + dy));
      for (let x = 0; x < 16; x++) {
        const v = baseGrid[y * 16 + x];
        if (v !== 0) f0[ny * 16 + x] = v;
      }
    }
    const trailY0 = isDown ? Math.max(0, topY - 1) : Math.min(H - 1, botY + 1);
    f0[trailY0 * 16 + 5] = 2;
    f0[trailY0 * 16 + 10] = 2;

    // Frame 1: Tight vertical tuck / forward somersault
    const f1 = new Uint8Array(16 * H);
    const tuckSpan = H > 16 ? 12 : 9;
    const tuckStart = isDown ? (H - tuckSpan - 2) : 2;
    const srcSpan = Math.max(1, botY - topY);
    for (let dy = 0; dy < tuckSpan; dy++) {
      const sy = isDown
        ? Math.min(botY, Math.max(topY, Math.round(botY - (dy / (tuckSpan - 1)) * srcSpan)))
        : Math.min(botY, Math.max(topY, Math.round(topY + (dy / (tuckSpan - 1)) * srcSpan)));
      const ny = tuckStart + dy;
      if (ny < 0 || ny >= H) continue;
      for (let x = 2; x < 14; x++) {
        const v = baseGrid[sy * 16 + x];
        if (!v) continue;
        if ((dy === 0 || dy === tuckSpan - 1) && (x < 5 || x > 10)) continue;
        f1[ny * 16 + x] = (dy >= 2 && dy <= tuckSpan - 4 && v === 1) ? 2 : v;
      }
    }
    const streakRows1 = isDown ? [yPad + 0, yPad + 1, yPad + 2, yPad + 3] : [H - 4, H - 3, H - 2, H - 1];
    for (const ry of streakRows1) {
      if (ry < 0 || ry >= H) continue;
      f1[ry * 16 + 4] = (ry % 2 === 0) ? 2 : 1;
      f1[ry * 16 + 8] = 1;
      f1[ry * 16 + 11] = (ry % 2 === 0) ? 1 : 2;
    }

    // Frame 2: High-speed vertical dash / inverted flip recovery with long speed lines
    const f2 = new Uint8Array(16 * H);
    const dashShift = isDown ? 2 : -2;
    const midY = H / 2;
    for (let y = topY; y <= botY; y++) {
      const ny = Math.max(0, Math.min(H - 1, Math.round(midY + (y - midY) * 0.75 + dashShift)));
      for (let x = 2; x < 14; x++) {
        const v = baseGrid[y * 16 + x];
        if (v !== 0) f2[ny * 16 + x] = v;
      }
    }
    const streakRows2 = isDown ? [yPad + 0, yPad + 1, yPad + 2, yPad + 3, yPad + 4] : [H - 5, H - 4, H - 3, H - 2, H - 1];
    for (const ry of streakRows2) {
      if (ry < 0 || ry >= H) continue;
      if (f2[ry * 16 + 5] === 0) f2[ry * 16 + 5] = 1;
      if (f2[ry * 16 + 7] === 0 && ry % 2 === 0) f2[ry * 16 + 7] = 2;
      if (f2[ry * 16 + 10] === 0) f2[ry * 16 + 10] = 1;
    }

    // Frame 3: Landing slide & recovery squash (upright facing dir, crouched low + dust puffs)
    const f3 = new Uint8Array(16 * H);
    for (let y = legStart; y <= botY; y++) {
      for (let x = 0; x < 16; x++) {
        if (baseGrid[y * 16 + x] !== 0) f3[y * 16 + x] = baseGrid[y * 16 + x];
      }
    }
    for (let y = topY; y < legStart; y++) {
      const ny = Math.min(H - 1, y + 2);
      for (let x = 0; x < 16; x++) {
        const v = baseGrid[y * 16 + x];
        if (v !== 0) f3[ny * 16 + x] = v;
      }
    }
    const dustY = Math.min(H - 1, botY);
    const lx = Math.max(0, leftX - 2);
    const rx = Math.min(15, rightX + 2);
    f3[dustY * 16 + lx] = 1;
    f3[dustY * 16 + rx] = 1;
    if (dustY - 1 >= 0) {
      f3[(dustY - 1) * 16 + lx] = 2;
      f3[(dustY - 1) * 16 + rx] = 2;
    }

    return [f0, f1, f2, f3];
  }

  // Horizontal roll (right / left): form a 16x16 tucked roll ball, rotate 0/90/180/270,
  // and place into the bottom 16 rows of the 16xH frame.
  const g16 = H === 16 ? baseGrid : resizeGridHeight(baseGrid, 16, true);
  const a16 = analyzeAnatomy(g16);
  const ball16 = new Uint8Array(256);
  for (let y = a16.topY; y <= a16.headEnd; y++) {
    const ny = Math.min(14, y + 2);
    for (let x = 2; x < 14; x++) {
      const v = g16[y * 16 + x];
      if (v !== 0) ball16[ny * 16 + x] = v;
    }
  }
  for (let y = a16.headEnd + 1; y <= a16.botY; y++) {
    const ny = Math.max(2, y - 2);
    for (let x = 2; x < 14; x++) {
      const v = g16[y * 16 + x];
      if (v !== 0 && ball16[ny * 16 + x] === 0) ball16[ny * 16 + x] = v;
    }
  }
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      if (Math.hypot(x - 7.5, y - 7.5) > 6.8) ball16[y * 16 + x] = 0;
    }
  }
  const r90 = rotateGrid90(ball16);
  const r180 = rotateGrid90(r90);
  const r270 = rotateGrid90(r180);
  const balls = [ball16, r90, r180, r270];
  if (H === 16) return balls;
  return balls.map((b) => {
    const out = new Uint8Array(16 * H);
    out.set(b, yPad * 16);
    return out;
  });
}

/**
 * Synthesize the 3-frame Zelda: Oracle of Ages/Seasons body swing sequence
 * (Phase 0: Windup -> Phase 1: Diagonal Twist -> Phase 2: Forward Lunge).
 */
export function synthesizeAttackFrames(baseGrid, dir = 'down') {
  const { H, topY, headEnd, legStart, botY, leftX, rightX } = analyzeAnatomy(baseGrid);

  // Phase 0: Windup (cock arm back to starting side, slight torso coil)
  const f0 = cloneGrid(baseGrid);
  const coilDx = dir === 'down' ? 1 : dir === 'up' ? -1 : 0;
  const coilDy = dir === 'right' || dir === 'left' ? -1 : 0;
  if (coilDx || coilDy) {
    const coiled = new Uint8Array(16 * H);
    for (let y = topY; y < legStart; y++) {
      for (let x = 0; x < 16; x++) {
        const v = baseGrid[y * 16 + x];
        if (!v) continue;
        const nx = Math.max(0, Math.min(15, x + (y <= headEnd ? 0 : coilDx)));
        const ny = Math.max(0, Math.min(H - 1, y + coilDy));
        coiled[ny * 16 + nx] = v;
      }
    }
    for (let y = legStart; y <= botY; y++) {
      for (let x = 0; x < 16; x++) {
        if (baseGrid[y * 16 + x]) coiled[y * 16 + x] = baseGrid[y * 16 + x];
      }
    }
    f0.set(coiled);
  }

  // Phase 1: Mid-Slash Diagonal Twist (extend lead hand toward diagonal corner)
  const f1 = cloneGrid(baseGrid);
  const handY = Math.min(H - 2, headEnd + 2);
  const handX = dir === 'down' ? Math.max(1, leftX - 1) : Math.min(14, rightX + 1);
  f1[handY * 16 + handX] = 1;
  if (handY - 1 >= 0) f1[(handY - 1) * 16 + handX] = 3;

  // Phase 2: Forward Lunge & Thrust (body shifts 1px into the strike, wide stance)
  const f2 = new Uint8Array(16 * H);
  const lungeDx = dir === 'right' ? 1 : dir === 'left' ? -1 : 0;
  const lungeDy = dir === 'down' ? 1 : dir === 'up' ? -1 : 0;
  for (let y = topY; y < legStart; y++) {
    const ny = Math.max(0, Math.min(H - 1, y + lungeDy));
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (!v) continue;
      const nx = Math.max(0, Math.min(15, x + lungeDx));
      f2[ny * 16 + nx] = v;
    }
  }
  for (let y = legStart; y <= botY; y++) {
    const ny = Math.max(0, Math.min(H - 1, y + (lungeDy > 0 ? 1 : 0)));
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (!v) continue;
      const nx = Math.max(0, Math.min(15, x + lungeDx));
      f2[ny * 16 + nx] = v;
    }
  }
  return [f0, f1, f2];
}

/** Synthesize a 1-frame hurt/knockback pose. */
export function synthesizeHurtFrame(baseGrid) {
  const H = gridHeight(baseGrid);
  const out = new Uint8Array(16 * H);
  const midY = Math.round(H * 0.55);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < 16; x++) {
      const v = baseGrid[y * 16 + x];
      if (v === 0) continue;
      const nx = Math.max(0, Math.min(15, x + (y < midY ? -1 : 0)));
      out[y * 16 + nx] = v === 2 && y < midY ? 1 : v;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Oracle-Style Melee Swipe & Weapon Synthesizer
//
// Rotate a horizontal right-facing weapon by -45 degrees (pointing Up-Right)
// around the grip pivot so any custom weapon automatically gets 45° diagonal sprites!
export function synthesizeDiagUpRightFromRight(rightGrid) {
  const out = new Uint8Array(256);
  const cos = 0.70710678;
  const sin = 0.70710678;
  // Pivot near left-center hilt (4, 11) in output -> (4, 6.5) in source
  const pivOutX = 4, pivOutY = 11;
  const pivSrcX = 4, pivSrcY = 6.5;
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const dx = x - pivOutX;
      const dy = y - pivOutY;
      // Rotate +45 deg back into horizontal source space
      const sx = Math.round(pivSrcX + (dx * cos - dy * sin));
      const sy = Math.round(pivSrcY + (dx * sin + dy * cos));
      if (sx >= 0 && sx < 16 && sy >= 0 && sy < 16) {
        const v = rightGrid[sy * 16 + sx];
        if (v) out[y * 16 + x] = v;
      }
    }
  }
  return out;
}

/**
 * Add crackling electric discharge & Oracle-style motion-blur smear wings
 * above and below a horizontal right-facing melee weapon (Phase 2 impact frame).
 */
export function synthesizeSmearRightFromRight(rightGrid) {
  const out = cloneGrid(rightGrid);
  // Jagged lightning / energy-arc wings flaring above and below the blade (cols 6..14).
  const wingTop = [
    [6, 1, 2], [7, 1, 1], [8, 1, 3], [11, 0, 1],
    [7, 2, 3], [8, 2, 1], [9, 2, 2], [10, 2, 1], [11, 2, 3],
    [8, 3, 2], [9, 3, 3], [10, 3, 1], [11, 3, 2], [12, 3, 1], [13, 3, 3],
  ];
  for (const [x, y, c] of wingTop) {
    if (out[y * 16 + x] === 0) out[y * 16 + x] = c;
    const by = 13 - y;
    if (by >= 0 && by < 16 && out[by * 16 + x] === 0) out[by * 16 + x] = c;
  }
  return out;
}

export const WEAPON_TEMPLATES = {
  melee_sword: {
    id: 'melee_sword',
    category: 'melee',
    name: 'Volt Sabre (Electric Blade)',
    palette: 'gbc_volt_blade',
    paletteId: 'gbc_volt_blade',
    // Twin-prong inductor hilt + jagged serrated metal conductor spine with
    // crackling white-hot (1) & cyan (2) lightning arcs jumping along the edges!
    right: parseAsciiGrid([
      '................',
      '................',
      '........1...1...',
      '....333.21.212..',
      '....32133131313.',
      '.333323311111113',
      '3212312113333121',
      '.333323322112113',
      '....32133131323.',
      '....333.21.12...',
      '........1.......',
      '................',
      '................',
      '................',
      '................',
      '................',
    ]),
    // Phase 1 (45° Diagonal Slash): angled rail-blade with forked lightning
    // bolts arcing off the trailing edge of the swing!
    diag_ur: parseAsciiGrid([
      '...........2121.',
      '........1.311213',
      '.......21311312.',
      '......12.313123.',
      '.......3311323..',
      '....21.311322...',
      '...323311323.1..',
      '...31231323.21..',
      '....321223..1...',
      '..33321233......',
      '.321333213......',
      '.333...33.......',
      '................',
      '................',
      '................',
      '................',
    ]),
    // Phase 2 (Impact Lunge): full-width jagged electric shockwave arc with
    // zig-zagging lightning bolts & high-voltage sparks!
    smear_right: parseAsciiGrid([
      '......21..1.....',
      '.....3121.21....',
      '....312312121...',
      '....3213311213..',
      '....32132121123.',
      '.333323311111113',
      '3212312113333121',
      '.333323322112113',
      '....32132121123.',
      '....3213311213..',
      '....312312121...',
      '.....3121.21....',
      '......21..1.....',
      '................',
      '................',
      '................',
    ]),
  },
  melee_club: {
    id: 'melee_club',
    category: 'melee',
    name: 'Spiked Club (Artwork Melee)',
    palette: 'gbc_monk_gold',
    paletteId: 'gbc_monk_gold',
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
    diag_ur: parseAsciiGrid([
      '................',
      '.........133....',
      '.......1.3113...',
      '......31311113..',
      '......32111113..',
      '.....131111213..',
      '.....322112331..',
      '....1332123.1...',
      '....3222231.....',
      '...1322233......',
      '..1332331.......',
      '.32333.1........',
      '32231...........',
      '3223............',
      '.33.............',
      '................',
    ]),
    smear_right: parseAsciiGrid([
      '.......313......',
      '......3123......',
      '.....3213.......',
      '.....31133..3...',
      '.....333313313..',
      '.33.33332212213.',
      '3213222322222223',
      '.33.33333222223.',
      '.....333313313..',
      '.....31133..3...',
      '.....3213.......',
      '......3123......',
      '.......313......',
      '................',
      '................',
      '................',
    ]),
  },
  small_pistol: {
    id: 'small_pistol',
    category: 'small_gun',
    name: 'Rogue Sidearm (Small Gun)',
    palette: 'gbc_gunmetal',
    paletteId: 'gbc_gunmetal',
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
    name: 'Arcane Rod (Small Gun / Wand)',
    palette: 'gbc_guard_blue',
    paletteId: 'gbc_guard_blue',
    right: parseAsciiGrid([
      '................',
      '................',
      '................',
      '................',
      '.........333....',
      '....333332123...',
      '...3111132123...',
      '....333332123...',
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
      '...333332123111.',
      '..31111321231211',
      '...333332123111.',
      '........33311...',
      '...........1....',
      '................',
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
    palette: 'gbc_gunmetal',
    paletteId: 'gbc_gunmetal',
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
    palette: 'gbc_monk_gold',
    paletteId: 'gbc_monk_gold',
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

/**
 * Build all directional and diagonal frames for a weapon template, including:
 *   - cardinal ready frames: `right`, `left`, `down`, `up`
 *   - 45° diagonal frames (for melee Phase 1): `diag_ur`, `diag_ul`, `diag_dr`, `diag_dl`
 *   - cardinal smear / muzzle-flash frames (for Phase 2 / fire): `smear_right`, `smear_left`, `smear_down`, `smear_up`
 */
export function buildWeaponDirectionalFrames(weaponTemplate) {
  const rightReady = weaponTemplate.right;
  const leftReady = flipGridX(rightReady);
  const downReady = rotateGrid90(rightReady);
  const upReady = flipGridY(downReady);

  const diagUR = weaponTemplate.diag_ur || synthesizeDiagUpRightFromRight(rightReady);
  const diagUL = flipGridX(diagUR);
  const diagDR = flipGridY(diagUR);
  const diagDL = flipGridX(diagDR);

  const rightSmear = weaponTemplate.smear_right || weaponTemplate.fire_right || synthesizeSmearRightFromRight(rightReady);
  const leftSmear = flipGridX(rightSmear);
  const downSmear = rotateGrid90(rightSmear);
  const upSmear = flipGridY(downSmear);

  return {
    down: [downReady, downSmear],
    up: [upReady, upSmear],
    right: [rightReady, rightSmear],
    left: [leftReady, leftSmear],
    diag_ur: diagUR,
    diag_ul: diagUL,
    diag_dr: diagDR,
    diag_dl: diagDL,
  };
}

/**
 * Compute the exact weapon sprite grid and (dx, dy, behind) placement for an
 * Oracle of Ages / Seasons 3-phase melee swipe (`phase` = 0, 1, or 2):
 *   Phase 0 (Windup):   Weapon held 90° perpendicular to the side of the body
 *   Phase 1 (Slash):    Weapon sweeps through the 45° diagonal corner
 *   Phase 2 (Extension): Weapon thrusts straight ahead with motion-smear wings
 */
export function getOracleMeleeSwipeStep(weaponFrames, dir, phaseIdx, charHeight = 16) {
  const p = Math.max(0, Math.min(2, phaseIdx));
  const yOff = charHeight > 16 ? 5 : 0;
  if (dir === 'down') {
    if (p === 0) return { grid: weaponFrames.left[0], dx: -13, dy: 1 + yOff, behind: false };
    if (p === 1) return { grid: weaponFrames.diag_dl, dx: -11, dy: 10 + yOff, behind: false };
    return { grid: weaponFrames.down[1], dx: -2, dy: 14 + yOff, behind: false };
  }
  if (dir === 'up') {
    if (p === 0) return { grid: weaponFrames.right[0], dx: 13, dy: -1 + yOff, behind: true };
    if (p === 1) return { grid: weaponFrames.diag_ur, dx: 11, dy: -10 + Math.round(yOff * 0.5), behind: true };
    return { grid: weaponFrames.up[1], dx: 2, dy: -14 + Math.round(yOff * 0.4), behind: true };
  }
  if (dir === 'right') {
    if (p === 0) return { grid: weaponFrames.up[0], dx: 1, dy: -13 + Math.round(yOff * 0.5), behind: true };
    if (p === 1) return { grid: weaponFrames.diag_ur, dx: 11, dy: -10 + yOff, behind: false };
    return { grid: weaponFrames.right[1], dx: 14, dy: 2 + yOff, behind: false };
  }
  // left
  if (p === 0) return { grid: weaponFrames.up[0], dx: -1, dy: -13 + Math.round(yOff * 0.5), behind: true };
  if (p === 1) return { grid: weaponFrames.diag_ul, dx: -11, dy: -10 + yOff, behind: false };
  return { grid: weaponFrames.left[1], dx: -14, dy: 2 + yOff, behind: false };
}

/**
 * Compute the default weapon overlay offset (dx, dy) and draw order (`behind`)
 * relative to the character's 16x16 or 16x24 top-left for non-melee-swipe states.
 */
export function defaultWeaponAnchor(dir, animKind, frameIdx, weaponCategory = 'small_gun', activeAttack = false, charHeight = 16) {
  const bob = (animKind === 'idle' && frameIdx === 1) || (animKind === 'run' && frameIdx === 2) ? 1 : 0;
  const jumpDy = animKind === 'jump' ? (frameIdx === 0 ? 1 : -1) : 0;
  const recoil = activeAttack ? (weaponCategory === 'big_gun' ? -2 : weaponCategory === 'small_gun' ? -1 : 2) : 0;
  const yOff = charHeight > 16 ? 5 : 0;
  const baseDy = bob + jumpDy + yOff;

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
  const dy = 4 + recoil + baseDy;
  return { dx: -2, dy, behind: false };
}

/**
 * Generate a complete character animation rig (including 3-frame Oracle melee
 * attack poses) from 1 front sprite (`down`) or 3 directional bases (`down`, `up`, `right`).
 */
export function generateCharacterRig({
  name = 'hero',
  down,
  up = null,
  right = null,
  walk_down = null,
  walk_up = null,
  walk_right = null,
  attack_down = null,
  attack_up = null,
  attack_right = null,
  palette = null,
}) {
  const baseDown = cloneGrid(down);
  const H = gridHeight(baseDown);
  const baseUp = up && gridHeight(up) === H ? cloneGrid(up) : synthesizeUpFromDown(baseDown);
  const baseRight = right && gridHeight(right) === H ? cloneGrid(right) : synthesizeRightFromDown(baseDown);
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

  const rollDown = synthesizeRollFrames(baseDown, 'down');
  const rollUp = synthesizeRollFrames(baseUp, 'up');
  const rollRight = synthesizeRollFrames(baseRight, 'right');
  const rollLeft = rollRight.map(flipGridX);

  const atkDown = attack_down?.length === 3 ? attack_down.map(cloneGrid) : synthesizeAttackFrames(baseDown, 'down');
  const atkUp = attack_up?.length === 3 ? attack_up.map(cloneGrid) : synthesizeAttackFrames(baseUp, 'up');
  const atkRight = attack_right?.length === 3 ? attack_right.map(cloneGrid) : synthesizeAttackFrames(baseRight, 'right');
  const atkLeft = atkRight.map(flipGridX);

  const hurtDown = [synthesizeHurtFrame(baseDown)];
  const hurtUp = [synthesizeHurtFrame(baseUp)];
  const hurtRight = [synthesizeHurtFrame(baseRight)];
  const hurtLeft = hurtRight.map(flipGridX);

  return {
    name,
    height: H,
    palette: palette || GBC_PALETTES.gbc_hero_red.colors,
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
      attack_down: atkDown,
      attack_up: atkUp,
      attack_right: atkRight,
      attack_left: atkLeft,
      hurt_down: hurtDown,
      hurt_up: hurtUp,
      hurt_right: hurtRight,
      hurt_left: hurtLeft,
    },
  };
}

// ---------------------------------------------------------------------------
// GBDK 8x16 Column Compiler (deduplicates columns + hardware X/Y flips + GBC palettes)

const hex = (b) => '0x' + b.toString(16).toUpperCase().padStart(2, '0');
const reverseBits = (b) => {
  let r = 0;
  for (let i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i;
  return r;
};
const flipRows = (bytes) =>
  Array.from({ length: 16 }, (_, r) => bytes.slice((15 - r) * 2, (15 - r) * 2 + 2)).flat();

export function encodeColumn8x16(grid, x0, y0 = 0) {
  const H = gridHeight(grid);
  const out = [];
  for (let r = 0; r < 16; r++) {
    const y = y0 + r;
    let lo = 0, hi = 0;
    if (y >= 0 && y < H) {
      for (let x = 0; x < 8; x++) {
        const c = grid[y * 16 + x0 + x] & 3;
        lo |= (c & 1) << (7 - x);
        hi |= (c >> 1) << (7 - x);
      }
    }
    out.push(lo, hi);
  }
  return out;
}

/**
 * Compile a character rig into deduplicated GBDK 8x16 C arrays, header, and CGB palette.
 * Supports both 16x16 (2 columns/frame) and 16x24 (4 columns/frame: top L/R + bottom L/R).
 */
export function compileRigToGbdk(rig, cName = 'spr_custom', weaponPalColors = null) {
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

  const charH = rig.height || gridHeight(rig.bases?.down) || 16;
  const colsPerFrame = charH > 16 ? 4 : 2;
  const frames = [];
  const anims = {};
  const animNames = Object.keys(rig.anims);
  for (const name of animNames) {
    const grids = rig.anims[name];
    anims[name] = [frames.length, grids.length];
    for (const g of grids) {
      if (colsPerFrame === 4) {
        frames.push([
          ...addColumn(encodeColumn8x16(g, 0, 0)),
          ...addColumn(encodeColumn8x16(g, 8, 0)),
          ...addColumn(encodeColumn8x16(g, 0, 16)),
          ...addColumn(encodeColumn8x16(g, 8, 16)),
        ]);
      } else {
        frames.push([...addColumn(encodeColumn8x16(g, 0, 0)), ...addColumn(encodeColumn8x16(g, 8, 0))]);
      }
    }
  }

  const NAME = cName.toUpperCase();
  const romBytes = columns.length * 32 + frames.length * (colsPerFrame * 2) + animNames.length * 2 + 8;
  const charPal = rig.palette || GBC_PALETTES.gbc_hero_red.colors;
  const weapPal = weaponPalColors || GBC_PALETTES.gbc_steel_cyan.colors;

  const header = [
    `// Generated by RogueGB Sprite Studio (${rig.name}, 16x${charH}).`,
    `#ifndef GEN_${NAME}_H`,
    `#define GEN_${NAME}_H`,
    '#include <stdint.h>',
    '#include <gb/cgb.h>',
    `#define ${NAME}_SPRITE_WIDTH 16`,
    `#define ${NAME}_SPRITE_HEIGHT ${charH}`,
    `#define ${NAME}_COLS_PER_FRAME ${colsPerFrame}`,
    `#define ${NAME}_COLUMN_COUNT ${columns.length}`,
    `#define ${NAME}_FRAME_COUNT ${frames.length}`,
    `#define ${NAME}_ANIM_COUNT ${animNames.length}`,
    ...animNames.map((n, i) => `#define ${NAME}_ANIM_${n.toUpperCase()} ${i}`),
    `extern const palette_color_t ${cName}_pal[4];`,
    `extern const palette_color_t ${cName}_weapon_pal[4];`,
    `extern const uint8_t ${cName}_tiles[];`,
    `extern const uint8_t ${cName}_frames[];`,
    `extern const uint8_t ${cName}_anims[];`,
    '#endif',
    '',
  ].join('\n');

  const source = [
    `// Generated by RogueGB Sprite Studio (${rig.name}, 16x${charH}).`,
    `#include "${cName}.h"`,
    formatGbcPaletteC(charPal, `${cName}_pal`),
    formatGbcPaletteC(weapPal, `${cName}_weapon_pal`),
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
      height: charH,
    },
  };
}

// ---------------------------------------------------------------------------
// Built-in Character Presets (16x16 and 16x24 4-shade grids + GBC palette IDs)
export const CHARACTER_PRESETS = {
  adventurer: {
    id: 'adventurer',
    name: 'Adventurer (16x16 Default Hero)',
    mode: '3pose',
    height: 16,
    palette: GBC_PALETTES.gbc_hero_red.colors,
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
  gungeon_ranger: {
    id: 'gungeon_ranger',
    name: 'Gungeon Ranger (16x24 Tall Hero)',
    mode: '1pose',
    height: 24,
    palette: GBC_PALETTES.gbc_oracle_green.colors,
    down: parseAsciiGrid([
      '....33333333....',
      '...3222222223...',
      '..321111111123..',
      '..321133331123..',
      '..333312213333..',
      '..311133331113..',
      '..313111111313..',
      '..313111111313..',
      '...3111331113...',
      '....33333333....',
      '...3223113223...',
      '..322231132223..',
      '..312232232213..',
      '..313223322313..',
      '..333211112333..',
      '...3223333223...',
      '...3222332223...',
      '....32233223....',
      '....323..323....',
      '....323..323....',
      '....333..333....',
      '...3323..3233...',
      '...3333..3333...',
      '................',
    ]),
  },
  knight_guard: {
    id: 'knight_guard',
    name: 'Armored Guard (16x16 Single-Sprite)',
    mode: '1pose',
    height: 16,
    palette: GBC_PALETTES.gbc_guard_blue.colors,
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
    name: 'Mystic Monk (16x16 Single-Sprite)',
    mode: '1pose',
    height: 16,
    palette: GBC_PALETTES.gbc_monk_gold.colors,
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
    name: 'Hooded Rogue (16x16 Single-Sprite)',
    mode: '1pose',
    height: 16,
    palette: GBC_PALETTES.gbc_nocturne_purple.colors,
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
