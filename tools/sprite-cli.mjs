#!/usr/bin/env node
// CLI tool & preset loader for translating a single 16x16 character sprite (or
// 3-pose down/up/side base from artwork/) into full Game Boy animations + weapons.
//
// Usage:
//   node tools/sprite-cli.mjs --list
//   node tools/sprite-cli.mjs --preset monk --out build/gen/spr_monk
//   node tools/sprite-cli.mjs --in "artwork/8bitadventure/NPCs/monk down (1).png" --name spr_monk
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { decodePng } from './assets.mjs';
import { encodePng } from '../harness/node.mjs';
import {
  quantizeRgbaToGrid16,
  extractGridAndPalette,
  flipGridX,
  generateCharacterRig,
  compileRigToGbdk,
  CHARACTER_PRESETS,
  GBC_PALETTES,
  DMG_PALETTES,
} from '../harness/sprite-gen.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function loadGridAndPal(relPath) {
  const full = path.join(ROOT, relPath);
  if (!fs.existsSync(full)) return null;
  const img = decodePng(fs.readFileSync(full));
  return extractGridAndPalette(img.rgba, img.width, img.height);
}

function loadGrid(relPath) {
  const res = loadGridAndPal(relPath);
  return res ? res.grid : null;
}

/** Load real 16x16 character presets + GBC 4-color palettes from artwork/8bitadventure/ (if present). */
export function loadArtworkPresets() {
  const out = {};
  const playerDir = 'artwork/8bitadventure/Player';
  const pDownRes = loadGridAndPal(`${playerDir}/player walk down (1).png`);
  if (pDownRes) {
    const pDown = pDownRes.grid;
    const atkD = [1, 2, 3].map((i) => loadGrid(`${playerDir}/player attack down (${i}).png`)).filter(Boolean);
    const atkU = [1, 2, 3].map((i) => loadGrid(`${playerDir}/player attack up (${i}).png`)).filter(Boolean);
    const atkR = [1, 2, 3].map((i) => loadGrid(`${playerDir}/player attack right (${i}).png`)).filter(Boolean);
    out.player_pack = {
      id: 'player_pack',
      name: '8-Bit Hero (Artwork Pack)',
      mode: '3pose',
      palette: pDownRes.palette,
      down: Array.from(pDown),
      up: Array.from(loadGrid(`${playerDir}/player walk up (1).png`) || pDown),
      right: Array.from(loadGrid(`${playerDir}/player walk right (1).png`) || pDown),
      attack_down: atkD.length === 3 ? atkD.map((g) => Array.from(g)) : null,
      attack_up: atkU.length === 3 ? atkU.map((g) => Array.from(g)) : null,
      attack_right: atkR.length === 3 ? atkR.map((g) => Array.from(g)) : null,
    };
  }
  const npcs = [
    { id: 'npc_guard', name: 'Guard (Artwork NPC)', prefix: 'guard b', hasRight: true },
    { id: 'npc_monk', name: 'Monk (Artwork NPC)', prefix: 'monk', hasRight: false },
    { id: 'npc_nocturne', name: 'Nocturne (Artwork NPC)', prefix: 'nocturne', hasRight: false },
    { id: 'npc_prince', name: 'Prince (Artwork NPC)', prefix: 'prince', hasRight: false },
    { id: 'npc_spiky', name: 'Spiky (Artwork NPC)', prefix: 'spiky', hasRight: false },
    { id: 'npc_lass', name: 'Lass (Artwork NPC)', prefix: 'lass', hasRight: false },
    { id: 'npc_warden', name: 'Warden (Artwork NPC)', prefix: 'warden', hasRight: false },
    { id: 'npc_santa', name: 'Santa (Artwork NPC)', prefix: 'santa', hasRight: false },
    { id: 'npc_template', name: 'Base Template (Artwork)', prefix: 'template', hasRight: false },
    { id: 'npc_template_fat', name: 'Stout Template (Artwork)', prefix: 'template fat', hasRight: false },
  ];
  const npcDir = 'artwork/8bitadventure/NPCs';
  for (const n of npcs) {
    const downRes = loadGridAndPal(`${npcDir}/${n.prefix} down (1).png`);
    if (!downRes) continue;
    const down = downRes.grid;
    const up = loadGrid(`${npcDir}/${n.prefix} up (1).png`);
    const rightRaw = n.hasRight
      ? loadGrid(`${npcDir}/${n.prefix} right (1).png`)
      : loadGrid(`${npcDir}/${n.prefix} left (1).png`);
    const right = rightRaw ? (n.hasRight ? rightRaw : flipGridX(rightRaw)) : null;
    out[n.id] = {
      id: n.id,
      name: n.name,
      mode: '3pose',
      palette: downRes.palette,
      down: Array.from(down),
      up: up ? Array.from(up) : null,
      right: right ? Array.from(right) : null,
    };
  }
  return out;
}

/** Render a character rig's animations to an RGB contact sheet PNG buffer using its GBC palette. */
export function renderRigSheetPng(rig, scale = 2) {
  const animEntries = Object.entries(rig.anims);
  const maxFrames = Math.max(...animEntries.map(([, f]) => f.length));
  const cols = maxFrames;
  const rows = animEntries.length;
  const cell = 16;
  const W = cols * cell;
  const H = rows * cell;
  const rgb = new Uint8Array(W * H * 3);
  const pal = rig.palette || GBC_PALETTES.gbc_hero_red.colors;
  // Fill background with light neutral backdrop
  for (let i = 0; i < W * H; i++) {
    rgb[i * 3] = 0xf4;
    rgb[i * 3 + 1] = 0xf3;
    rgb[i * 3 + 2] = 0xee;
  }
  animEntries.forEach(([, frames], r) => {
    frames.forEach((g, c) => {
      for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
          const v = g[y * 16 + x];
          if (v === 0) continue;
          const [pr, pg, pb] = pal[v];
          const di = ((r * 16 + y) * W + (c * 16 + x)) * 3;
          rgb[di] = pr;
          rgb[di + 1] = pg;
          rgb[di + 2] = pb;
        }
      }
    });
  });
  return encodePng(rgb, { width: W, height: H, scale });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const allPresets = { ...loadArtworkPresets(), ...CHARACTER_PRESETS };
  if (args.includes('--list')) {
    console.log('Available character presets:');
    for (const [k, v] of Object.entries(allPresets)) {
      console.log(`  ${k.padEnd(18)} ${v.name} (${v.mode})`);
    }
    process.exit(0);
  }
  const argVal = (flag, def) => {
    const i = args.indexOf(flag);
    return i >= 0 && args[i + 1] ? args[i + 1] : def;
  };
  const inFile = argVal('--in', null);
  const presetId = argVal('--preset', 'player_pack');
  const cName = argVal('--name', 'spr_custom');
  const outBase = argVal('--out', path.join('build', 'gen', cName));
  const singlePose = args.includes('--single');

  let rigInput;
  if (inFile) {
    const res = loadGridAndPal(inFile);
    if (!res) {
      console.error(`[sprite-cli] File not found: ${inFile}`);
      process.exit(1);
    }
    rigInput = { name: path.basename(inFile, '.png'), down: res.grid, palette: res.palette };
  } else {
    const p = allPresets[presetId] || Object.values(allPresets)[0];
    rigInput = {
      name: p.name,
      palette: p.palette,
      down: new Uint8Array(p.down),
      up: !singlePose && p.up ? new Uint8Array(p.up) : null,
      right: !singlePose && p.right ? new Uint8Array(p.right) : null,
      attack_down: !singlePose && p.attack_down ? p.attack_down.map((g) => new Uint8Array(g)) : null,
      attack_up: !singlePose && p.attack_up ? p.attack_up.map((g) => new Uint8Array(g)) : null,
      attack_right: !singlePose && p.attack_right ? p.attack_right.map((g) => new Uint8Array(g)) : null,
    };
  }

  const rig = generateCharacterRig(rigInput);
  const { header, source, stats } = compileRigToGbdk(rig, cName);
  const absOut = path.resolve(ROOT, outBase);
  fs.mkdirSync(path.dirname(absOut), { recursive: true });
  fs.writeFileSync(`${absOut}.h`, header);
  fs.writeFileSync(`${absOut}.c`, source);
  fs.writeFileSync(`${absOut}-sheet.png`, renderRigSheetPng(rig, 2));
  console.log(
    `[sprite-cli] Generated ${cName}: ${stats.anims} anims, ${stats.frames} frames, ` +
    `${stats.columns} unique 8x16 columns (${stats.romBytes} ROM bytes) -> ${path.relative(ROOT, absOut)}.{h,c} + ${path.basename(absOut)}-sheet.png`,
  );
}
