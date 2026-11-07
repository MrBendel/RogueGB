// Draws the entrance room's goals board (artwork/tiles/goals_board.png, 32x32: a
// piece of content/tilesets/lunar_int.json, stamped from its "board" sheet) from
// ASCII art. Its four colours are the room's orange lights' (one palette), so it
// fits the tileset's palettes. Our own art: committed.
//   node tools/make-board-art.mjs
//
// Characters: 'k' black, 'w' white, 'o' orange, 'r' red.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { encodePng } from '../harness/node.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'artwork', 'tiles', 'goals_board.png');
const COLORS = { k: [0, 0, 0], w: [248, 248, 248], o: [224, 104, 16], r: [192, 48, 16] };

// A framed board: "GOALS" on an orange bar, then a checklist (the first ticked).
// The inside is 26 x 26, framed in black, white and red.
const LETTERS = {
  G: ['.##', '#..', '#.#', '#.#', '.##'],
  O: ['.#.', '#.#', '#.#', '#.#', '.#.'],
  A: ['.#.', '#.#', '###', '#.#', '#.#'],
  L: ['#..', '#..', '#..', '#..', '###'],
  S: ['.##', '#..', '.#.', '..#', '##.'],
};
const inside = [];
for (let y = 0; y < 26; y++) inside.push(Array(26).fill(y < 7 ? 'o' : 'k'));
[...'GOALS'].forEach((ch, i) => LETTERS[ch].forEach((r, y) => [...r].forEach((c, x) => { if (c === '#') inside[1 + y][3 + i * 4 + x] = 'k'; })));
// Checklist items: a box (the first ticked) and a line of "text".
[[9, 14, 'o'], [14, 17, 'k'], [19, 12, 'k']].forEach(([y, len, tick]) => {
  for (let dy = 0; dy < 3; dy++) for (let dx = 0; dx < 3; dx++) inside[y + dy][1 + dx] = 'w';
  inside[y + 1][2] = tick;
  for (let x = 0; x < len; x++) inside[y + 1][6 + x] = 'w';
});
const ART = [
  'k'.repeat(32),
  'k' + 'w'.repeat(30) + 'k',
  'kw' + 'r'.repeat(28) + 'wk',
  ...inside.map((r) => 'kwr' + r.join('') + 'rwk'),
  'kw' + 'r'.repeat(28) + 'wk',
  'k' + 'w'.repeat(30) + 'k',
  'k'.repeat(32),
];

if (ART.length !== 32 || ART.some((r) => r.length !== 32)) throw new Error(`art must be 32x32 (${ART.map((r) => r.length)})`);
const rgb = new Uint8Array(32 * 32 * 3);
ART.forEach((row, y) => [...row].forEach((c, x) => rgb.set(COLORS[c], (y * 32 + x) * 3)));
fs.writeFileSync(OUT, encodePng(rgb, { width: 32, height: 32 }));
console.log(`wrote ${path.relative(ROOT, OUT)}`);
