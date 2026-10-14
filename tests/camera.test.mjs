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

/**
 * Every background tile visible at camera position `view` matches the room map.
 * Returns the number checked. Pass the camera from the *previous* frame: a frame's
 * update queues its new rows/columns and scroll, and the vblank after it applies
 * both, so that's the view the hardware is showing (and the tiles it has).
 */
function assertViewMatchesRoom(emu, label, view = cam(emu)) {
  const { x, y } = view;
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
  let shown = null;
  walkToBottomRight(startRun().idle(2), (emu, label) => {
    // The scroll registers hold the previous frame's camera: that's what's on screen.
    if (shown) {
      assert.deepEqual({ x: emu.read8(0xff43), y: emu.read8(0xff42) }, { x: shown.x & 0xff, y: shown.y & 0xff });
      checked += assertViewMatchesRoom(emu, label, shown);
    }
    shown = cam(emu);
  });
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

test('CPU budget: no frame of walking, scrolling and jumping uses more than about a quarter of the frame', () => {
  // lastFrameCycles = cycles the update ran past the end of vblank (a frame is
  // 140,448 at the Color's double speed). About a quarter (with the HUD and sound effects (~1.5k a frame, a few hundred more per sound started))
  // keeps room for everything else.
  const BUDGET = 38000;
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
  const handler = emu.symbols.addr('gfx_vbl');
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

// --- Screen shake (camera_setup.c) ---
const SHAKE_BIG = 14;
const s8 = (v) => (v > 127 ? v - 256 : v);
/** Hurt the player for dmg (debug command 4, play.c) from just left of them. */
function hurtPlayer(emu, dmg) {
  const a = (n) => emu.symbols.addr(n);
  emu.write8(a('debug_type'), dmg);
  const x = emu.v('player_x', 'u16') - 8, y = emu.v('player_y', 'u16') + 12;
  emu.writeBytes(a('debug_x'), [x & 255, x >> 8]);
  emu.writeBytes(a('debug_y'), [y & 255, y >> 8]);
  emu.write8(a('debug_cmd'), 4);
  return emu.idle(1);
}

test('getting hurt shakes the screen, a few px, then it settles', () => {
  const emu = startRun().idle(60);
  hurtPlayer(emu, 5);
  assert.ok(emu.v('cam_shake') > 0, 'shaking');
  const offs = [];
  for (let f = 0; f < SHAKE_BIG + 4; f++) {
    emu.idle(1);
    offs.push([s8(emu.v('shake_dx')), s8(emu.v('shake_dy'))]);
  }
  const max = emu.v('cam_shake_max');
  assert.ok(offs.some(([x, y]) => x || y), 'it moved');
  assert.ok(offs.every(([x, y]) => Math.abs(x) <= max && Math.abs(y) <= max), `within ${max} px: ${JSON.stringify(offs)}`);
  assert.deepEqual(offs.at(-1), [0, 0], 'and settled');
  // The scroll follows the shaken view (sprites draw from cam_x too).
  assert.equal(emu.v('scroll_x'), emu.v('cam_x', 'u16') & 255);
});

test('the shake never shows outside the room, and SHAKE 0 turns it off', () => {
  const emu = startRun().idle(60);  // the start is near the room's top-left corner
  for (let n = 0; n < 3; n++) {
    hurtPlayer(emu, 1);
    for (let f = 0; f < SHAKE_BIG; f++) {
      emu.idle(1);
      assert.ok(s8(emu.v('cam_x', 'u16') & 255) >= 0 && emu.v('cam_y', 'u16') < 0x8000, 'not past the top-left');
    }
    emu.idle(70);  // the blink's invulnerability
  }
  emu.write8(emu.symbols.addr('cam_shake_max'), 0);
  hurtPlayer(emu, 1);
  for (let f = 0; f < SHAKE_BIG; f++) {
    emu.idle(1);
    assert.equal(emu.v('shake_dx'), 0);
    assert.equal(emu.v('shake_dy'), 0);
  }
});

// --- Per-tile attributes streamed with the map (room.c, vram_queue.s) ---
test('scrolling streams each tile\'s attributes (palette) along with it, from the visual blocks', () => {
  const emu = startRun().idle(30);
  const a = (n) => emu.symbols.addr(n);
  // Floor (visual block 0) in palette 5, everything else as loaded: attributes are no longer uniform.
  for (let q = 0; q < 4; q++) emu.write8(a('room_attr_lut') + q, 5);
  emu.write8(a('room_attr_uniform'), 0);
  // What's drawn already keeps its attributes; only cells streamed in from now on
  // (outside the drawn area: 23x19 tiles from drawn_tx/ty) get the new ones.
  const tx0 = emu.v('drawn_tx', 'i8'), ty0 = emu.v('drawn_ty', 'i8');  // -1: a tile left of the room
  const streamed = (tx, ty) => tx >= tx0 + 23 || ty >= ty0 + 19;
  // Walk right and down across the lab so new columns and rows stream in.
  emu.hold('RIGHT', 120).hold('DOWN', 90).idle(10);
  const vram = emu.snapshot().vram;
  const camx = emu.v('cam_x', 'u16'), camy = emu.v('cam_y', 'u16');
  const map = emu.readBytes(0xd400, 1024);
  let checked = 0;
  // The view's cells: tile from the visual block, attribute from the attribute LUT.
  for (let ty = (camy >> 3) + 1; ty < (camy >> 3) + 16; ty++) {
    for (let tx = (camx >> 3) + 1; tx < (camx >> 3) + 19; tx++) {
      const blk = map[((ty >> 1) << 5) | (tx >> 1)];
      const cell = ((ty & 31) << 5) | (tx & 31);
      const attr = vram[0x2000 + 0x1800 + cell];
      if (blk === 0 && streamed(tx, ty)) {
        assert.equal(attr & 7, 5, `floor at ${tx},${ty}: palette 5`);
        checked++;
      }
    }
  }
  assert.ok(checked > 30, `checked ${checked} streamed floor cells`);
});
