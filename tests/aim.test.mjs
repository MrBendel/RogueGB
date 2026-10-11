// Auto-aim and the player's shots (#5): target choice, stickiness, line of sight,
// the no-target fallback, the 32 directions, hits and kills, and the reticle.
//
// Enemies are placed with the debug spawn command (play.c: debug_cmd 2) and held
// still by raising their timers, so each test controls exactly what's around.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const SLIME = 1;
const AIM_NONE = 0xff;
const DIR_UP = 0, DIR_RIGHT = 2, DIR_DOWN = 4, DIR_LEFT = 6;
const SHOT_SIZE = 13;  // Bullet record: alive 0, vx 1-2, x 5-6, vy 7-8, y 11-12
const RETICLE_OAM = 4;

const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v, i = 0) => emu.write8(addr(emu, name) + i, v);
const w16 = (emu, name, v, i = 0) => emu.writeBytes(addr(emu, name) + i * 2, [v & 0xff, (v >> 8) & 0xff]);
const s16 = (v) => (v > 32767 ? v - 65536 : v);
const shot = (emu, i) => {
  const a = addr(emu, 'shots') + i * SHOT_SIZE;
  return { alive: emu.read8(a), vx: s16(emu.read16(a + 1)), x: emu.read16(a + 5), vy: s16(emu.read16(a + 7)), y: emu.read16(a + 11) };
};

/** Put the player at (x, y) facing `facing`, standing still. */
function place(emu, x, y, facing = DIR_DOWN) {
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y);
  w16(emu, 'player_vx', 0);
  w16(emu, 'player_vy', 0);
  w8(emu, 'player_facing', facing);
  return emu;
}

/** Spawn an enemy with its frame's top-left at (x, y); it stays put (long rest). Returns its slot. */
function spawn(emu, x, y, { type = SLIME, still = true } = {}) {
  w8(emu, 'debug_type', type);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  if (still) w8(emu, 'actor_timer', 254, slot);
  return slot;
}

/** Wait until every enemy has been scored again (one slot every other frame). */
const rescore = (emu) => emu.idle(24);

// The lab's top-left screen around the start: open floor in row 6 (y 96-111)
// from column 1 to 11, a wall block at columns 4-5 of row 4 (y 64-79).
const PX = 72, PY = 96;

test('no target: A fires straight ahead, and the shot stops at a wall', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(2);
  assert.equal(emu.v('aim_target'), AIM_NONE);
  emu.hold('A', 1);
  assert.equal(emu.v('shots_fired', 'u16'), 1);
  assert.equal(emu.v('aim_dir'), 0, 'right');
  const s = shot(emu, 0);
  assert.ok(s.alive > 0);
  assert.equal(s.vx, 768, '3 px/frame');
  assert.equal(s.vy, 0);
  // Facing up there's a wall 2 blocks away: the shot is gone well before its range.
  const up = place(startRun().idle(2), PX, PY, DIR_UP).idle(2).hold('A', 1);
  assert.equal(up.v('aim_dir'), 24, 'up');
  assert.equal(up.v('shot_count'), 1);
  up.idle(14);
  assert.equal(up.v('shot_count'), 0, 'stopped by the wall');
  assert.equal(up.v('shots_hit', 'u16'), 0);
});

test('holding A fires every fire_interval frames', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(2);
  const interval = emu.v('fire_interval');
  emu.hold('A', interval * 3);
  assert.equal(emu.v('shots_fired', 'u16'), 3);
});

test('the target is the nearest enemy', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_DOWN).idle(2);
  const right = spawn(emu, PX + 40, PY);
  spawn(emu, PX - 56, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), right);
});

test('enemies in front of the player count as nearer (facing bias)', () => {
  // Same layout, facing left: the left one (57 - aim_bias 24 = 33) scores better
  // than the nearer one behind (41), and is the target when it comes first.
  const emu = place(startRun().idle(2), PX, PY, DIR_LEFT).idle(2);
  const left = spawn(emu, PX - 56, PY);
  const right = spawn(emu, PX + 40, PY);
  rescore(emu);
  const score = (i) => emu.read8(addr(emu, 'aim_score') + i);
  assert.equal(score(right), 41);
  assert.equal(score(left), 33);
  assert.equal(emu.v('aim_target'), left);
  // Facing right instead, the right one gets the bias: 41 - 24 = 17.
  w8(emu, 'player_facing', DIR_RIGHT);
  rescore(emu);
  assert.equal(score(right), 17);
  assert.equal(score(left), 57);
  assert.equal(emu.v('aim_target'), right, '57 vs 17: clearly better, so it takes over');
});

test('targeting is sticky: another enemy must be clearly nearer to take over', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_DOWN).idle(2);
  const a = spawn(emu, PX + 40, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), a);
  // 8 px nearer than the target: not enough (aim_sticky = 16).
  const b = spawn(emu, PX - 32, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), a, 'keeps the target');
  // 20 px nearer: takes over.
  w16(emu, 'actor_x', PX - 20, b);
  rescore(emu);
  assert.equal(emu.v('aim_target'), b, 'switches');
});

test('an enemy behind a wall is not targeted', () => {
  // Above the player, behind the wall block in row 4: nearer (61 px) but hidden.
  // To the right: farther (70 px) but in the open.
  const emu = place(startRun().idle(2), PX, PY, DIR_DOWN).idle(2);
  const hidden = spawn(emu, PX, 32);
  const open = spawn(emu, PX + 70, PY);
  rescore(emu);
  assert.equal(emu.read8(addr(emu, 'aim_score') + hidden), AIM_NONE, 'hidden: no score');
  assert.equal(emu.v('aim_target'), open);
});

test('enemies out of range are not targeted', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(2);
  const range = emu.v('aim_range');
  spawn(emu, PX + range + 16, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), AIM_NONE);
});

test('shots fly at the target in one of 32 directions', () => {
  // Open floor in the top-right screen. The target's hitbox middle is 40 right
  // and 16 down of the shot's start: 21.8 degrees, nearest step 2 (22.5).
  const emu = place(startRun().idle(2), 136, 24, DIR_DOWN).idle(2);
  const t = spawn(emu, 176, 37);
  rescore(emu);
  assert.equal(emu.v('aim_target'), t);
  emu.hold('A', 1);
  assert.equal(emu.v('aim_dir'), 2);
  const s = shot(emu, 0);
  assert.equal(s.vx, 711, '3 px * cos 22.5');
  assert.equal(s.vy, 294, '3 px * sin 22.5');
});

test('shots hit and kill the target, then the reticle goes away', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(2);
  const t = spawn(emu, PX + 48, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), t);
  const hp = emu.read8(addr(emu, 'actor_hp') + t);
  for (let f = 0; f < 200 && emu.v('enemies_killed', 'u16') === 0; f++) emu.hold('A', 1);
  assert.equal(emu.v('enemies_killed', 'u16'), 1, 'killed');
  assert.ok(emu.v('shots_hit', 'u16') >= hp, `${emu.v('shots_hit', 'u16')} hits for ${hp} HP`);
  assert.equal(emu.read8(addr(emu, 'actor_type') + t), 0, 'slot freed');
  assert.equal(emu.v('aim_target'), AIM_NONE);
  emu.idle(1);
  assert.equal(emu.read8(0xc000 + RETICLE_OAM * 4), 0, 'reticle hidden');
});

test('a hit flashes the enemy', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(2);
  const t = spawn(emu, PX + 32, PY);
  rescore(emu);
  const hits = () => emu.v('shots_hit', 'u16');
  for (let f = 0; f < 60 && hits() === 0; f++) emu.hold('A', 1);
  assert.equal(hits(), 1);
  emu.idle(2);
  assert.ok(emu.read8(addr(emu, 'actor_hurt') + t) > 0 || emu.read8(addr(emu, 'actor_flash') + t) === 1, 'flashing');
});

test('the reticle frames the target', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(40);  // let the camera settle
  const t = spawn(emu, PX + 40, PY);
  rescore(emu);
  assert.equal(emu.v('aim_target'), t);
  const ex = emu.read16(addr(emu, 'actor_x') + t * 2), ey = emu.read16(addr(emu, 'actor_y') + t * 2);
  const hx = ex - emu.v('cam_x', 'u16') + 8, hy = ey - emu.v('cam_y', 'u16') + 16;
  const oam = 0xc000 + RETICLE_OAM * 4;
  assert.equal(emu.read8(oam), hy, 'left half y');
  assert.equal(emu.read8(oam + 1), hx, 'left half x');
  assert.equal(emu.read8(oam + 4), hy, 'right half y');
  assert.equal(emu.read8(oam + 5), hx + 8, 'right half x');
});

test('you can back away from a chasing slime while hitting it', () => {
  // Row 1 of the lab is open from wall to wall; the slime starts 64 px to the
  // right and hops after you.
  const emu = place(startRun().idle(2), 136, 10, DIR_LEFT).idle(2);
  spawn(emu, 200, 10, { still: false });
  const x0 = emu.v('player_x', 'u16');
  emu.hold('LEFT+A', 90);
  assert.ok(emu.v('player_x', 'u16') < x0 - 60, 'backed away');
  assert.ok(emu.v('shots_hit', 'u16') >= 2, `${emu.v('shots_hit', 'u16')} hits while backing away`);
});

test('screen: shooting at a target', () => {
  const emu = place(startRun().idle(2), PX, PY, DIR_RIGHT).idle(40);
  spawn(emu, PX + 64, PY);
  rescore(emu);
  matchGolden(emu.hold('A', 15), 'aim-shooting');
});
