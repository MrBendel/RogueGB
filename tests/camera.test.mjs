// Scrolling playfield: camera follow, lead, clamping, background streaming. Issue #19.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const VIEW_W = 160, VIEW_H = 128;
const ROOM_PX_W = 20 * 16, ROOM_PX_H = 16 * 16; // the lab room is 2x2 screens
const TILE_FLOOR = 80, TILE_WALL = 81; // SCENE_TILES + 0/1 in src/room.c

const cam = (emu) => ({ x: emu.v('cam_x', 'u16'), y: emu.v('cam_y', 'u16') });
const pos = (emu) => ({ x: emu.v('player_x', 'u16'), y: emu.v('player_y', 'u16') });

/** Every visible background tile matches the room map. Returns the number checked. */
function assertViewMatchesRoom(emu, label) {
  const { x, y } = cam(emu);
  const map = emu.symbols.addr('room_map');
  let n = 0;
  for (let ty = y >> 3; ty <= (y + VIEW_H - 1) >> 3; ty++) {
    for (let tx = x >> 3; tx <= (x + VIEW_W - 1) >> 3; tx++) {
      const solid = emu.read8(map + (ty >> 1) * 32 + (tx >> 1)) === 1;
      const tile = emu.read8(0x9800 + (ty & 31) * 32 + (tx & 31));
      assert.equal(tile, solid ? TILE_WALL : TILE_FLOOR, `${label}: tile (${tx},${ty})`);
      n++;
    }
  }
  return n;
}

/** Walk to the bottom-right corner along clear corridors: left column, then bottom row. */
function walkToBottomRight(emu, check) {
  for (const [dir, frames] of [['LEFT', 80], ['DOWN', 200], ['RIGHT', 320]]) {
    for (let f = 0; f < frames; f++) {
      emu.hold(dir, 1);
      if (check && f % 7 === 0) check(emu, `${dir} frame ${f}`);
    }
  }
  return emu;
}

test('the camera starts centered on the player, clamped to the room', () => {
  const emu = startRun().idle(1);
  const p = pos(emu);
  assert.deepEqual(cam(emu), {
    x: Math.max(0, p.x + 8 - VIEW_W / 2),
    y: Math.min(ROOM_PX_H - VIEW_H, Math.max(0, p.y + 8 - VIEW_H / 2)),
  });
});

test('the scroll registers follow the camera in the next vblank', () => {
  const emu = startRun().idle(2);
  for (let f = 0; f < 60; f++) {
    const c = cam(emu);
    emu.hold('DOWN+LEFT', 1);
    // SCY = 0xFF42, SCX = 0xFF43; written by the vblank handler from the previous frame's camera.
    assert.equal(emu.read8(0xff43), c.x & 0xff, `SCX at frame ${f}`);
    assert.equal(emu.read8(0xff42), c.y & 0xff, `SCY at frame ${f}`);
  }
});

test('the camera stops at the room edges', () => {
  const emu = walkToBottomRight(startRun().idle(2)).idle(90);
  assert.deepEqual(cam(emu), { x: ROOM_PX_W - VIEW_W, y: ROOM_PX_H - VIEW_H });
  emu.hold('LEFT', 80).hold('UP', 300).idle(90);
  assert.equal(cam(emu).y, 0, 'top edge');
});

test('while moving, the camera leads in the direction of travel', () => {
  const emu = startRun().idle(2).hold('LEFT', 80).hold('DOWN', 200);
  emu.hold('RIGHT', 120); // along the bottom row, away from the side edges
  const p = pos(emu), c = cam(emu);
  const centered = p.x + 8 - VIEW_W / 2;
  assert.ok(c.x > centered, `camera ${c.x} is ahead of the centered position ${centered}`);
});

test('background tiles streamed in while scrolling always match the room', () => {
  let checked = 0;
  walkToBottomRight(startRun().idle(2), (emu, label) => (checked += assertViewMatchesRoom(emu, label)));
  assert.ok(checked > 10000, `checked ${checked} tiles`);
});

test('scrolling keeps one game loop per frame', () => {
  const emu = startRun().idle(2);
  const f0 = emu.v('frame_count', 'u16');
  walkToBottomRight(emu);
  assert.equal(emu.v('frame_count', 'u16') - f0, 80 + 200 + 320);
});

test('screen: scrolled to the bottom-right corner', () => {
  matchGolden(walkToBottomRight(startRun().idle(2)).idle(90), 'scrolled-corner');
});
