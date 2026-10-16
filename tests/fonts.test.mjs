// The font converter (tools/fonts.mjs): letter widths, trimmed or the artist's.
import { test } from 'node:test';
import { assert } from './helpers.mjs';
import { sliceFont } from '../tools/fonts.mjs';

/** A sheet of 8x8 cells from rows of characters: '#' ink, 'm' magenta, '.' see-through. */
function sheet(cells) {
  const width = 8 * cells.length, height = 8;
  const rgba = new Uint8Array(width * height * 4);
  cells.forEach((rows, c) => rows.forEach((line, y) => [...line].forEach((ch, x) => {
    const o = (y * width + c * 8 + x) * 4;
    if (ch === '#') rgba.set([0x18, 0x14, 0x25, 255], o);
    if (ch === 'm') rgba.set([255, 0, 255, 255], o);
  })));
  return { width, height, rgba };
}
const font = { name: 't', x: 0, y: 0, cellW: 8, cellH: 8, gap: 1, space: 3, rows: ['il'], colors: { '181425': 3 } };
const blank = '........';

test('without magenta, letters are trimmed to their ink and the font gap goes between them', () => {
  const img = sheet([
    [blank, '..#.....', blank, '..#.....', '..#.....', '..#.....', blank, blank],
    [blank, '..##....', '...#....', '...#....', '...#....', '...##...', blank, blank],
  ]);
  const { glyphs, gap, variable } = sliceFont(img, font);
  assert.equal(variable, false);
  assert.equal(gap, 1);
  assert.deepEqual(glyphs.map((g) => g.w), [1, 3]);
});

test('with magenta (GB Studio variable width), a width is the columns before the magenta, blanks kept, no gap', () => {
  const img = sheet([
    ['...mmmmm', '.#.mmmmm', '...mmmmm', '.#.mmmmm', '.#.mmmmm', '.#.mmmmm', '...mmmmm', '...mmmmm'],
    ['....mmmm', '.##.mmmm', '..#.mmmm', '..#.mmmm', '..#.mmmm', '..##mmmm', '....mmmm', '....mmmm'],
  ]);
  const { glyphs, gap, variable } = sliceFont(img, font);
  assert.equal(variable, true);
  assert.equal(gap, 0);
  assert.deepEqual(glyphs.map((g) => g.w), [3, 4], 'i is 3 wide (ink in the middle), l is 4');
  // i's ink is in its second column: nothing trimmed from the left.
  assert.equal((glyphs[0].rows[1] >> 12) & 3, 3);
});
