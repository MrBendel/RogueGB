// RogueGB Gemini 2bpp Asset Forge Engine (Weapons, UI/HUD, Chests/Props, Power-Ups, Enemies)
// Shared between the browser Asset Forge (web/forge.html) and Node CLI (tools/forge-cli.mjs).
//
// Enforces strict Game Boy / Game Boy Color hardware constraints:
// - 2bpp (4 shades per tile/sprite):
//     '.' or 0 = Transparent (or BG backdrop for UI tiles)
//     '1' or 1 = Light / Highlight / Specular
//     '2' or 2 = Mid / Primary Body Color
//     '3' or 3 = Dark / Silhouette Outline
// - Exact tile-aligned dimensions: 8x8 (1 tile), 16x16 (2x2 tiles / 2x 8x16 OAM), 16x24 (2x3 tiles / 4x 8x16 OAM)
// - 15-bit Game Boy Color RGB555 hardware palettes (snapped to 5 bits/channel).

import { GBC_PALETTES, snapColorToGbc, rgbToCgb555, formatGbcPaletteC } from './sprite-gen.mjs';

export const ASSET_CATEGORIES = {
  chest: {
    id: 'chest',
    name: '📦 Chests, Crates & Props',
    defaultSize: '16x16',
    allowedSizes: ['16x16', '8x8', '16x24'],
    defaultStates: ['closed', 'opening', 'open'],
    stateHint: 'Generate 3 states: "closed" (intact/locked), "opening" (lid popping / cracking / glowing), and "open" (opened / looted).',
    suggestions: [
      'Reinforced iron dungeon chest with a skull padlock (closed, popping open, open)',
      'Mimic treasure chest that reveals jagged teeth and tongue when opening',
      'Wooden supply crate with a stenciled ammo icon (intact, cracked, smashed)',
      'Ancient glowing blood shrine altar (dormant, activated, spent)',
      'Boss room spiked dungeon door (locked, unlocking, open)',
    ],
  },
  powerup: {
    id: 'powerup',
    name: '🧪 Power-Ups, Relics & Pickups',
    defaultSize: '16x16',
    allowedSizes: ['16x16', '8x8'],
    defaultStates: ['idle', 'pulse'],
    stateHint: 'Generate 2 animation states: "idle" (base item) and "pulse" (shifted up 1px or with bright shade-1 energy sparkles).',
    suggestions: [
      'Bubbling crimson health potion flask with cork stopper',
      'Winged cyber dash boots with electric sparks',
      'Floating arcane shield battery cell with glowing core',
      'Golden skeleton key with a diamond bow',
      'Heavy brass shotgun shell ammo box pickup',
      'Vampire fang relic charm that boosts life steal',
    ],
  },
  ui: {
    id: 'ui',
    name: '❤️ UI, HUD & Minimap Tiles',
    defaultSize: '8x8',
    allowedSizes: ['8x8', '16x16'],
    defaultStates: ['full', 'half', 'empty'],
    stateHint: 'Generate 3 HUD states: e.g. "full" (active/full), "half" (half/damaged), and "empty" (depleted/outline).',
    suggestions: [
      '8x8 HUD heart container (full heart, half heart, empty heart outline)',
      '8x8 HUD armor shield icon (full plate shield, cracked shield, broken outline)',
      '8x8 HUD ammo bullet pip (live brass cartridge, firing flash, empty casing)',
      '8x8 Minimap room icons (current room, unexplored room, boss skull room, chest room)',
      '16x16 HUD active weapon frame box with ornate pixel corners',
    ],
  },
  weapon: {
    id: 'weapon',
    name: '⚔️ Weapons (Melee & Guns)',
    defaultSize: '16x16',
    allowedSizes: ['16x16'],
    defaultStates: ['ready', 'active', 'diag_ur'],
    stateHint:
      'Generate 3 states for a 16x16 weapon: "ready" (pointing RIGHT horizontally, grip near left x=2..5), "active" (firing muzzle flash or melee swing smear pointing RIGHT), and "diag_ur" (angled 45 degrees UP-RIGHT from bottom-left hilt to top-right tip).',
    suggestions: [
      'Arcane plasma pistol with glowing barrel and muzzle blast',
      'Jagged thunder broadsword with crackling lightning arc',
      'Double-barrel sawed-off boomstick shotgun with fiery blast',
      'Bone necromancer wand topped with a horned skull',
      'Heavy gatling minigun with spinning barrels and flash',
    ],
  },
  enemy: {
    id: 'enemy',
    name: '👾 Dungeon Enemies & Hazards',
    defaultSize: '16x16',
    allowedSizes: ['16x16', '16x24', '8x8'],
    defaultStates: ['idle', 'move', 'attack'],
    stateHint: 'Generate 3 states: "idle" (standing/floating), "move" (stepping/bouncing), and "attack" (lunging/firing).',
    suggestions: [
      'Floating one-eyed dungeon beholder orb that shoots energy beams',
      'Armored skeleton warrior with a horned helmet',
      'Gelatinous dungeon slime cube with a skull floating inside',
      'Clockwork bomb spider bot that flashes before exploding',
    ],
  },
};

export function parseSizeSpec(sizeStr = '16x16') {
  const [w, h] = String(sizeStr).split('x').map((n) => parseInt(n, 10));
  return {
    width: w === 8 ? 8 : 16,
    height: h === 8 ? 8 : h === 24 ? 24 : 16,
  };
}

/** Parse an array of `.0123` strings into a Uint8Array(width * height) of shades 0..3. */
export function parseAssetRows(rows, width = 16, height = 16) {
  const out = new Uint8Array(width * height);
  const srcRows = Array.isArray(rows) ? rows : String(rows).trim().split(/\r?\n/);
  for (let y = 0; y < height; y++) {
    const line = String(srcRows[y] || '');
    for (let x = 0; x < width; x++) {
      const ch = line[x] || '.';
      out[y * width + x] = ch === '1' ? 1 : ch === '2' ? 2 : ch === '3' ? 3 : 0;
    }
  }
  return out;
}

/** Convert a Uint8Array(width * height) grid into an array of `.123` strings. */
export function gridToAssetRows(grid, width = 16, height = 16) {
  const rows = [];
  for (let y = 0; y < height; y++) {
    let s = '';
    for (let x = 0; x < width; x++) {
      const v = grid[y * width + x] & 3;
      s += v === 0 ? '.' : String(v);
    }
    rows.push(s);
  }
  return rows;
}

export function cloneAssetGrid(g) {
  return new Uint8Array(g);
}

export function flipAssetX(grid, width = 16, height = 16) {
  const out = new Uint8Array(width * height);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      out[y * width + x] = grid[y * width + (width - 1 - x)];
    }
  }
  return out;
}

export function shiftAssetGrid(grid, width = 16, height = 16, dx = 0, dy = 0) {
  const out = new Uint8Array(width * height);
  for (let y = 0; y < height; y++) {
    const sy = y - dy;
    if (sy < 0 || sy >= height) continue;
    for (let x = 0; x < width; x++) {
      const sx = x - dx;
      if (sx < 0 || sx >= width) continue;
      out[y * width + x] = grid[sy * width + sx];
    }
  }
  return out;
}

export function floodFillAssetGrid(grid, width = 16, height = 16, startX = 0, startY = 0, newShade = 3) {
  if (startX < 0 || startX >= width || startY < 0 || startY >= height) return grid;
  const target = grid[startY * width + startX];
  if (target === newShade) return grid;
  const stack = [[startX, startY]];
  while (stack.length) {
    const [x, y] = stack.pop();
    if (x < 0 || x >= width || y < 0 || y >= height) continue;
    if (grid[y * width + x] !== target) continue;
    grid[y * width + x] = newShade;
    stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
  }
  return grid;
}

/** Reinforce outer silhouette boundary pixels with Shade 3 (Dark Outline). */
export function outlineAssetGrid(grid, width = 16, height = 16) {
  const copy = cloneAssetGrid(grid);
  const out = cloneAssetGrid(grid);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      if (copy[y * width + x] === 0) continue;
      const touchesAir =
        x === 0 || x === width - 1 || y === 0 || y === height - 1 ||
        copy[y * width + (x - 1)] === 0 ||
        copy[y * width + (x + 1)] === 0 ||
        copy[(y - 1) * width + x] === 0 ||
        copy[(y + 1) * width + x] === 0;
      if (touchesAir) out[y * width + x] = 3;
    }
  }
  return out;
}

function hexToRgbaSnapped(hexStr, fallback) {
  const m = /^#?([0-9a-fA-F]{6})$/.exec(String(hexStr || '').trim());
  if (!m) return [...fallback];
  const n = parseInt(m[1], 16);
  return snapColorToGbc([(n >> 16) & 255, (n >> 8) & 255, n & 255, 255]);
}

// ---------------------------------------------------------------------------
// Pre-Loaded Starter Asset Library (Chests, Power-Ups, UI/HUD, Weapons, Enemies)

export const STARTER_ASSETS = [
  {
    id: 'chest_dungeon',
    name: 'Dungeon Treasure Chest',
    category: 'chest',
    width: 16,
    height: 16,
    palette: GBC_PALETTES.gbc_monk_gold.colors.map((c) => [...c]),
    states: [
      {
        name: 'closed',
        rows: [
          '................',
          '................',
          '..333333333333..',
          '.32111111111123.',
          '.32222311322223.',
          '.32222311322223.',
          '.33333311333333.',
          '.31111311311113.',
          '.33333333333333.',
          '.32222311322223.',
          '.32322333322323.',
          '.32222222222223.',
          '.32222222222223.',
          '.33222222222233.',
          '..333333333333..',
          '................',
        ],
      },
      {
        name: 'opening',
        rows: [
          '................',
          '..333333333333..',
          '.32111111111123.',
          '.32222311322223.',
          '.33333311333333.',
          '.31111111111113.',
          '.33113111131133.',
          '.33333333333333.',
          '.32222311322223.',
          '.32322333322323.',
          '.32222222222223.',
          '.32222222222223.',
          '.33222222222233.',
          '..333333333333..',
          '................',
          '................',
        ],
      },
      {
        name: 'open',
        rows: [
          '..333333333333..',
          '.32111111111123.',
          '.32222222222223.',
          '.33333333333333.',
          '.33311333311333.',
          '.33111133111133.',
          '.33333333333333.',
          '.31111311311113.',
          '.33333333333333.',
          '.32222311322223.',
          '.32322333322323.',
          '.32222222222223.',
          '.32222222222223.',
          '.33222222222233.',
          '..333333333333..',
          '................',
        ],
      },
    ],
  },
  {
    id: 'crate_supply',
    name: 'Supply Ammo Crate',
    category: 'chest',
    width: 16,
    height: 16,
    palette: GBC_PALETTES.gbc_gunmetal.colors.map((c) => [...c]),
    states: [
      {
        name: 'closed',
        rows: [
          '................',
          '..333333333333..',
          '.31111111111113.',
          '.31322222222313.',
          '.31232211223213.',
          '.31223111132213.',
          '.31221122112213.',
          '.31221222212213.',
          '.31221122112213.',
          '.31223111132213.',
          '.31232211223213.',
          '.31322222222313.',
          '.31111111111113.',
          '..333333333333..',
          '................',
          '................',
        ],
      },
      {
        name: 'opening',
        rows: [
          '................',
          '..33333..33333..',
          '.31113....31113.',
          '.31323.11.32313.',
          '.31233111133213.',
          '.312211..112213.',
          '..3221.11.1223..',
          '.312211..112213.',
          '.31223111132213.',
          '.31232211223213.',
          '.31322222222313.',
          '.31111111111113.',
          '..333333333333..',
          '................',
          '................',
          '................',
        ],
      },
      {
        name: 'open',
        rows: [
          '................',
          '................',
          '................',
          '................',
          '....33....33....',
          '...3123..3213...',
          '................',
          '..333......333..',
          '.31223.33.32213.',
          '.32323311332323.',
          '..333322223333..',
          '.31222233222213.',
          '..333333333333..',
          '................',
          '................',
          '................',
        ],
      },
    ],
  },
  {
    id: 'pwr_vital_flask',
    name: 'Vitality Potion Flask',
    category: 'powerup',
    width: 16,
    height: 16,
    palette: GBC_PALETTES.gbc_hero_red.colors.map((c) => [...c]),
    states: [
      {
        name: 'idle',
        rows: [
          '................',
          '......3333......',
          '.....311113.....',
          '......3223......',
          '......3113......',
          '.....311113.....',
          '....31111113....',
          '...3112222113...',
          '..311221122113..',
          '..312211222213..',
          '..312222222213..',
          '..312222222213..',
          '...3122222213...',
          '....33333333....',
          '................',
          '................',
        ],
      },
      {
        name: 'pulse',
        rows: [
          '......3333......',
          '..1..311113..1..',
          '......3223......',
          '......3113......',
          '.....311113.....',
          '....31111113....',
          '...3112112113...',
          '..311211112213..',
          '..312212212213..',
          '..312222222213..',
          '..312221122213..',
          '...3122222213...',
          '..1.33333333.1..',
          '................',
          '................',
          '................',
        ],
      },
    ],
  },
  {
    id: 'pwr_bolt_boots',
    name: 'Hermes Volt Boots',
    category: 'powerup',
    width: 16,
    height: 16,
    palette: GBC_PALETTES.gbc_volt_blade.colors.map((c) => [...c]),
    states: [
      {
        name: 'idle',
        rows: [
          '................',
          '.......3333.....',
          '....33311113....',
          '...311132223....',
          '....33113223....',
          '.....3221323....',
          '.....3222223....',
          '.....3211223....',
          '....321122233...',
          '...32222222223..',
          '..3211222222123.',
          '..3222222222223.',
          '..3111111111113.',
          '..3333333333333.',
          '................',
          '................',
        ],
      },
      {
        name: 'pulse',
        rows: [
          '.......3333..1..',
          '....33311113....',
          '..1311132223....',
          '....33113223....',
          '.....3221323.1..',
          '.....3222223....',
          '.....3211223....',
          '....321122233...',
          '...32222222223..',
          '..3211222222123.',
          '..3222222222223.',
          '..3111111111113.',
          '..3333333333333.',
          '....1......1....',
          '................',
          '................',
        ],
      },
    ],
  },
  {
    id: 'ui_hearts',
    name: 'HUD Heart Container (8x8)',
    category: 'ui',
    width: 8,
    height: 8,
    palette: GBC_PALETTES.gbc_hero_red.colors.map((c) => [...c]),
    states: [
      {
        name: 'full',
        rows: [
          '.33..33.',
          '31233223',
          '31122223',
          '32222223',
          '.322223.',
          '..3223..',
          '...33...',
          '........',
        ],
      },
      {
        name: 'half',
        rows: [
          '.33..33.',
          '31233..3',
          '3112...3',
          '3222...3',
          '.322..3.',
          '..32.3..',
          '...33...',
          '........',
        ],
      },
      {
        name: 'empty',
        rows: [
          '.33..33.',
          '3..33..3',
          '3......3',
          '3......3',
          '.3....3.',
          '..3..3..',
          '...33...',
          '........',
        ],
      },
    ],
  },
  {
    id: 'ui_minimap',
    name: 'HUD Minimap Room Icons (8x8)',
    category: 'ui',
    width: 8,
    height: 8,
    palette: GBC_PALETTES.gbc_steel_cyan.colors.map((c) => [...c]),
    states: [
      {
        name: 'current',
        rows: [
          '33333333',
          '31111113',
          '31222213',
          '31211213',
          '31211213',
          '31222213',
          '31111113',
          '33333333',
        ],
      },
      {
        name: 'boss',
        rows: [
          '33333333',
          '32111123',
          '31311313',
          '31111113',
          '32133123',
          '321.1.23',
          '32222223',
          '33333333',
        ],
      },
      {
        name: 'chest',
        rows: [
          '33333333',
          '32222223',
          '33333333',
          '31133113',
          '33311333',
          '32222223',
          '32222223',
          '33333333',
        ],
      },
    ],
  },
  {
    id: 'wpn_plasma_blaster',
    name: 'Arcane Plasma Sidearm',
    category: 'weapon',
    width: 16,
    height: 16,
    palette: GBC_PALETTES.gbc_nocturne_purple.colors.map((c) => [...c]),
    states: [
      {
        name: 'ready',
        rows: [
          '................',
          '................',
          '................',
          '................',
          '................',
          '....33333333....',
          '...3211211123...',
          '...32222222213..',
          '...3332233333...',
          '....3223........',
          '...3223.........',
          '...333..........',
          '................',
          '................',
          '................',
          '................',
        ],
      },
      {
        name: 'active',
        rows: [
          '................',
          '................',
          '................',
          '............1...',
          '...........111..',
          '....33333333111.',
          '...3211211121111',
          '...322222222111.',
          '...333223333111.',
          '....3223....1...',
          '...3223.........',
          '...333..........',
          '................',
          '................',
          '................',
          '................',
        ],
      },
      {
        name: 'diag_ur',
        rows: [
          '................',
          '................',
          '..........33....',
          '.........3123...',
          '........31223...',
          '.......31223....',
          '......32223.....',
          '.....32223......',
          '....33223.......',
          '...32233........',
          '...3223.........',
          '...333..........',
          '................',
          '................',
          '................',
          '................',
        ],
      },
    ],
  },
];

// ---------------------------------------------------------------------------
// Gemini Structured 2bpp Pixel Matrix Generator

function buildGeminiSystemPrompt(category, width, height, stateNames) {
  const catInfo = ASSET_CATEGORIES[category] || ASSET_CATEGORIES.chest;
  return [
    `You are a master Nintendo Game Boy / Game Boy Color (DMG/CGB) 2bpp pixel artist designing assets for the roguelike dungeon crawler "RogueGB".`,
    `You output STRICT, hardware-exact 2bpp pixel matrices where every row is a string of EXACTLY ${width} characters from the set { '.', '1', '2', '3' }, and each state has EXACTLY ${height} rows.`,
    ``,
    `SHADE MEANINGS (2bpp = 4 slots):`,
    `  '.' = Shade 0: Hardware Transparent (background air around the sprite)`,
    `  '1' = Shade 1: Light / Specular Highlight / Bright Core (highest luminance)`,
    `  '2' = Shade 2: Medium / Primary Body Fill Color (mid luminance)`,
    `  '3' = Shade 3: Dark / Silhouette Outline & Deep Crevices (lowest luminance, near-black)`,
    ``,
    `ESSENTIAL GAME BOY PIXEL ART RULES:`,
    `1. CONTIGUOUS DARK OUTLINE ('3'): Wrap the outer silhouette of the object in '3' so it reads crisply against any floor tile. Leave a 1px '.' transparent margin on the outer edges when possible.`,
    `2. TOP-LEFT LIGHTING ('1'): Place '1' highlights along top/left inner edges, metal rims, blade edges, gem cores, or energy sparks.`,
    `3. SOLID BODY READABILITY ('2'): Fill the primary volume with '2' and use internal '3' lines to separate mechanical parts (e.g. chest lid vs base, gun barrel vs grip, potion neck vs bulb).`,
    `4. MULTI-STATE COHERENCE: You must generate these states in order: [${stateNames.map((s) => `"${s}"`).join(', ')}].`,
    `   ${catInfo.stateHint}`,
    `   Keep the anchor position and silhouette proportions consistent across states so animating between them looks hand-pixelled!`,
    `5. EXACT DIMENSIONS: Every single row string MUST have length === ${width}, and every state MUST have rows.length === ${height}.`,
  ].join('\n');
}

function normalizeRowString(rawRow, width) {
  const cleaned = String(rawRow || '')
    .replace(/0|\s/g, '.')
    .replace(/[^.123]/g, '.');
  if (cleaned.length === width) return cleaned;
  if (cleaned.length > width) return cleaned.slice(0, width);
  return cleaned.padEnd(width, '.');
}

function normalizeRowsArray(rawRows, width, height) {
  const list = Array.isArray(rawRows) ? rawRows : [];
  const out = [];
  for (let y = 0; y < height; y++) {
    out.push(normalizeRowString(list[y] || '', width));
  }
  return out;
}

/**
 * Call the Gemini API (`generateContent`) with a strict JSON schema to generate
 * or refine a multi-state 2bpp Game Boy asset + 15-bit GBC RGB555 palette.
 */
export async function generateAssetWithGemini({
  apiKey,
  model = 'gemini-2.5-flash',
  category = 'chest',
  width = 16,
  height = 16,
  prompt = '',
  existingAsset = null,
}) {
  if (!apiKey || !String(apiKey).trim()) {
    throw new Error('Missing Gemini API key — paste your key in the Gemini API Key box first.');
  }
  const catInfo = ASSET_CATEGORIES[category] || ASSET_CATEGORIES.chest;
  const stateNames = existingAsset?.states?.length
    ? existingAsset.states.map((s) => s.name)
    : catInfo.defaultStates;

  const systemText = buildGeminiSystemPrompt(category, width, height, stateNames);
  const userLines = [
    `Asset Category: ${catInfo.name} (${category})`,
    `Target Grid Size: ${width} columns wide × ${height} rows tall`,
    `Required States (${stateNames.length}): ${stateNames.join(', ')}`,
    `User Prompt: ${prompt || catInfo.suggestions[0]}`,
  ];
  if (existingAsset && Array.isArray(existingAsset.states)) {
    userLines.push(
      ``,
      `EXISTING PIXEL GRIDS TO REFINE / MODIFY (preserve what works, apply the user's requested changes):`,
      JSON.stringify(
        {
          name: existingAsset.name,
          states: existingAsset.states.map((s) => ({ name: s.name, rows: s.rows })),
        },
        null,
        2,
      ),
    );
  }

  const responseSchema = {
    type: 'OBJECT',
    required: ['id', 'name', 'paletteHex', 'states'],
    properties: {
      id: {
        type: 'STRING',
        description: 'Short snake_case identifier for C code, e.g. chest_mimic or wpn_railgun',
      },
      name: {
        type: 'STRING',
        description: 'Human-readable title of the asset',
      },
      paletteHex: {
        type: 'OBJECT',
        required: ['light', 'mid', 'dark'],
        properties: {
          light: { type: 'STRING', description: 'Hex color #RRGGBB for Shade 1 (bright highlight)' },
          mid: { type: 'STRING', description: 'Hex color #RRGGBB for Shade 2 (primary fill)' },
          dark: { type: 'STRING', description: 'Hex color #RRGGBB for Shade 3 (dark outline, near black)' },
        },
      },
      states: {
        type: 'ARRAY',
        items: {
          type: 'OBJECT',
          required: ['name', 'rows'],
          properties: {
            name: { type: 'STRING', description: 'State name (e.g. closed, opening, open)' },
            rows: {
              type: 'ARRAY',
              description: `Array of EXACTLY ${height} strings, each of length ${width}, using only '.', '1', '2', '3'.`,
              items: { type: 'STRING' },
            },
          },
        },
      },
    },
  };

  const url = `https://generativelanguage.googleapis.com/v1beta/models/${encodeURIComponent(model)}:generateContent?key=${encodeURIComponent(apiKey.trim())}`;
  const body = {
    systemInstruction: { parts: [{ text: systemText }] },
    contents: [{ role: 'user', parts: [{ text: userLines.join('\n') }] }],
    generationConfig: {
      temperature: 0.45,
      responseMimeType: 'application/json',
      responseSchema,
    },
  };

  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  if (!res.ok) {
    let errDetail = `${res.status} ${res.statusText}`;
    try {
      const errJson = await res.json();
      if (errJson?.error?.message) errDetail = errJson.error.message;
    } catch {}
    throw new Error(`Gemini API error: ${errDetail}`);
  }

  const data = await res.json();
  const rawText = data?.candidates?.[0]?.content?.parts?.[0]?.text;
  if (!rawText) {
    throw new Error('Gemini returned an empty response — try rephrasing your prompt.');
  }
  const parsed = JSON.parse(rawText);

  const defPal = GBC_PALETTES.gbc_monk_gold.colors;
  const palette = [
    [0, 0, 0, 0],
    hexToRgbaSnapped(parsed.paletteHex?.light, defPal[1]),
    hexToRgbaSnapped(parsed.paletteHex?.mid, defPal[2]),
    hexToRgbaSnapped(parsed.paletteHex?.dark, defPal[3]),
  ];

  const states = (parsed.states || []).map((st, idx) => ({
    name: String(st.name || stateNames[idx] || `state_${idx}`).replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase(),
    rows: normalizeRowsArray(st.rows, width, height),
  }));
  while (states.length < stateNames.length) {
    const fallbackRows = states[0]?.rows || Array.from({ length: height }, () => '.'.repeat(width));
    states.push({
      name: stateNames[states.length],
      rows: [...fallbackRows],
    });
  }

  const cleanId = String(parsed.id || `${category}_custom`)
    .toLowerCase()
    .replace(/[^a-z0-9_]/g, '_')
    .replace(/^_+|_+$/g, '') || `${category}_custom`;

  return {
    id: cleanId,
    name: parsed.name || 'Gemini Custom Asset',
    category,
    width,
    height,
    palette,
    states,
  };
}

// ---------------------------------------------------------------------------
// GBDK 2bpp Tile & Sprite Compiler for Multi-State Assets

const hexByte = (b) => '0x' + (b & 0xff).toString(16).toUpperCase().padStart(2, '0');

/** Encode an 8x8 tile at (x0, y0) from a WxH asset grid into 16 Game Boy 2bpp bytes. */
export function encodeAssetTile8x8(grid, width, height, x0, y0) {
  const out = [];
  for (let r = 0; r < 8; r++) {
    const y = y0 + r;
    let lo = 0, hi = 0;
    if (y >= 0 && y < height) {
      for (let c = 0; c < 8; c++) {
        const x = x0 + c;
        const shade = x >= 0 && x < width ? (grid[y * width + x] & 3) : 0;
        lo |= (shade & 1) << (7 - c);
        hi |= (shade >> 1) << (7 - c);
      }
    }
    out.push(lo, hi);
  }
  return out;
}

/**
 * Compile a multi-state asset (8x8, 16x16, or 16x24) into deduplicated GBDK 2bpp 8x8 tiles,
 * per-state tile index maps, and a 15-bit CGB `palette_color_t` array.
 */
export function compileAssetToGbdk(asset, cPrefix = null) {
  const width = asset.width || 16;
  const height = asset.height || 16;
  const cName = (cPrefix || asset.id || 'asset_custom').replace(/[^a-zA-Z0-9_]/g, '_').toLowerCase();
  const NAME = cName.toUpperCase();
  const tilesX = Math.ceil(width / 8);
  const tilesY = Math.ceil(height / 8);
  const tilesPerState = tilesX * tilesY;

  const uniqueTiles = [];
  const tileMap = new Map();
  const addTile = (bytes) => {
    const key = bytes.join(',');
    if (tileMap.has(key)) return tileMap.get(key);
    const idx = uniqueTiles.length;
    uniqueTiles.push(bytes);
    tileMap.set(key, idx);
    return idx;
  };

  const stateMaps = [];
  for (const st of asset.states || []) {
    const grid = st.grid || parseAssetRows(st.rows, width, height);
    const indices = [];
    for (let ty = 0; ty < tilesY; ty++) {
      for (let tx = 0; tx < tilesX; tx++) {
        indices.push(addTile(encodeAssetTile8x8(grid, width, height, tx * 8, ty * 8)));
      }
    }
    stateMaps.push({ name: st.name, indices });
  }

  const pal = asset.palette || GBC_PALETTES.gbc_monk_gold.colors;
  const romBytes = uniqueTiles.length * 16 + stateMaps.length * tilesPerState + 8;

  const header = [
    `// Generated by RogueGB Asset Forge (${asset.name}, ${width}x${height}, ${stateMaps.length} states).`,
    `#ifndef GEN_${NAME}_H`,
    `#define GEN_${NAME}_H`,
    '#include <stdint.h>',
    '#include <gb/cgb.h>',
    `#define ${NAME}_WIDTH ${width}`,
    `#define ${NAME}_HEIGHT ${height}`,
    `#define ${NAME}_TILES_X ${tilesX}`,
    `#define ${NAME}_TILES_Y ${tilesY}`,
    `#define ${NAME}_TILE_COUNT ${uniqueTiles.length}`,
    `#define ${NAME}_STATE_COUNT ${stateMaps.length}`,
    ...stateMaps.map((s, i) => `#define ${NAME}_STATE_${s.name.toUpperCase()} ${i}`),
    `extern const palette_color_t ${cName}_pal[4];`,
    `extern const uint8_t ${cName}_tiles[];`,
    `extern const uint8_t ${cName}_states[${stateMaps.length}][${tilesPerState}];`,
    '#endif',
    '',
  ].join('\n');

  const source = [
    `// Generated by RogueGB Asset Forge (${asset.name}, ${width}x${height}).`,
    `#include "${cName}.h"`,
    formatGbcPaletteC(pal, `${cName}_pal`),
    `const uint8_t ${cName}_tiles[] = {`,
    ...uniqueTiles.map((t, i) => `    /* tile ${i} */ ${t.map(hexByte).join(', ')},`),
    '};',
    `const uint8_t ${cName}_states[${stateMaps.length}][${tilesPerState}] = {`,
    ...stateMaps.map((s) => `    { ${s.indices.join(', ')} }, // ${s.name}`),
    '};',
    '',
  ].join('\n');

  return {
    header,
    source,
    stats: {
      width,
      height,
      states: stateMaps.length,
      uniqueTiles: uniqueTiles.length,
      totalTileSlots: stateMaps.length * tilesPerState,
      romBytes,
    },
  };
}
