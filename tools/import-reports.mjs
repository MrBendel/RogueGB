#!/usr/bin/env node
// Converts bug reports / recordings pulled from the hosted playtest page's
// database into replay files under replays/, so they can be replayed and
// turned into tests.
//
//   node tools/import-reports.mjs <dir-of-pulled-json> [--delete-source]
//
// Claude pulls the reports with its ArtifactData tool (list "reports" with
// out_dir), then runs this. Each document looks like
// { kind, note, created, length, rom_crc32, replay: "<replay JSON string>" }.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const src = process.argv[2];
if (!src || !fs.existsSync(src)) {
  console.error('usage: node tools/import-reports.mjs <dir> [--delete-source]');
  process.exit(1);
}
const files = fs.readdirSync(src, { recursive: true }).filter((f) => String(f).endsWith('.json'));
let n = 0;
for (const f of files) {
  const full = path.join(src, String(f));
  let doc = JSON.parse(fs.readFileSync(full, 'utf8'));
  doc = doc.data ?? doc; // tolerate wrapped exports
  if (typeof doc.replay !== 'string') continue;
  let replay = JSON.parse(doc.replay);
  const sub = doc.kind === 'bug' ? 'bugs' : 'recordings';
  const dir = path.join(ROOT, 'replays', sub);
  fs.mkdirSync(dir, { recursive: true });
  const stamp = (doc.created || new Date().toISOString()).replace(/[:.]/g, '-').slice(0, 19);
  const slug = (doc.note || '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '').slice(0, 40);
  const id = path.basename(String(f), '.json');
  const name = `${stamp}${slug ? '_' + slug : ''}_hosted-${id}`;
  if (replay.screenshot?.startsWith('data:image/png;base64,')) {
    fs.writeFileSync(path.join(dir, name + '.png'), Buffer.from(replay.screenshot.split(',')[1], 'base64'));
    replay = { ...replay, screenshot: name + '.png' };
  }
  fs.writeFileSync(path.join(dir, name + '.json'), JSON.stringify(replay, null, 1));
  console.log(`replays/${sub}/${name}.json  (${replay.length} frames, rom ${replay.rom?.crc32}) ${doc.note ? '- ' + doc.note : ''}`);
  if (process.argv.includes('--delete-source')) fs.rmSync(full);
  n++;
}
console.log(`[import] ${n} report(s) imported`);
