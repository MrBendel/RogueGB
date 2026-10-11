// Player sprite: pre-rendered frames, facing, walk cycle, mirroring and the
// double-buffered VRAM slot. Issue #3.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const ANIM = { DOWN: 0, UP: 1, RIGHT: 2, LEFT: 3 };
const S_FLIPX = 0x20;

/** OAM entry i: { y, x, tile, flags } */
function oam(emu, i) {
  const [y, x, tile, flags] = emu.readBytes(0xfe00 + i * 4, 4);
  return { y, x, tile, flags };
}
/** The 32 bytes of an 8x16 sprite column starting at VRAM tile t. */
const column = (emu, t) => Array.from(emu.readBytes(0x8000 + t * 16, 32));
const reverseBits = (b) => {
  let r = 0;
  for (let i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i;
  return r;
};
/** Both columns as drawn, with flips applied, so frames can be compared visually. */
function drawnColumns(emu) {
  return [0, 1].map((i) => {
    const o = oam(emu, i);
    const bytes = column(emu, o.tile);
    return o.flags & S_FLIPX ? bytes.map(reverseBits) : bytes;
  });
}

test('the art faces the way you walk', () => {
  for (const [dir, anim] of Object.entries(ANIM)) {
    const emu = startRun().hold(dir, 6).idle(12);
    assert.equal(emu.v('player_anim_dir'), anim, dir);
    assert.equal(emu.v('player_anim_frame'), 0, `${dir}: standing pose after stopping`);
    matchGolden(emu, `player-${dir.toLowerCase()}`);
  }
});

test('left-facing frames are the right-facing ones mirrored', () => {
  const right = startRun().hold('RIGHT', 6).idle(12);
  const left = startRun().hold('LEFT', 6).idle(12);
  const [rl, rr] = drawnColumns(right);
  const [ll, lr] = drawnColumns(left);
  // Mirroring a 16-px frame: the left column is the right column flipped, and vice versa.
  assert.deepEqual(ll, rr.map(reverseBits));
  assert.deepEqual(lr, rl.map(reverseBits));
  // And it's done with the flip flag, not extra tile data.
  assert.ok(oam(left, 0).flags & S_FLIPX);
  assert.ok(oam(left, 1).flags & S_FLIPX);
});

test('the walk cycle steps with distance and resets when standing', () => {
  const emu = startRun().idle(2);
  const seen = new Set();
  for (let f = 0; f < 40; f++) {
    emu.hold('UP', 1);
    seen.add(emu.v('player_anim_frame'));
  }
  assert.deepEqual([...seen].sort(), [0, 1, 2, 3]);
  emu.idle(12);
  assert.equal(emu.v('player_anim_frame'), 0);
});

test('pushing into a wall walks in place', () => {
  // The start position touches the bottom wall.
  const emu = startRun().idle(2);
  const y0 = emu.v('player_y', 'u16');
  const seen = new Set();
  for (let f = 0; f < 30; f++) {
    emu.hold('DOWN', 1);
    seen.add(emu.v('player_anim_frame'));
  }
  assert.equal(emu.v('player_y', 'u16'), y0);
  assert.ok(seen.size > 1, 'animation advanced');
});

test('on a diagonal the sprite keeps facing a direction that is part of it', () => {
  const emu = startRun().idle(2).hold('RIGHT', 5).hold('UP+RIGHT', 5);
  assert.equal(emu.v('player_anim_dir'), ANIM.RIGHT);
  emu.hold('UP', 3).hold('UP+LEFT', 5);
  assert.equal(emu.v('player_anim_dir'), ANIM.UP);
  emu.hold('DOWN+LEFT', 3);
  assert.equal(emu.v('player_anim_dir'), ANIM.LEFT, 'neither part matches: face sideways');
});

test('the player uses a fixed 8-tile VRAM slot, alternating halves', () => {
  const emu = startRun().idle(2);
  const tiles = new Set();
  let swaps = 0;
  let last = oam(emu, 0).tile;
  for (let f = 0; f < 60; f++) {
    emu.hold(f < 30 ? 'RIGHT' : 'LEFT', 1);
    const t0 = oam(emu, 0).tile, t1 = oam(emu, 1).tile;
    tiles.add(t0).add(t1);
    assert.equal(t1, t0 + 2, 'right column follows the left column');
    if (t0 !== last) swaps++;
    last = t0;
  }
  assert.deepEqual([...tiles].sort((a, b) => a - b), [0, 2, 4, 6]);
  assert.ok(swaps > 4, `frame changes swapped halves (${swaps})`);
});
