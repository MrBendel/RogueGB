#!/usr/bin/env node
// CLI tool for generating Game Boy / GBC 2bpp assets (Chests, Power-Ups, UI/HUD, Weapons, Enemies)
// using Gemini Structured Pixel Matrices or exporting starter assets.
//
// Usage:
//   node tools/forge-cli.mjs --list
//   node tools/forge-cli.mjs --starter chest_dungeon --out build/gen/chest_dungeon
//   GEMINI_API_KEY=AIza... node tools/forge-cli.mjs --category chest --size 16x16 --prompt "Mimic chest with teeth" --out build/gen/chest_mimic
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { encodePng } from '../harness/node.mjs';
import {
  ASSET_CATEGORIES,
  STARTER_ASSETS,
  parseSizeSpec,
  parseAssetRows,
  generateAssetWithGemini,
  compileAssetToGbdk,
} from '../harness/asset-forge.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

export function renderAssetSheetPng(asset, scale = 4) {
  const W = asset.width || 16;
  const H = asset.height || 16;
  const states = asset.states || [];
  const totalW = W * Math.max(1, states.length);
  const rgb = new Uint8Array(totalW * H * 3);
  for (let i = 0; i < totalW * H; i++) {
    rgb[i * 3] = 0xd8;
    rgb[i * 3 + 1] = 0xdf;
    rgb[i * 3 + 2] = 0xd0;
  }
  states.forEach((st, idx) => {
    const grid = st.grid || parseAssetRows(st.rows || [], W, H);
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        const v = grid[y * W + x] & 3;
        if (!v) continue;
        const [r, g, b] = asset.palette[v] || [0, 0, 0];
        const di = (y * totalW + (idx * W + x)) * 3;
        rgb[di] = r;
        rgb[di + 1] = g;
        rgb[di + 2] = b;
      }
    }
  });
  return encodePng(rgb, { width: totalW, height: H, scale });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  if (args.includes('--list')) {
    console.log('Starter 2bpp assets:');
    for (const a of STARTER_ASSETS) {
      console.log(`  ${a.id.padEnd(20)} [${a.category} ${a.width}x${a.height}] ${a.name} (${a.states.map((s) => s.name).join(', ')})`);
    }
    console.log('\nCategories:', Object.keys(ASSET_CATEGORIES).join(', '));
    process.exit(0);
  }

  const argVal = (flag, def) => {
    const i = args.indexOf(flag);
    return i >= 0 && args[i + 1] ? args[i + 1] : def;
  };
  const starterId = argVal('--starter', null);
  const category = argVal('--category', 'chest');
  const sizeStr = argVal('--size', ASSET_CATEGORIES[category]?.defaultSize || '16x16');
  const prompt = argVal('--prompt', '');
  const model = argVal('--model', 'auto');
  const apiKey = argVal('--key', process.env.GEMINI_API_KEY || '');

  let asset;
  if (starterId || (!prompt && !apiKey)) {
    asset = STARTER_ASSETS.find((a) => a.id === starterId) || STARTER_ASSETS[0];
  } else {
    const { width, height } = parseSizeSpec(sizeStr);
    console.log(`[forge-cli] Requesting ${width}x${height} ${category} from Gemini (${model})...`);
    asset = await generateAssetWithGemini({
      apiKey,
      model,
      category,
      width,
      height,
      prompt,
    });
  }

  const outBase = argVal('--out', path.join('build', 'gen', asset.id));
  const absOut = path.resolve(ROOT, outBase);
  const { header, source, stats } = compileAssetToGbdk(asset, path.basename(absOut));
  fs.mkdirSync(path.dirname(absOut), { recursive: true });
  fs.writeFileSync(`${absOut}.h`, header);
  fs.writeFileSync(`${absOut}.c`, source);
  fs.writeFileSync(`${absOut}.json`, JSON.stringify(asset, null, 2));
  fs.writeFileSync(`${absOut}-sheet.png`, renderAssetSheetPng(asset, 4));
  console.log(
    `[forge-cli] Generated ${asset.name} (${stats.width}x${stats.height}, ${stats.states} states, ` +
      `${stats.uniqueTiles} unique 8x8 tiles, ${stats.romBytes} B ROM) -> ${path.relative(ROOT, absOut)}.{h,c,json} + ${path.basename(absOut)}-sheet.png`,
  );
}
