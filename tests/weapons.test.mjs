// Weapons (weapon.c, firing in aim.c): SELECT switches between the pistol,
// shotgun, minigun and sniper rifle, and each fires its own way (ROADMAP
// "Weapons").
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const PISTOL = 0, SHOTGUN = 1, MINIGUN = 2, SNIPER = 3;
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
  for (const w of [SHOTGUN, MINIGUN, SNIPER, PISTOL]) {
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

test('minigun: spins up before it fires, then fires fast, and you walk at half speed', () => {
  const emu = holding(MINIGUN);
  const windup = wpn(emu, 'windup');
  emu.hold('A', windup - 1);
  assert.equal(emu.v('shots_fired', 'u16'), 0, 'nothing while spinning up');
  emu.hold('A', 31);
  const fired = emu.v('shots_fired', 'u16');
  assert.ok(fired >= 9, `${fired} shots in about half a second after spinning up`);
  // Walking while firing: half the top speed (row 6 of the lab is open to the right).
  const top = emu.v('move_top', 'u16');
  emu.hold('RIGHT+A', 20);
  assert.equal(emu.v('player_vx', 'i16'), top >> 1);
  emu.hold('RIGHT', 20);
  assert.equal(emu.v('player_vx', 'i16'), top, 'full speed again once A is let go');
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
  // The heaviest: the shotgun (5 pellets spawned at once) and the minigun (up to
  // ~9 shots in flight), ~58-59k at worst; 65,664 is where a frame drops.
  const names = ['pistol', 'shotgun', 'minigun', 'sniper'];
  for (const w of [PISTOL, SHOTGUN, MINIGUN, SNIPER]) {
    const emu = startRun().idle(2);
    for (let k = 0; k < w; k++) emu.tap('SELECT');
    emu.hold('LEFT', 80).hold('DOWN', 200);
    while (emu.v('player_x', 'u16') < 148) emu.hold('RIGHT', 1);
    emu.idle(10);
    while (emu.v('player_y', 'u16') < 256) emu.hold('DOWN', 1);
    let worst = 0;
    for (let f = 0; f < 600; f++) {
      emu.hold((f % 45 === 0 ? 'LEFT+B' : f % 90 < 45 ? 'LEFT' : 'RIGHT') + '+A', 1);
      worst = Math.max(worst, emu.lastFrameCycles);
      if (emu.v('next_state') !== emu.v('game_state')) break;
    }
    assert.ok(emu.v('shots_hit', 'u16') > 0, `${names[w]}: the shots land`);
    assert.ok(worst < 62000, `${names[w]}: worst frame ${worst} cycles past vblank`);
  }
});
