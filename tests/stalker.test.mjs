import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v),put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(){const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('DOWN').tap('DOWN').tap('DOWN').idle(9);assert.equal(e.v('title_choice'),3);e.shot('stalker-menu');return settle(e.tap('A'),PLAY).idle(3);}
test('stalker: shimmer flanks, warns a committed diagonal dash, then exposes itself',()=>{
 const e=trial();assert.equal(e.v('actor_scripted'),15);assert.equal(e.v('play_actor_oam'),15);assert.equal(e.v('actor_guard'),1);
 const x=e.v('actor_x','u16'),y=e.v('actor_y','u16');e.idle(12);assert.ok(e.v('actor_x','u16')!==x||e.v('actor_y','u16')!==y);e.shot('stalker-shimmer');
 e.until(t=>t.v('stalker_state')===1,80);e.shot('stalker-warning');
 const vx=e.v('actor_vx','i16'),vy=e.v('actor_vy','i16');assert.ok(vx&&vy,'diagonal approach');
 e.hold('RIGHT',12);assert.equal(e.v('actor_vx','i16'),vx);assert.equal(e.v('actor_vy','i16'),vy);
 e.until(t=>t.v('stalker_state')===2,30).idle(4);e.shot('stalker-dash');assert.equal(e.v('stalker_dashes'),1);
 e.until(t=>t.v('stalker_state')===3,20);assert.equal(e.v('actor_guard'),0);e.shot('stalker-recovery');
 const landed=e.v('actor_x','u16');e.idle(12);assert.equal(e.v('actor_x','u16'),landed);
});
test('stalker: shimmer and warning cannot damage on contact; solid dash can, jump avoids it',()=>{
 for(const state of [0,1,2,3])for(const air of [false,true]) {
  const e=trial();if(air)e.hold('B',6);
  put(e,'stalker_state',state);put(e,'stalker_timer',20);put16(e,'actor_vx',0);put16(e,'actor_vy',0);
  put16(e,'actor_x',e.v('player_x','u16'));put16(e,'actor_y',e.v('player_y','u16'));put(e,'player_invuln',0);put(e,'shield_hp',40);
  e.idle(2);assert.equal(e.v('shield_hp'),state===2&&!air?24:40,`state=${state} air=${air}`);
 }
});
test('stalker: pause freezes dash; walls end it safely; death clears enemy',()=>{
 const e=trial();e.until(t=>t.v('stalker_state')===2,100).idle(2);e.tap('START');
 const timer=e.v('stalker_timer'),x=e.v('actor_x','u16');e.idle(30);assert.equal(e.v('stalker_timer'),timer);assert.equal(e.v('actor_x','u16'),x);e.tap('START');
 put16(e,'actor_x',16);put16(e,'actor_y',32);put16(e,'actor_vx',-768);put16(e,'actor_vy',0);put(e,'stalker_timer',16);e.idle(8);
 assert.equal(e.v('stalker_state'),3);assert.ok(e.v('actor_x','u16')<144);
 put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);assert.equal(e.v('enemy_trial_cleared'),1);
 assert.equal(e.read8(at(e,'shadow_OAM')+15*4),0);
});
test('stalker: movement and starter pistol can win inside sprite and CPU budgets',()=>{
 const e=trial();let peak=0,waypoint=0;const route=[[120,96],[120,40],[24,40],[24,96]];
 for(let f=0;f<4000&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++) {
  const keys=['A'],x=e.v('player_x','u16'),y=e.v('player_y','u16'),[tx,ty]=route[waypoint];
  if(Math.abs(x-tx)<5&&Math.abs(y-ty)<5)waypoint=(waypoint+1)%4;
  if(x<tx-3)keys.push('RIGHT');else if(x>tx+3)keys.push('LEFT');
  if(y<ty-3)keys.push('DOWN');else if(y>ty+3)keys.push('UP');
  if(e.v('stalker_state')===2&&!e.v('player_air'))keys.push('B');
  e.hold(keys);peak=Math.max(peak,e.lastFrameCycles);assert.ok(e.v('sprites_used')<=40);
 }
 assert.equal(e.v('enemy_trial_cleared'),1,`hp=${e.v('player_hp')} enemy=${e.v('actor_hp')}`);assert.ok(peak<FRAME_LIMIT,`peak ${peak}`);e.shot('stalker-victory');console.log(`Stalker peak ${peak}/${FRAME_LIMIT}`);
});
