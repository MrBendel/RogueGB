// Jump (B) and mid-air dash (B again). Issues #6 and #7.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

// Defaults, must match src/player.c.
const JUMP_VEL = 448, GRAV = 32, DASH_SPEED = 768, DASH_FRAMES = 8, DASH_IFRAMES = 12;
const DIAG = 181;

const i16 = (emu, n) => emu.v(n, 'i16');
const px = (emu) => emu.v('player_x', 'u16');

/** Frames from takeoff to landing for a plain jump, same integer math as the game. */
function airFrames(vel = JUMP_VEL, grav = GRAV) {
  let z = 0, vz = vel, f = 0;
  do {
    z += vz;
    vz -= grav;
    f++;
  } while (z > 0);
  return f;
}

/** Hold buttons for one frame at a time until the player lands; returns frames. */
function framesToLand(emu, buttons = []) {
  for (let f = 1; f <= 200; f++) {
    emu.hold(buttons, 1);
    if (!emu.v('player_air')) return f;
  }
  assert.fail('never landed');
}

test('B jumps: height rises, peaks and lands after the computed air time', () => {
  const emu = startRun().idle(2).hold('B', 1);
  assert.equal(emu.v('player_air'), 1);
  assert.equal(emu.v('player_jumps', 'u16'), 1);
  let peak = 0;
  const frames = 1 + framesToLand(emu, 'B'); // holding B doesn't re-jump: jumps are on press
  for (let z = 0, vz = JUMP_VEL; z + vz > 0; vz -= GRAV) peak = Math.max(peak, (z += vz));
  assert.equal(frames, airFrames());
  assert.equal(i16(emu, 'player_z'), 0);
  assert.equal(emu.v('player_jumps', 'u16'), 1);
  assert.ok(peak >> 8 >= 12, `peak height ${peak >> 8}px`);
});

test('the jump arc is the same every time', () => {
  const trace = () => {
    const emu = startRun().idle(2).hold('B', 1);
    const zs = [];
    while (emu.v('player_air')) zs.push(i16(emu.idle(1), 'player_z'));
    return zs;
  };
  assert.deepEqual(trace(), trace());
});

test('you can steer while jumping', () => {
  const emu = startRun().idle(2);
  const x0 = px(emu);
  emu.hold('B', 1);
  framesToLand(emu, 'RIGHT');
  assert.ok(px(emu) > x0 + 20, `moved ${px(emu) - x0}px during the jump`);
});

test('the shadow shows only while airborne', () => {
  const emu = startRun().idle(2);
  const shadowY = () => emu.readBytes(0xfe00 + 2 * 4, 1)[0];
  assert.equal(shadowY(), 0, 'hidden on the ground');
  emu.hold('B', 1).idle(4);
  // Sprite positions reach OAM by DMA at the next vblank, so OAM shows the
  // height from one frame earlier.
  const z = i16(emu, 'player_z') >> 8;
  emu.idle(1);
  assert.ok(shadowY() > 0, 'visible in the air');
  // The body is drawn above its shadow by its height.
  const bodyY = emu.readBytes(0xfe00, 1)[0];
  assert.equal(shadowY() - bodyY, z);
  framesToLand(emu);
  emu.idle(1); // OAM lags a frame
  assert.equal(shadowY(), 0, 'hidden again after landing');
});

test('B in mid-air dashes toward the d-pad direction', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(5);
  const z = i16(emu, 'player_z');
  emu.hold('RIGHT+B', 1);
  assert.equal(emu.v('player_dashes', 'u16'), 1);
  assert.equal(i16(emu, 'player_vx'), DASH_SPEED);
  assert.equal(i16(emu, 'player_vy'), 0);
  assert.ok(emu.v('player_invuln') > 0, 'invulnerable while dashing');
  // Height holds during the dash.
  emu.hold('RIGHT', DASH_FRAMES - 2);
  assert.equal(i16(emu, 'player_z'), z);
  assert.equal(i16(emu, 'player_vx'), DASH_SPEED);
});

test('a dash with no d-pad goes the way you face, diagonals included', () => {
  const emu = startRun().idle(2).hold('UP+LEFT', 3).idle(10).hold('B', 1).idle(3).hold('B', 1);
  const d = Math.floor((DASH_SPEED * DIAG) / 256);
  assert.equal(i16(emu, 'player_vx'), -d);
  assert.equal(i16(emu, 'player_vy'), -d);
});

test('one dash per jump', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(3).hold('B', 1).idle(2).hold('B', 1);
  assert.equal(emu.v('player_dashes', 'u16'), 1);
});

test('a dash carries you further than a plain jump', () => {
  const plain = startRun().idle(2);
  const x0 = px(plain);
  plain.hold('B', 1);
  framesToLand(plain, 'LEFT');
  const plainDist = x0 - px(plain);

  const dash = startRun().idle(2).hold('B', 1).hold('LEFT', 4).hold('LEFT+B', 1);
  framesToLand(dash, 'LEFT');
  const dashDist = x0 - px(dash);
  assert.ok(dashDist > plainDist + 10, `dash ${dashDist}px vs plain ${plainDist}px`);
});

test('invulnerability from a dash runs out', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(3).hold('B', 1);
  assert.equal(emu.v('player_invuln'), DASH_IFRAMES - 1);
  emu.idle(DASH_IFRAMES);
  assert.equal(emu.v('player_invuln'), 0);
});

test('B pressed just before landing (after the dash) jumps again on touchdown', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(3).hold('B', 1).idle(1);
  // Run until a few frames before landing, then press B.
  while (i16(emu, 'player_z') > 3 * 256 || emu.v('player_dash_timer')) emu.idle(1);
  emu.hold('B', 1);
  assert.equal(emu.v('player_jumps', 'u16'), 1, 'still in the first jump');
  for (let f = 0; f < 6 && emu.v('player_jumps', 'u16') < 2; f++) emu.idle(1);
  assert.equal(emu.v('player_jumps', 'u16'), 2, 'buffered jump fired on landing');
});

test('jump values can be tuned from the pause panel', () => {
  const emu = startRun().idle(2).tap('START').idle(8);
  for (let i = 0; i < 4; i++) emu.tap('DOWN'); // SPEED, ACCEL, DECEL, REVERSE -> JUMP
  emu.tap('RIGHT');
  assert.equal(emu.v('jump_vel', 'u16'), JUMP_VEL + 32);
  emu.tap('DOWN').tap('LEFT');
  assert.equal(emu.v('jump_grav'), GRAV - 4);
  emu.tap('START').idle(2).hold('B', 1);
  assert.equal(1 + framesToLand(emu), airFrames(JUMP_VEL + 32, GRAV - 4));
});

test('screen: mid-jump with shadow', () => {
  matchGolden(startRun().idle(2).hold('B', 1).idle(12), 'jump-mid');
});

// --- the dash's roll animation ---------------------------------------------

const reverseBits = (b) => {
  let r = 0;
  for (let i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i;
  return r;
};
/** The player's 16x16 image as drawn (OAM tiles + flip flags applied), as rows of color indices. */
function drawnImage(emu) {
  const img = Array.from({ length: 16 }, () => new Array(16).fill(0));
  for (let c = 0; c < 2; c++) {
    const [, , tile, flags] = emu.readBytes(0xfe00 + c * 4, 4);
    const bytes = emu.readBytes(0x8000 + tile * 16, 32);
    for (let r = 0; r < 16; r++) {
      const sr = flags & 0x40 ? 15 - r : r;
      let lo = bytes[sr * 2], hi = bytes[sr * 2 + 1];
      if (flags & 0x20) [lo, hi] = [reverseBits(lo), reverseBits(hi)];
      for (let x = 0; x < 8; x++) img[r][c * 8 + x] = ((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1);
    }
  }
  return img;
}
const rot90 = (img) => img.map((_, y) => img.map((_, x) => img[15 - x][y]));
const key = (img) => img.map((r) => r.join('')).join('/');

/** Sample the drawn image each frame from a dash until landing; returns runs of identical frames. */
function rollRuns(emu) {
  const runs = [];
  for (let f = 0; f < 80; f++) {
    emu.idle(1); // OAM shows the previous frame's sprite, so this samples one frame behind
    const img = drawnImage(emu);
    if (runs.length && key(runs.at(-1).img) === key(img)) runs.at(-1).frames++;
    else runs.push({ img, frames: 1 });
    if (!emu.v('player_air')) break;
  }
  return runs;
}

test('the dash plays a roll: one full turn in 90-degree steps, ending on landing', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(3).hold('RIGHT+B', 1);
  const len = emu.v('player_roll_len');
  const runs = rollRuns(emu);
  const turns = runs.slice(0, 4);
  assert.equal(new Set(turns.map((r) => key(r.img))).size, 4, 'four distinct roll frames');
  for (let i = 1; i < 4; i++) assert.equal(key(turns[i].img), key(rot90(turns[i - 1].img)), `frame ${i} is the previous one turned 90 degrees clockwise`);
  // The roll lasts the dash plus the fall, so each quarter turn is held for several frames.
  assert.ok(len > DASH_FRAMES + 8, `roll lasts ${len} frames`);
  for (const r of turns) assert.ok(r.frames >= Math.floor(len / 4) - 1, `quarter turn held ${r.frames} frames`);
  assert.equal(emu.v('player_roll_len'), 0, 'roll over once landed');
});

test('rolling left turns the other way', () => {
  const emu = startRun().idle(2).hold('B', 1).idle(3).hold('LEFT+B', 1);
  const turns = rollRuns(emu).slice(0, 4).map((r) => r.img);
  // Counter-clockwise: each frame turned 90 degrees clockwise gives the previous one.
  for (let i = 1; i < 4; i++) assert.equal(key(rot90(turns[i])), key(turns[i - 1]));
});

test('screen: mid-roll', () => {
  matchGolden(startRun().idle(2).hold('B', 1).idle(3).hold('RIGHT+B', 1).idle(8), 'roll-mid');
});
