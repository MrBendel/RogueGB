import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,TITLE,RUN_OVER,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),get=(e,n,i=0)=>e.read8(at(e,n)+i),put=(e,n,v,i=0)=>e.write8(at(e,n)+i,v);
function trial(e=bootToTitle()){
 e.tap('DOWN').tap('DOWN').tap('A');
 for(let i=0;i<4;i++)e.tap('DOWN');
 e.idle(9);assert.equal(e.v('title_choice'),4);e.shot('globfather-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
const living=e=>Array.from({length:8},(_,i)=>i).filter(i=>get(e,'glob_size',i)&&get(e,'actor_type',i));
function kill(e,ids){for(const i of ids)put(e,'actor_hp',0,i);put(e,'shots_killed',1);e.idle(3);}
test('globfather: independent splits preserve remaining family HP and reward only final kill',()=>{
 const e=trial();assert.equal(e.v('boss_kind'),12);assert.equal(e.v('boss_hp'),60);assert.equal(e.v('glob_alive'),1);
 e.idle(25).shot('globfather-king');kill(e,living(e));
 assert.equal(e.v('glob_alive'),2);assert.equal(e.v('boss_hp'),40);assert.equal(e.v('boss_reward'),0);e.idle(25).shot('globfather-medium');
 kill(e,[living(e)[0]]);assert.equal(e.v('glob_alive'),3);assert.equal(e.v('boss_hp'),30);
 assert.deepEqual(living(e).map(i=>get(e,'glob_size',i)).sort(),[1,1,2]);e.idle(25).shot('globfather-mixed');
 kill(e,living(e).filter(i=>get(e,'glob_size',i)===2));assert.equal(e.v('glob_alive'),4);assert.equal(e.v('boss_hp'),20);
 e.idle(25).shot('globfather-small');assert.equal(e.v('glob_splits','u16'),3);
 while(living(e).length>1){kill(e,[living(e)[0]]);assert.equal(e.v('boss_reward'),0);}
 kill(e,living(e));assert.equal(e.v('boss_state'),5);assert.equal(e.v('boss_reward'),1);assert.equal(e.v('glob_defeated'),7);
 for(let i=11;i<40;i++)assert.equal(e.read8(at(e,'shadow_OAM')+4*i),0);
});
test('globfather: simultaneous medium kills produce all four children without slot reuse loss',()=>{
 const e=trial();kill(e,living(e));kill(e,living(e));
 assert.equal(e.v('glob_alive'),4);assert.equal(e.v('actor_count'),4);assert.equal(e.v('glob_defeated'),3);
 assert.deepEqual(living(e).map(i=>get(e,'glob_size',i)),[1,1,1,1]);assert.equal(e.v('boss_hp'),20);
});
test('globfather: warned committed hop, pause freezes motion, landing opens damage window',()=>{
 const e=trial();e.until(t=>t.v('glob_state')===1,100);e.shot('globfather-warning');assert.equal(e.v('actor_guard'),1);
 e.until(t=>t.v('glob_state')===2,40);const vx=e.v('actor_vx','i16'),vy=e.v('actor_vy','i16');
 e.hold('LEFT',5);assert.equal(e.v('actor_vx','i16'),vx);assert.equal(e.v('actor_vy','i16'),vy);assert.ok(e.v('glob_z')>0);e.shot('globfather-hop');
 e.tap('START');const timer=e.v('glob_timer'),x=e.v('actor_x','u16');e.idle(30);assert.equal(e.v('glob_timer'),timer);assert.equal(e.v('actor_x','u16'),x);e.tap('START');
 e.until(t=>t.v('glob_landings','u16')===1,30);assert.equal(e.v('glob_z'),0);assert.equal(e.v('actor_guard'),0);assert.equal(e.v('bullets_fired','u16'),6);
 e.idle(2).shot('globfather-splash');assert.ok(e.v('sprites_used')<=40);assert.ok(e.lastFrameCycles<FRAME_LIMIT);
});
test('globfather: full shot pool preserves large body and damaging splashes within frame budget',()=>{
 const e=trial();e.until(t=>t.v('glob_landings','u16')===1,130);
 for(let n=0;n<16;n++)e.writeBytes(at(e,'shots')+13*n,[40,0,0,0,0,24,0,0,0,0,0,80,0]);
 put(e,'shot_count',16);e.idle(2);
 for(let i=11;i<28;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4),`body slot ${i}`);
 for(let i=3;i<=4;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4),'player intact');
 assert.ok(e.v('sprites_used')<=40);assert.ok(e.lastFrameCycles<FRAME_LIMIT);e.shot('globfather-crowded');
});
test('globfather: jumping clears landing damage; smaller forms reduce splash size',()=>{
 for(const airborne of [false,true]) {
  const e=trial();e.until(t=>t.v('glob_state')===2,110);put(e,'shield_hp',40);
  if(airborne)e.hold('B',8);else e.idle(8);
  // Follow the committed landing point so both cases exercise the same hit.
  for(let f=0;f<8;f++) {
   e.writeBytes(at(e,'player_x'),[e.v('actor_x','u16'),0]);e.writeBytes(at(e,'player_y'),[e.v('actor_y','u16'),0]);
   e.idle();
  }
  assert.equal(e.v('shield_hp'),airborne?40:20);
 }
 const e=trial();kill(e,living(e));
 e.until(t=>t.v('glob_landings','u16')===1,150);assert.equal(e.v('bullets_fired','u16'),3);
 kill(e,living(e));const fired=e.v('bullets_fired','u16'),landed=e.v('glob_landings','u16');
 e.until(t=>t.v('glob_landings','u16')>landed,150);assert.equal(e.v('bullets_fired','u16'),fired);
});
test('globfather: mobile starter pistol can clear the whole family within hardware budget',()=>{
 const e=trial();let peak=0;
 for(let f=0;f<5000&&e.v('boss_state')!==5&&e.v('player_hp');f++) {
  const keys=['A'],x=e.v('player_x','u16');
  if((f/65|0)&1){if(x>24)keys.push('LEFT');}else if(x<120)keys.push('RIGHT');
  const i=e.v('glob_attacker');
  if(i!==255&&!e.v('player_air')&&get(e,'glob_state',i)===2&&get(e,'glob_timer',i)<12)keys.push('B');
  e.hold(keys);peak=Math.max(peak,e.lastFrameCycles);
 }
 assert.equal(e.v('boss_state'),5,`player=${e.v('player_hp')} family=${e.v('boss_hp')}`);
 assert.equal(e.v('glob_defeated'),7);assert.ok(peak<FRAME_LIMIT);e.shot('globfather-victory');
 console.log(`Globfather victory peak ${peak}/${FRAME_LIMIT}`);
});
test('globfather: leaving a split fight and restarting resets the family',()=>{
 const e=trial();kill(e,living(e));assert.equal(e.v('glob_alive'),2);
 settle(e.tap('START').tap('SELECT'),RUN_OVER).idle(40);
 settle(e.tap('START'),TITLE);trial(e);
 assert.equal(e.v('glob_alive'),1);assert.equal(e.v('glob_defeated'),0);assert.equal(e.v('glob_splits','u16'),0);
 assert.equal(e.v('boss_hp'),60);assert.equal(e.v('boss_reward'),0);assert.equal(e.v('bullet_count'),0);
});
