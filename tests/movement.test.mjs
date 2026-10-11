// Movement lab: easing presets, diagonal speed, collision, wall sliding, corner nudging,
// and the pause-screen tuner. Issues #1 and #2.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

// Must match move_profiles in src/player.c.
const PRESETS = [
  { name: 'SNAPPY', top: 256, accel: 5, decel: 4, reverse: 3 },
  { name: 'SMOOTH', top: 384, accel: 10, decel: 8, reverse: 6 },
  { name: 'FLOATY', top: 384, accel: 20, decel: 24, reverse: 14 },
  { name: 'CLASSIC', top: 384, accel: 1, decel: 1, reverse: 1 },
];
const CUSTOM = 0xff;

// Same rounding as rate_for() in player.c: never slower than the requested frame count.
const rate = (top, frames) => Math.ceil(top / frames);
const framesFor = (top, frames) => Math.ceil(top / rate(top, frames));

// Positions are room pixels (player_x/y) plus a 1/256 fraction (player_xs/ys).
const px = (emu) => emu.v('player_x', 'u16');
const py = (emu) => emu.v('player_y', 'u16');
const fineX = (emu) => emu.v('player_x', 'u16') * 256 + emu.v('player_xs');
const vx = (emu) => emu.v('player_vx', 'i16');
const vy = (emu) => emu.v('player_vy', 'i16');

/** Start a run with the given preset selected (SELECT cycles presets). */
function runWithPreset(i) {
  const emu = startRun().idle(2);
  for (let k = 0; k < i; k++) emu.tap('SELECT');
  assert.equal(emu.v('move_preset'), i);
  return emu;
}

/** Hold buttons one frame at a time until pred holds; returns frames taken. */
function framesUntil(emu, buttons, pred, max = 120) {
  for (let f = 1; f <= max; f++) {
    emu.hold(buttons, 1);
    if (pred(emu)) return f;
  }
  assert.fail(`not reached within ${max} frames`);
}

test('SELECT cycles the movement presets', () => {
  const emu = startRun().idle(2);
  assert.equal(emu.v('move_preset'), 0);
  for (const i of [1, 2, 3, 0]) {
    emu.tap('SELECT');
    assert.equal(emu.v('move_preset'), i);
    assert.equal(emu.v('move_accel_frames'), PRESETS[i].accel);
  }
});

for (const [i, p] of PRESETS.entries()) {
  test(`preset ${p.name}: accelerate, stop and reverse take the tuned frame counts`, () => {
    const emu = runWithPreset(i);
    assert.equal(emu.v('move_top', 'u16'), p.top);

    const up = framesUntil(emu, 'RIGHT', (e) => vx(e) === p.top);
    assert.equal(up, framesFor(p.top, p.accel), 'frames to top speed');

    const stop = framesUntil(emu, [], (e) => vx(e) === 0);
    assert.equal(stop, framesFor(p.top, p.decel), 'frames to stop');

    framesUntil(emu, 'RIGHT', (e) => vx(e) === p.top);
    const turn = framesUntil(emu, 'LEFT', (e) => vx(e) <= 0);
    assert.equal(turn, framesFor(p.top, p.reverse), 'frames to cancel speed when reversing');
  });
}

test('input moves the player on the same frame', () => {
  const emu = startRun().idle(5);
  const x0 = fineX(emu);
  emu.hold('RIGHT', 1);
  assert.ok(vx(emu) > 0, 'velocity after one frame');
  assert.ok(fineX(emu) > x0, 'position after one frame');
});

test('diagonal speed matches straight-line speed', () => {
  const emu = startRun().idle(2).hold('RIGHT+UP', 8);
  const top = PRESETS[0].top;
  assert.equal(vx(emu), -vy(emu));
  const speed = Math.hypot(vx(emu), vy(emu));
  assert.ok(Math.abs(speed - top) / top < 0.02, `diagonal speed ${speed} vs ${top}`);
});

test('facing follows the d-pad, including diagonals', () => {
  const emu = startRun().idle(2);
  const cases = [['UP', 0], ['UP+RIGHT', 1], ['RIGHT', 2], ['DOWN+RIGHT', 3], ['DOWN', 4], ['DOWN+LEFT', 5], ['LEFT', 6], ['UP+LEFT', 7]];
  for (const [b, dir] of cases) {
    emu.hold(b, 1);
    assert.equal(emu.v('player_facing'), dir, b);
  }
  emu.idle(30);
  assert.equal(emu.v('player_facing'), 7, 'facing is kept when the d-pad is released');
});

test('walls stop the player flush against them', () => {
  const emu = startRun().hold('LEFT', 120).hold('UP', 120);
  // Hitbox is 10x8 at offset (3, 8); the room's inner corner is at pixel (16, 16).
  assert.equal(px(emu), 16 - 3);
  assert.equal(py(emu), 16 - 8);
  assert.equal(vx(emu), 0);
  assert.equal(vy(emu), 0);
});

test('pushing diagonally into a wall slides along it', () => {
  const emu = startRun().hold('LEFT', 120).hold('UP', 120);
  const x0 = px(emu);
  emu.hold('UP+RIGHT', 20);
  assert.equal(py(emu), 8, 'still against the top wall');
  assert.ok(px(emu) > x0 + 10, `slid right: ${x0} -> ${px(emu)}`);
});

test('clipping a pillar corner by a pixel slides around it', () => {
  // CLASSIC moves exactly 1.5 px/frame, which makes the setup precise.
  const emu = runWithPreset(3).hold('LEFT', 120).hold('UP', 120);
  framesUntil(emu, 'DOWN', (e) => py(e) >= 17);
  emu.idle(1);
  // The hitbox's bottom row is now 1 px into the pillar's top row (pillar at x 32-47, y 32-47).
  assert.equal(py(emu), 17);
  emu.hold('RIGHT', 40);
  assert.ok(px(emu) > 30, `got past the pillar corner: x=${px(emu)}`);
  assert.ok(py(emu) <= 16, `nudged up: y=${py(emu)}`);
});

test('pause panel tunes movement values', () => {
  const emu = startRun().idle(2).tap('START').idle(8); // the pause panel takes a couple of frames to draw
  assert.equal(emu.v('paused'), 1);
  emu.tap('RIGHT');
  assert.equal(emu.v('move_top', 'u16'), 256 + 64, 'speed +0.25');
  assert.equal(emu.v('move_preset'), CUSTOM);
  emu.tap('DOWN').tap('LEFT').tap('LEFT');
  assert.equal(emu.v('move_accel_frames'), 3);
  emu.tap('DOWN').tap('RIGHT');
  assert.equal(emu.v('move_decel_frames'), 5);
  emu.tap('START');
  // The tuned values take effect.
  const up = framesUntil(emu, 'RIGHT', (e) => vx(e) === 320);
  assert.equal(up, framesFor(320, 3));
  // SELECT from CUSTOM goes back to the first preset.
  emu.idle(30).tap('SELECT');
  assert.equal(emu.v('move_preset'), 0);
  assert.equal(emu.v('move_top', 'u16'), 256);
});

test('screen: movement lab after moving', () => {
  matchGolden(startRun().play('0-20:UP+RIGHT 20-30:RIGHT', 40), 'lab-moved');
});

test('screen: tuning panel', () => {
  matchGolden(startRun().idle(2).tap('START').idle(8).tap('DOWN').tap('RIGHT').idle(2), 'tuning');
});

// --- wall contact and camera wobble (jitter reports) --------------------------

test('pushing into a wall builds no speed into it, in every direction', () => {
  // Left wall, then top wall (left column), then the ledge below the start (bottom).
  const emu = startRun().idle(2);
  for (const [setup, push, axis] of [
    [(e) => e.hold('LEFT', 80), 'LEFT', 'player_vx'],
    [(e) => e.hold('UP', 200), 'UP', 'player_vy'],
  ]) {
    setup(emu);
    for (let f = 0; f < 12; f++) {
      emu.hold(push, 1);
      assert.equal(emu.v(axis, 'i16'), 0, `${push}: speed into the wall at frame ${f}`);
    }
  }
  const ledge = startRun().idle(2);
  for (let f = 0; f < 12; f++) {
    ledge.hold('DOWN', 1);
    assert.equal(ledge.v('player_vy', 'i16'), 0, `DOWN onto the ledge at frame ${f}`);
  }
});

test('sliding along a wall below: full speed along it, and the camera does not bob', () => {
  // The start position stands on the ledge (row 7, columns 3-6); push DOWN+RIGHT along it.
  const emu = startRun().idle(2);
  const camY = [];
  for (let f = 0; f < 14; f++) {
    emu.hold('DOWN+RIGHT', 1);
    assert.equal(emu.v('player_vy', 'i16'), 0, `no speed into the ledge at frame ${f}`);
    camY.push(emu.v('cam_y', 'u16'));
  }
  assert.equal(emu.v('player_vx', 'i16'), PRESETS[0].top, 'full speed along the wall, not the diagonal component');
  assert.equal(new Set(camY).size, 1, `camera y steady: ${camY.join(' ')}`);
});

test('steady diagonal movement keeps the player at a fixed spot on screen (no wobble)', () => {
  const emu = startRun().idle(2).hold('UP', 60).hold('RIGHT', 300).hold('UP', 40);
  emu.hold('RIGHT+DOWN', 34); // camera settles into its lead
  const screen = [];
  for (let f = 0; f < 12; f++) {
    emu.hold('RIGHT+DOWN', 1);
    screen.push(`${emu.v('player_x', 'u16') - emu.v('cam_x', 'u16')},${emu.v('player_y', 'u16') - emu.v('cam_y', 'u16')}`);
  }
  assert.equal(new Set(screen).size, 1, `screen positions: ${screen.join(' ')}`);
});
