import { test } from 'node:test';
import { assert, bootToTitle, settle, PLAY, RUN_OVER, FRAME_LIMIT } from './helpers.mjs';
const at = (e, n) => e.symbols.addr(n);
const read = (e,n,i=0) => e.read8(at(e,n)+i);
const put = (e,n,v,i=0) => e.write8(at(e,n)+i,v);
const pos = (e,n,i=0) => e.read16(at(e,n)+i*2);
const put16 = (e,n,v,i=0) => e.writeBytes(at(e,n)+i*2,[v&255,v>>8]);
function trial(kind) {
  const e = bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A');
  for(let n=0;n<kind;n++) e.tap('DOWN');
  e.idle(9);
  if(kind===7) e.shot('enemy-family-menu');
  return settle(e.tap('A'),PLAY).idle(3);
}

test('enemy families: scrolling menu launches each silhouette and correct family', () => {
  for(const [kind, families] of [[3,[2,2,2]], [4,[3]], [5,[4]], [6,[5]], [7,[2,3,4,5]], [8,[6,6]]]) {
    const e=trial(kind);
    const actual=Array.from({length:8},(_,i)=>read(e,'actor_scripted',i)).filter(Boolean);
    assert.deepEqual(actual, families);
    assert.equal(e.v('enemy_trial_cleared'),0);
    e.shot(`enemy-family-${kind}`);
  }
  const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('UP');
  assert.equal(e.v('title_choice'),9);
  e.tap('DOWN');
  assert.equal(e.v('title_choice'),0);
});

test('enemy families: husks pursue and stay within floor bounds', () => {
  const e=trial(3), y=pos(e,'actor_y',1);
  e.idle(60);
  assert.ok(pos(e,'actor_y',1)>y);
  put16(e,'player_x',16); put16(e,'player_y',16);
  for(let f=0;f<240;f++) {
    put(e,'player_hp',60); e.idle();
    for(let i=0;i<3;i++) {
      assert.ok(pos(e,'actor_x',i)>=14 && pos(e,'actor_x',i)<=130);
      assert.ok(pos(e,'actor_y',i)>=10 && pos(e,'actor_y',i)<=96);
    }
  }
});

test('enemy families: ragebound commits to a warned lane, then rests at the wall', () => {
  const e=trial(4);
  e.until(x=>read(x,'actor_state')===1,100);
  assert.equal(read(e,'actor_face'),4);
  e.hold('RIGHT',28);
  assert.equal(read(e,'actor_face'),4,'warning cannot track the dodge');
  e.idle().until(x=>read(x,'actor_state')===2,50);
  e.until(x=>read(x,'actor_state')===0,70);
  assert.ok(read(e,'actor_timer')>=64);
  assert.ok(pos(e,'actor_y')<=96);
  assert.equal(e.v('player_hp'),60);
  assert.equal(e.v('shield_hp'),40);
});

test('enemy families: contractor repositions, warns and fires exactly three burst shots', () => {
  const e=trial(5), x=pos(e,'actor_x');
  e.idle(32);
  assert.notEqual(pos(e,'actor_x'),x);
  e.until(x=>read(x,'actor_state')===1,80);
  assert.equal(e.v('bullets_fired','u16'),0);
  e.until(x=>read(x,'actor_state')===2,40);
  const dir=read(e,'actor_face');
  e.hold('RIGHT',20);
  assert.equal(read(e,'actor_face'),dir);
  e.idle().until(x=>read(x,'actor_state')===0,20);
  assert.equal(e.v('bullets_fired','u16'),3);
  e.idle(50);
  assert.equal(e.v('bullets_fired','u16'),3,'reload gives a safe attack window');
});

test('enemy families: bastion guard blocks shots, opens to attack, and becomes damageable', () => {
  const e=trial(6);
  e.hold('A',45);
  assert.equal(read(e,'actor_guard'),1);
  assert.equal(read(e,'actor_hp'),8);
  assert.notEqual(e.v('aim_target'),0);
  e.idle().until(x=>read(x,'actor_guard')===0,70);
  assert.equal(e.v('bullets_fired','u16'),3);
  const hp=read(e,'actor_hp');
  e.hold('A',65);
  assert.ok(read(e,'actor_hp')<hp);
  e.idle().until(x=>read(x,'actor_guard')===1,40);
});

test('enemy families: starter pistol can clear every trial with ordinary health and movement', () => {
  for(const kind of [3,4,5,6,7,8]) {
    const e=trial(kind);
    const waypoints=[[120,96],[120,24],[24,24],[24,96]];
    let waypoint=0, worst=0;
    for(let f=0;f<5000 && !e.v('enemy_trial_cleared');f++) {
      const [x,y]=waypoints[waypoint];
      const dx=x-e.v('player_x','u16'), dy=y-e.v('player_y','u16');
      if(Math.abs(dx)<5 && Math.abs(dy)<5) waypoint=(waypoint+1)&3;
      const buttons=['A'];
      if(dx>4) buttons.push('RIGHT'); else if(dx< -4) buttons.push('LEFT');
      if(dy>4) buttons.push('DOWN'); else if(dy< -4) buttons.push('UP');
      e.hold(buttons,1);
      worst=Math.max(worst,e.lastFrameCycles);
      assert.ok(e.v('player_hp')>0,`trial ${kind}: survives on regular resources`);
      assert.ok(e.v('sprites_used')<=40);
    }
    assert.equal(e.v('enemy_trial_cleared'),1,`trial ${kind} is beatable`);
    assert.ok(worst<FRAME_LIMIT,`trial ${kind}: ${worst}/${FRAME_LIMIT}`);
    console.log(`enemy family ${kind}: worst=${worst}/${FRAME_LIMIT}`);
  }
});

test('enemy families: pause freezes attacks and contact death names the family', () => {
  const e=trial(4);
  e.until(x=>read(x,'actor_state')===1,100).tap('START');
  const timer=read(e,'actor_timer');
  e.idle(90);
  assert.equal(read(e,'actor_timer'),timer);
  e.tap('START');
  put(e,'player_hp',1); put(e,'shield_hp',0);
  put16(e,'player_x',pos(e,'actor_x')); put16(e,'player_y',pos(e,'actor_y'));
  e.idle(4);
  settle(e,RUN_OVER);
  assert.equal(e.v('death_cause'),6);
  e.shot('enemy-family-death');
});

test('enemy families: full actor and bullet pools stay inside the frame and OAM limits', () => {
  const e=trial(7).tap('SELECT'); // shotgun
  put(e,'debug_cmd',1); e.idle();
  for(let i=0;i<8;i++) {
    put(e,'actor_scripted',2+(i%4),i);
    put(e,'actor_guard',0,i); put(e,'actor_hp',200,i);
    put(e,'actor_state',0,i); put(e,'actor_timer',60,i);
    put16(e,'actor_x',24+(i%4)*32,i); put16(e,'actor_y',i<4?32:64,i);
  }
  for(let n=0;n<20;n++) {
    const x=24+(n%5)*24,y=24+Math.floor(n/5)*20;
    e.writeBytes(at(e,'bullets')+n*13,[1,64,0,0,0,x,0,0,0,0,0,y,0]);
  }
  put(e,'bullet_count',20); e.idle();
  assert.equal(e.v('actors_active'),8);
  assert.equal(e.v('bullets_active'),20);
  let worst=0;
  for(let f=0;f<300;f++) {
    put(e,'player_hp',60); put(e,'shield_hp',40);
    for(let i=0;i<8;i++) put(e,'actor_hp',200,i);
    e.hold('A',1);
    worst=Math.max(worst,e.lastFrameCycles);
    assert.equal(e.v('actors_active'),8);
    assert.ok(e.v('bullets_active')<=20);
    assert.ok(e.v('sprites_used')<=40);
  }
  assert.ok(worst<FRAME_LIMIT,`stress: ${worst}/${FRAME_LIMIT}`);
  console.log(`enemy family stress: worst=${worst}/${FRAME_LIMIT}`);
});

test('enemy families: hound signals a diagonal pounce, commits, and recovers', () => {
  const e=trial(8);
  put(e,'actor_hp',0,1); put(e,'shots_killed',1); e.idle(2); // isolate one pounce
  put16(e,'actor_x',40); put16(e,'actor_y',32);
  put(e,'actor_timer',2); // isolate the attack from its faster bounding approach
  e.until(x=>read(x,'actor_state')===1,100);
  const dir=read(e,'actor_face');
  assert.equal(dir,3,'hound can pounce diagonally toward the player');
  const x=pos(e,'actor_x'), y=pos(e,'actor_y');
  e.hold('RIGHT',18);
  assert.equal(read(e,'actor_face'),dir,'warning locks the target direction');
  e.idle().until(x=>read(x,'actor_state')===2,30);
  e.until(x=>read(x,'actor_state')===3,40);
  assert.ok(pos(e,'actor_x')>x && pos(e,'actor_y')>y);
  assert.ok(read(e,'actor_timer')>=38,'pounce leaves a recovery window');
  assert.equal(e.v('player_hp'),60);
  assert.equal(e.v('shield_hp'),40);
  e.shot('scrap-hound-recovery');
});

test('enemy families: hounds bound side to side toward the player and freeze when paused', () => {
  const e=trial(8);
  put16(e,'actor_x',72); put16(e,'actor_y',24);
  put16(e,'player_x',72); put16(e,'player_y',96);
  put(e,'hound_step',0); put(e,'actor_timer',60);
  const startX=pos(e,'actor_x'), startY=pos(e,'actor_y');
  e.idle(8).shot('scrap-hound-bound-air');
  assert.equal(read(e,'actor_t0'),100,'tucked airborne pose');
  e.idle(8);const left=pos(e,'actor_x');
  assert.ok(left<startX,'first bound goes left');
  e.idle(16).shot('scrap-hound-bound-land');
  assert.ok(pos(e,'actor_x')>left,'next bound goes right');
  assert.ok(pos(e,'actor_y')>startY+20,'both bounds close the distance');
  e.tap('START');
  const frozen=[pos(e,'actor_x'),pos(e,'actor_y'),read(e,'hound_step')];
  e.idle(60);
  assert.deepEqual([pos(e,'actor_x'),pos(e,'actor_y'),read(e,'hound_step')],frozen);
  e.tap('START');
  put16(e,'player_x',16);put16(e,'player_y',16);
  for(let f=0;f<240;f++) {
    put(e,'player_hp',60);put(e,'shield_hp',40);e.idle();
    for(let i=0;i<2;i++) {
      assert.ok(pos(e,'actor_x',i)>=14 && pos(e,'actor_x',i)<=130);
      assert.ok(pos(e,'actor_y',i)>=10 && pos(e,'actor_y',i)<=96);
    }
  }
});
