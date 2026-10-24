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
function kill(e,ids){for(const i of ids)put(e,'actor_hp',0,i);put(e,'shots_killed',1);for(let f=0;f<3;f++){e.idle();assert.ok(e.lastFrameCycles<FRAME_LIMIT,'split/death frame: '+e.lastFrameCycles);}}
test('globfather: independent splits preserve remaining family HP and reward only final kill',()=>{
 const e=trial();assert.equal(e.v('boss_kind'),12);assert.equal(e.v('boss_hp'),80);assert.equal(e.v('glob_alive'),1);
 e.idle(25).shot('globfather-king');kill(e,living(e));
 assert.equal(e.v('glob_alive'),2);assert.equal(e.v('boss_hp'),60);assert.equal(e.v('boss_reward'),0);e.idle(25).shot('globfather-medium');
 kill(e,[living(e)[0]]);assert.equal(e.v('glob_alive'),5);assert.equal(e.v('boss_hp'),50);
 assert.deepEqual(living(e).map(i=>get(e,'glob_size',i)).sort(),[1,1,1,1,2]);e.idle(25).shot('globfather-mixed');
 kill(e,living(e).filter(i=>get(e,'glob_size',i)===2));assert.equal(e.v('glob_alive'),8);assert.equal(e.v('boss_hp'),40);
 e.idle(25).shot('globfather-small');assert.equal(e.v('glob_splits','u16'),3);
 while(living(e).length>1){kill(e,[living(e)[0]]);assert.equal(e.v('boss_reward'),0);}
 kill(e,living(e));assert.equal(e.v('boss_state'),5);assert.equal(e.v('boss_reward'),1);assert.equal(e.v('glob_defeated'),11);
 for(let i=11;i<40;i++)assert.equal(e.read8(at(e,'shadow_OAM')+4*i),0);
});
test('globfather: simultaneous medium kills produce all eight children without slot reuse loss',()=>{
 const e=trial();kill(e,living(e));kill(e,living(e));
 assert.equal(e.v('glob_alive'),8);assert.equal(e.v('actor_count'),8);assert.equal(e.v('glob_defeated'),3);
 assert.deepEqual(living(e).map(i=>get(e,'glob_size',i)),[1,1,1,1,1,1,1,1]);assert.equal(e.v('boss_hp'),40);
 kill(e,living(e));assert.equal(e.v('glob_defeated'),11);assert.equal(e.v('boss_reward'),1);assert.equal(e.v('actor_count'),0);
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
 const e=trial();let peak=0,peakInfo,waypoint=0;
 const route=[[120,96],[120,40],[24,40],[24,96]];
 for(let f=0;f<5000&&e.v('boss_state')!==5&&e.v('player_hp');f++) {
  const keys=['A'],x=e.v('player_x','u16');
  const y=e.v('player_y','u16'),[tx,ty]=route[waypoint];
  if(Math.abs(x-tx)<5&&Math.abs(y-ty)<5)waypoint=(waypoint+1)%route.length;
  if(x<tx-3)keys.push('RIGHT');else if(x>tx+3)keys.push('LEFT');
  if(y<ty-3)keys.push('DOWN');else if(y>ty+3)keys.push('UP');
  const i=e.v('glob_attacker');
  const close=living(e).some(j=>get(e,'glob_state',j)===0&&Math.abs(e.read16(at(e,'actor_x')+j*2)-x)<24&&Math.abs(e.read16(at(e,'actor_y')+j*2)-e.v('player_y','u16'))<24);
  if(!e.v('player_air')&&(close||(i!==255&&get(e,'glob_state',i)===2&&get(e,'glob_timer',i)<12)))keys.push('B');
  e.hold(keys);if(e.lastFrameCycles>peak){peak=e.lastFrameCycles;peakInfo={f,peak,phase:e.v('boss_phase'),alive:e.v('glob_alive'),state:e.v('boss_state'),shots:e.v('shot_count'),bullets:e.v('bullet_count'),hops:e.v('glob_hops','u16')};}
 }
 assert.equal(e.v('boss_state'),5,`player=${e.v('player_hp')} family=${e.v('boss_hp')}`);
 assert.equal(e.v('glob_defeated'),11);assert.ok(peak<FRAME_LIMIT,JSON.stringify(peakInfo));e.shot('globfather-victory');
 console.log(`Globfather victory peak ${peak}/${FRAME_LIMIT}`);
});
test('globfather: leaving a split fight and restarting resets the family',()=>{
 const e=trial();kill(e,living(e));assert.equal(e.v('glob_alive'),2);
 settle(e.tap('START').tap('SELECT'),RUN_OVER).idle(40);
 settle(e.tap('START'),TITLE);trial(e);
 assert.equal(e.v('glob_alive'),1);assert.equal(e.v('glob_defeated'),0);assert.equal(e.v('glob_splits','u16'),0);
 assert.equal(e.v('boss_hp'),80);assert.equal(e.v('boss_reward'),0);assert.equal(e.v('bullet_count'),0);
});

test('globfather: complete king silhouette survives player, weapon and jump-shadow scanline overlap',()=>{
 let checked=0;
 for(const pose of [0,1,2])for(const airborne of [false,true]) {
  const e=trial();
  put(e,'glob_state',pose);put(e,'glob_timer',pose===2?16:200);put(e,'actor_guard',1);
  e.writeBytes(at(e,'player_x'),[76,0]);e.writeBytes(at(e,'player_y'),[76,0]);
  if(airborne)e.tap('B');
  for(let f=0;f<8;f++) {
   put(e,'glob_timer',pose===2?16:200);put(e,'player_invuln',30);e.hold('A');
   const oam=Array.from({length:40},(_,i)=>({y:e.read8(at(e,'shadow_OAM')+i*4),tile:e.read8(at(e,'shadow_OAM')+i*4+2)}));
   for(let i=11;i<28;i++)if(oam[i].y)for(let y=Math.max(0,oam[i].y-16);y<Math.min(128,oam[i].y);y++) {
    const earlier=oam.slice(0,i).filter(s=>s.y&&y>=s.y-16&&y<s.y).length;
    assert.ok(earlier<10,`pose=${pose}, jump=${airborne}, frame=${f}, body slot=${i}, line=${y}, earlier=${earlier}`);checked++;
   }
   assert.ok(e.read8(at(e,'shadow_OAM')+3*4));assert.ok(e.read8(at(e,'shadow_OAM')+4*4));
   if(e.v('player_z'))for(const slot of [7,8])assert.ok(e.read8(at(e,'shadow_OAM')+slot*4),'jump shadow preserved');
   assert.ok(e.lastFrameCycles<FRAME_LIMIT);
  }
  e.shot('globfather-overlap-fixed-'+pose+'-'+airborne);
 }
 assert.ok(checked>1000);
});

test('globfather: eight crowded small slimes rotate visibility within the sprite and frame budgets',()=>{
 const e=trial();kill(e,living(e));kill(e,living(e));
 const hidden=Array(8).fill(0);let drops=0,worst=0,peak=0;
 for(let f=0;f<32;f++) {
  for(let i=0;i<8;i++) {
   e.writeBytes(at(e,'actor_x')+i*2,[16+i*14,0]);e.writeBytes(at(e,'actor_y')+i*2,[64,0]);
   put(e,'glob_state',0,i);put(e,'glob_timer',200,i);
  }
  e.idle();peak=Math.max(peak,e.lastFrameCycles);
  const oam=Array.from({length:40},(_,i)=>({x:e.read8(at(e,'shadow_OAM')+i*4+1),y:e.read8(at(e,'shadow_OAM')+i*4)}));
  const line=64-e.v('cam_y','u16'),visible=oam.filter(s=>s.y&&line>=s.y-16&&line<s.y).slice(0,10);
  for(let i=0;i<8;i++) {
   const x=16+i*14+8-e.v('cam_x','u16'),seen=visible.some(s=>s.x===x||s.x===x+8);
   hidden[i]=seen?0:hidden[i]+1;if(!seen)drops++;worst=Math.max(worst,hidden[i]);
  }
  assert.ok(e.v('sprites_used')<=40);assert.equal(e.v('glob_alive'),8);
 }
 assert.ok(drops>0);assert.ok(worst<=2,`hidden for ${worst} frames`);assert.ok(peak<FRAME_LIMIT,`peak ${peak}`);
 e.shot('globfather-eight-crowded');
});
