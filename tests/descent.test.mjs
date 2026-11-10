// Generated areas (level.c) and the descent (descent.c), M2: open maps linked at
// their edges, camps that wake as you approach, and a shaft down to the next area.
import { test } from 'node:test';
import { assert, matchGolden, startRun, startDescent, bootToTitle, settle, PLAY } from './helpers.mjs';

const W = 4, CELLS = 12;
const MAP_START = 1, MAP_FIELD = 2, MAP_EXIT = 3, MAP_YARD = 4, MAP_HALL = 5, MAP_HOUSE = 6;
const indoors = (t) => t === MAP_START || t >= MAP_HALL;  // (level.h MAP_INDOORS: one-block doorways)
const AREA_BOSS_BEATEN = 0x80, MODE_MINIBOSS = 2;  // descent.h, run.h
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
  const cx = words(emu, 'camp_x', 4), cy = words(emu, 'camp_y', 4);
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
      const wide = indoors(L.type[i]) ? 1 : OPENING_W;  // (indoors, doorways are one block)
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
      if (L.type[i] === MAP_START || L.type[i] === MAP_EXIT) assert.equal(m.camps.length, 0, 'no camps in the entrance or the exit');
      // Everything that must connect, over floor.
      const points = [];
      for (const [s, o] of m.open.entries()) {
        if (o === 255) continue;
        const mid = o + (wide >> 1);
        points.push([[mid, 2], [m.w - 3, mid], [mid, m.h - 3], [2, mid]][s]);
      }
      points.push(...m.camps, ...m.chests);
      if (!indoors(L.type[i])) points.push([m.w >> 1, m.h >> 1]);  // (indoors the middle can be a wall)
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

test("an area's maps are told apart: a yard, a building or hall, two fields at least", () => {
  const emu = startRun().idle(2);
  const sizes = { [MAP_YARD]: ['2x2'], [MAP_HALL]: ['4x2', '6x2'], [MAP_HOUSE]: ['2x3', '2x4'] };
  for (let n = 1; n <= 300; n++) {
    const seed = (n * 7919) & 0xffff;
    const L = generate(emu, seed);
    const count = (k) => L.type.filter((t) => t === k).length;
    assert.ok(count(MAP_YARD) >= 1, `seed ${seed}: a yard`);
    assert.ok(count(MAP_HALL) + count(MAP_HOUSE) >= 1, `seed ${seed}: a building or a hall`);
    assert.ok(count(MAP_FIELD) >= 2, `seed ${seed}: two fields`);
    L.type.forEach((t, i) => {
      if (sizes[t]) assert.ok(sizes[t].includes(`${L.cw[i]}x${L.ch[i]}`), `seed ${seed} map ${i}: kind ${t} is ${L.cw[i]}x${L.ch[i]}`);
      if (t === MAP_HALL) assert.ok(!(L.links[i] & 5) || (L.links[i] & 10) === 10, `seed ${seed}: a hall runs east-west`);
    });
  }
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
  const inset = indoors(bytes(emu, 'area_type')[emu.v('level_map')]) ? 0 : 1;
  const o = (bytes(emu, 'level_open', 4)[SIDE[e]] + inset) * 16;
  const at = { 1: [o, 2 * 16 - 8], 4: [o, h * 16 - 3 * 16 + 4], 8: [2 * 16, o - 4], 2: [w * 16 - 3 * 16, o - 4] }[e];
  w16(emu, 'player_x', at[0]);
  w16(emu, 'player_y', at[1]);
}
function leaveBy(emu, e) {
  toOpening(emu, e);
  const dir = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e];
  const before = emu.v('maps_entered', 'u16');
  // (A quiet walk out: fields are busy, and a hound's lunge knocks you off the way.)
  for (let f = 0; f < 120 && emu.v('maps_entered', 'u16') === before; f++) { for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0); emu.hold(dir, 1); }
  assert.notEqual(emu.v('maps_entered', 'u16'), before, `left by ${e}`);
  return emu.idle(30);
}
/** On the exit map, walk at its shaft (the middle) from whichever side is clear, into the miniboss's arena. */
function intoShaft(emu) {
  const w = emu.v('room_w'), h = emu.v('room_h');
  const sx = ((w >> 1) - 1) * 16, sy = ((h >> 1) - 1) * 16;
  for (const [dx, dy, dir] of [[-56, 0, 'RIGHT'], [0, 56, 'UP'], [56, 0, 'LEFT'], [0, -56, 'DOWN']]) {
    w16(emu, 'player_x', sx + dx);
    w16(emu, 'player_y', sy + dy);
    for (let f = 0; f < 60 && emu.v('play_mode') !== MODE_MINIBOSS; f++) emu.hold(dir, 1);
    if (emu.v('play_mode') === MODE_MINIBOSS) return;
  }
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

test("camps are in the middle of a map: none wakes as you come in, and big maps keep them off the edges", () => {
  const emu = startRun().idle(2);
  let camps = 0, byDoor = 0, ring = 0, inner = 0;
  for (let n = 1; n <= 40; n++) {
    const seed = (n * 104729) & 0xffff;
    const L = generate(emu, seed);
    for (let i = 0; i < CELLS; i++) {
      if (L.type[i] !== MAP_FIELD) continue;  // (a yard is one screen: its camp wakes as you come in)
      generate(emu, seed, i);
      const m = built(emu);
      // Where you stand coming in through each opening (descent.c place_player).
      const entries = [];
      for (const [s, o] of m.open.entries()) {
        if (o === 255) continue;
        const mid = o + (OPENING_W >> 1);
        entries.push([[mid, 2], [m.w - 3, mid], [mid, m.h - 3], [2, mid]][s]);
      }
      for (const [x, y] of m.camps) {
        camps++;
        // (descent.c WAKE_X, WAKE_Y: 7 by 6 blocks either way)
        if (entries.some(([ex, ey]) => Math.abs(x - ex) < 7 && Math.abs(y - ey) < 6)) byDoor++;
        if (L.cw[i] > 2 && L.ch[i] > 2) {
          const cx = Math.floor(x / 5), cy = Math.floor(y / 4);
          if (cx === 0 || cx === L.cw[i] - 1 || cy === 0 || cy === L.ch[i] - 1) ring++;
          else inner++;
        }
      }
    }
  }
  assert.ok(camps > 300, `${camps} camps`);
  assert.equal(byDoor, 0, `${byDoor} of ${camps} camps wake as you come in`);
  // (By chance, about a third would be inside: most chunks are on the edge, and no two camps are neighbours.)
  assert.ok(inner * 5 >= ring * 4, `${inner} camps in the middle of big maps, ${ring} on their edges`);
});

test("leaving a map takes a push into its opening's far edge (not a brush past it)", () => {
  const emu = startDescent(bootToTitle(), 4242);
  const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
  const e = [1, 2, 4, 8].find((x) => links & x), dir = { 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e];
  const before = emu.v('maps_entered', 'u16');
  const quiet = () => { for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0); };
  toOpening(emu, e);
  // Up to the edge and a little way into the push, then let go: still here.
  for (let f = 0; f < 60 && emu.v('edge_push') < 5; f++) { quiet(); emu.hold(dir, 1); }
  assert.equal(emu.v('edge_push'), 5, 'pushing on the edge');
  emu.idle(20);
  assert.equal(emu.v('maps_entered', 'u16'), before, 'let go: still on the map');
  assert.equal(emu.v('edge_push'), 0, 'the push starts over');
  // Push again: out after EDGE_PUSH (10) frames of it.
  let pushed = 0;
  for (let f = 0; f < 60 && emu.v('maps_entered', 'u16') === before; f++) { quiet(); emu.hold(dir, 1); pushed++; }
  assert.notEqual(emu.v('maps_entered', 'u16'), before, 'out');
  assert.ok(pushed >= 10, `out after ${pushed} frames`);
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
    // The HUD counts the fight's foes: the camp's living and the rest still to come.
    let living = 0;
    for (let i = 0; i < 8; i++) if ((emu.v('camp_actors') >> i) & 1 && emu.read8(addr(emu, 'actor_type') + i)) living++;
    assert.ok(emu.v('hud_foes') >= living + emu.v('camp_left'), `${emu.v('hud_foes')} foes on the HUD`);
    matchGolden(emu.idle(40), 'descent-camp');  // once the camera has caught up
    emu.writeBytes(addr(emu, 'camp_left'), [0, 0, 0, 0]);  // (no more coming, to any camp awake)
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    emu.idle(3);
    assert.equal(emu.v('camp_state'), CAMP_CLEARED);
    assert.equal(emu.v('hud_foes'), 0, 'the count goes when the fight is won');
    // Leave and come back: still cleared, nobody there.
    leaveBy(emu, OPP[e]);
    leaveBy(emu, e);
    assert.equal(emu.v('camp_state'), CAMP_CLEARED);
    w16(emu, 'player_x', nearX);
    w16(emu, 'player_y', cy - 12);
    emu.idle(3);
    // (Another camp can be near enough to wake: camps are only kept off each other's chunks.)
    assert.equal(emu.v('camp_state'), CAMP_CLEARED, 'still cleared');
    assert.equal(emu.v('camp_actors'), 0, 'no respawn');
    return;
  }
  assert.fail('no camp found beside the start map');
});

test("a camp's enemies warp in: a glow on the floor, a beam, then they flicker in; harmless and unhittable till then", () => {
  const WARPING = 16, WARP_FIRST = 28;  // descent.h
  for (let wait = 0; wait < 60; wait += 7) {
    const emu = startDescent(bootToTitle(), 4242 + wait);
    const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
    leaveBy(emu, [1, 2, 4, 8].find((x) => links & x));
    if (!emu.v('level_camps')) continue;
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    for (let c = 0; c < 4; c++) emu.write8(addr(emu, 'camp_state') + c, CAMP_CLEARED);
    // Beside it, the camera settled, then camp 0 woken afresh (as if just walked up to).
    const cx = words(emu, 'camp_x', 1)[0], cy = words(emu, 'camp_y', 1)[0];
    const side = cx > 120 ? -1 : 1;
    w16(emu, 'player_x', cx - 8 + side * 56);
    w16(emu, 'player_y', cy + 8);
    emu.idle(120);
    emu.write8(addr(emu, 'camp_state'), CAMP_ASLEEP);
    emu.idle(1);
    assert.equal(emu.v('camp_state'), CAMP_AWAKE);
    const warping = [...Array(8).keys()].filter((i) => emu.read8(addr(emu, 'actor_type') + i));
    assert.ok(warping.length > 0, 'its first wave');
    for (const i of warping) {
      assert.equal(emu.read8(addr(emu, 'actor_scripted') + i), WARPING, `enemy ${i} warping in`);
      assert.equal(emu.read8(addr(emu, 'actor_guard') + i), 1, `enemy ${i} can't be hit (shots, the sword and the aim pass it by)`);
    }
    const a = warping[0];
    emu.idle(3);
    assert.equal(emu.read8(addr(emu, 'actor_t0') + a), 8, 'the glow on the floor first');
    matchGolden(emu, 'warp-glow');
    emu.idle(8);  // (and on screen: the sprites go up a frame after they're set)
    assert.equal(emu.read8(addr(emu, 'actor_t0') + a), 126, 'then the beam');
    matchGolden(emu, 'warp-beam');
    // Standing in it does nothing.
    const hits = emu.v('player_hits', 'u16') + emu.v('shield_hits', 'u16');
    w16(emu, 'player_x', emu.read16(addr(emu, 'actor_x') + a * 2));
    w16(emu, 'player_y', emu.read16(addr(emu, 'actor_y') + a * 2));
    emu.idle(4);
    assert.equal(emu.v('player_hits', 'u16') + emu.v('shield_hits', 'u16'), hits, 'no touch while it warps');
    emu.idle(WARP_FIRST - 15);
    for (const i of warping) {
      if (!emu.read8(addr(emu, 'actor_type') + i)) continue;
      assert.equal(emu.read8(addr(emu, 'actor_scripted') + i), 0, `enemy ${i} let loose`);
      assert.equal(emu.read8(addr(emu, 'actor_guard') + i), 0, `enemy ${i} hittable`);
    }
    return;
  }
  assert.fail('no camp found beside the start map');
});

test("the first area's maps draw on every kind but the medic and the stalker; from the second they join; the third has stalkers, bulwarks and medics; a map keeps to 2 kinds (3 deeper)", () => {
  const kinds = { 1: new Set(), 2: new Set(), 3: new Set() };
  for (const depth of [1, 2, 3])
    for (let n = 0; n < 24; n++) {  // (8 seeds, out each of the entrance's three doors; every camp there)
      const emu = startDescent(bootToTitle(), 4242 + 7 * Math.floor(n / 3));
      const here = new Set();
      leaveBy(emu, [1, 2, 8][n % 3]);
      w8(emu, 'layer_depth', depth);  // (the roster's tier: world.c)
      const camps = emu.v('level_camps'), cx = words(emu, 'camp_x', camps), cy = words(emu, 'camp_y', camps);
      for (let c = 0; c < camps; c++) {
        for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
        w16(emu, 'player_x', cx[c]);
        w16(emu, 'player_y', cy[c]);
        emu.idle(2);
        for (const t of bytes(emu, 'actor_type', 8)) if (t) kinds[depth].add(t), here.add(t);
      }
      assert.ok(here.size <= (depth >= 2 ? 3 : 2), `depth ${depth}, seed ${n}: a map's camps keep to its few kinds (${[...here]})`);
    }
  assert.ok([...kinds[1]].every((k) => k !== 6 && k !== 9) && [1, 2, 3, 4, 7, 8].every((k) => kinds[1].has(k)), `area 1: all but the medic and the stalker (${[...kinds[1]]})`);
  assert.ok(kinds[2].has(7) && kinds[2].has(9) && kinds[2].has(6), `area 2: spiders, stalkers and medics (${[...kinds[2]]})`);
  // (a camp by the entrance can wake at depth 1, before the test sets the depth)
  assert.ok(kinds[3].has(9) && kinds[3].has(8) && kinds[3].has(6), `area 3: stalkers, bulwarks and medics (${[...kinds[3]]})`);
});

test("the world's colours: drones and hounds in one palette, turrets and mercs in another, bullets and elites a third", () => {
  const emu = startDescent();
  const pal = (p) => [1, 2, 3].map((k) => emu.read16(addr(emu, 'pal_base_obj') + (p * 4 + k) * 2));
  const rgb = (r, g, b) => r | (g << 5) | (b << 10);
  assert.deepEqual(pal(2), [rgb(10, 30, 31), rgb(11, 15, 26), rgb(2, 3, 10)], 'creatures: slate blue, a cyan eye');
  assert.deepEqual(pal(3), [rgb(31, 8, 6), rgb(28, 22, 4), rgb(5, 3, 9)], 'machines: yellow, a red visor');
  assert.deepEqual(pal(4), [rgb(31, 26, 31), rgb(31, 4, 16), rgb(6, 0, 4)], 'bullets and elites: hot magenta');
});

test('the exit map has a shaft: dropping in leads to a new area, one deeper', () => {
  const emu = startDescent();
  const firstLinks = bytes(emu, 'area_links');
  walkTo(emu, emu.v('area_exit'));
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);  // nothing in the way
  const exitChests = addr(emu, 'area_chests_opened') + emu.v('area_exit');
  emu.write8(exitChests, emu.read8(exitChests) | AREA_BOSS_BEATEN);  // (its miniboss: the next test)
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

test('every area starts in an entrance room, one screen: doors N, E and W, three ways in; south to town', () => {
  const emu = startRun().idle(2);
  for (let n = 1; n <= 200; n++) {
    const seed = (n * 7907) & 0xffff;
    const L = generate(emu, seed);
    assert.equal(L.type[L.start], MAP_START);
    assert.equal(Math.floor(L.start / W), 2, `seed ${seed}: on the bottom row (south of it is out)`);
    assert.deepEqual([L.cw[L.start], L.ch[L.start]], [2, 2], '10 x 8 blocks');
    assert.equal(L.links[L.start] & 11, 11, `seed ${seed}: doors N, E and W`);
    assert.equal(L.links[L.start] & 4, 0, 'no map south of it');
  }
  // Its doors make a cross through the room: the north and south ones in line (column
  // 4, ENTRANCE_DOOR_N), the east and west ones on row 4 (ENTRANCE_DOOR_EW).
  const L = generate(emu, 1234);
  generate(emu, 1234, L.start);
  const m = built(emu);
  assert.deepEqual([m.w, m.h], [10, 8]);
  assert.equal(m.open[2], 4, 'the south door, centred');
  for (const e of [1, 2, 8]) assert.equal(m.open[SIDE[e]], 4);
  // One block wide: the wall either side of each doorway.
  for (const [e, x, y, dx, dy] of [[1, 4, 0, 1, 0], [4, 4, 7, 1, 0], [2, 9, 4, 0, 1], [8, 0, 4, 0, 1]]) {
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
  // Up to the device (on the back wall, blocks 2-3 of rows 1-2), and A.
  w16(emu, 'player_x', 2 * 16);
  w16(emu, 'player_y', 3 * 16);
  emu.idle(2);
  assert.notEqual(emu.v('talk_near'), 0xff, 'the device is in reach');
  emu.tap('A').idle(3);
  assert.equal(emu.v('dlg_active'), 1, 'the briefing');
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.equal(emu.v('dlg_active'), 0);
});

test("the entrance room's goals board: the area's goal, your jobs, how much you've seen", () => {
  const emu = startDescent().idle(4);
  const P = addr(emu, 'profile');
  emu.writeBytes(P + 227, [3]);  // a bounty job (mission.h) ...
  emu.writeBytes(P + 230, [0]);  // ... on the Claimjaw ...
  emu.writeBytes(P + 233, [1]);  // ... once
  // Up to the board (on the back wall right of the north door, blocks 6-7 of rows 1-2), and A.
  w16(emu, 'player_x', 6 * 16);
  w16(emu, 'player_y', 3 * 16);
  emu.idle(2);
  assert.notEqual(emu.v('talk_near'), 0xff, 'the board is in reach');
  emu.tap('A').idle(4);
  assert.equal(emu.v('paused'), 1, 'the world waits');
  assert.equal(emu.v('menu_page'), 8, 'the goals page (PAGE_GOALS)');
  matchGolden(emu, 'goals-board');
  emu.tap('B').idle(4);
  assert.equal(emu.v('paused'), 0, 'B leaves');
});

test("a hab module's door leads into its interior and back out onto its step", () => {
  // The first seed from 4251 whose map through the entrance room's first door has a hab module.
  let emu;
  for (let seed = 4251; ; seed++) {
    emu = startDescent(bootToTitle(), seed);
    const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
    leaveBy(emu, [1, 2, 8].find((x) => links & x));
    if (emu.v('level_doors') || seed > 4300) break;
  }
  assert.ok(emu.v('level_doors') > 0, 'a door on this map');
  const bx = emu.read8(addr(emu, 'door_bx')), by = emu.read8(addr(emu, 'door_by')), map = emu.v('level_map');
  // Up the step into the door.
  w16(emu, 'player_x', bx * 16);
  w16(emu, 'player_y', by * 16 + 4);
  for (let f = 0; f < 60 && emu.v('level_inside') === 0xff; f++) emu.hold('UP', 1);
  assert.equal(emu.v('level_inside'), 0, 'inside building 0');
  emu.idle(40);
  assert.deepEqual([emu.v('room_w'), emu.v('room_h')], [10, 8], 'one screen');
  assert.equal(emu.v('level_chests'), 1, 'its chest');
  matchGolden(emu, 'interior');
  // The side walls are a strip in the outer half of their blocks: you walk right up to them.
  const [px, py] = [emu.v('player_x', 'u16'), emu.v('player_y', 'u16')];
  w16(emu, 'player_y', 5 * 16 - 8);  // (the row between the couches and the counters)
  const walk = (dir, n) => {  // (its guards cleared as you go: just the walls)
    for (let f = 0; f < n; f++) {
      for (let k = 0; k < 8; k++) emu.write8(addr(emu, 'actor_type') + k, 0);
      emu.hold(dir, 1);
    }
  };
  walk('LEFT', 60);
  assert.equal(emu.v('player_x', 'u16') + 3, 8, 'the hitbox against the west wall strip');
  walk('RIGHT', 150);
  assert.equal(emu.v('player_x', 'u16') + 3 + 10, 9 * 16 + 8, 'and against the east one');
  w16(emu, 'player_x', px);
  w16(emu, 'player_y', py);
  // Out of its south door: the same map, on the step.
  for (let f = 0; f < 120 && emu.v('level_inside') !== 0xff; f++) emu.hold('DOWN', 1);
  emu.idle(10);
  assert.equal(emu.v('level_map'), map, 'the same map');
  assert.deepEqual([emu.v('player_x', 'u16') >> 4, emu.v('player_y', 'u16') >> 4], [bx, by], 'on the step');
});

test("the exit map's shaft is guarded: its miniboss's arena, then back beside the shaft, open", () => {
  const emu = startDescent();
  emu.writeBytes(addr(emu, 'profile') + 227, [3]);  // a bounty job (mission.h) ...
  emu.writeBytes(addr(emu, 'profile') + 230, [0]);  // ... on the Claimjaw ...
  emu.writeBytes(addr(emu, 'profile') + 233, [1]);  // ... once
  walkTo(emu, emu.v('area_exit'));
  const exit = emu.v('area_exit');
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
  const w = emu.v('room_w'), h = emu.v('room_h');
  const sx = ((w >> 1) - 1) * 16, sy = ((h >> 1) - 1) * 16;
  emu.write8(addr(emu, 'player_hp'), 3);  // hurt on the way
  intoShaft(emu);
  assert.equal(emu.v('play_mode'), MODE_MINIBOSS, 'into its arena');
  emu.until((e) => e.v('in_transition'), 60, 'the scene change');
  settle(emu, PLAY, 'the arena');
  assert.equal(emu.v('layer_depth'), 1);
  assert.ok(emu.v('boss_kind') !== 0, 'a miniboss');
  assert.equal(emu.v('boss_campaign'), 1);
  assert.equal(emu.v('player_hp'), 3, 'no free heal going in');
  const level = emu.read8(addr(emu, 'profile') + 1);
  // Won (its fight is the miniboss tests'): back down, beside the open shaft.
  emu.write8(addr(emu, 'boss_state'), 5);  // BOSS_WON
  emu.until((e) => e.v('play_mode') === 1, 300, 'back to the descent');
  emu.until((e) => e.v('in_transition'), 60, 'the scene change');
  settle(emu, PLAY, 'the descent');
  assert.equal(emu.v('level_map'), exit, 'the exit map');
  // Nor coming back: the 3 it went in with, unless the win's XP levelled it up (a full heal).
  const up = emu.read8(addr(emu, 'profile') + 1) > level;
  assert.equal(emu.v('player_hp'), up ? emu.v('player_hp_max') : 3, up ? 'levelled up: full' : 'as it went in');
  assert.ok(emu.read8(addr(emu, 'area_chests_opened') + exit) & AREA_BOSS_BEATEN, 'its miniboss beaten');
  assert.equal(emu.read8(addr(emu, 'profile') + 236), 1, 'the bounty job done');
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  assert.ok(Math.abs(px - sx) < 48 && Math.abs(py - sy) < 48, `beside the shaft: ${px},${py}`);
  const drops = Array.from(emu.readBytes(addr(emu, 'drop_kind'), 8));
  assert.ok(drops.includes(2), 'its reward: a gun at your feet');
  // Into it: the next area.
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
  for (let f = 0; f < 200 && emu.v('layer_depth') === 1; f++) emu.hold('RIGHT', 1);
  emu.idle(40);
  assert.equal(emu.v('layer_depth'), 2, 'one deeper');
});

test('containers beside the set pieces open as you walk up: their look, their loot, saved', () => {
  for (let seed = 4242; seed < 4242 + 40; seed += 3) {
    const emu = startDescent(bootToTitle(), seed);
    const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
    leaveBy(emu, [1, 2, 8].find((x) => links & x));
    // (one with floor below it, to walk up into)
    const c = bytes(emu, 'cont_kind', emu.v('level_conts')).findIndex((kind, k) => {
      const bx = emu.read8(addr(emu, 'cont_bx') + k), by = emu.read8(addr(emu, 'cont_by') + k);
      return emu.read8(0xd400 + (by + (kind === 1 ? 1 : 2)) * 32 + bx) === BLK_FLOOR;
    });
    if (c < 0) continue;
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    const map = emu.v('level_map'), bx = emu.read8(addr(emu, 'cont_bx') + c), by = emu.read8(addr(emu, 'cont_by') + c), bit = 1 << c;
    const kind = emu.read8(addr(emu, 'cont_kind') + c), h = kind === 1 ? 1 : 2;
    const vis = () => emu.read8(addr(emu, 'room_vis') + (by + h - 1) * 32 + bx);  // (its bottom block: a locker's head is the same open or shut)
    const closed = vis();
    // Below it, then up into it.
    w16(emu, 'player_x', bx * 16);
    w16(emu, 'player_y', (by + h) * 16 + 4);
    for (let f = 0; f < 30 && !(emu.read8(addr(emu, 'area_conts_opened') + map) & bit); f++) emu.hold('UP', 1);
    assert.ok(emu.read8(addr(emu, 'area_conts_opened') + map) & bit, 'opened');
    assert.notEqual(vis(), closed, 'its open look');
    assert.ok(bytes(emu, 'drop_kind', 6).some((d) => d), 'its loot on the floor');
    assert.equal(emu.read8(addr(emu, 'profile') + 202 + map) & bit, bit, 'in the file');
    return;
  }
  assert.fail('no map with containers found');
});

test("each lap of the world's minibosses is tougher: area 4's Claimjaw has half again its health", () => {
  const hp = {};
  for (const depth of [1, 4]) {
    const emu = startDescent();
    walkTo(emu, emu.v('area_exit'));
    emu.write8(addr(emu, 'layer_depth'), depth);
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    intoShaft(emu);
    emu.until((e) => e.v('in_transition'), 60, 'the scene change');
    settle(emu, PLAY, 'the arena');
    hp[depth] = emu.v('boss_hp_max');
  }
  assert.equal(hp[1], 18, "area 1's Claimjaw");
  assert.equal(hp[4], 27, "area 4's: half again");
});

test("the vendor post before the miniboss: green for ammo, red for health, paid in credits", () => {
  const emu = startDescent();
  walkTo(emu, emu.v('area_exit'));
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
  assert.equal(emu.v('level_vends'), 2, 'two machines');
  emu.write8(addr(emu, 'level_conts'), 0);  // (no lockers opening on the way: their credits)
  const P = addr(emu, 'profile'), CREDITS = 7, AMMO = 198;
  const credits = () => Array.from(emu.readBytes(P + CREDITS, 4)).reduce((v, b, i) => v + b * 256 ** i, 0);
  emu.writeBytes(P + CREDITS, [244, 1, 0, 0]);  // 500
  emu.writeBytes(P + AMMO, [1, 1, 1, 1]);       // nearly dry
  emu.write8(addr(emu, 'player_hp'), 5);
  const buy = (k) => {
    for (let i = 0; i < 6; i++) emu.write8(addr(emu, 'drop_kind') + i, 0);  // (no pickups underfoot)
    w16(emu, 'player_x', emu.read8(addr(emu, 'vend_bx') + k) * 16);
    w16(emu, 'player_y', (emu.read8(addr(emu, 'vend_by') + k) + 2) * 16 - 4);
    emu.hold('UP', 4).idle(4);
    assert.notEqual(emu.v('talk_near'), 0xff, `machine ${k} in reach`);
    emu.tap('A').idle(4);
  };
  buy(0);
  assert.equal(credits(), 500 - 25, 'the ammo machine: 25 credits at depth 1');
  assert.ok(Array.from(emu.readBytes(P + AMMO, 4)).every((n) => n > 1), 'every reserve filled');
  emu.shot('vendor-post');
  buy(1);
  assert.equal(credits(), 500 - 25 - 40, 'the health machine: 40');
  assert.equal(emu.v('player_hp'), emu.v('player_hp_max'), 'healed');
  buy(1);
  assert.equal(credits(), 500 - 25 - 40, 'full already: nothing charged');
});

/** From seed's entrance room out its first door, then into that map's first building. */
function intoBuilding(seed) {
  const emu = startDescent(bootToTitle(), seed);
  const links = emu.read8(addr(emu, 'area_links') + emu.v('level_map'));
  leaveBy(emu, [1, 2, 8].find((x) => links & x));
  if (!emu.v('level_doors')) return null;
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
  w16(emu, 'player_x', emu.read8(addr(emu, 'door_bx')) * 16);
  w16(emu, 'player_y', emu.read8(addr(emu, 'door_by')) * 16 + 4);
  for (let f = 0; f < 60 && emu.v('level_inside') === 0xff; f++) emu.hold('UP', 1);
  return emu.idle(40);
}

test("a building's lockers open and stay open (their own bits, saved)", () => {
  let emu = null;  // (the first seed whose building has lockers and no guard)
  for (let seed = 4251; seed < 4351 && !emu; seed++) {
    const e = intoBuilding(seed);
    if (e && e.v('level_inside') === 0 && e.v('level_conts') === 2 && !e.v('level_camps')) emu = e;
  }
  assert.ok(emu, 'a building with lockers');
  const map = emu.v('level_map'), bx = emu.read8(addr(emu, 'cont_bx'));
  w16(emu, 'player_x', bx * 16);
  w16(emu, 'player_y', 3 * 16 + 4);
  for (let f = 0; f < 30 && !(emu.read8(addr(emu, 'area_inside_opened') + map) & 1); f++) emu.hold('UP', 1);
  assert.equal(emu.read8(addr(emu, 'area_inside_opened') + map) & 1, 1, 'building 0, locker 0');
  assert.equal(emu.read8(addr(emu, 'area_conts_opened') + map), 0, "not the map's own containers");
  assert.equal(emu.read8(addr(emu, 'profile') + 214 + map) & 1, 1, 'in the file (exp_inside)');
  emu.idle(30);
  matchGolden(emu, 'building-lockers');  // (against the wall: one open, one shut)
});

test("every building's door leads inside (nothing's built on its step)", () => {
  let doors = 0;
  for (let seed = 4240; seed < 4300; seed++) {
    const emu = intoBuilding(seed);
    if (!emu) continue;
    doors++;
    assert.equal(emu.v('level_inside'), 0, `seed ${seed}: inside its first building`);
  }
  assert.ok(doors >= 5, `${doors} maps with a building`);
});

test('a guarded building: its camp wakes, and its chest waits until the camp is cleared', () => {
  let emu = null;  // (the first seed whose building is guarded)
  for (let seed = 4240; seed < 4340 && !emu; seed++) {
    const e = intoBuilding(seed);
    if (e && e.v('level_inside') === 0 && e.v('level_camps') === 1) emu = e;
  }
  assert.ok(emu, 'a guarded building');
  assert.ok(emu.v('actors_active') > 0, 'its guards, awake');
  const map = emu.v('level_map'), cx = emu.v('chest_x', 'u16'), cy = emu.v('chest_y', 'u16');
  const opened = () => emu.read8(addr(emu, 'area_chests_opened') + map) & (1 << 3);
  // Up to the chest with the guards about: it stays shut. (From the side: a couch
  // can stand below it.)
  const left = cx < 5 * 16;
  const toChest = () => {
    w16(emu, 'player_x', left ? cx + 24 : cx - 24);
    w16(emu, 'player_y', cy - 4);
    emu.hold(left ? 'LEFT' : 'RIGHT', 24).idle(2);
  };
  toChest();
  assert.equal(opened(), 0, 'locked while they live');
  for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);  // (cleared, and none to come)
  emu.write8(addr(emu, 'camp_left'), 0);
  emu.idle(4);
  assert.ok(emu.read8(addr(emu, 'area_camps_cleared') + map) & (1 << 4), 'cleared: bit 4 + door 0');
  toChest();
  assert.ok(opened(), 'then it opens');
});

test('a building or a hall is walked through: in by one door, out by another onto another map', () => {
  for (const [kind, name] of [[MAP_HOUSE, 'building'], [MAP_HALL, 'hall']]) {
    let emu = null, m = -1;
    for (let seed = 4242; seed < 4342 && !emu; seed++) {
      const e = startDescent(bootToTitle(), seed);
      const t = bytes(e, 'area_type'), links = bytes(e, 'area_links');
      m = t.findIndex((k, i) => k === kind && [1, 2, 4, 8].filter((x) => links[i] & x).length >= 2);
      if (m >= 0) emu = e;
    }
    assert.ok(emu, `a ${name} with two doors`);
    const L = { links: bytes(emu, 'area_links') };
    let prev = -1;
    for (let guard = 0; guard < 12 && emu.v('level_map') !== m; guard++) {
      const d = distances(L, m), here = emu.v('level_map');
      prev = here;
      leaveBy(emu, [1, 2, 4, 8].find((x) => L.links[here] & x && d.get(here + STEP[x]) === d.get(here) - 1));
    }
    assert.equal(emu.v('level_map'), m);
    assert.equal(emu.v('level_roofed'), 1, 'indoors');
    for (let i = 0; i < 8; i++) emu.write8(addr(emu, 'actor_type') + i, 0);
    emu.idle(4);
    matchGolden(emu, name);
    const e = [1, 2, 4, 8].find((x) => L.links[m] & x && m + STEP[x] !== prev);
    leaveBy(emu, e);
    assert.equal(emu.v('level_map'), m + STEP[e], `out the ${name}'s other door`);
  }
});
