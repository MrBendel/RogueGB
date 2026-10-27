#!/usr/bin/env node
// Builds build/roguegb.gb from src/ (plus tiles generated from artwork/ by
// tools/assets.mjs) with GBDK-2020.
//
//   node tools/build.mjs            incremental build
//   node tools/build.mjs --clean    rebuild everything
//   node tools/build.mjs --quiet    only print errors and the final line
//
// GBDK is found in this order:
//   1. $GBDK_HOME (folder containing bin/lcc)
//   2. tools/gbdk/            (extracted release)
//   3. tools/gbdk-win64.zip or tools/gbdk-linux64.tar.gz, auto-extracted
//      into tools/.gbdk-<platform>/ on first use
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { buildAssets, GEN_DIR } from './assets.mjs';
import { buildChunks } from './chunks.mjs';
import { buildFonts } from './fonts.mjs';
import { buildDialogue } from './dialogue.mjs';
import { buildSkills } from './skills.mjs';
import { buildTilesets } from './tileset.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

// ---- project settings --------------------------------------------------------
export const CONFIG = {
  name: 'roguegb',
  title: 'ROGUEGB', // ROM header title, max 11 chars for CGB-aware carts
  // MBC5 + RAM + battery: room to grow (banks) and to save meta-progression.
  cartType: '0x1B',
  romBanks: 'A', // A = let makebin size the ROM automatically
  ramBanks: 1, // 8 KB SRAM
  cgb: 'only', // Game Boy Color only (-Wm-yC): decided 2026-10; 'compat' => -Wm-yc, false => DMG
  sgb: false,
  cflags: ['-Wa-l', '-Wf--max-allocs-per-node50000'],
};

const isWin = process.platform === 'win32';
const exe = (p) => (isWin ? p + '.exe' : p);

export function findGbdk() {
  const candidates = [];
  if (process.env.GBDK_HOME) candidates.push(process.env.GBDK_HOME);
  // Common results of extracting the release zip by hand on Windows.
  for (const d of ['gbdk', path.join('gbdk', 'gbdk'), path.join('gbdk-win64', 'gbdk')]) candidates.push(path.join(ROOT, 'tools', d));
  const plat = isWin ? 'win64' : process.platform === 'darwin' ? 'macos' : 'linux64';
  const cache = path.join(ROOT, 'tools', `.gbdk-${plat}`);
  candidates.push(path.join(cache, 'gbdk'));
  for (const c of candidates) if (fs.existsSync(path.join(c, 'bin', exe('lcc')))) return c;

  const archive = ['gbdk-win64.zip', 'gbdk-linux64.tar.gz', 'gbdk-macos-arm64.tar.gz', 'gbdk-macos.tar.gz']
    .filter((a) => a.includes(plat === 'macos' ? 'macos' : plat))
    .map((a) => path.join(ROOT, 'tools', a))
    .find((a) => fs.existsSync(a));
  if (archive) {
    console.log(`[build] extracting ${path.basename(archive)} -> tools/.gbdk-${plat}/`);
    fs.mkdirSync(cache, { recursive: true });
    // bsdtar ships with Windows 10+ and reads .zip; GNU tar handles .tar.gz.
    const r = spawnSync('tar', ['-xf', archive, '-C', cache], { stdio: 'inherit' });
    if (r.status !== 0) throw new Error(`could not extract ${archive}`);
    if (fs.existsSync(path.join(cache, 'gbdk', 'bin', exe('lcc')))) return path.join(cache, 'gbdk');
  }
  throw new Error(
    'GBDK-2020 not found. Either set GBDK_HOME, extract the release to tools/gbdk/, ' +
      `or put gbdk-${plat}${isWin ? '.zip' : '.tar.gz'} in tools/ (https://github.com/gbdk-2020/gbdk-2020/releases).`,
  );
}

function walk(dir, ext) {
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((d) => {
    const p = path.join(dir, d.name);
    if (d.isDirectory()) return walk(p, ext);
    return ext.some((e) => d.name.endsWith(e)) ? [p] : [];
  });
}

const mtime = (p) => (fs.existsSync(p) ? fs.statSync(p).mtimeMs : 0);

function run(cmd, args, quiet) {
  const r = spawnSync(cmd, args, { cwd: ROOT, encoding: 'utf8' });
  const out = (r.stdout || '') + (r.stderr || '');
  if (r.error) throw r.error;
  if (r.status !== 0 || (out.trim() && (!quiet || /warning|error/i.test(out)))) process.stdout.write(out);
  return { ok: r.status === 0, out };
}

// Bank 0 (0x0000-0x3FFF) is always visible; banks 1+ share the 0x4000 window and
// are switched in on demand. Code and data that aren't marked `#pragma bank 255`
// go in fixed areas that must all fit in bank 0: if they overflow past 0x4000 they
// land in whatever bank is switched in, and the game crashes the first time a
// different bank is selected. So the build fails on overflow, and also when bank 0
// is nearly full, to leave room for hot code.
export const BANK0_LIMIT = 0.9;
const FIXED_AREAS = ['_HEADER', '_HOME', '_BASE', '_CODE', '_LIT', '_GSINIT', '_GSFINAL', '_INITIALIZER'];

// Work RAM: globals start at 0xC0A0 and grow up; the room maps and the tables
// beside them sit at fixed addresses from ROOM_MAP_ADDR (src/room.h), and the
// linker won't notice globals growing into them. Fail the build if they do.
// The bosses' state is overlaid just below the room map (src/room.h BOSSES_ADDR,
// src/bosses.s): only one boss fights at a time, so each of these files' globals
// start at BOSSES_ADDR. Only files whose state is untouched outside their own
// fight belong here (play_enter zeroes the area for every fight).
export const BOSSES_ADDR = 0xd3c0;
const BOSSES_SIZE = 0x40;
const BOSS_FILES = ['cragback.c', 'flynt.c', 'globfather.c', 'gunship.c', 'scrapback.c', 'warden.c'];
export function checkRam(noiPath) {
  const syms = new Map();
  for (const m of fs.readFileSync(noiPath, 'utf8').matchAll(/^DEF (\S+) 0x([0-9A-F]+)/gim)) syms.set(m[1], parseInt(m[2], 16));
  let end = 0;
  for (const area of ['_DATA', '_BSS', '_INITIALIZED']) {
    const start = syms.get('s_' + area), len = syms.get('l_' + area);
    if (start !== undefined && len && start >= 0xc000 && start < 0xe000) end = Math.max(end, start + len);
  }
  const bosses = syms.get('l___BOSSES') || 0;
  if (syms.get('s___BOSSES') !== BOSSES_ADDR || bosses > BOSSES_SIZE) {
    return { ok: false, error: `[build] the bosses' state is ${bosses} bytes at 0x${(syms.get('s___BOSSES') || 0).toString(16)}: ` +
      `it must fit ${BOSSES_SIZE} at 0x${BOSSES_ADDR.toString(16)} (src/room.h BOSSES_ADDR; is src/bosses.s linked first?)` };
  }
  if (end > BOSSES_ADDR) {
    return { ok: false, error: `[build] globals end at 0x${end.toString(16)}, into the bosses' state at 0x${BOSSES_ADDR.toString(16)} ` +
      '(src/room.h). Shrink them, or move cold data to far RAM (src/far.h).' };
  }
  return { ok: true, end, bosses };
}

export function checkBank0(noiPath) {
  const syms = new Map();
  for (const m of fs.readFileSync(noiPath, 'utf8').matchAll(/^DEF (\S+) 0x([0-9A-F]+)/gim)) syms.set(m[1], parseInt(m[2], 16));
  let used = 0;
  for (const area of FIXED_AREAS) {
    const start = syms.get('s_' + area), len = syms.get('l_' + area);
    if (start === undefined || !len) continue;
    if (start + len > 0x4000) {
      return { ok: false, error: `[build] ${area} ends at 0x${(start + len).toString(16)}, past bank 0 (0x4000). ` +
        'Move cold code or data to a switchable bank with #pragma bank 255 (see README, ROM banks).' };
    }
    if (start < 0x4000) used += len;
  }
  // The header and interrupt vectors fill 0x000-0x1FF.
  used += 0x200;
  if (used > 0x4000 * BANK0_LIMIT) {
    return { ok: false, error: `[build] bank 0 is ${Math.round((used / 0x4000) * 100)}% full (limit ${BANK0_LIMIT * 100}%). ` +
      'Move cold code or data to a switchable bank with #pragma bank 255 (see README, ROM banks).' };
  }
  return { ok: true, used };
}

/** Returns { ok, rom, ms, changed } */
export function build({ clean = false, quiet = false } = {}) {
  const t0 = Date.now();
  const gbdk = findGbdk();
  const lcc = path.join(gbdk, 'bin', exe('lcc'));
  const srcDir = path.join(ROOT, 'src');
  const objDir = path.join(ROOT, 'build', 'obj');
  const out = path.join(ROOT, 'build', `${CONFIG.name}.gb`);
  if (clean) fs.rmSync(path.join(ROOT, 'build', 'obj'), { recursive: true, force: true });
  fs.mkdirSync(objDir, { recursive: true });

  // Generated sources (artwork -> tiles, room chunks) live in build/gen/ and are never committed.
  buildFonts({ quiet });  // a header, included by src/text.c
  const genSources = [...buildAssets({ quiet }), buildChunks({ quiet }), buildDialogue({ quiet }), buildSkills({ quiet }), ...buildTilesets({ quiet })];
  const sources = [...walk(srcDir, ['.c', '.s']), ...genSources];
  if (!sources.length) throw new Error('no sources in src/');
  const newestHeader = Math.max(0, ...walk(srcDir, ['.h', '.inc']).map(mtime), ...walk(GEN_DIR, ['.h']).map(mtime), mtime(fileURLToPath(import.meta.url)));

  const objs = [];
  let compiled = 0;
  for (const src of sources) {
    const rel = src.startsWith(GEN_DIR) ? 'gen__' + path.basename(src) : path.relative(srcDir, src).replace(/[\\/]/g, '__');
    const obj = path.join(objDir, rel.replace(/\.(c|s)$/, '.o'));
    objs.push(obj);
    if (mtime(obj) > Math.max(mtime(src), newestHeader)) continue;
    if (!quiet) console.log(`[build] cc ${path.relative(ROOT, src)}`);
    const seg = BOSS_FILES.includes(path.basename(src)) ? ['-Wf--dataseg', '-Wf_BOSSES'] : [];  // (checkRam)
    const r = run(lcc, [...CONFIG.cflags, ...seg, '-debug', `-I${srcDir}`, `-I${GEN_DIR}`, '-c', '-o', obj, src], quiet);
    if (!r.ok) return { ok: false, ms: Date.now() - t0, error: r.out };
    compiled++;
  }

  const linkNeeded = compiled > 0 || mtime(out) < Math.max(...objs.map(mtime));
  if (linkNeeded) {
    const flags = [
      // -autobank runs bankpack, which by default rewrites the .o files in place;
      // that broke incremental links. -ext=.rel makes it write copies instead.
      '-debug', '-autobank', '-Wb-ext=.rel',
      `-Wm-yt${CONFIG.cartType}`, `-Wm-yo${CONFIG.romBanks}`, `-Wm-ya${CONFIG.ramBanks}`,
      `-Wm-yn${CONFIG.title}`, '-Wm-yS',
      `-Wl-b__BOSSES=0x${BOSSES_ADDR.toString(16)}`,
    ];
    if (CONFIG.cgb === 'only') flags.push('-Wm-yC');
    else if (CONFIG.cgb) flags.push('-Wm-yc');
    if (CONFIG.sgb) flags.push('-Wm-ys');
    if (!quiet) console.log(`[build] link ${path.relative(ROOT, out)}`);
    // (bosses.o first: its area's flags make the bosses' state overlay: checkRam)
    const first = objs.filter((o) => path.basename(o) === 'bosses.o');
    const r = run(lcc, [...flags, '-o', out, ...first, ...objs.filter((o) => !first.includes(o))], quiet);
    if (!r.ok) return { ok: false, ms: Date.now() - t0, error: r.out };

    const bank0 = checkBank0(out.replace(/\.gb$/, '.noi'));
    const ram = checkRam(out.replace(/\.gb$/, '.noi'));
    if (!ram.ok) {
      fs.rmSync(out, { force: true });
      console.error(ram.error);
      return { ok: false, ms: Date.now() - t0, error: ram.error };
    }
    // Work RAM headroom, every build (far RAM, src/far.h, is the place for cold data).
    const free = BOSSES_ADDR - ram.end;
    console.log(`[build] work RAM: globals end at 0x${ram.end.toString(16)}, ${free} bytes before the bosses' state (${ram.bosses} of ${BOSSES_SIZE})${free < 64 ? ' (tight: move cold data to far RAM, src/far.h)' : ''}`);
    if (!bank0.ok) {
      // Remove the ROM so the next build can't treat it as up to date and skip this check.
      fs.rmSync(out, { force: true });
      console.error(bank0.error);
      return { ok: false, ms: Date.now() - t0, error: bank0.error };
    }
    const romusage = path.join(gbdk, 'bin', exe('romusage'));
    if (!quiet && fs.existsSync(romusage)) run(romusage, [out.replace(/\.gb$/, '.noi'), '-g'], false);
  }
  const ms = Date.now() - t0;
  console.log(`[build] ${linkNeeded ? 'built' : 'up to date'}: build/${CONFIG.name}.gb (${ms} ms)`);
  return { ok: true, rom: out, ms, changed: linkNeeded };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const r = build({ clean: process.argv.includes('--clean'), quiet: process.argv.includes('--quiet') });
    if (!r.ok) {
      console.error('[build] FAILED');
      process.exit(1);
    }
  } catch (e) {
    console.error('[build] ' + e.message);
    process.exit(1);
  }
}
