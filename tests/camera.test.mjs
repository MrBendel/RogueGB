// Scrolling playfield: camera follow, lead, clamping, background streaming. Issue #19.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const VIEW_W = 160, VIEW_H = 128;
const ROOM_PX_W = 20 * 16, ROOM_PX_H = 24 * 16; // the lab: 2x2 screens plus the arena below
// Must match src/room.c: floor, wall, then 4-tile raised blocks and pits.
const TILE_FLOOR = 80, TILE_WALL = 81, TILE_RAISED = 82, TILE_PIT = 86;
const BLK_SOLID = 1, BLK_PIT = 2, BLK_RAISED = 3;

const cam = (emu) => ({ x: emu.v('cam_x', 'u16'), y: emu.v('cam_y', 'u16') });
const pos = (emu) => ({ x: emu.v('player_x', 'u16'), y: emu.v('player_y', 'u16') });

/** Every visible background tile matches the room map. Returns the number checked. */
function assertViewMatchesRoom(emu, label) {
  const { x, y } = cam(emu);
  const map = emu.symbols.addr('room_map');
  let n = 0;
  for (let ty = y >> 3; ty <= (y + VIEW_H - 1) >> 3; ty++) {
    for (let tx = x >> 3; tx <= (x + VIEW_W - 1) >> 3; tx++) {
      const block = emu.read8(map + (ty >> 1) * 32 + (tx >> 1));
      const quad = ((ty & 1) << 1) | (tx & 1);
      const want = block === BLK_SOLID ? TILE_WALL : block === BLK_RAISED ? TILE_RAISED + quad : block === BLK_PIT ? TILE_PIT + quad : TILE_FLOOR;
      const tile = emu.read8(0x9800 + (ty & 31) * 32 + (tx & 31));
      assert.equal(tile, want, `${label}: tile (${tx},${ty})`);
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
      if (check) check(emu, `${dir} frame ${f}`);
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
  // Right edge; vertically the lab's bottom-right corner isn't the room's bottom any more.
  const p = pos(emu);
  assert.deepEqual(cam(emu), { x: ROOM_PX_W - VIEW_W, y: Math.min(ROOM_PX_H - VIEW_H, p.y + 8 - VIEW_H / 2) });
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
  walkToBottomRight(startRun().idle(2), (emu, label) => { if (emu.frame % 7 === 0) checked += assertViewMatchesRoom(emu, label); });
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

test('CPU budget: no frame of walking, scrolling and jumping uses more than half the frame', () => {
  // lastFrameCycles = cycles the update ran past the end of vblank (a frame is 70224).
  // Half a frame keeps room for enemies and bullets (M1).
  const BUDGET = 35112;
  const emu = startRun().idle(2);
  let worst = 0;
  const track = () => (worst = Math.max(worst, emu.lastFrameCycles));
  walkToBottomRight(emu, track);
  for (let f = 0; f < 200; f++) {
    emu.hold(f % 40 === 0 ? 'UP+LEFT+B' : 'UP+LEFT', 1);
    track();
  }
  assert.ok(worst < BUDGET, `worst frame used ${worst} cycles past vblank (budget ${BUDGET})`);
});

test('queued VRAM writes and the scroll update finish inside vblank', () => {
  // The vblank handler sets the scroll registers, then writes the queued rows,
  // columns and sprite frames (vram_queue.s). If that ran past vblank, the screen
  // would tear and the hardware would ignore some VRAM writes. Stop at every run
  // of the writer while scrolling diagonally (rows, columns and animation frames
  // all queue up) and check every instruction executes on a vblank line (144-153).
  const emu = startRun().idle(2).hold('LEFT', 80).hold('UP', 200);
  const entry = emu.symbols.addr('vram_queue_write');
  const end = entry + 0x100; // generous: the routine is well under 256 bytes
  const pc = () => {
    const r = emu.gb.registers();
    const p = r.pc;
    r.free();
    return p;
  };
  emu.setButtons(0x01 | 0x08); // RIGHT+DOWN: scroll on both axes
  let runs = 0, worstLy = 0;
  for (; runs < 40; runs++) {
    emu.gb.step_to(entry);
    for (let p = pc(); p >= entry && p < end; p = pc()) {
      const ly = emu.gb.ppu_ly();
      assert.ok(ly >= 144 && ly <= 153, `VRAM queue write on visible line ${ly}`);
      worstLy = Math.max(worstLy, ly);
      emu.gb.clock();
    }
  }
  assert.equal(runs, 40);
  assert.ok(worstLy >= 144, 'the writer was observed running');
});

test('no tearing: the scroll handler always runs inside vblank across a play session', () => {
  // A tear means the scroll registers changed while the screen was being drawn.
  // They are only written by the vblank handler, so check the scanline it starts
  // on, every frame, through walking, dash-jumps, pausing, scrolling and a fight.
  const emu = startRun();
  const handler = emu.symbols.addr('Fgfx$vbl_scroll$0$0');
  const m = { RIGHT: 1, LEFT: 2, UP: 4, DOWN: 8, B: 32, START: 128 };
  const mask = (s) => s.split('+').filter(Boolean).reduce((a, d) => a | m[d], 0);
  const script = [
    ['RIGHT', 30], ['RIGHT+B', 1], ['RIGHT', 4], ['RIGHT+B', 1], ['RIGHT', 30],
    ['START', 1], ['', 20], ['START', 1], ['', 10],
    ['LEFT', 80], ['DOWN', 200], ['RIGHT', 60], ['DOWN', 70], // into the arena
    ['LEFT', 40], ['LEFT+B', 1], ['RIGHT', 60], ['UP+RIGHT', 40], ['DOWN+LEFT', 60], ['', 60],
  ];
  let frames = 0;
  for (const [b, n] of script) {
    for (let i = 0; i < n; i++, frames++) {
      emu.setButtons(mask(b));
      emu.gb.step_to(handler);
      const ly = emu.gb.ppu_ly();
      assert.ok(ly >= 144 && ly <= 153, `frame ${frames} (${b || 'idle'}): scroll handler ran on visible line ${ly}`);
      emu.run(1);
    }
  }
});
