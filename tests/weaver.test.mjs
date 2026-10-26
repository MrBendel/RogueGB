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
test('weaver: web slows ground steering without damage; jumping escapes and pause freezes it',()=>{
 const distances=[];
 for(const mode of ['clear','web','jump']) {
  const e=trial();put(e,'weaver_state',3);put(e,'weaver_timer',200);
  put(e,'web_x',e.v('player_x','u16'));put(e,'web_y',e.v('player_y','u16'));put(e,'web_life',mode==='clear'?0:100);
  const x=e.v('player_x','u16');e.hold(mode==='jump'?['RIGHT','B']:['RIGHT'],12);distances.push(e.v('player_x','u16')-x);
  assert.equal(e.v('shield_hp'),40);assert.ok(e.v('sprites_used')<=40);
  e.tap('START');const life=e.v('web_life');e.idle(20);assert.equal(e.v('web_life'),life);
 }
 assert.ok(distances[1]<distances[0],distances);assert.ok(distances[2]>distances[1],distances);
});
test('weaver: close range lunge is warned and committed, jump avoids damage, death clears web',()=>{
 for(const jump of [false,true]) {
  const e=trial();put16(e,'actor_x',e.v('player_x','u16')-20);put16(e,'actor_y',e.v('player_y','u16')-10);put(e,'weaver_state',0);put(e,'weaver_timer',1);e.idle();
  assert.equal(e.v('weaver_state'),4);assert.equal(e.v('shield_hp'),40);
  e.until(t=>t.v('weaver_state')===5,24);if(jump)e.hold('B',12);else e.idle(12);
  assert.equal(e.v('shield_hp'),jump?40:28);assert.equal(e.v('weaver_state'),3);
  put(e,'web_life',100);put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);assert.equal(e.v('web_life'),0);assert.equal(e.v('web_slow'),0);assert.equal(e.v('enemy_trial_cleared'),1);
  for(let slot=11;slot<17;slot++)assert.equal(e.read8(at(e,'shadow_OAM')+slot*4),0);
 }
});
test('weaver: moving starter pistol can clear the trial within hardware limits',()=>{
 const e=trial();let peak=0;
 for(let f=0;f<4000&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++) {
  const keys=['A'];const x=e.v('player_x','u16');if((f/70|0)&1){if(x>24)keys.push('LEFT');}else if(x<120)keys.push('RIGHT');
  if((e.v('web_slow')||e.v('weaver_state')===5)&&!e.v('player_air'))keys.push('B');
  e.hold(keys);peak=Math.max(peak,e.lastFrameCycles);assert.ok(e.v('sprites_used')<=40);
 }
 assert.equal(e.v('enemy_trial_cleared'),1);assert.ok(peak<FRAME_LIMIT,`peak ${peak}`);e.shot('weaver-victory');console.log(`Wireweaver peak ${peak}/${FRAME_LIMIT}`);
});
