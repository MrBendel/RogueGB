import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v),put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(){const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('UP').idle(9);assert.equal(e.v('title_choice'),4);e.shot('weaver-menu');return settle(e.tap('A'),PLAY).idle(3);}
test('weaver: scuttles, locks a warned web shot, then leaves a bounded persistent patch',()=>{
 const e=trial();assert.equal(e.v('actor_scripted'),16);assert.equal(e.v('play_actor_oam'),15);
 const x=e.v('actor_x','u16');e.idle(12);assert.notEqual(e.v('actor_x','u16'),x);e.shot('weaver-scuttle');
 e.until(t=>t.v('weaver_state')===1,60);const tx=e.v('weaver_tx'),ty=e.v('weaver_ty');e.shot('weaver-warning');
 e.hold('LEFT',12);assert.equal(e.v('weaver_tx'),tx);assert.equal(e.v('weaver_ty'),ty);
 e.until(t=>t.v('weaver_state')===2,30).idle(8);e.shot('weaver-shot');
 e.until(t=>t.v('web_life')>0,20);assert.equal(e.v('web_x'),tx);assert.equal(e.v('web_y'),ty);assert.equal(e.v('weaver_spits'),1);e.idle(2).shot('weaver-web');
 put(e,'weaver_state',3);put(e,'weaver_timer',250);put(e,'web_life',3);e.idle(4);assert.equal(e.v('web_life'),0);
});
test('weaver: web snares for 36 frames, permits shooting, pauses, and grants release grace',()=>{
 const e=trial();put(e,'weaver_state',3);put(e,'weaver_timer',250);
 put(e,'web_x',e.v('player_x','u16'));put(e,'web_y',e.v('player_y','u16'));put(e,'web_life',200);
 const x=e.v('player_x','u16'),y=e.v('player_y','u16');
 put(e,'player_jump_buffer',6); // A buffered jump must not escape a grounded snare.
 e.hold(['RIGHT','B','A'],12);assert.equal(e.v('web_trap'),24);
 assert.equal(e.v('player_x','u16'),x);assert.equal(e.v('player_y','u16'),y);assert.equal(e.v('player_air'),0);
 assert.ok(e.v('shot_count')>0,'can defend yourself while trapped');assert.equal(e.v('shield_hp'),40);e.shot('weaver-trapped');
 e.tap('START');const held=e.v('web_trap'),life=e.v('web_life');e.idle(30);
 assert.equal(e.v('web_trap'),held);assert.equal(e.v('web_life'),life);e.tap('START');
 e.until(t=>!t.v('web_trap'),40);assert.ok(e.v('web_grace')>0);
 e.idle(12);assert.equal(e.v('web_trap'),0,'same patch cannot immediately trap again');
 e.hold('RIGHT',12);assert.ok(e.v('player_x','u16')>x);assert.equal(e.v('web_trap'),0);
});
test('weaver: airborne crossing avoids snare and expired webs cannot catch the player',()=>{
 const e=trial();put(e,'weaver_state',3);put(e,'weaver_timer',200);e.hold('B',6);
 put(e,'web_x',e.v('player_x','u16'));put(e,'web_y',e.v('player_y','u16'));put(e,'web_life',100);
 e.hold('RIGHT',6);assert.equal(e.v('web_trap'),0);assert.ok(e.v('player_air'));
 put(e,'web_life',0);e.idle(50);assert.equal(e.v('web_trap'),0);
});
test('weaver: close range lunge is warned and committed, jump avoids damage, death clears web',()=>{
 for(const jump of [false,true]) {
  const e=trial();put16(e,'actor_x',e.v('player_x','u16')-20);put16(e,'actor_y',e.v('player_y','u16')-10);put(e,'weaver_state',0);put(e,'weaver_timer',1);e.idle();
  assert.equal(e.v('weaver_state'),4);assert.equal(e.v('shield_hp'),40);
  e.until(t=>t.v('weaver_state')===5,24);if(jump)e.hold('B',12);else e.idle(12);
  assert.equal(e.v('shield_hp'),jump?40:28);assert.equal(e.v('weaver_state'),3);
  put(e,'web_life',100);put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);assert.equal(e.v('web_life'),0);assert.equal(e.v('web_trap'),0);assert.equal(e.v('enemy_trial_cleared'),1);
  for(let slot=11;slot<17;slot++)assert.equal(e.read8(at(e,'shadow_OAM')+slot*4),0);
 }
});
test('weaver: moving starter pistol can clear the trial within hardware limits',()=>{
 const e=trial();let peak=0;
 for(let f=0;f<4000&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++) {
  const keys=['A'];const x=e.v('player_x','u16');if((f/70|0)&1){if(x>24)keys.push('LEFT');}else if(x<120)keys.push('RIGHT');
  if((e.v('web_trap')||e.v('weaver_state')===5)&&!e.v('player_air'))keys.push('B');
  e.hold(keys);peak=Math.max(peak,e.lastFrameCycles);assert.ok(e.v('sprites_used')<=40);
 }
 assert.equal(e.v('enemy_trial_cleared'),1);assert.ok(peak<FRAME_LIMIT,`peak ${peak}`);e.shot('weaver-victory');console.log(`Wireweaver peak ${peak}/${FRAME_LIMIT}`);
});
