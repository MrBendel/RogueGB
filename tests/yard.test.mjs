// The ice yard (room_yard.c): a hand-laid map in six material palettes, with a
// shack roof you walk behind (the Color's background priority flag).
import { test } from 'node:test';
import { assert, matchGolden, bootToTitle, settle, PLAY } from './helpers.mjs';

const MODE_YARD = 5;
const yard = () => settle(bootToTitle().tap('B'), PLAY, 'the ice yard');
const w16 = (emu, n, v) => emu.writeBytes(emu.symbols.addr(n), [v & 255, v >> 8]);

/** Red pixels (the player's outfit) inside the player's on-screen box. */
function redAtPlayer(emu) {
  const fb = emu.frameBuffer();
  const sx = emu.v('player_x', 'u16') - emu.v('cam_x', 'u16'), sy = emu.v('player_y', 'u16') - emu.v('cam_y', 'u16');
  let red = 0;
  for (let y = Math.max(0, sy); y < Math.min(128, sy + 16); y++) {
    for (let x = Math.max(0, sx); x < Math.min(160, sx + 16); x++) {
      const k = (y * 160 + x) * 3;
      if (fb[k] > 180 && fb[k + 1] < 90 && fb[k + 2] < 90) red++;
    }
  }
  return red;
}
function place(emu, bx, by) {
  w16(emu, 'player_x', bx * 16);
  w16(emu, 'player_y', by * 16);
  return emu.idle(60);  // the camera catches up
}

test('B on the title opens the ice yard', () => {
  const emu = yard();
  assert.equal(emu.v('play_mode'), MODE_YARD);
  matchGolden(emu.idle(4), 'yard-start');
});

test('the shack roof is drawn over the player: you walk behind it', () => {
  const emu = yard();
  place(emu, 10, 4);  // on open ice
  assert.ok(redAtPlayer(emu) > 10, 'visible on the ice');
  place(emu, 15, 3);  // behind the roof
  assert.equal(redAtPlayer(emu), 0, 'hidden behind the roof');
  matchGolden(emu, 'yard-behind-roof');
});

test('the catwalks are raised: from the ice you jump up onto them', () => {
  const emu = yard();
  place(emu, 5, 5);
  emu.hold('DOWN', 30);
  assert.ok(emu.v('player_y', 'u16') < 7 * 16, 'stopped by the catwalk edge');
  emu.hold('B', 1).hold('DOWN', 20).idle(30);
  assert.ok(emu.v('player_y', 'u16') >= 7 * 16 - 12 && emu.v('player_z', 'i16') > 0, 'up on the catwalk');
  matchGolden(emu, 'yard-catwalk');
});
