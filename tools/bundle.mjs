#!/usr/bin/env node
// Bundles the playtest page, emulator core and current ROM into one
// self-contained HTML file (build/playtest.html) for the hosted playtest
// artifact. Bug reports and recordings made there go to the artifact's
// database instead of the local replays/ folder.
//
//   node tools/bundle.mjs            -> build/playtest.html (artifact body form)
//   node tools/bundle.mjs --standalone  full HTML document you can open from disk
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { CONFIG } from './build.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (p) => fs.readFileSync(path.join(ROOT, p), 'utf8');
const standalone = process.argv.includes('--standalone');

const romPath = path.join(ROOT, 'build', `${CONFIG.name}.gb`);
if (!fs.existsSync(romPath)) {
  console.error('[bundle] build/roguegb.gb missing: run node tools/build.mjs first');
  process.exit(1);
}
const rom = fs.readFileSync(romPath);
const noi = romPath.replace(/\.gb$/, '.noi');
const sym = fs.existsSync(noi) ? fs.readFileSync(noi, 'utf8') : '';
// Keep only RAM variables from the symbol file; the page doesn't need the rest.
const symSmall = sym.split(/\r?\n/).filter((l) => /^DEF _[A-Za-z]\w* 0x(C|D|FF)[0-9A-F]{2,3}\b/i.test(l)).join('\n');
const wasm = fs.readFileSync(path.join(ROOT, 'harness', 'vendor', 'boytacean', 'boytacean_bg.wasm'));

// Turn an ES module into a function body returning its exports.
function moduleToIife(src, name) {
  const names = [];
  let out = src.replace(/^export\s+(async\s+function|function|class|const|let|var)\s+([A-Za-z_$][\w$]*)/gm, (_m, kw, id) => {
    names.push(id);
    return `${kw} ${id}`;
  });
  out = out.replace(/^export\s*\{([^}]*)\};?\s*$/gm, (_m, list) => {
    for (const part of list.split(',')) {
      const [local, alias] = part.trim().split(/\s+as\s+/);
      if (local) names.push(alias ? `${alias}: ${local}` : local);
    }
    return '';
  });
  out = out.replace(/new URL\('boytacean_bg\.wasm', import\.meta\.url\)/g, 'null');
  return `const ${name} = (() => {\n${out}\nreturn { ${names.join(', ')} };\n})();\n`;
}

const boy = moduleToIife(read('harness/vendor/boytacean/boytacean.js'), '__boy');
const emu = moduleToIife(read('harness/emu.mjs'), '__emu');
let app = read('web/app.js');
app = app.replace(/^import initCore, \* as B from [^;]+;/m, 'const B = __boy; const initCore = __boy.default;');
app = app.replace(/^import \{([\s\S]*?)\} from '\.\.\/harness\/emu\.mjs';/m, (_m, list) => `const {${list}} = __emu;`);
if (/^import /m.test(app)) throw new Error('bundle: unhandled import in web/app.js');

const backend = `
// Hosted playtest backend: ROM is embedded; reports go to the artifact db.
const __b64 = (s) => { const b = atob(s); const u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i); return u; };
window.RGB_WASM = __b64(${JSON.stringify(wasm.toString('base64'))});
const __ROM = ${JSON.stringify(rom.toString('base64'))};
const __SYM = ${JSON.stringify(symSmall)};
const __BUILT = ${JSON.stringify(new Date().toISOString())};
const __claude = window.claude;
const __db = __claude?.use ? __claude.use('db') : Promise.resolve(null);
const __dl = __claude?.use ? __claude.use('downloads') : Promise.resolve(null);
window.RGB_BACKEND = {
  name: 'hosted',
  async loadRom() { return { rom: __b64(__ROM), sym: __SYM, label: 'hosted build ' + __BUILT.slice(0, 16).replace('T', ' ') }; },
  async listReplays() {
    const db = await __db; if (!db) return [];
    const snap = await db.collection('reports').orderBy('created', 'desc').limit(40).get();
    return snap.docs.map((d) => { const v = d.data(); return { id: d.id, name: (v.kind === 'bug' ? 'bug' : 'rec') + ' · ' + v.length + 'f · ' + (v.note || v.created.slice(0, 16)) }; });
  },
  async getReplay(id) {
    const db = await __db; const d = await db.doc('reports/' + id).get();
    return JSON.parse(d.data().replay);
  },
  async saveReplay(replay, kind) {
    const db = await __db;
    if (!db) throw new Error('saving needs the Claude artifact viewer');
    const ref = await db.collection('reports').add({
      kind, note: replay.note || '', created: replay.created, length: replay.length,
      rom_crc32: replay.rom.crc32, replay: JSON.stringify(replay),
    });
    return ref.id;
  },
  async download(filename, data) {
    const dl = await __dl;
    if (!dl) throw new Error('downloads unavailable here');
    await dl.save({ filename, data });
  },
};
`;

const css = read('web/style.css');
let body = read('web/index.html');
body = body.slice(body.indexOf('<body>') + 6, body.indexOf('</body>'));
body = body.replace(/<script type="module" src="app.js"><\/script>/, '');
body = body.replace('<span>playtest</span>', `<span>playtest · hosted</span>`);

const script = `<script type="module">\n${backend}\n${boy}\n${emu}\n${app}\n</script>`;
const page = `<title>RogueGB Playtest</title>\n<style>\n${css}\n</style>\n${body}\n${script}\n`;
const html = standalone
  ? `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>${page}</body></html>`
  : page;
const out = path.join(ROOT, 'build', standalone ? 'playtest-standalone.html' : 'playtest.html');
fs.writeFileSync(out, html);
console.log(`[bundle] ${path.relative(ROOT, out)} (${(html.length / 1024).toFixed(0)} KB, ROM ${rom.length} bytes, crc ${(await import('../harness/emu.mjs')).crc32(new Uint8Array(rom))})`);
