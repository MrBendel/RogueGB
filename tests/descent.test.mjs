// Generated layers (level.c) and the descent (descent.c), M2.
import { test } from 'node:test';
import { assert, matchGolden, startRun, startDescent, bootToTitle } from './helpers.mjs';

const W = 4, CELLS = 16;
const START = 1, COMBAT = 2, EXIT = 5;
const STEP = { 1: -W, 2: 1, 4: W, 8: -1 };
const OPP = { 1: 4, 2: 8, 4: 1, 8: 2 };
const BLK_FLOOR = 0;

const addr = (emu, n) => emu.symbols.addr(n);
const arr = (emu, n, len = CELLS) => Array.from(emu.readBytes(addr(emu, n), len));
const w8 = (emu, n, v) => emu.write8(addr(emu, n), v);
const w16 = (emu, n, v) => emu.writeBytes(addr(emu, n), [v & 255, v >> 8]);

/** Generate a layer from seed and build room i (debug command 5). */
function generate(emu, seed, room = 0) {
  w16(emu, 'debug_x', seed);
  w8(emu, 'debug_type', room);
  w8(emu, 'debug_cmd', 5);
  emu.idle(1);
  return {
    type: arr(emu, 'layer_type'), doors: arr(emu, 'layer_doors'), cw: arr(emu, 'layer_cw'), ch: arr(emu, 'layer_ch'),
    start: emu.v('layer_start'), exit: emu.v('layer_exit'), rooms: emu.v('layer_rooms'),
  };
}
/** The room map: its size and a block lookup. */
function roomMap(emu) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const raw = emu.readBytes(0xd000, 1024);
  return { w, h, at: (x, y) => raw[(y << 5) | x] };
}

test('every layer: 8-12 rooms, start on top, exit at the bottom, all rooms connected', () => {
  const emu = startRun().idle(2);
  for (let n = 1; n <= 300; n++) {
    const seed = (n * 7919) & 0xffff;
    const L = generate(emu, seed);
    const rooms = L.type.map((t, i) => (t ? i : -1)).filter((i) => i >= 0);
    assert.ok(rooms.length >= 8 && rooms.length <= 12, `seed ${seed}: ${rooms.length} rooms`);
    assert.equal(rooms.length, L.rooms);
    assert.equal(L.type[L.start], START);
    assert.equal(L.type[L.exit], EXIT);
    assert.ok(L.start < W, 'start in the top row');
    assert.ok(L.exit >= CELLS - W, 'exit in the bottom row');
    for (const i of rooms) {
      for (const d of [1, 2, 4, 8]) {
        if (!(L.doors[i] & d)) continue;
        const j = i + STEP[d];
        assert.ok(j >= 0 && j < CELLS && L.type[j], `seed ${seed}: room ${i} door ${d} leads nowhere`);
        assert.ok(L.doors[j] & OPP[d], `seed ${seed}: room ${j} lacks the door back`);
      }
    }
    const seen = new Set([L.start]), queue = [L.start];
    while (queue.length) {
      const i = queue.shift();
      for (const d of [1, 2, 4, 8]) {
        if (L.doors[i] & d && !seen.has(i + STEP[d])) {
          seen.add(i + STEP[d]);
          queue.push(i + STEP[d]);
        }
      }
    }
    assert.equal(seen.size, rooms.length, `seed ${seed}: unreachable rooms`);
  }
});

test('every room: its size in chunks, a wall ring, and doors that reach each other', () => {
  const emu = startRun().idle(2);
  let carved = 0, built = 0;
  for (let n = 1; n <= 40; n++) {
    const seed = (n * 104729) & 0xffff;
    const L = generate(emu, seed);
    for (let i = 0; i < CELLS; i++) {
      if (!L.type[i]) continue;
      generate(emu, seed, i);
      built++;
      carved += emu.v('level_carved');
      const m = roomMap(emu);
      assert.equal(m.w, L.cw[i] * 5);
      assert.equal(m.h, L.ch[i] * 4);
      const mx = (m.w >> 1) - 1, my = (m.h >> 1) - 1;
      const inner = { 1: [mx, 2], 2: [m.w - 3, my], 4: [mx, m.h - 3], 8: [2, my] };
      const doors = [1, 2, 4, 8].filter((d) => L.doors[i] & d);
      for (let y = 0; y < m.h; y++) {
        for (let x = 0; x < m.w; x++) {
          if (!(x === 0 || y === 0 || x === m.w - 1 || y === m.h - 1)) continue;
          const side = y === 0 ? 1 : y === m.h - 1 ? 4 : x === 0 ? 8 : 2;
          const inGap = (side === 1 || side === 4) ? (x === mx || x === mx + 1) : (y === my || y === my + 1);
          if (!(inGap && L.doors[i] & side)) assert.notEqual(m.at(x, y), BLK_FLOOR, `seed ${seed} room ${i}: hole in the wall at ${x},${y}`);
        }
      }
      if (doors.length < 2) continue;
      const [sx, sy] = inner[doors[0]];
      const seen = new Set([`${sx},${sy}`]), q = [[sx, sy]];
      while (q.length) {
        const [x, y] = q.pop();
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const nx = x + dx, ny = y + dy, k = `${nx},${ny}`;
          if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h || seen.has(k) || m.at(nx, ny) !== BLK_FLOOR) continue;
          seen.add(k);
          q.push([nx, ny]);
        }
      }
      for (const d of doors.slice(1)) assert.ok(seen.has(inner[d].join(',')), `seed ${seed} room ${i}: door ${d} cut off`);
    }
  }
  assert.ok(carved < built / 2, `paths had to be carved in ${carved} of ${built} rooms`);
});

test('the same seed gives the same layer and rooms', () => {
  const emu = startRun().idle(2);
  const rooms = (seed) => {
    const out = [generate(emu, seed)];
    for (let i = 0; i < CELLS; i++) {
      if (!out[0].type[i]) continue;
      generate(emu, seed, i);
      out.push(Array.from(emu.readBytes(0xd000, 1024)));
    }
    return out;
  };
  const a = rooms(4242);
  rooms(999);
  assert.deepEqual(rooms(4242), a);
});

// --- Playing a descent ---

/** Walk out of the current room through door d (from just inside it). */
function leaveBy(emu, d) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const mx = ((w >> 1) - 1) * 16, my = ((h >> 1) - 1) * 16;
  const at = { 1: [mx + 8, 2 * 16 - 8], 4: [mx + 8, h * 16 - 3 * 16 + 4], 8: [2 * 16, my + 4], 2: [w * 16 - 3 * 16, my + 4] }[d];
  w16(emu, 'player_x', at[0]);
  w16(emu, 'player_y', at[1]);
  const dir = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[d];
  const before = emu.v('rooms_entered', 'u16');
  for (let f = 0; f < 120 && emu.v('rooms_entered', 'u16') === before; f++) emu.hold(dir, 1);
  return emu.idle(30);
}

test('START on the title begins a descent in the start room of a generated layer', () => {
  const emu = startDescent();
  assert.equal(emu.v('play_mode'), 1);
  assert.equal(emu.v('level_room'), emu.v('layer_start'));
  assert.equal(emu.v('actors_active'), 0, 'the start room is quiet');
  matchGolden(emu.idle(4), 'descent-start');
});

test('walking through a door moves to the next room, arriving at its matching door', () => {
  const emu = startDescent();
  const start = emu.v('level_room');
  const doors = emu.read8(addr(emu, 'layer_doors') + start);
  const d = [1, 2, 4, 8].find((x) => doors & x);
  leaveBy(emu, d);
  assert.equal(emu.v('level_room'), start + STEP[d], 'in the neighbouring room');
  assert.equal(emu.v('rooms_entered', 'u16'), 2);
  leaveBy(emu, OPP[d]);
  assert.equal(emu.v('level_room'), start, 'and back');
});

test('a combat room spawns enemies until cleared, and stays cleared', () => {
  // Try a few runs until the start room has a combat room beside it.
  for (let wait = 0; wait < 40; wait += 7) {
    const emu = startDescent(bootToTitle().idle(wait));
    const start = emu.v('level_room');
    const types = arr(emu, 'layer_type'), doors = arr(emu, 'layer_doors');
    const d = [1, 2, 4, 8].find((x) => doors[start] & x && types[start + STEP[x]] === COMBAT);
    if (!d) continue;
    leaveBy(emu, d);
    assert.ok(emu.v('actors_active') > 0, 'enemies');
    matchGolden(emu, 'descent-combat');
    assert.equal(emu.v('room_live'), 1);
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    w8(emu, 'actor_count', 0);
    emu.idle(3);
    assert.equal(emu.read8(addr(emu, 'layer_cleared') + emu.v('level_room')), 1);
    leaveBy(emu, OPP[d]);
    leaveBy(emu, d);
    assert.equal(emu.v('actors_active'), 0, 'no respawn');
    return;
  }
  assert.fail('no start room with a combat room beside it');
});
