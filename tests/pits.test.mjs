// Pits and raised blocks. Issue #8 (and the dash-over-a-gap check from #7).
// The features live in the lab room's other screens; see the layout in src/room.c.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const RAISED_Z = 8 * 256;
const FALL_FRAMES = 32;

const x = (emu) => emu.v('player_x', 'u16');
const y = (emu) => emu.v('player_y', 'u16');
const z = (emu) => emu.v('player_z', 'i16');

/** Hold buttons one frame at a time until pred holds; returns frames taken. */
function until(emu, buttons, pred, max = 400, what = 'condition') {
  for (let f = 1; f <= max; f++) {
    emu.hold(buttons, 1);
    if (pred(emu)) return f;
  }
  assert.fail(`timed out waiting for ${what}`);
}

/** Bottom-left screen, standing on the floor just left of the raised platform (columns 3-4, rows 9-11). */
function besidePlatform() {
  const emu = startRun().idle(2).hold('LEFT', 80);
  until(emu, 'DOWN', (e) => y(e) >= 158, 400, 'row 10');
  return emu.idle(10).hold('RIGHT', 40).idle(10); // up against the platform's side
}

/** Top-right screen, on the floor right of the 2-block pit (columns 16-17), facing it. */
function rightOfWidePit() {
  const emu = startRun().idle(2).hold('LEFT', 80).hold('UP', 200).hold('RIGHT', 320);
  until(emu, 'DOWN', (e) => y(e) >= 64, 200, 'row 4');
  return emu.idle(10);
}

test('raised blocks are walls from the floor', () => {
  const emu = besidePlatform().hold('RIGHT', 30);
  // Hitbox right edge (x + 12) stops at the platform's left edge (x = 48).
  assert.equal(x(emu), 48 - 13);
  assert.equal(z(emu), 0);
});

test('jumping onto a raised block lands on top of it', () => {
  const emu = besidePlatform().hold('B', 1);
  until(emu, 'RIGHT', (e) => !e.v('player_air'), 60, 'landing');
  assert.equal(z(emu), RAISED_Z, 'standing at the platform height');
  assert.ok(x(emu) > 40, `moved onto the platform: x=${x(emu)}`);
  // And walking along it stays on top.
  emu.hold('UP', 6).idle(6);
  assert.equal(z(emu), RAISED_Z);
  assert.equal(emu.v('player_air'), 0);
});

test('walking off a raised block drops you to the floor', () => {
  const emu = besidePlatform().hold('B', 1);
  until(emu, 'RIGHT', (e) => !e.v('player_air'), 60, 'landing');
  until(emu, 'RIGHT', (e) => e.v('player_air') === 1, 60, 'walking off the edge');
  until(emu, [], (e) => !e.v('player_air'), 60, 'landing on the floor');
  assert.equal(z(emu), 0);
  assert.equal(emu.v('player_falls', 'u16'), 0, 'landed beside the pit, not in it');
});

/** From the platform, drop to the floor between the platform and the 1-block pit (column 6). */
function besideNarrowPit() {
  const emu = besidePlatform().hold('B', 1);
  until(emu, 'RIGHT', (e) => !e.v('player_air'), 60, 'landing');
  until(emu, 'RIGHT', (e) => e.v('player_air') === 1, 60, 'walking off the edge');
  until(emu, [], (e) => !e.v('player_air'), 60, 'landing on the floor');
  return emu.idle(10);
}

test('walking into a pit falls in, then reappears on safe ground, blinking', () => {
  const emu = besideNarrowPit();
  until(emu, 'RIGHT', (e) => e.v('player_fall_timer') > 0, 60, 'falling in');
  assert.equal(emu.v('player_falls', 'u16'), 1);
  const hp = emu.v('player_hp');
  // No control while falling.
  const x0 = x(emu);
  emu.hold('LEFT', 10);
  assert.equal(x(emu), x0);
  until(emu, [], (e) => e.v('player_fall_timer') === 0, FALL_FRAMES + 2, 'the fall to finish');
  assert.ok(e2pit(emu) === false, 'reappeared off the pit');
  assert.ok(emu.v('player_blink') > 0, 'blinking');
  assert.equal(emu.v('player_hp'), hp - 1, 'a fall costs 1 HP');
  emu.idle(70);
  assert.equal(emu.v('player_blink'), 0);
  assert.equal(emu.v('player_falls', 'u16'), 1, 'no second fall');
});

/** Is the middle of the player's feet over the pit column (x 96-111)? */
const e2pit = (emu) => x(emu) + 8 >= 96 && x(emu) + 8 < 112;

test('a 1-block pit can be jumped', () => {
  const emu = besideNarrowPit();
  until(emu, 'RIGHT', (e) => x(e) >= 84, 60, 'the pit edge');
  emu.hold('RIGHT+B', 1);
  until(emu, 'RIGHT', (e) => !e.v('player_air'), 60, 'landing');
  assert.equal(emu.v('player_falls', 'u16'), 0);
  assert.ok(x(emu) + 8 >= 112, `landed past the pit: feet at ${x(emu) + 8}`);
});

test('a plain jump falls short of the 2-block pit', () => {
  const emu = rightOfWidePit();
  until(emu, 'LEFT', (e) => x(e) <= 282, 60, 'the pit edge');
  emu.hold('LEFT+B', 1);
  until(emu, 'LEFT', (e) => !e.v('player_air'), 60, 'landing');
  emu.idle(1);
  assert.equal(emu.v('player_falls', 'u16'), 1);
});

test('jump plus dash clears the 2-block pit', () => {
  const emu = rightOfWidePit();
  until(emu, 'LEFT', (e) => x(e) <= 282, 60, 'the pit edge');
  emu.hold('LEFT+B', 1).hold('LEFT', 5).hold('LEFT+B', 1);
  assert.equal(emu.v('player_dashes', 'u16'), 1);
  until(emu, 'LEFT', (e) => !e.v('player_air'), 80, 'landing');
  emu.idle(2);
  assert.equal(emu.v('player_falls', 'u16'), 0);
  assert.ok(x(emu) + 8 < 256, `landed past the pit: feet at ${x(emu) + 8}`);
});

test('pits only matter on the ground: hovering over one in a jump is fine', () => {
  const emu = besideNarrowPit();
  until(emu, 'RIGHT', (e) => x(e) >= 84, 60, 'the pit edge');
  emu.hold('RIGHT+B', 1).hold('RIGHT', 8);
  assert.ok(e2pit(emu), 'over the pit mid-jump');
  assert.equal(emu.v('player_fall_timer'), 0);
});

test('screen: standing on a raised block', () => {
  const emu = besidePlatform().hold('B', 1);
  until(emu, 'RIGHT', (e) => !e.v('player_air'), 60, 'landing');
  matchGolden(emu.idle(30), 'on-platform');
});

test('screen: falling into a pit', () => {
  const emu = besideNarrowPit();
  until(emu, 'RIGHT', (e) => e.v('player_fall_timer') > 0, 60, 'falling in');
  matchGolden(emu.idle(10), 'pit-fall');
});
