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
let atlases = 0;
for (const f of fs.readdirSync(GEN).filter((f) => /^atlas-.*\.(png|json)$/.test(f))) {
  fs.copyFileSync(path.join(GEN, f), path.join(OUT, f));
  atlases++;
}
console.log(`[world-data] build/world-data: the ROM, ${files.length} chunk files, ${atlases / 2} atlases`);
