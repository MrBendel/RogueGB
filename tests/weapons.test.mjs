// Weapons (weapon.c, firing in aim.c): SELECT switches between the pistol,
// shotgun, assault rifle, sniper rifle and sword, and each fires its own way (ROADMAP
// "Weapons").
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const PISTOL = 0, SHOTGUN = 1, RIFLE = 2, SNIPER = 3, SWORD = 4;
const RIFLE_TAP = 0, RIFLE_BURST = 1, RIFLE_AUTO = 2;
const SLIME = 1;
const DIR_RIGHT = 2;
const SHOT_SIZE = 13, SHOT_MAX = 16;

const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const s16 = (v) => (v > 32767 ? v - 65536 : v);
// The held weapon's stats (WeaponStats in weapon.h).
const WPN = { mode: 0, fire_interval: 1, tap_interval: 2, pellets: 3, spread: 4, jitter: 5, range: 8, damage: 10, windup: 13 };
const wpn = (emu, field) => emu.read8(addr(emu, 'wpn') + WPN[field]);
const liveShots = (emu) => Array.from({ length: SHOT_MAX }, (_, i) => {
  const a = addr(emu, 'shots') + i * SHOT_SIZE;
  return { alive: emu.read8(a), vx: s16(emu.read16(a + 1)), vy: s16(emu.read16(a + 7)) };
}).filter((s) => s.alive);

/** A run in the lab, standing still facing right, holding weapon w (SELECT cycles). */
function holding(w, x = 72, y = 96) {
  const emu = startRun().idle(2);
  for (let k = 0; k < w; k++) emu.tap('SELECT');
  assert.equal(emu.v('weapon_cur'), w);
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y);
  w16(emu, 'player_vx', 0);
  w16(emu, 'player_vy', 0);
  w8(emu, 'player_facing', DIR_RIGHT);
  return emu.idle(2);
}

function spawnStill(emu, x, y) {
  w8(emu, 'debug_type', SLIME);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  w8(emu, 'actor_timer', 254, slot);
  return slot;
}

test('SELECT switches weapons in turn, and the HUD names the one held', () => {
  const emu = startRun().idle(2);
  assert.equal(emu.v('weapon_cur'), PISTOL);
  for (const w of [SHOTGUN, RIFLE, SNIPER, SWORD, PISTOL]) {
    emu.tap('SELECT');
    assert.equal(emu.v('weapon_cur'), w);
  }
  matchGolden(holding(SHOTGUN).idle(2), 'hud-shotgun');
});

test('pistol: tapping fires faster than holding', () => {
  const held = holding(PISTOL).hold('A', 42);
  const holdShots = held.v('shots_fired', 'u16');
  const tapped = holding(PISTOL);
  for (let f = 0; f < 6; f++) tapped.hold('A', 1).idle(wpn(tapped, 'tap_interval'));
  const tapShots = tapped.v('shots_fired', 'u16');
  assert.equal(holdShots, 3, 'held: one every fire_interval (14)');
  assert.equal(tapShots, 6, 'tapped: one per press, tap_interval apart');
});

test('shotgun: one shot is a fan of pellets, and it shoves you back', () => {
  const emu = holding(SHOTGUN).hold('A', 1);
  const n = wpn(emu, 'pellets');
  assert.equal(emu.v('shots_fired', 'u16'), n);
  const shots = liveShots(emu);
  assert.equal(shots.length, n);
  // Fanned around "right": all moving right, at different angles, symmetric.
  const angles = shots.map((s) => Math.round((Math.atan2(s.vy, s.vx) * 180) / Math.PI)).sort((a, b) => a - b);
  assert.ok(shots.every((s) => s.vx > 0), 'all to the right');
  assert.equal(new Set(angles).size, n, `different angles: ${angles}`);
  assert.equal(angles[0], -angles[n - 1], `symmetric: ${angles}`);
  assert.ok(emu.v('player_vx', 'i16') < 0, 'shoved back (left)');
});

/** Holding the rifle with its trigger set to `mode` (rifle_mode, weapon.h). */
function rifle(mode) {
  const emu = holding(RIFLE);
  w8(emu, 'rifle_mode', mode);
  return emu.idle(2);
}
const fired = (emu) => emu.v('shots_fired', 'u16');

test('rifle on auto: holding A fires every fire_interval frames', () => {
  const emu = rifle(RIFLE_AUTO);
  const interval = wpn(emu, 'fire_interval');
  emu.hold('A', interval * 6);
  assert.equal(fired(emu), 6);
});

test('rifle on tap: one shot per press, however long it is held', () => {
  const emu = rifle(RIFLE_TAP).hold('A', 40);
  assert.equal(fired(emu), 1, 'held: one shot');
  emu.idle(10);
  const tap = wpn(emu, 'tap_interval');
  for (let k = 0; k < 6; k++) emu.hold('A', 1).idle(tap);
  assert.equal(fired(emu), 7, 'each tap fires');
});

test('rifle on burst: a press fires 3 rounds a few frames apart, then pauses', () => {
  const emu = rifle(RIFLE_BURST).hold('A', 1);
  const times = [0];
  let last = fired(emu);
  assert.equal(last, 1, 'the first round at once');
  for (let f = 1; f <= 30; f++) {
    emu.idle(1);  // the burst goes on with A let go
    if (fired(emu) !== last) times.push(f);
    last = fired(emu);
  }
  assert.equal(fired(emu), 3, `three rounds: at frames ${times}`);
  assert.equal(times[1] - times[0], times[2] - times[1], 'evenly spaced');
  // Holding A doesn't start another; the next press does (after the pause).
  const emu2 = rifle(RIFLE_BURST).hold('A', 40);
  assert.equal(fired(emu2), 3, 'held: one burst');
  emu2.idle(2).hold('A', 1).idle(12);
  assert.equal(fired(emu2), 6, 'the next press: another burst');
});

test('the RIFLE row on the pause panel switches tap, burst and auto', () => {
  const emu = startRun().idle(2);
  assert.equal(emu.v('rifle_mode'), RIFLE_AUTO, 'auto to start');
  emu.tap('START').idle(8);
  for (let k = 0; k < 11; k++) emu.tap('DOWN');  // MOVE ... LEAD SPD -> RIFLE
  for (const m of [RIFLE_TAP, RIFLE_BURST, RIFLE_AUTO]) {
    emu.tap('RIGHT');
    assert.equal(emu.v('rifle_mode'), m);
  }
});

test('sniper: holding steadies into a critical that one-shots a slime; a quick tap fires a normal shot', () => {
  const emu = holding(SNIPER);
  const t = spawnStill(emu, 72 + 96, 96);
  emu.idle(24);  // scored and targeted
  assert.equal(emu.v('aim_target'), t);
  const windup = wpn(emu, 'windup');
  emu.hold('A', windup - 1);
  assert.equal(emu.v('shots_fired', 'u16'), 0, 'steadying');
  emu.hold('A', 1);
  assert.equal(emu.v('shots_fired', 'u16'), 1, 'fires at lock');
  assert.equal(emu.v('crits', 'u16'), 1);
  assert.equal(emu.v('shot_damage'), wpn(emu, 'damage') * 2);
  emu.idle(30);
  assert.equal(emu.v('enemies_killed', 'u16'), 1, 'a slime (3 HP) in one critical');
  // A quick tap (after the cooldown): a normal shot.
  emu.hold('A', 2).idle(1);
  assert.equal(emu.v('shots_fired', 'u16'), 2);
  assert.equal(emu.v('crits', 'u16'), 1, 'no critical');
  assert.equal(emu.v('shot_damage'), wpn(emu, 'damage'));
});

test('CPU budget: an arena fight with each weapon, A held, stays within the frame', () => {
  // The heaviest: the shotgun (5 pellets spawned at once) and the rifle on auto
  // (several shots in flight), ~58-59k at worst; 65,664 is where a frame drops. The
  // sword taps A instead (it swings once per press).
  const names = ['pistol', 'shotgun', 'rifle', 'sniper', 'sword'];
  for (const w of [PISTOL, SHOTGUN, RIFLE, SNIPER, SWORD]) {
    const emu = startRun().idle(2);
    for (let k = 0; k < w; k++) emu.tap('SELECT');
    emu.hold('LEFT', 80).hold('DOWN', 200);
    while (emu.v('player_x', 'u16') < 148) emu.hold('RIGHT', 1);
    emu.idle(10);
    while (emu.v('player_y', 'u16') < 256) emu.hold('DOWN', 1);
    let worst = 0;
    for (let f = 0; f < 600; f++) {
      // The sword swings once per press, so it taps A (a combo every few frames).
      const a = w === SWORD ? (f % 8 === 0 ? '+A' : '') : '+A';
      emu.hold((f % 45 === 0 ? 'LEFT+B' : f % 90 < 45 ? 'LEFT' : 'RIGHT') + a, 1);
      worst = Math.max(worst, emu.lastFrameCycles);
      if (emu.v('next_state') !== emu.v('game_state')) break;
    }
    assert.ok(emu.v('shots_hit', 'u16') + emu.v('melee_hits', 'u16') > 0, `${names[w]}: the hits land`);
    assert.ok(worst < 62000, `${names[w]}: worst frame ${worst} cycles past vblank`);
  }
});

// --- The sword ---
const hp = (emu, i) => emu.read8(addr(emu, 'actor_hp') + i);
const actorX = (emu, i) => emu.read16(addr(emu, 'actor_x') + i * 2);

test('sword: taps chain a 3-hit combo, and it starts over after a pause', () => {
  const emu = holding(SWORD);
  emu.hold('A', 1);
  assert.equal(emu.v('swing_step'), 1);
  emu.idle(8).hold('A', 1);  // during the first swing's recovery
  assert.equal(emu.v('swing_step'), 2);
  emu.idle(8).hold('A', 1);
  assert.equal(emu.v('swing_step'), 3, 'the thrust');
  emu.idle(8).hold('A', 1);
  assert.equal(emu.v('swing_step'), 1, 'after the thrust, from the top');
  emu.idle(40).hold('A', 1);
  assert.equal(emu.v('swing_step'), 1, 'after a pause, from the top');
});

test('sword: the blade sweeps across the aim during a swing', () => {
  const emu = holding(SWORD).hold('A', 1);
  const dirs = [];
  for (let f = 0; f < 8; f++) dirs.push(emu.idle(1).v('gun_dir16'));
  assert.ok(new Set(dirs).size >= 4, `the sprite turns: ${dirs}`);
});

test('sword: a swing hits an enemy in front once, knocks it back, and freezes the game for a moment', () => {
  // Open floor in the lab's top band (rows 1-2), so the knockback has room.
  const emu = holding(SWORD, 136, 10);
  const t = spawnStill(emu, 156, 10);  // just in front, to the right
  emu.idle(24);
  const before = hp(emu, t), x0 = actorX(emu, t);
  let froze = false;
  emu.hold('A', 1);
  for (let f = 0; f < 14; f++) {
    emu.idle(1);
    if (emu.v('hitstop')) froze = true;
  }
  assert.equal(emu.v('melee_hits', 'u16'), 1, 'hit once in the swing');
  assert.equal(hp(emu, t), before - wpn(emu, 'damage'));
  assert.ok(actorX(emu, t) > x0, `knocked back: ${x0} -> ${actorX(emu, t)}`);
  assert.ok(froze, 'hitstop');
});

test('sword: the third hit, a thrust, does double damage', () => {
  const emu = holding(SWORD);
  // Swing twice at the air, then thrust at a slime that steps in (it has 3 HP; the thrust does 4).
  emu.hold('A', 1).idle(8).hold('A', 1).idle(8);
  const t = spawnStill(emu, 72 + 20, 96);
  emu.idle(1).hold('A', 1);
  assert.equal(emu.v('swing_step'), 3);
  emu.idle(16);
  assert.equal(emu.v('enemies_killed', 'u16'), 1, 'one thrust');
  assert.equal(emu.read8(addr(emu, 'actor_type') + t), 0);
});

test('sword: knockback never shoves an enemy into a wall', () => {
  // The pillar at x 192 (column 12) in row 6: an enemy against it, hit toward it.
  const emu = holding(SWORD, 150, 96);
  const t = spawnStill(emu, 174, 96);  // its hitbox ends at 174 + 14 = 188, 4 px from the wall
  emu.idle(24).hold('A', 1).idle(14);
  assert.equal(emu.v('melee_hits', 'u16'), 1);
  const x = actorX(emu, t);
  assert.ok(x + 2 + 12 <= 192, `stays out of the wall: x=${x}`);
});

test('sword: swats enemy bullets back as your own shots', () => {
  const emu = holding(SWORD);
  // An enemy bullet just in front of the player, flying at it (the Bullet record: actor.h).
  const b = addr(emu, 'bullets');
  emu.writeBytes(b, [1, 0x00, 0xff, 0xff, 0, (72 + 18) & 0xff, 0, 0, 0, 0, 0, 96, 0]);  // vx = -1 px/frame
  emu.write8(addr(emu, 'bullet_count'), 1);
  emu.hold('A', 1).idle(6);
  assert.equal(emu.v('deflects', 'u16'), 1);
  assert.equal(emu.read8(b), 0, 'the enemy bullet is gone');
  assert.equal(emu.v('bullet_count'), 0);
  const mine = liveShots(emu);
  assert.equal(mine.length, 1, 'one shot, now the player one');
  assert.ok(mine[0].vx > 0, 'flying back the way the sword swung');
});

test('screen: mid-swing', () => {
  const emu = holding(SWORD, 136, 10);
  spawnStill(emu, 160, 10);
  emu.idle(24).hold('A', 1).idle(3);
  matchGolden(emu, 'sword-swing');
});

test('sword: the blade shows its motion smear while the swing can hit, then the plain blade', () => {
  const emu = holding(SWORD, 136, 10);
  const smear = emu.v('smear_vram'), plain = emu.v('gun_vram');
  // The blade's sprites: OAM 1-2 in front of the body or 5-6 behind (oam.h); the
  // tile of whichever is shown.
  const bladeTile = () => {
    for (const i of [1, 2, 5, 6]) {
      const [y, , tile] = emu.readBytes(0xc000 + i * 4, 3);
      if (y) return tile;
    }
    return -1;
  };
  const isSmear = (t) => t >= smear && t < 256;
  const isPlain = (t) => t >= plain && t < smear;
  assert.ok(isPlain(bladeTile()), 'at rest: the plain blade');
  emu.hold('A', 1);
  const seen = [];
  for (let f = 0; f < 14; f++) {
    emu.idle(1);
    seen.push(isSmear(bladeTile()) ? 'S' : 'p');
  }
  assert.ok(seen.join('').includes('SSS'), `smeared through the active frames: ${seen.join('')}`);
  assert.ok(isPlain(bladeTile()), 'and plain again after');
});

test('screen: the sword smear mid-swing', () => {
  // Nothing to hit, so no hitstop: the frame shown is the swing's.
  matchGolden(holding(SWORD, 136, 10).idle(60).hold('A', 1).idle(4), 'sword-smear');
});

test('weapon art lives in the second VRAM bank (Game Boy Color), leaving bank 0 for the rest', () => {
  const S_BANK = 0x08;
  const emu = holding(PISTOL);
  const vram = emu.snapshot().vram;  // both banks: 0x2000 bytes each
  for (let w = PISTOL; w <= SWORD; w++) {
    if (w) emu.tap('SELECT').idle(2);
    // The weapon's sprites (OAM 1-2 in front of the body, 5-6 behind: oam.h).
    const shown = [1, 2, 5, 6].map((i) => emu.readBytes(0xc000 + i * 4, 4)).filter(([y]) => y);
    assert.ok(shown.length, `weapon ${w} is drawn`);
    for (const [, , , flags] of shown) assert.ok(flags & S_BANK, `weapon ${w}: its sprites read VRAM bank 1`);
  }
  // Bank 1 holds the art from tile 0 (the pistol's first column is drawn).
  const tile = (bank, t) => emu.snapshot().vram.subarray(bank * 0x2000 + t * 16, bank * 0x2000 + t * 16 + 16);
  const any = (bytes) => bytes.some((b) => b);
  assert.ok(Array.from({ length: 8 }, (_, t) => any(tile(1, t))).some(Boolean), 'bank 1 has weapon tiles');
  assert.ok(vram.length >= 0x4000);
  // Bank 0's sprite tiles end after the muzzle flash; everything past is free.
  const used = emu.v('flash_vram') + 4;
  assert.ok(used <= 128, `bank 0 sprite tiles used: ${used}`);
});
