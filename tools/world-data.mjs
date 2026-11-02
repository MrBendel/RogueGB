// The world viewer's data for a static site (GitHub Pages, where there's no dev
// server): build/world-data/ gets the built ROM and its symbols, every chunk file
// (as the dev server's /api/chunks lists them) and the tilesets' block atlases.
// The Pages workflow copies it next to web/world.html (docs/design/level-generation.md).
//
//   node tools/world-data.mjs     (after a build)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'build', 'world-data');
const CHUNKS = path.join(ROOT, 'content', 'chunks');
const GEN = path.join(ROOT, 'build', 'gen');

fs.mkdirSync(OUT, { recursive: true });
for (const f of ['roguegb.gb', 'roguegb.noi']) {
  const src = path.join(ROOT, 'build', f);
  if (!fs.existsSync(src)) throw new Error(`build/${f} is missing: build first`);
  fs.copyFileSync(src, path.join(OUT, f));
}
const files = fs.readdirSync(CHUNKS).filter((f) => f.endsWith('.json')).sort()
  .map((file) => ({ file, doc: JSON.parse(fs.readFileSync(path.join(CHUNKS, file), 'utf8')) }));
fs.writeFileSync(path.join(OUT, 'chunks.json'), JSON.stringify(files));
// A bought pack's atlas is its art as a plain image: kept off the site (the ROM
// has it, the viewer draws those maps without art). Stand-in atlases are ours.
const packArt = (ts) => {
  const def = path.join(ROOT, 'content', 'tilesets', `${ts}.json`);
  if (!fs.existsSync(def) || !JSON.parse(fs.readFileSync(def, 'utf8')).local) return false;
  return !JSON.parse(fs.readFileSync(path.join(GEN, `atlas-${ts}.json`), 'utf8')).standIn;
};
let atlases = 0, held = 0;
for (const f of fs.readdirSync(GEN).filter((f) => /^atlas-.*\.json$/.test(f))) {
  const ts = f.slice(6, -5);
  if (packArt(ts)) { held++; continue; }
  for (const ext of ['json', 'png']) fs.copyFileSync(path.join(GEN, `atlas-${ts}.${ext}`), path.join(OUT, `atlas-${ts}.${ext}`));
  atlases++;
}
console.log(`[world-data] build/world-data: the ROM, ${files.length} chunk files, ${atlases} atlases (${held} art packs left out)`);
