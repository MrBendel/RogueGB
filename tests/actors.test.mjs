// Actor system and the first enemies: slimes (chasers) and peashooters (shooters).
// Issues #4 and #9, plus damage basics and jumping over danger (#6).
import { test } from 'node:test';
import { assert, matchGolden, startRun, settle, RUN_OVER, FRAME_CYCLES, FRAME_LIMIT } from './helpers.mjs';

const SLIME = 1, PEASHOOTER = 2;
const ACTOR_MAX = 8, BULLET_MAX = 20;
const ARENA_TOP = 16 * 16;
const CLEAR_Z = 6 * 256;

const px = (emu) => emu.v('player_x', 'u16');
const py = (emu) => emu.v('player_y', 'u16');
/** Read element i of a global array. */
const arr = (emu, name, i, type = 'u8') => {
  const size = type === 'u16' || type === 'i16' ? 2 : 1;
  const addr = emu.symbols.addr(name) + i * size;
  const v = size === 2 ? emu.read16(addr) : emu.read8(addr);
  return type === 'i16' && v > 32767 ? v - 65536 : v;
};
// Bullets are 13-byte records (Bullet in actor.h): alive 0, x 5-6, y 11-12.
const BULLET_SIZE = 13;
const bullet = (emu, i) => {
  const a = emu.symbols.addr('bullets') + i * BULLET_SIZE;
  return { alive: emu.read8(a), x: emu.read16(a + 5), y: emu.read16(a + 11) };
};
const actors = (emu) => Array.from({ length: ACTOR_MAX }, (_, i) => ({
  i, type: arr(emu, 'actor_type', i), x: arr(emu, 'actor_x', i, 'u16'), y: arr(emu, 'actor_y', i, 'u16'),
})).filter((a) => a.type);

function until(emu, buttons, pred, max = 600, what = 'condition') {
  for (let f = 1; f <= max; f++) {
    emu.hold(buttons, 1);
    if (pred(emu)) return f;
  }
  assert.fail(`timed out waiting for ${what}`);
}

/** Walk from the start to the gap in the lab's bottom wall, just above the arena. */
function aboveArena() {
  const emu = startRun().idle(2).hold('LEFT', 80).hold('DOWN', 200);
  until(emu, 'RIGHT', (e) => px(e) >= 148, 200, 'the gap');
  return emu.idle(10);
}
/** Step into the arena (spawns the wave). */
function inArena() {
  const emu = aboveArena();
  until(emu, 'DOWN', (e) => py(e) >= ARENA_TOP, 120, 'the arena');
  return emu;
}

test('the lab stays enemy-free until you enter the arena', () => {
  const emu = aboveArena();
  assert.equal(emu.v('actors_active'), 0);
  assert.equal(emu.v('wave_spawned'), 0);
  until(emu, 'DOWN', (e) => py(e) >= ARENA_TOP, 120, 'the arena');
  emu.idle(1);
  assert.equal(emu.v('wave_spawned'), 1);
  const a = actors(emu);
  assert.equal(a.length, 5);
  assert.equal(a.filter((x) => x.type === SLIME).length, 3);
  assert.equal(a.filter((x) => x.type === PEASHOOTER).length, 2);
});

test('slimes hop toward the player', () => {
  const emu = inArena().hold('DOWN', 12).idle(2);
  const dist = (e) => actors(e).filter((a) => a.type === SLIME)
    .reduce((s, a) => s + Math.abs(a.x - px(e)) + Math.abs(a.y - py(e)), 0);
  const d0 = dist(emu);
  emu.idle(120);
  assert.ok(dist(emu) < d0 - 40, `slimes closed in: ${d0} -> ${dist(emu)}`);
});

test('peashooters flash a warning, then fire at the player', () => {
  const emu = inArena();
  const fired0 = emu.v('bullets_fired', 'u16');
  const shooters = actors(emu).filter((a) => a.type === PEASHOOTER).map((a) => a.i);
  let flashed = false;
  until(emu, [], (e) => {
    if (shooters.some((i) => arr(e, 'actor_flash', i))) flashed = true;
    return e.v('bullets_fired', 'u16') > fired0;
  }, 300, 'a shot');
  assert.ok(flashed, 'flashed before firing');
  assert.ok(emu.v('bullets_active') >= 1);
});

test('bullets fly toward the player and stop at walls', () => {
  const emu = inArena();
  until(emu, [], (e) => e.v('bullets_active') > 0, 300, 'a bullet');
  const b = Array.from({ length: BULLET_MAX }, (_, i) => i).find((i) => bullet(emu, i).alive);
  const d = () => Math.hypot(bullet(emu, b).x + 4 - (px(emu) + 8), bullet(emu, b).y + 8 - (py(emu) + 12));
  const d0 = d();
  emu.idle(6);
  if (bullet(emu, b).alive) assert.ok(d() < d0, `moving toward the player: ${d0} -> ${d()}`);
  // No bullet flies forever: each ends at a wall or on the player.
  emu.idle(300);
  for (let i = 0; i < BULLET_MAX; i++) {
    const bl = bullet(emu, i);
    if (!bl.alive) continue;
    assert.ok(bl.x < 20 * 16 && bl.y < 24 * 16, 'inside the room');
  }
});

test('getting hit costs HP, knocks back and blinks', () => {
  const emu = inArena();
  const hp0 = emu.v('player_hp');
  until(emu, [], (e) => e.v('player_hits', 'u16') > 0, 900, 'a hit');
  assert.equal(emu.v('player_hp'), hp0 - 1);
  assert.ok(emu.v('player_blink') > 0, 'blinking');
  assert.ok(emu.v('player_vx', 'i16') !== 0 || emu.v('player_vy', 'i16') !== 0, 'knocked back');
  // Invulnerable while blinking: no second hit right away.
  const hits = emu.v('player_hits', 'u16');
  emu.idle(30);
  assert.equal(emu.v('player_hits', 'u16'), hits);
});

test('hits only land while you are near the ground (jumping clears enemies and bullets)', () => {
  const emu = inArena();
  let hits = emu.v('player_hits', 'u16');
  let landed = 0;
  for (let f = 0; f < 900; f++) {
    // Keep jumping in place among the enemies.
    emu.hold(f % 30 === 0 ? 'B' : [], 1);
    // Hits are checked after the player moves, so this frame's height is what counts.
    const h = emu.v('player_hits', 'u16');
    const z = emu.v('player_z', 'i16');
    if (h > hits) {
      assert.ok(z < CLEAR_Z, `hit while ${z / 256}px up`);
      landed++;
    }
    hits = h;
    if (emu.v('next_state') === RUN_OVER) break;
  }
});

test('losing all HP ends the run', () => {
  const emu = inArena();
  until(emu, [], (e) => e.v('next_state') === RUN_OVER, 4000, 'the run to end');
  assert.equal(emu.v('player_hp'), 0);
  settle(emu, RUN_OVER, 'run over screen');
});

test('the HUD shows HP', () => {
  const emu = inArena();
  until(emu, [], (e) => e.v('player_hits', 'u16') > 0, 900, 'a hit');
  emu.idle(2);
  assert.equal(emu.v('hud_hp'), emu.v('player_hp'));
});

test('hardware sprites stay within 40 and enemies are drawn while on screen', () => {
  const emu = inArena();
  for (let f = 0; f < 200; f++) {
    emu.idle(1);
    const used = emu.v('sprites_used');
    assert.ok(used <= 40, `${used} sprites`);
    assert.ok(used >= 4 + 2, `player plus at least one enemy (${used})`);
  }
});

const fight = (f) => (f % 45 === 0 ? 'LEFT+B' : f % 90 < 45 ? 'LEFT' : 'RIGHT');

test('CPU budget: a fight in the arena never slows down, with room to spare', () => {
  // The hot loops are in assembly (actor_draw.s, bullets.s, room_fill.s) and
  // enemies think at 30 Hz. The real wave peaks around 47-50k (auto-aim and the
  // held weapon cost ~2.5k a frame, more when scoring lands on the same frame as
  // a peashooter firing); the cap keeps a fifth of the frame free.
  const emu = inArena();
  let worst = 0;
  for (let f = 0; f < 600; f++) {
    emu.hold(fight(f), 1);
    worst = Math.max(worst, emu.lastFrameCycles);
    if (emu.v('next_state') === RUN_OVER) break;
  }
  assert.ok(worst < 52000, `worst frame used ${worst} cycles past vblank`);
});

test('CPU budget: fighting with A held (auto-aim and shots) stays within the cap', () => {
  // The same fight, shooting throughout: targeting and the held weapon (~2.5k a
  // frame, ~4k more on scoring frames) plus up to 6 shots in flight, a shot every
  // 10 frames with its recoil and muzzle flash.
  const emu = inArena();
  let worst = 0;
  for (let f = 0; f < 600; f++) {
    emu.hold(fight(f) + '+A', 1);
    worst = Math.max(worst, emu.lastFrameCycles);
    if (emu.v('next_state') === RUN_OVER) break;
  }
  assert.ok(emu.v('shots_hit', 'u16') > 0, 'the shots land');
  assert.ok(worst < 55000, `worst frame used ${worst} cycles past vblank`);
});

test('sprite flicker never hides an enemy for more than 2 frames, even 8 in a row', () => {
  // The DMG draws at most 10 sprites per scanline: the 10 lowest OAM entries on
  // that line (8x16 sprites cover y-16 .. y-1). Eight enemies side by side are 16
  // sprites on the same lines, so 6 are dropped every frame; actors_update rotates
  // the draw order so that which ones changes from frame to frame.
  const emu = startRun().idle(2);
  const a = (n) => emu.symbols.addr(n);
  const w16 = (n, v) => emu.writeBytes(a(n), [v & 0xff, v >> 8]);
  for (let k = 0; k < 8; k++) {
    // A row across the view, above the player (the camera starts at y 40). The
    // slimes rest (long timer) so they stay in line; walls under them don't matter.
    emu.write8(a('debug_type'), SLIME);
    w16('debug_x', 16 + k * 18);
    w16('debug_y', 56);
    emu.write8(a('debug_cmd'), 2);
    emu.idle(1);
    emu.write8(a('actor_timer') + emu.v('debug_slot'), 254);
  }
  const hidden = new Array(8).fill(0);
  let worst = 0, drops = 0;
  for (let f = 0; f < 120; f++) {
    emu.idle(1);
    const oam = Array.from({ length: 40 }, (_, i) => ({ y: emu.read8(0xc000 + i * 4), x: emu.read8(0xc000 + i * 4 + 1) }));
    const drawn = new Set();
    for (let line = 0; line < 144; line++) {
      let n = 0;
      for (let i = 0; i < 40 && n < 10; i++) {
        const top = oam[i].y - 16;
        if (oam[i].y && line >= top && line < top + 16) { n++; drawn.add(i); }
      }
    }
    const camX = emu.v('cam_x', 'u16'), camY = emu.v('cam_y', 'u16');
    for (const e of actors(emu)) {
      // An enemy counts as seen if either of its two sprites is drawn.
      const sx = e.x - camX + 8, sy = e.y - camY + 16;
      const seen = [...drawn].some((i) => oam[i].y === sy && (oam[i].x === sx || oam[i].x === sx + 8));
      if (!seen) drops++;
      hidden[e.i] = seen ? 0 : hidden[e.i] + 1;
      worst = Math.max(worst, hidden[e.i]);
    }
  }
  assert.equal(actors(emu).length, 8);
  assert.equal(emu.v('cam_y', 'u16'), 40, 'the row is on screen');
  assert.ok(drops > 0, 'the scanline limit really did drop enemies');
  assert.ok(worst <= 2, `an enemy stayed invisible for ${worst} frames in a row`);
});

test('CPU budget: full pools (8 enemies, 20 bullets) stay within the frame', () => {
  // Stress: every 30 frames the debug command refills both pools around the
  // player (and HP is topped up so the run goes on). Refill frames spawn a whole
  // wave at once, so they aren't counted. Worst ~70-75k, median ~55k with auto-aim
  // and the held weapon. At the Color's double speed that's well inside the frame
  // (131k): no frame drops even with every slot busy.
  const emu = inArena().hold('DOWN', 24);
  const cycles = [];
  for (let f = 0; f < 600; f++) {
    const refill = f % 30 === 0;
    if (refill) {
      emu.write8(emu.symbols.addr('debug_cmd'), 1);
      emu.write8(emu.symbols.addr('player_hp'), 6);
    }
    emu.hold(fight(f), 1);
    assert.equal(emu.v('next_state'), emu.v('game_state'), 'still fighting');
    // The refill frame overruns (a whole wave at once), so the next starts late.
    if (f % 30 > 1) cycles.push(emu.lastFrameCycles);
  }
  assert.ok(emu.v('actors_active') >= 6 && emu.v('bullets_active') >= 15, 'the pools are nearly full');
  cycles.sort((x, y) => x - y);
  const median = cycles[cycles.length >> 1], worst = cycles.at(-1);
  const over = cycles.filter((c) => c > FRAME_LIMIT).length;
  assert.ok(median < 57000, `median frame ${median} cycles`);
  assert.equal(over, 0, `${over} of ${cycles.length} frames over the limit (worst ${worst})`);
  assert.ok(worst < FRAME_CYCLES, `worst frame ${worst} cycles: more than a whole frame`);
});

test('screen: the arena wave', () => {
  matchGolden(inArena().hold('DOWN', 40).idle(50), 'arena');
});
