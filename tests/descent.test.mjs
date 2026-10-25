// Generated areas (level.c) and the descent (descent.c), M2: open maps linked at
// their edges, camps that wake as you approach, and a shaft down to the next area.
import { test } from 'node:test';
import { assert, matchGolden, startRun, startDescent, bootToTitle } from './helpers.mjs';

const W = 4, CELLS = 12;
const MAP_START = 1, MAP_FIELD = 2, MAP_EXIT = 3;
const STEP = { 1: -W, 2: 1, 4: W, 8: -1 };
const OPP = { 1: 4, 2: 8, 4: 1, 8: 2 };
const SIDE = { 1: 0, 2: 1, 4: 2, 8: 3 };  // edge bit -> index into level_open
const OPENING_W = 4;
const BLK_FLOOR = 0;
const CAMP_ASLEEP = 0, CAMP_AWAKE = 1, CAMP_CLEARED = 2;

const addr = (emu, n) => emu.symbols.addr(n);
const bytes = (emu, n, len = CELLS) => Array.from(emu.readBytes(addr(emu, n), len));
const words = (emu, n, len) => Array.from({ length: len }, (_, i) => emu.read16(addr(emu, n) + i * 2));
const w8 = (emu, n, v) => emu.write8(addr(emu, n), v);
const w16 = (emu, n, v) => emu.writeBytes(addr(emu, n), [v & 255, v >> 8]);

/** Generate an area from seed and build map i (debug command 5); the area's layout. */
function generate(emu, seed, map = 0) {
  w16(emu, 'debug_x', seed);
  w8(emu, 'debug_type', map);
  w8(emu, 'debug_cmd', 5);
  emu.idle(1);
  return {
    type: bytes(emu, 'area_type'), links: bytes(emu, 'area_links'), cw: bytes(emu, 'area_cw'), ch: bytes(emu, 'area_ch'),
    start: emu.v('area_start'), exit: emu.v('area_exit'), maps: emu.v('area_maps'),
  };
}
/** The built map: size, block lookup, openings, camps, chests (block coordinates). */
function built(emu) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const raw = emu.readBytes(0xd400, 1024);
  const camps = emu.v('level_camps'), chests = emu.v('level_chests');
  const cx = words(emu, 'camp_x', 2), cy = words(emu, 'camp_y', 2);
  const hx = words(emu, 'chest_x', 3), hy = words(emu, 'chest_y', 3);
  return {
    w, h, at: (x, y) => raw[(y << 5) | x], open: bytes(emu, 'level_open', 4),
    camps: Array.from({ length: camps }, (_, c) => [cx[c] >> 4, cy[c] >> 4]),
    chests: Array.from({ length: chests }, (_, c) => [hx[c] >> 4, hy[c] >> 4]),
  };
}
/** Breadth-first distances over the links from map s. */
function distances(L, s) {
  const d = new Map([[s, 0]]), q = [s];
  while (q.length) {
    const i = q.shift();
    for (const e of [1, 2, 4, 8]) {
      if (L.links[i] & e && !d.has(i + STEP[e])) {
        d.set(i + STEP[e], d.get(i) + 1);
        q.push(i + STEP[e]);
      }
    }
  }
  return d;
}

test('every area: 6-9 linked maps, all reachable, the way onward farthest from the start', () => {
  const emu = startRun().idle(2);
  for (let n = 1; n <= 300; n++) {
    const seed = (n * 7919) & 0xffff;
    const L = generate(emu, seed);
    const maps = L.type.map((t, i) => (t ? i : -1)).filter((i) => i >= 0);
    assert.ok(maps.length >= 6 && maps.length <= 9, `seed ${seed}: ${maps.length} maps`);
    assert.equal(maps.length, L.maps);
    assert.equal(L.type[L.start], MAP_START);
    assert.equal(L.type[L.exit], MAP_EXIT);
    for (const i of maps) {
      for (const e of [1, 2, 4, 8]) {
        if (!(L.links[i] & e)) continue;
        const j = i + STEP[e];
        const offGrid = j < 0 || j >= CELLS || (e === 2 && i % W === W - 1) || (e === 8 && i % W === 0);
        assert.ok(!offGrid && L.type[j], `seed ${seed}: map ${i} links ${e} to nothing`);
        assert.ok(L.links[j] & OPP[e], `seed ${seed}: map ${j} lacks the link back`);
      }
    }
    const d = distances(L, L.start);
    assert.equal(d.size, maps.length, `seed ${seed}: unreachable maps`);
    assert.equal(d.get(L.exit), Math.max(...d.values()), `seed ${seed}: the exit isn't the farthest map`);
  }
});

test('every map: open edges only at its openings, and openings, camps and chests all connected', () => {
  const emu = startRun().idle(2);
  let carved = 0, mapsBuilt = 0, camps = 0, chests = 0;
  for (let n = 1; n <= 40; n++) {
    const seed = (n * 104729) & 0xffff;
    const L = generate(emu, seed);
    for (let i = 0; i < CELLS; i++) {
      if (!L.type[i]) continue;
      generate(emu, seed, i);
      mapsBuilt++;
      carved += emu.v('level_carved');
      const m = built(emu);
      assert.equal(m.w, L.cw[i] * 5);
      assert.equal(m.h, L.ch[i] * 4);
      // Edges: wall, except an opening OPENING_W wide on each linked side.
      const wide = i === L.start ? 1 : OPENING_W;  // (the entrance room's doorways are one block)
      const gap = (side, k) => m.open[side] !== 255 && k >= m.open[side] && k < m.open[side] + wide;
      for (let x = 0; x < m.w; x++) {
        if (!gap(0, x)) assert.notEqual(m.at(x, 0), BLK_FLOOR, `seed ${seed} map ${i}: hole in the top edge at ${x}`);
        if (!gap(2, x)) assert.notEqual(m.at(x, m.h - 1), BLK_FLOOR, `seed ${seed} map ${i}: hole in the bottom edge at ${x}`);
      }
      for (let y = 0; y < m.h; y++) {
        if (!gap(3, y)) assert.notEqual(m.at(0, y), BLK_FLOOR, `seed ${seed} map ${i}: hole in the left edge at ${y}`);
        if (!gap(1, y)) assert.notEqual(m.at(m.w - 1, y), BLK_FLOOR, `seed ${seed} map ${i}: hole in the right edge at ${y}`);
      }
      // (The entrance room also has its south door, the way back to town.)
      const doors = L.links[i] | (i === L.start ? 4 : 0);
      for (const e of [1, 2, 4, 8]) assert.equal(m.open[SIDE[e]] !== 255, !!(doors & e), `seed ${seed} map ${i}: opening ${e}`);
      camps += m.camps.length;
      chests += m.chests.length;
      if (L.type[i] !== MAP_FIELD) assert.equal(m.camps.length, 0, 'camps only in fields');
      // Everything that must connect, over floor.
      const points = [];
      for (const [s, o] of m.open.entries()) {
        if (o === 255) continue;
        const mid = o + (wide >> 1);
        points.push([[mid, 2], [m.w - 3, mid], [mid, m.h - 3], [2, mid]][s]);
      }
      points.push(...m.camps, ...m.chests);
      if (i !== L.start) points.push([m.w >> 1, m.h >> 1]);  // (the entrance room's middle is its comms device)
      const [sx, sy] = points[0];
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
      // (An exit map's middle is its shaft, 2x2 pits up and left of it: reached
      // if the ground beside it is.)
      const shaft = (x, y) => L.type[i] === MAP_EXIT && x === m.w >> 1 && y === m.h >> 1;
      const reached = (x, y) => seen.has(`${x},${y}`)
        || (shaft(x, y) && [[x + 1, y], [x + 1, y - 1], [x, y + 1], [x - 1, y + 1], [x - 2, y], [x - 2, y - 1], [x, y - 2], [x - 1, y - 2]]
          .some(([a, b]) => seen.has(`${a},${b}`)));
      for (const [x, y] of points.slice(1)) assert.ok(reached(x, y), `seed ${seed} map ${i}: ${x},${y} cut off`);
    }
  }
  assert.ok(camps > mapsBuilt / 2, `camps: ${camps} in ${mapsBuilt} maps`);
  assert.ok(chests > 0, 'chests');
  assert.ok(carved < mapsBuilt / 2, `paths had to be carved in ${carved} of ${mapsBuilt} maps`);
});

test('the same seed gives the same area and maps', () => {
  const emu = startRun().idle(2);
  const area = (seed) => {
    const out = [generate(emu, seed)];
    for (let i = 0; i < CELLS; i++) {
      if (!out[0].type[i]) continue;
      generate(emu, seed, i);
      out.push(Array.from(emu.readBytes(0xd400, 1024)));
    }
    return out;
  };
  const a = area(4242);
  area(999);
  assert.deepEqual(area(4242), a);
});

// --- Playing a descent ---

/** Walk out of the current map through its opening on side e. */
/** Stand just inside the opening on side e (an entrance room's are one block wide). */
function toOpening(emu, e) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const inset = emu.v('level_map') === emu.v('area_start') ? 0 : 1;
  const o = (bytes(emu, 'level_open', 4)[SIDE[e]] + inset) * 16;
  const at = { 1: [o, 2 * 16 - 8], 4: [o, h * 16 - 3 * 16 + 4], 8: [2 * 16, o - 4], 2: [w * 16 - 3 * 16, o - 4] }[e];
  w16(emu, 'player_x', at[0]);
  w16(emu, 'player_y', at[1]);
}
function leaveBy(emu, e) {
  toOpening(emu, e);
  const dir = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e];
  const before = emu.v('maps_entered', 'u16');
  for (let f = 0; f < 120 && emu.v('maps_entered', 'u16') === before; f++) emu.hold(dir, 1);
  assert.notEqual(emu.v('maps_entered', 'u16'), before, `left by ${e}`);
  return emu.idle(30);
}
/** Walk map to map (following links) to map `goal`. */
function walkTo(emu, goal) {
  const L = { links: bytes(emu, 'area_links') };
  for (let guard = 0; guard < 12 && emu.v('level_map') !== goal; guard++) {
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);  // a quiet walk
    const d = distances(L, goal);
    const here = emu.v('level_map');
    const e = [1, 2, 4, 8].find((x) => L.links[here] & x && d.get(here + STEP[x]) === d.get(here) - 1);
    leaveBy(emu, e);
  }
  assert.equal(emu.v('level_map'), goal);
}

test('START on the title begins a descent in the start map of a generated area', () => {
  const emu = startDescent();
  assert.equal(emu.v('play_mode'), 1);
  assert.equal(emu.v('level_map'), emu.v('area_start'));
  assert.equal(emu.v('actors_active'), 0, 'the start map is quiet');
  matchGolden(emu.idle(4), 'descent-start');
});

test('walking out through an opening moves to the linked map, and back', () => {
  const emu = startDescent();
  const start = emu.v('level_map');
  const links = emu.read8(addr(emu, 'area_links') + start);
  const e = [1, 2, 4, 8].find((x) => links & x);
  leaveBy(emu, e);
  assert.equal(emu.v('level_map'), start + STEP[e], 'in the linked map');
  leaveBy(emu, OPP[e]);
  assert.equal(emu.v('level_map'), start, 'and back');
});

test('after a map change no sprite is left where it was on the old screen (the gun follows the player)', () => {
  const emu = startDescent();
  emu.hold('A', 30);  // the gun drawn, firing
  const start = emu.v('level_map');
  const links = emu.read8(addr(emu, 'area_links') + start);
  const e = [1, 2, 4, 8].find((x) => links & x);
  // Leave, then look at the sprites on each frame of the fade-in.
  toOpening(emu, e);
  const dir = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e];
  const before = emu.v('maps_entered', 'u16');
  for (let f = 0; f < 120 && emu.v('maps_entered', 'u16') === before; f++) emu.hold(dir, 1);
  assert.notEqual(emu.v('maps_entered', 'u16'), before, 'left the map');
  for (let f = 0; f < 20; f++) {
    // Player on screen (OAM 3) vs the gun's sprites (1-2 in front, 5-6 behind).
    const oam = emu.readBytes(0xfe00, 40 * 4);
    const py = oam[3 * 4], px = oam[3 * 4 + 1];
    // The reticle (9-10) belongs on a target when there is one; else it's the reload bar, over the player.
    const sprites = emu.v('aim_target') === 0xff ? [0, 1, 2, 5, 6, 9, 10] : [0, 1, 2, 5, 6];
    for (const i of sprites) {
      const y = oam[i * 4], x = oam[i * 4 + 1];
      if (y === 0 || y >= 160) continue;  // hidden
      assert.ok(Math.abs(x - px) < 40 && Math.abs(y - py) < 40, `frame ${f}: sprite ${i} at ${x},${y} far from the player at ${px},${py}`);
    }
    emu.idle(1);
  }
});

test('a camp wakes as you approach, and once cleared it stays cleared', () => {
  for (let wait = 0; wait < 60; wait += 7) {
    const emu = startDescent(bootToTitle(), 4242 + wait);  // seeds until the first map links to one with a camp
    const start = emu.v('level_map');
    const links = emu.read8(addr(emu, 'area_links') + start);
    const e = [1, 2, 4, 8].find((x) => links & x);
    leaveBy(emu, e);
    if (!emu.v('level_camps')) continue;
    // Arriving can be near a camp already; clear the field first, then walk up to camp 0.
    const cx = words(emu, 'camp_x', 1)[0], cy = words(emu, 'camp_y', 1)[0];
    // Step up from the side with more room (never into an opening at the edge).
    const nearX = cx < emu.v('room_w') * 8 ? cx - 8 + 64 : cx - 8 - 64;
    // On open ground: the floor block nearest that spot (not a building or a raised
    // platform, which a blind teleport could land on).
    const map = emu.readBytes(addr(emu, 'room_map'), 768);
    let at = null;
    for (let by = 1; by < emu.v('room_h') - 1; by++)
      for (let bx = 1; bx < emu.v('room_w') - 1; bx++) {
        if (map[by * 32 + bx] !== 0 || map[by * 32 + bx + 1] !== 0 || map[(by + 1) * 32 + bx] !== 0) continue;
        const d = Math.abs(bx * 16 - nearX) + Math.abs(by * 16 - (cy - 12));
        if (!at || d < at.d) at = { x: bx * 16, y: by * 16, d };
      }
    w16(emu, 'player_x', at.x);
    w16(emu, 'player_y', at.y);
    emu.idle(3);
    assert.equal(emu.v('camp_state'), CAMP_AWAKE, 'awake once you are near');
    assert.ok(emu.v('actors_active') > 0, 'its enemies');
    matchGolden(emu.idle(40), 'descent-camp');  // once the camera has caught up
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    emu.idle(3);
    assert.equal(emu.v('camp_state'), CAMP_CLEARED);
    // Leave and come back: still cleared, nobody there.
    leaveBy(emu, OPP[e]);
    leaveBy(emu, e);
    assert.equal(emu.v('camp_state'), CAMP_CLEARED);
    w16(emu, 'player_x', nearX);
    w16(emu, 'player_y', cy - 12);
    emu.idle(3);
    // (camps are at least 3 chunks apart, farther than a camp wakes from)
    assert.equal(emu.v('actors_active'), 0, 'no respawn');
    return;
  }
  assert.fail('no camp found beside the start map');
});

test('the exit map has a shaft: dropping in leads to a new area, one deeper', () => {
  const emu = startDescent();
  const firstLinks = bytes(emu, 'area_links');
  walkTo(emu, emu.v('area_exit'));
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);  // nothing in the way
  // Beside the shaft (2x2 pit blocks in the middle), then into it.
  const w = emu.v('room_w'), h = emu.v('room_h');
  const sx = ((w >> 1) - 1) * 16, sy = ((h >> 1) - 1) * 16;
  w16(emu, 'player_x', sx - 24);
  w16(emu, 'player_y', sy);
  const hp = emu.v('player_hp');
  for (let f = 0; f < 200 && emu.v('layer_depth') === 1; f++) emu.hold('RIGHT', 1);
  emu.idle(40);
  assert.equal(emu.v('layer_depth'), 2, 'one deeper');
  assert.equal(emu.v('level_map'), emu.v('area_start'), 'at the start of the new area');
  assert.ok(emu.v('player_hp') >= hp, "no fall damage (the shaft's XP can level you up: health only goes up)");
  assert.notDeepEqual(bytes(emu, 'area_links'), firstLinks, 'a different area');
});

test('every area starts in an entrance room, two rooms deep: doors on 2 or 3 of N, E, W; south to town', () => {
  const emu = startRun().idle(2);
  for (let n = 1; n <= 200; n++) {
    const seed = (n * 7907) & 0xffff;
    const L = generate(emu, seed);
    assert.equal(L.type[L.start], MAP_START);
    assert.equal(Math.floor(L.start / W), 2, `seed ${seed}: on the bottom row (south of it is out)`);
    assert.deepEqual([L.cw[L.start], L.ch[L.start]], [2, 3], '10 x 12 blocks');
    const doors = [1, 2, 8].filter((e) => L.links[L.start] & e).length;
    assert.ok(doors >= 2 && doors <= 3, `seed ${seed}: ${doors} doors in`);
    assert.equal(L.links[L.start] & 4, 0, 'no map south of it');
  }
  // Its doors sit in the middle of its walls, the south one too; the north one beside
  // the comms monitor (ENTRANCE_DOOR_N).
  const L = generate(emu, 1234);
  generate(emu, 1234, L.start);
  const m = built(emu);
  assert.deepEqual([m.w, m.h], [10, 12]);
  assert.equal(m.open[2], 4, 'the south door, centred');
  for (const [e, mid] of [[1, 7], [2, 5], [8, 5]]) if (L.links[L.start] & e) assert.equal(m.open[SIDE[e]], mid);
  // One block wide: the wall either side of each doorway.
  for (const [e, x, y, dx, dy] of [[1, 7, 0, 1, 0], [4, 4, 11, 1, 0], [2, 9, 5, 0, 1], [8, 0, 5, 0, 1]]) {
    if (e !== 4 && !(L.links[L.start] & e)) continue;
    assert.equal(m.at(x, y), BLK_FLOOR, `door ${e}`);
    assert.notEqual(m.at(x - dx, y - dy), BLK_FLOOR, `door ${e}: wall before it`);
    assert.notEqual(m.at(x + dx, y + dy), BLK_FLOOR, `door ${e}: wall after it`);
  }
});

test("the entrance room's comms device gives the briefing; you arrive through the south door", () => {
  const emu = startDescent().idle(4);
  assert.equal(emu.v('level_map'), emu.v('area_start'));
  const h = emu.v('room_h');
  assert.ok(emu.v('player_y', 'u16') >= (h - 3) * 16, 'by the south door');
  matchGolden(emu, 'entrance-room');
  // Up to the device (on the upper room's back wall, blocks 4-5 of rows 1-2), and A.
  w16(emu, 'player_x', 4 * 16);
  w16(emu, 'player_y', 3 * 16);
  emu.idle(2);
  assert.notEqual(emu.v('talk_near'), 0xff, 'the device is in reach');
  emu.tap('A').idle(3);
  assert.equal(emu.v('dlg_active'), 1, 'the briefing');
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.equal(emu.v('dlg_active'), 0);
});
