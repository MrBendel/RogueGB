import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { decodePng } from '../tools/assets.mjs';
import { convert, tileBudget } from '../tools/tileset.mjs';

test('Sands preserves the original scene outside four authored ground transitions', () => {
  const ts = convert('sands');
  const src = decodePng(fs.readFileSync('artwork/tiles/sands.png'));
  const round = v => ((v >> 3) << 3) | (v >> 5);
  for (let y = 0; y < 192; y++) for (let x = 0; x < 240; x++) {
    const b = ts.maps.town.vis[(y >> 4) * ts.maps.town.w + (x >> 4)];
    const tile = ts.tiles[ts.blocks[b][((y & 15) >> 3) * 2 + ((x & 15) >> 3)]];
    const actual = tile.idx[(y & 7) * 8 + (x & 7)];
    const i = (y * src.width + x) * 4;
    if (y >= 176 && [2,6,7,8].includes(x >> 4)) continue; // four ground transition cells
    const rgb = src.rgba[i + 3] ? [...src.rgba.slice(i, i + 3)] : ts.def.backdrop;
    const expected = (round(rgb[0]) << 16) | (round(rgb[1]) << 8) | round(rgb[2]);
    const shade = [0xe7ffd6, 0x8cc673, 0x316b52, 0x081821].indexOf(expected);
    assert.notEqual(shade, -1);
    assert.equal(actual, shade, `source detail at ${x},${y}`);
  }
  assert.deepEqual(ts.palettes[0], [0xf7dea5, 0xe7ce8c, 0x946b42, 0x312921], 'warm rock ramp replaces the green guide colours');
  assert.ok(ts.stored.length > 172, 'unique details use the second bank');
  assert.ok(ts.stored.length <= tileBudget(ts.def), 'second bank stops before tile 128');
  assert.ok(ts.blocks.length <= 128, 'visual-block RAM budget');
});

test('extra background tiles require an explicit authored-scene loader', () => {
  assert.equal(tileBudget({}), 172);
  assert.equal(tileBudget({ vramBanks: 2 }), 220);
  assert.throws(() => tileBudget({ vramBanks: 2, theme: {} }));
  assert.throws(() => tileBudget({ vramBanks: 3 }));
});


test('authored town pieces are used, and tile flips save VRAM without losing detail', () => {
  for (const name of ['sands', 'town_inside']) {
    const ts = convert(name), used = new Set(Object.values(ts.maps).flatMap(m => m.vis));
    assert.equal(used.size, ts.blocks.length, `${name}: no unused blocks`);
    const unflipped = new Set(ts.tiles.map(t => t.idx.join(''))).size;
    assert.ok(ts.stored.length < unflipped, `${name}: flipped reuse is active`);
    assert.ok(ts.stored.length < ts.tiles.length, `${name}: repeated pieces are deduplicated`);
  }
});


test('storage tiers darken through palette attributes without duplicating patterns', () => {
  const ts = convert('sands');
  const tilesAt = (x,y) => ts.blocks[ts.maps.town.vis[y*16+x]].map(t => ts.tiles[t]);
  for (let x=10;x<14;x++) {
    const top=tilesAt(x,12);
    for(let tier=0;tier<3;tier++) {
      const row=tilesAt(x,12+tier);
      assert.deepEqual(row.map(t => [t.t,t.fx,t.fy]),top.map(t => [t.t,t.fx,t.fy]));
      assert.ok(row.every(t => t.pal===tier));
      assert.equal(ts.maps.town.type[(12+tier)*16+x],1);
    }
  }
  for(const ramp of ts.palettes) assert.equal(ramp[1],ts.palettes[0][1],'shared sand shade');
  for(let tier=1;tier<3;tier++) for(const shade of [0,2,3])
    assert.ok(ts.palettes[tier][shade]<ts.palettes[tier-1][shade]);
});
