// Dialogue: content/dialogue/*.json -> build/gen/dialogue.c + dialogue.h, banked
// tables for the dialogue box (src/dialog.c). Called by tools/build.mjs; also
// runnable on its own: node tools/dialogue.mjs
//
// A file has speakers and scripts:
//   "speakers": { "echo": { "name": "ECHO", "color": "#207880", "sound": "relay" } }
//   "scripts": [ { "id": "yard_hello", "lines": [ { "who": "echo", "text": "..." } ] } ]
// Each line is wrapped here, with the italic font's real letter widths, into
// pages of DLG_PAGE_LINES lines that fit the box; a long line takes several pages.
// A "\n" in the text starts a new line, and "\n\n" a new page. Sounds play when a
// speaker's line opens: "relay" (Echo's channel), "chime" (the company's three notes).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { GEN_DIR, decodePng } from './assets.mjs';
import { FONTS, sliceFont } from './fonts.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const SRC_DIR = path.join(ROOT, 'content', 'dialogue');
const SHEET = path.join(ROOT, 'artwork', 'fonts', 'font-sheet.png');

export const DLG_WRAP_PX = 140;  // a line's width in the box (dialog.c: 18 tiles, the pen 2 px in)
export const DLG_PAGE_LINES = 2;
const NAME_MAX = 12;
const SOUNDS = { relay: 'SFX_RELAY', chime: 'SFX_CHIME' };

let metrics = null;
/** The italic font's widths: { width(char), has(char) }. */
function italic() {
  if (metrics) return metrics;
  const f = FONTS.find((x) => x.name === 'italic');
  const { chars, glyphs } = sliceFont(decodePng(fs.readFileSync(SHEET)), f);
  metrics = {
    has: (ch) => ch === ' ' || chars.has(ch),
    width: (s) => {
      let w = 0;
      for (const ch of s) w += ch === ' ' ? f.space : glyphs[chars.get(ch)].w + f.gap;
      return s.length ? w - (s.at(-1) === ' ' ? 0 : f.gap) : 0;
    },
  };
  return metrics;
}

/** Wrap one speech into pages (arrays of lines). Throws on letters the font lacks. */
export function wrap(text, where = 'dialogue') {
  const m = italic();
  for (const ch of text.replace(/\n/g, '')) if (!m.has(ch)) throw new Error(`${where}: the italic font has no '${ch}'`);
  const pages = [];
  for (const para of text.split(/\n\n/)) {
    const lines = [];
    for (const hard of para.split('\n')) {
      let line = '';
      for (const word of hard.split(/ +/).filter(Boolean)) {
        const next = line ? `${line} ${word}` : word;
        if (m.width(next) <= DLG_WRAP_PX) line = next;
        else {
          if (!line) throw new Error(`${where}: "${word}" is too wide for a line`);
          lines.push(line);
          line = word;
        }
      }
      lines.push(line);
    }
    for (let i = 0; i < lines.length; i += DLG_PAGE_LINES) pages.push(lines.slice(i, i + DLG_PAGE_LINES));
  }
  return pages;
}

const rgb555 = (hex) => {
  const v = parseInt(hex.replace('#', ''), 16);
  const r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
  return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);
};

/** Every dialogue file, checked: { speakers: [{ key, name, color, sound }], scripts: [{ id, pages: [{ who, lines }] }] }. */
export function loadDialogue() {
  const files = fs.existsSync(SRC_DIR) ? fs.readdirSync(SRC_DIR).filter((f) => f.endsWith('.json')).sort() : [];
  const speakers = [], scripts = [];
  const who = new Map();
  for (const f of files) {
    const doc = JSON.parse(fs.readFileSync(path.join(SRC_DIR, f), 'utf8'));
    for (const [key, s] of Object.entries(doc.speakers || {})) {
      if (who.has(key)) throw new Error(`content/dialogue/${f}: speaker '${key}' is defined twice`);
      if (!s.name || s.name.length > NAME_MAX) throw new Error(`content/dialogue/${f}: speaker '${key}' needs a name of 1-${NAME_MAX} letters`);
      if (s.sound && !SOUNDS[s.sound]) throw new Error(`content/dialogue/${f}: speaker '${key}': sound is one of ${Object.keys(SOUNDS).join(', ')}`);
      who.set(key, speakers.length);
      speakers.push({ key, name: s.name, color: s.color || '#181425', sound: s.sound });
    }
  }
  for (const f of files) {
    const doc = JSON.parse(fs.readFileSync(path.join(SRC_DIR, f), 'utf8'));
    for (const s of doc.scripts || []) {
      if (!/^[a-z][a-z0-9_]*$/.test(s.id || '')) throw new Error(`content/dialogue/${f}: script id '${s.id}' must be lower_snake_case`);
      if (scripts.some((x) => x.id === s.id)) throw new Error(`content/dialogue/${f}: script '${s.id}' is defined twice`);
      const pages = [];
      for (const [i, l] of (s.lines || []).entries()) {
        const where = `content/dialogue/${f}, ${s.id} line ${i + 1}`;
        if (!who.has(l.who)) throw new Error(`${where}: no speaker '${l.who}'`);
        for (const p of wrap(l.text || '', where)) pages.push({ who: who.get(l.who), lines: p });
      }
      if (!pages.length) throw new Error(`content/dialogue/${f}: script '${s.id}' has no lines`);
      scripts.push({ id: s.id, pages });
    }
  }
  return { speakers, scripts };
}

/** Write build/gen/dialogue.{c,h} when a dialogue file, the font sheet or this tool is newer. Returns the .c path. */
export function buildDialogue({ quiet = false } = {}) {
  fs.mkdirSync(GEN_DIR, { recursive: true });
  const c = path.join(GEN_DIR, 'dialogue.c'), h = path.join(GEN_DIR, 'dialogue.h');
  const inputs = [fileURLToPath(import.meta.url), SHEET, path.join(ROOT, 'tools', 'fonts.mjs')];
  if (fs.existsSync(SRC_DIR)) for (const f of fs.readdirSync(SRC_DIR)) inputs.push(path.join(SRC_DIR, f));
  const newest = Math.max(...inputs.map((f) => fs.statSync(f).mtimeMs));
  if (fs.existsSync(c) && fs.existsSync(h) && fs.statSync(c).mtimeMs > newest) return c;
  const { speakers, scripts } = loadDialogue();
  const pages = scripts.flatMap((s) => s.pages);
  const text = [], at = [], len = [];
  for (const p of pages) {
    const bytes = Buffer.from(p.lines.join('\n'), 'latin1');
    if (bytes.length > 72) throw new Error(`dialogue: a page is ${bytes.length} bytes (at most 72: DLG_PAGE_MAX)`);
    at.push(text.length);
    len.push(bytes.length);
    text.push(...bytes);
  }
  const first = [];
  let k = 0;
  for (const s of scripts) { first.push(k); k += s.pages.length; }
  const list = (v) => v.join(', ');
  const strs = (v) => v.map((s) => JSON.stringify(s)).join(', ');
  fs.writeFileSync(h, [
    '// Generated by tools/dialogue.mjs from content/dialogue/*.json. Do not edit.',
    '#ifndef GEN_DIALOGUE_H', '#define GEN_DIALOGUE_H', '#include <stdint.h>', '#include <gb/gb.h>',
    'BANKREF_EXTERN(dialogue)',
    `#define DLG_PAGE_LINES ${DLG_PAGE_LINES}`, `#define DLG_SPEAKERS ${speakers.length}`, `#define DLG_SCRIPTS ${scripts.length}`,
    `#define DLG_NAME_MAX ${NAME_MAX}`,
    ...speakers.map((s, i) => `#define DLG_WHO_${s.key.toUpperCase()} ${i}`),
    ...scripts.map((s, i) => `#define DLG_${s.id.toUpperCase()} ${i}`),
    'extern const uint8_t dlg_text[];               // pages, their lines split by \'\\n\', no terminators',
    'extern const uint16_t dlg_page_at[];           // where each page starts in dlg_text',
    'extern const uint8_t dlg_page_len[];',
    'extern const uint8_t dlg_page_who[];           // the speaker',
    'extern const uint16_t dlg_script_first[DLG_SCRIPTS];  // its first page',
    'extern const uint8_t dlg_script_pages[DLG_SCRIPTS];',
    'extern const char dlg_speaker_name[DLG_SPEAKERS][DLG_NAME_MAX + 1];',
    'extern const uint16_t dlg_speaker_color[DLG_SPEAKERS];  // the name\'s color (RGB555)',
    'extern const uint8_t dlg_speaker_sound[DLG_SPEAKERS];   // an SFX_ id, or 0xFF',
    '#endif', '',
  ].join('\n'));
  fs.writeFileSync(c, [
    '#pragma bank 255',
    '// Generated by tools/dialogue.mjs from content/dialogue/*.json. Do not edit.',
    '#include <gb/gb.h>', '#include "dialogue.h"', '#include "sfx.h"', 'BANKREF(dialogue)',
    `const uint8_t dlg_text[] = {${list(text)}};`,
    `const uint16_t dlg_page_at[] = {${list(at)}};`,
    `const uint8_t dlg_page_len[] = {${list(len)}};`,
    `const uint8_t dlg_page_who[] = {${list(pages.map((p) => p.who))}};`,
    `const uint16_t dlg_script_first[DLG_SCRIPTS] = {${list(first)}};`,
    `const uint8_t dlg_script_pages[DLG_SCRIPTS] = {${list(scripts.map((s) => s.pages.length))}};`,
    `const char dlg_speaker_name[DLG_SPEAKERS][DLG_NAME_MAX + 1] = {${strs(speakers.map((s) => s.name))}};`,
    `const uint16_t dlg_speaker_color[DLG_SPEAKERS] = {${list(speakers.map((s) => rgb555(s.color)))}};`,
    `const uint8_t dlg_speaker_sound[DLG_SPEAKERS] = {${list(speakers.map((s) => (s.sound ? SOUNDS[s.sound] : '0xFF')))}};`,
    '',
  ].join('\n'));
  if (!quiet) console.log(`[dialogue] content/dialogue -> build/gen/dialogue.c (${scripts.length} scripts, ${pages.length} pages)`);
  return c;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const { scripts } = loadDialogue();
  for (const s of scripts) {
    console.log(`== ${s.id}`);
    for (const p of s.pages) console.log(`  [${p.who}] ${p.lines.join(' / ')}`);
  }
  buildDialogue();
}
