import { test } from 'node:test';
import { assert, bootToTitle, settle, PLAY, FRAME_LIMIT } from './helpers.mjs';

const at = (e, n) => e.symbols.addr(n);
const put = (e, n, v) => e.write8(at(e, n), v);
const put16 = (e, n, v) => e.writeBytes(at(e, n), [v & 255, (v >> 8) & 255]);

function trial() {
  const e = bootToTitle().tap('DOWN').tap('DOWN').tap('A').tap('UP').idle(9);
  assert.equal(e.v('title_choice'), 3);
  e.shot('flynt-menu');
  return settle(e.tap('A'), PLAY).idle(3);
}

function shootBoss(e) {
  const bx = e.v('actor_x', 'u16') + 4;
  const by = e.v('actor_y', 'u16') + 4;
  e.writeBytes(at(e, 'shots'), [12, 0, 0, 0, 0, bx, 0, 0, 0, 0, 0, by, 0]);
  put(e, 'shot_count', 1);
  put(e, 'shot_damage', 1);
  e.idle(3);
}

test('flynt: launches with 36 HP, stalk phase, guard=0, and clean sprite allocations', () => {
  const e = trial();
  assert.equal(e.v('boss_kind'), 10);
  assert.equal(e.v('boss_hp'), 36);
  assert.equal(e.v('flynt_state'), 0); // FLYNT_STALK
  assert.equal(e.v('actor_guard'), 0); // initially vulnerable
  assert.equal(e.v('play_actor_oam'), 15);

  // Player sprites (3-4) and gun/reticle intact
  assert.ok(e.read8(at(e, 'shadow_OAM') + 3 * 4));
  assert.ok(e.read8(at(e, 'shadow_OAM') + 4 * 4));
  e.shot('flynt-stalk');
});

test('flynt: telegraphs with windup flash and fires a 3-pellet incendiary fan', () => {
  const e = trial();
  // Wait until Flynt enters aim/windup state (state 1)
  e.until(x => x.v('flynt_state') === 1, 60);
  assert.equal(e.v('flynt_state'), 1); // FLYNT_AIM
  e.shot('flynt-cleave-aim');

  const bulletsBefore = e.v('bullets_fired', 'u16');
  // Wait for cleave execution (state 2)
  e.until(x => x.v('flynt_state') === 2, 40);
  const bulletsFired = e.v('bullets_fired', 'u16') - bulletsBefore;
  assert.ok(bulletsFired >= 3, 'cleave fired at least 3 incendiary pellets');
  e.shot('flynt-cleave-fire');
});

test('flynt furnace: grates cycle steam warning and active erupting fire', () => {
  const e = trial();
  // Initially vents are cool (cycle 0)
  assert.equal(e.v('flynt_vent_cycle'), 0);

  // Verify floor tiles are randomized: room_vis contains multiple floor variants
  const visAddr = at(e, 'room_vis');
  let hasVaried = false;
  for (let y = 1; y < 7; y++) {
    for (let x = 1; x < 9; x++) {
      const v = e.read8(visAddr + (y << 5) + x);
      if (v >= 7 && v <= 11) hasVaried = true;
    }
  }
  assert.ok(hasVaried, 'arena floor contains randomized visual floor block variants');

  // Advance to cycle 1 (spike tip warning)
  e.until(x => x.v('flynt_vent_cycle') === 1, 90);
  assert.equal(e.v('flynt_vent_cycle'), 1);
  e.shot('flynt-vent-steam-warn');

  // Advance to cycle 2 (erupting spikes)
  e.until(x => x.v('flynt_vent_cycle') === 2, 40);
  assert.equal(e.v('flynt_vent_cycle'), 2);
  e.idle(10); // allow full spike extension to stage 3 and redraw
  e.shot('flynt-vent-flame-erupt');
});

test('flynt furnace: grounded player on active flame takes damage, jumping safely clears it', () => {
  const e = trial();
  // Place player over top-left grate: (tx=4, ty=4) -> x=32, y=32.
  // Player hitbox: feet at x+3..13, y+8..16
  put16(e, 'player_x', 32);
  put16(e, 'player_y', 28);
  put(e, 'player_hp', 60);
  put(e, 'shield_hp', 40);

  // Set grate 0 to erupting spikes (stage 2)
  put(e, 'vent_stage', 2);
  e.idle(3);
  assert.ok(e.v('shield_hp') < 40 || e.v('player_hp') < 60, 'standing on extending spikes hurts');
  assert.equal(e.v('hurt_cause'), 15);

  // Set grate 0 to fully extended razor spikes (stage 3)
  put(e, 'vent_stage', 3);
  put(e, 'shield_hp', 40);
  put(e, 'player_hp', 60);
  put(e, 'player_invuln', 0);
  put(e, 'player_blink', 0);
  e.idle(3);
  assert.ok(e.v('shield_hp') < 40 || e.v('player_hp') < 60, 'standing on fully extended spikes hurts');
  assert.equal(e.v('hurt_cause'), 15);

  // Now jump (player_z > 0): jump safely over spikes with no damage!
  put(e, 'shield_hp', 40);
  put(e, 'player_hp', 60);
  put(e, 'player_invuln', 0);
  put(e, 'player_blink', 0);
  put16(e, 'player_z', 8 << 8); // in mid-air
  e.idle(5);
  assert.equal(e.v('shield_hp'), 40, 'jumping avoids active spike damage');
  assert.equal(e.v('player_hp'), 60);
});

test('flynt shield: dropping to 24 HP activates Flame Shield with actor_guard=1', () => {
  const e = trial();
  // Reduce HP down to 24
  put(e, 'actor_hp', 24);
  e.idle(2);

  assert.equal(e.v('flynt_phase'), 1);
  assert.equal(e.v('flynt_state'), 4); // FLYNT_CLOAK
  assert.equal(e.v('actor_guard'), 1); // Flame Shield active!
  e.shot('flynt-flame-shield');

  // Shooting shielded Flynt deals zero damage
  shootBoss(e);
  assert.equal(e.v('actor_hp'), 24, 'shots deflect off active Flame Shield');
});

test('flynt leap slam: high altitude leap with shadow, landing shockwave', () => {
  const e = trial();
  put(e, 'actor_hp', 24);
  e.idle(2); // triggers cloak

  // Fast forward to leap trigger (timer = 60 in cloak)
  put(e, 'flynt_timer', 61);
  e.idle(2);
  assert.equal(e.v('flynt_state'), 5); // FLYNT_LEAP

  // In mid leap, Flynt is airborne with shadow in OAM_PROP
  e.idle(10);
  assert.ok(e.v('flynt_z') > 0, 'Flynt is airborne');
  assert.ok(e.read8(at(e, 'shadow_OAM') + 11 * 4), 'shadow drawn in OAM_PROP');
  e.shot('flynt-airborne-leap');

  // Lands and triggers shockwave
  e.until(x => x.v('flynt_state') === 6, 25);
  assert.equal(e.v('flynt_state'), 6); // FLYNT_SLAM
  assert.equal(e.v('flynt_slams', 'u16'), 1);
  e.shot('flynt-slam-landing');
});

test('flynt overheat: steam vents blow, drops to stunned state with actor_guard=0 punish window', () => {
  const e = trial();
  put(e, 'actor_hp', 24);
  e.idle(2);

  // Fast forward to cloak expiry
  put(e, 'flynt_state', 4);
  put(e, 'flynt_timer', 1);
  e.idle(2);

  // Overheated and stunned!
  assert.equal(e.v('flynt_state'), 7); // FLYNT_STUNNED
  assert.equal(e.v('actor_guard'), 0); // Guard dropped!
  assert.equal(e.v('flynt_stuns', 'u16'), 1);
  e.shot('flynt-overheated-stun');

  // Punish window allows player weapon damage
  shootBoss(e);
  assert.equal(e.v('actor_hp'), 23, 'punish window takes weapon damage');
});

test('flynt desperation: below 12 HP fires 5-pellet cleave fan and spawns second-wind add', () => {
  const e = trial();
  put(e, 'actor_hp', 12);
  e.idle(2);
  assert.equal(e.v('flynt_phase'), 2);

  // Trigger cleave
  put(e, 'flynt_state', 1);
  put(e, 'flynt_timer', 1);
  const firedBefore = e.v('bullets_fired', 'u16');
  e.idle(2);
  assert.equal(e.v('flynt_state'), 2); // FLYNT_CLEAVE
  assert.ok(e.v('bullets_fired', 'u16') - firedBefore >= 5, 'fires 5-pellet fan in phase 2');

  // Second-wind minion add
  put(e, 'flynt_minion_timer', 1);
  e.idle(2);
  assert.ok(e.v('actor_count') >= 2, 'second-wind add spawned');
  e.shot('flynt-desperation-add');
});

test('flynt victory: reducing HP to 0 clears actors, bullets, and awards salvage token', () => {
  const e = trial();
  put(e, 'actor_hp', 1);
  e.idle(2);

  put(e, 'actor_hp', 0);
  put(e, 'shots_killed', 1);
  e.idle(4);

  assert.equal(e.v('boss_state'), 5); // BOSS_WON
  assert.equal(e.v('boss_reward'), 1); // Salvage token awarded!
  assert.equal(e.v('actor_count'), 0); // Actors cleaned up
  assert.equal(e.v('bullet_count'), 0); // Hostile bullets removed
  e.shot('flynt-victory');
});
