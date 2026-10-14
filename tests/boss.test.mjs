import { test } from 'node:test';
import { assert, bootToTitle, settle, PLAY, RUN_OVER, FRAME_LIMIT, startRun } from './helpers.mjs';

const RECOVER = 0, WARN = 1, CHARGE = 2, RELAYS = 3, REINFORCE = 4, WON = 5;
const at = (e, n) => e.symbols.addr(n);
const w8 = (e, n, v, i = 0) => e.write8(at(e, n) + i, v);
const w16 = (e, n, v, i = 0) => e.writeBytes(at(e, n) + i * 2, [v & 255, v >> 8]);
const r8 = (e, n, i = 0) => e.read8(at(e, n) + i);

function trial(kind = 1) {
  const e = bootToTitle();
  e.tap('DOWN');
  if (kind === 2) e.tap('DOWN');
  e.tap('A');
  settle(e.tap('START'), PLAY);
  assert.equal(e.v('boss_kind'), kind);
  return e.idle(3); // present the first actor frame and queued encounter header
}
function keepAlive(e) {
  w8(e, 'player_hp', 60);
  w8(e, 'shield_hp', 40);
}
function removeRelay(e, slot) {
  w8(e, 'actor_hp', 0, slot);
  w8(e, 'shots_killed', 1);
  e.idle(2);
}
function shotAtBoss(e) {
  const i = e.v('boss_slot');
  const x = e.read16(at(e, 'actor_x') + i * 2) + 4;
  const y = e.read16(at(e, 'actor_y') + i * 2);
  // A real live shot record already overlapping the hitbox, with zero velocity.
  // This isolates collision protection from auto-aim choosing a different target.
  e.writeBytes(at(e, 'shots'), [12,0,0,0,0,x & 255,x >> 8,0,0,0,0,y & 255,y >> 8]);
  w8(e, 'shot_count', 1); w8(e, 'shot_damage', 1);
  e.idle(3);
}

test('boss: projectile collision ignores closed armor and damages the exposed core', () => {
  const e = trial(2), i = e.v('boss_slot');
  shotAtBoss(e);
  assert.equal(r8(e, 'actor_hp', i), 48);
  assert.equal(e.v('shots_hit', 'u16'), 0);
  w8(e, 'shots', 0); w8(e, 'shot_count', 0);
  removeRelay(e, 1); removeRelay(e, 2);
  shotAtBoss(e);
  assert.equal(r8(e, 'actor_hp', i), 47);
  assert.equal(e.v('shots_hit', 'u16'), 1);
});

test('boss: title trials are selectable and ordinary lab has no encounter state', () => {
  const e = trial(2);
  assert.equal(e.v('play_mode'), 3);
  assert.equal(e.v('boss_state'), RELAYS);
  assert.equal(e.v('boss_relays'), 2);
  assert.equal(e.v('room_w'), 10);
  assert.equal(e.v('room_h'), 8);
  assert.equal(e.v('shield_hp'), 40);
  e.shot('boss-warden-relays');
  const lab = startRun();
  assert.equal(lab.v('boss_kind'), 0);
  assert.equal(lab.v('play_mode'), 0);
});

test('boss: Claimjaw warns, locks its direction, respects walls and opens a damage window', () => {
  const e = trial();
  const i = e.v('boss_slot');
  e.until(x => x.v('boss_state') === WARN, 130);
  const dx = e.v('boss_dx', 'i8'), dy = e.v('boss_dy', 'i8');
  assert.equal(dx, 0); assert.equal(dy, 1);
  e.idle(2).shot('boss-claimjaw-warning');
  e.hold('RIGHT', 30);
  assert.equal(e.v('boss_dx', 'i8'), dx, 'warning does not retarget a dodging player');
  assert.equal(e.v('boss_dy', 'i8'), dy);
  e.idle().until(x => x.v('boss_state') === CHARGE, 50);
  for (let n = 0; n < 80 && e.v('boss_state') === CHARGE; n++) {
    e.idle();
    assert.ok(e.read16(at(e, 'actor_y') + i * 2) <= 96, 'charge stays inside the wall');
  }
  assert.equal(e.v('boss_state'), RECOVER);
  assert.equal(r8(e, 'actor_guard', i), 0);
  assert.equal(e.v('player_hp'), 60, 'sidestepping the locked lane avoids damage');
  assert.equal(e.v('shield_hp'), 40);
  assert.ok(e.v('boss_timer') >= 95, 'long recovery rewards the dodge');
});

test('boss: protected targets are excluded from auto-aim and immune to bullets and melee', () => {
  const e = trial(2), i = e.v('boss_slot');
  // Directly below the body; the relays sit outside this firing lane.
  w16(e, 'player_x', 72); w16(e, 'player_y', 60);
  w8(e, 'player_facing', 0);
  for (let f = 0; f < 110; f++) {
    w8(e, 'actor_hp', 200, 1); w8(e, 'actor_hp', 200, 2);
    keepAlive(e); e.hold('A', 1);
    assert.notEqual(e.v('aim_target'), i, 'protected boss must not steal targeting');
  }
  assert.equal(r8(e, 'actor_hp', i), 48);
  for (let n = 0; n < 4; n++) e.tap('SELECT');
  assert.equal(e.v('weapon_cur'), 4);
  w16(e, 'player_x', 72); w16(e, 'player_y', 51);
  w8(e, 'player_facing', 0);
  for (let n = 0; n < 4; n++) {
    w8(e, 'actor_hp', 200, 1); w8(e, 'actor_hp', 200, 2);
    keepAlive(e); e.tap('A').idle(17);
  }
  assert.equal(r8(e, 'actor_hp', i), 48, 'sword cannot bypass relay protection');
});

test('boss: relays unlock Warden, half health adds happen once, victory clears danger', () => {
  const e = trial(2), i = e.v('boss_slot');
  removeRelay(e, 1);
  assert.equal(e.v('boss_relays'), 1);
  assert.equal(r8(e, 'actor_guard', i), 1);
  removeRelay(e, 2);
  assert.equal(e.v('boss_state'), RECOVER);
  assert.equal(r8(e, 'actor_guard', i), 0);
  assert.equal(e.v('bullets_active'), 0);
  w8(e, 'actor_hp', 24, i);
  e.idle(1);
  assert.equal(e.v('boss_state'), REINFORCE);
  assert.equal(e.v('actors_active'), 1, 'adds do not appear without warning');
  e.idle(2).shot('boss-warden-backup');
  e.idle(61);
  assert.equal(e.v('boss_phase'), 1);
  assert.equal(e.v('actors_active'), 3);
  assert.equal(e.v('boss_state'), RECOVER);
  e.idle(3).shot('boss-warden-adds');
  for (let n = 0; n < 400; n++) {
    keepAlive(e); e.idle();
    assert.notEqual(e.v('boss_state'), REINFORCE, 'threshold is one-shot');
    assert.ok(e.v('actors_active') <= 3);
    assert.ok(e.v('bullets_active') <= 20);
  }
  w8(e, 'actor_hp', 0, i); // emulate the last melee hit, before reaping
  e.idle(2);
  assert.equal(e.v('boss_state'), WON);
  assert.equal(e.v('boss_reward'), 1);
  assert.equal(e.v('actors_active'), 0);
  assert.equal(e.v('bullets_active'), 0);
  assert.equal(e.v('shot_count'), 0);
  e.idle(120);
  assert.equal(e.v('boss_reward'), 1, 'reward issued only once');
  for (let n = 15; n < 40; n++)
    assert.equal(e.read8(at(e, 'shadow_OAM') + n * 4), 0, 'victory hides old enemies/projectiles');
  e.shot('boss-victory');
});

test('boss: pause freezes warning and player death still reaches run over', () => {
  const e = trial();
  e.idle().until(x => x.v('boss_state') === WARN, 130);
  e.tap('START');
  const t = e.v('boss_timer');
  e.idle(90);
  assert.equal(e.v('boss_timer'), t);
  e.tap('START').idle(2);
  assert.ok(e.v('boss_timer') < t);
  w8(e, 'player_hp', 0);
  e.idle();
  settle(e, RUN_OVER);
  // Returning through the title resets all encounter state on another attempt.
  e.idle(35).tap('START');
  settle(e, 0);
  e.tap('DOWN').tap('A');
  settle(e.tap('START'), PLAY);
  assert.equal(e.v('boss_reward'), 0);
  assert.equal(e.v('boss_hp'), 18);
});

test('boss: actual pistol fire completes both trials within bounded pools and frame budget', () => {
  for (const kind of [1, 2]) {
    const e = trial(kind);
    let worst = 0, maxActors = 0, maxBullets = 0;
    const waypoints = [[120,96], [120,24], [24,24], [24,96]];
    let waypoint = 0;
    // Real inputs only: a simple circling route, starter pistol, normal health
    // and shield. No damage/HP injection in this end-to-end completion check.
    for (let f = 0; f < 4000 && e.v('boss_state') !== WON; f++) {
      const [x,y] = waypoints[waypoint];
      const dx = x - e.v('player_x', 'u16'), dy = y - e.v('player_y', 'u16');
      if (Math.abs(dx) < 4 && Math.abs(dy) < 4) waypoint = (waypoint + 1) % waypoints.length;
      const buttons = ['A'];
      if (Math.abs(dx) >= 4) buttons.push(dx > 0 ? 'RIGHT' : 'LEFT');
      if (Math.abs(dy) >= 4) buttons.push(dy > 0 ? 'DOWN' : 'UP');
      e.hold(buttons, 1);
      assert.ok(e.v('player_hp') > 0, `trial ${kind}: patrol survived with ordinary resources`);
      worst = Math.max(worst, e.lastFrameCycles);
      maxActors = Math.max(maxActors, e.v('actors_active'));
      maxBullets = Math.max(maxBullets, e.v('bullets_active'));
      assert.ok(e.v('sprites_used') <= 40);
    }
    assert.equal(e.v('boss_state'), WON, `trial ${kind} is beatable with the starter pistol`);
    assert.ok(e.v('shots_hit', 'u16') > 0);
    assert.ok(worst < FRAME_LIMIT, `${worst} exceeds ${FRAME_LIMIT}`);
    console.log(`boss trial ${kind}: worst=${worst}/${FRAME_LIMIT}, actors=${maxActors}, bullets=${maxBullets}`);
  }
});

test('boss: Warden transition tolerates full actor and bullet pools while firing a shotgun', () => {
  const e = trial(2), i = e.v('boss_slot');
  removeRelay(e, 1); removeRelay(e, 2);
  e.tap('SELECT');
  w8(e, 'actor_hp', 24, i);
  e.idle(); // start reinforcement warning
  w8(e, 'debug_cmd', 1);
  e.idle(); // fill pools before the reinforcement timer expires
  assert.equal(e.v('actors_active'), 8);
  // The lab stress grid extends beyond this smaller arena. Put all twenty
  // projectiles on floor so the trial really exercises a full bullet pool.
  for (let n = 0; n < 20; n++) {
    const x = 24 + (n % 5) * 24, y = 24 + Math.floor(n / 5) * 20;
    e.writeBytes(at(e, 'bullets') + n * 13, [1,64,0,0,0,x,0,0,0,0,0,y,0]);
  }
  w8(e, 'bullet_count', 20);
  e.idle();
  assert.equal(e.v('bullets_active'), 20);
  let worst = 0;
  for (let n = 0; n < 240; n++) {
    keepAlive(e); e.hold('A', 1);
    worst = Math.max(worst, e.lastFrameCycles);
    assert.ok(e.v('actors_active') <= 8);
    assert.ok(e.v('bullets_active') <= 20);
    assert.ok(e.v('sprites_used') <= 40);
  }
  assert.ok(worst < FRAME_LIMIT, `stress frame ${worst}/${FRAME_LIMIT}`);
  console.log(`boss stress: worst=${worst}/${FRAME_LIMIT}`);
});


test('boss: Warden defeats stationary held fire with ordinary resources', () => {
  for (const pos of [null,[24,24],[120,24],[24,96],[120,96]]) {
  const e=trial(2);
  if(pos){w16(e,'player_x',pos[0]);w16(e,'player_y',pos[1]);}
  let frames=0;
  for (; frames<6000 && e.v('player_hp') && e.v('boss_state')!==WON; frames++) e.hold('A');
  assert.notEqual(e.v('boss_state'),WON,'standing still must not win');
  assert.equal(e.v('player_hp'),0,'locked floor claims punish camping');
  console.log(`Warden camping ${pos??"start"} defeated after ${frames} frames`);
  }
});

test('boss: Warden floor claim locks, warns, freezes on pause and can be escaped', () => {
  const e=trial(2);
  e.until(x=>x.v('warden_zone_state')===1);
  const x=e.v('warden_zone_x'),y=e.v('warden_zone_y');
  e.idle(2).shot('warden-floor-warning');
  assert.equal(e.v('shield_hp'),40,'outline is harmless');
  e.tap('START');
  const timer=e.v('warden_zone_timer');
  e.idle(60);
  assert.equal(e.v('warden_zone_timer'),timer);
  e.tap('START');
  e.hold('RIGHT',25);
  assert.equal(e.v('warden_zone_x'),x);
  assert.equal(e.v('warden_zone_y'),y);
  e.idle().until(e=>e.v('warden_zone_state')===2);
  e.idle(2).shot('warden-floor-live');
  assert.equal(e.v('shield_hp'),40,'leaving the locked square avoids the strike');
  w16(e,'player_x',x+8);w16(e,'player_y',y+8);
  e.idle(1);
  assert.ok(e.v('shield_hp')<40,'striped active floor hurts inside its bounds');
});

test('boss: Warden changes firing position and alternates fan and ring', () => {
  const e=trial(2);
  removeRelay(e,1);removeRelay(e,2);
  const positions=[];
  for(let cycle=0;cycle<2;cycle++) {
    for(let n=0;n<200&&e.v('boss_state')!==WARN;n++){keepAlive(e);e.idle();}
    assert.equal(e.v('boss_state'),WARN);
    for(let n=0;n<60&&e.v('boss_state')===WARN;n++){keepAlive(e);e.idle();}
    positions.push(e.read16(at(e,'actor_x')));
    assert.equal(e.v('boss_state'),RECOVER);
    assert.equal(r8(e,'actor_guard',0),0);
    assert.ok(e.v('bullets_active') >= (cycle?8:3));
    e.shot(cycle?'warden-ring':'warden-fan');
  }
  assert.deepEqual(positions,[104,40]);
});


test('boss: Warden floor claim remains visible with a full player shot pool',()=>{
  const e=trial(2);
  e.until(x=>x.v('warden_zone_state')===2);
  for(let n=0;n<16;n++) e.writeBytes(at(e,'shots')+n*13,[40,0,0,0,0,24,0,0,0,0,0,24,0]);
  w8(e,'shot_count',16);
  e.idle(2);
  assert.equal(e.v('shot_count'),16);
  for(let i=15;i<23;i++) assert.ok(e.read8(at(e,'shadow_OAM')+i*4),'every hazard segment has reserved OAM');
});
