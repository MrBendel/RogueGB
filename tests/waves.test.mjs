// The arena's waves (waves.c) and the run-over screen's cause of death.
import { test } from 'node:test';
import { assert, matchGolden, startRun, settle, RUN_OVER } from './helpers.mjs';

const ACTOR_MAX = 8, BULLET_MAX = 20, BULLET_SIZE = 13;
const SLIME = 1, PEASHOOTER = 2;
const WAVE_BREAK = 240;
const CAUSE_SLIME = 1;

const addr = (emu, n) => emu.symbols.addr(n);
const w8 = (emu, n, v, i = 0) => emu.write8(addr(emu, n) + i, v);
const w16 = (emu, n, v) => emu.writeBytes(addr(emu, n), [v & 255, v >> 8]);
const types = (emu) => Array.from({ length: ACTOR_MAX }, (_, i) => emu.read8(addr(emu, 'actor_type') + i)).filter(Boolean);
const actorAt = (emu, i) => ({ x: emu.read16(addr(emu, 'actor_x') + i * 2), y: emu.read16(addr(emu, 'actor_y') + i * 2) });

function teleport(emu, x, y) {
  w16(emu, 'player_x', x);
  w16(emu, 'player_y', y);
  w16(emu, 'player_vx', 0);
  w16(emu, 'player_vy', 0);
  return emu.idle(2);
}
/** Remove every enemy and bullet (as if the wave were beaten). */
function clearWave(emu) {
  for (let i = 0; i < ACTOR_MAX; i++) w8(emu, 'actor_type', 0, i);
  w8(emu, 'actor_count', 0);
  for (let i = 0; i < BULLET_MAX; i++) emu.write8(addr(emu, 'bullets') + i * BULLET_SIZE, 0);
  return emu.idle(2);
}
/** In the arena, wave 1 spawned. */
function wave1() {
  const emu = teleport(startRun().idle(2), 9 * 16, 17 * 16);
  assert.equal(emu.v('wave_num'), 1);
  assert.equal(types(emu).length, 5);
  return emu;
}

test('clearing a wave starts a break, then a bigger wave with more peashooters', () => {
  const emu = clearWave(wave1());
  assert.equal(emu.v('waves_cleared'), 1);
  assert.equal(emu.v('wave_live'), 0);
  emu.idle(WAVE_BREAK - 10);
  assert.equal(types(emu).length, 0, 'still the break');
  emu.idle(12);
  assert.equal(emu.v('wave_num'), 2);
  const t = types(emu);
  assert.equal(t.length, 6, 'wave 2: 6 enemies');
  assert.equal(t.filter((x) => x === PEASHOOTER).length, 2);
  // Wave 4 and later fill every slot.
  for (let n = 3; n <= 4; n++) {
    clearWave(emu).idle(WAVE_BREAK + 2);
    assert.equal(emu.v('wave_num'), n);
  }
  assert.equal(types(emu).length, ACTOR_MAX);
});

test('new waves spawn away from the player', () => {
  const emu = clearWave(wave1());
  for (let n = 2; n <= 5; n++) {
    emu.idle(WAVE_BREAK + 2);
    const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
    for (let i = 0; i < ACTOR_MAX; i++) {
      if (!emu.read8(addr(emu, 'actor_type') + i)) continue;
      const a = actorAt(emu, i);
      assert.ok(Math.abs(a.x - px) >= 40 || Math.abs(a.y - py) >= 40, `wave ${n}: enemy ${i} at ${a.x},${a.y} too close to ${px},${py}`);
    }
    clearWave(emu);
  }
});

test('the next wave waits while you are back in the lab', () => {
  const emu = clearWave(wave1());
  teleport(emu, 72, 96);
  emu.idle(WAVE_BREAK + 60);
  assert.equal(emu.v('wave_num'), 1, 'held');
  teleport(emu, 9 * 16, 17 * 16);
  emu.idle(WAVE_BREAK + 2);
  assert.equal(emu.v('wave_num'), 2);
});

test('the run-over screen names what killed you, with kills and waves', () => {
  const emu = clearWave(wave1());
  // A last hit from a slime: health down to 1, then a slime right on the player.
  w8(emu, 'player_hp', 1);
  w8(emu, 'debug_type', SLIME);
  w16(emu, 'debug_x', emu.v('player_x', 'u16'));
  w16(emu, 'debug_y', emu.v('player_y', 'u16'));
  w8(emu, 'debug_cmd', 2);
  emu.until((e) => e.v('next_state') === RUN_OVER, 120, 'the run to end');
  assert.equal(emu.v('death_cause'), CAUSE_SLIME);
  settle(emu, RUN_OVER, 'run over');
  matchGolden(emu, 'run-over-killed');
});
