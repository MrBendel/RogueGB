import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v);
const put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(){
 const e=bootToTitle().tap('DOWN').tap('A').tap('DOWN').tap('DOWN').idle(9);
 assert.equal(e.v('title_choice'),2);e.shot('cragback-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
function warning(e){
 put16(e,'actor_x',72);put16(e,'actor_y',40);
 put16(e,'player_x',40);put16(e,'player_y',96);
 put(e,'boss_state',1);put(e,'boss_timer',12);e.idle(3);
}
test('cragback: selectable 32x32 brute, overhead wind-up, aimed arc and digging recovery',()=>{
 const e=trial();assert.equal(e.v('boss_kind'),11);assert.equal(e.v('boss_hp'),32);
 assert.equal(e.v('play_actor_oam'),15);assert.equal(e.v('actor_guard'),1);
 for(let i=12;i<20;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4));
 e.shot('cragback-stalk');warning(e);e.shot('cragback-overhead');
 e.until(x=>x.v('boss_state')===2,30);
 const tx=e.v('crag_target_x'),ty=e.v('crag_target_y');
 assert.equal(tx,48);assert.equal(ty,104);
 e.hold('RIGHT',12);assert.equal(e.v('crag_target_x'),tx);assert.equal(e.v('crag_target_y'),ty);
 assert.ok(e.v('crag_rock_z')>=25);e.shot('cragback-airborne');
 e.until(x=>x.v('boss_state')===0,40);
 assert.equal(e.v('actor_guard'),0);assert.equal(e.v('crag_impacts','u16'),1);assert.equal(e.v('crag_rock_timer'),0);
 assert.equal(e.v('crag_rock_x'),tx);assert.equal(e.v('crag_rock_y'),ty);
 assert.equal(e.v('bullets_fired','u16'),3);e.idle(2).shot('cragback-impact');
 e.idle(12).shot('cragback-digging');
 const hp=e.v('actor_hp');e.hold('A',40);assert.ok(e.v('actor_hp')<hp);
});
test('cragback: rock hits grounded target, dodging or jumping avoids landing damage',()=>{
 for(const strategy of ['stand','dodge','jump']) {
  const e=trial();warning(e);e.until(x=>x.v('boss_state')===2,30);
  if(strategy==='dodge')e.hold('RIGHT',24);else e.idle(24);
  if(strategy==='jump')e.hold('B',8);else e.idle(8);
  if(strategy==='stand')assert.ok(e.v('shield_hp')<=22);
  else assert.equal(e.v('shield_hp'),40,strategy);
 }
});
test('cragback: close range warns a slam, jump clears it, then recovery gives an opening',()=>{
 for(const airborne of [false,true]) {
  const e=trial();put16(e,'player_x',98);put16(e,'player_y',40);
  e.idle(2);assert.equal(e.v('boss_state'),3);assert.ok(e.v('boss_timer')>=25);
  e.until(x=>x.v('boss_timer')===8,40);e.shot('cragback-slam-warning');
  if(airborne)e.hold('B',9);else e.idle(9);
  assert.equal(e.v('crag_slams','u16'),1);assert.equal(e.v('boss_state'),0);
  assert.equal(e.v('shield_hp'),airborne?40:24);
 }
});
test('cragback: pause freezes the airborne rock; defeat clears body, rock and fragments',()=>{
 const e=trial();warning(e);e.until(x=>x.v('boss_state')===2,30).idle(5).tap('START');
 const t=e.v('crag_rock_timer'),x=e.v('crag_rock_x');e.idle(50);
 assert.equal(e.v('crag_rock_timer'),t);assert.equal(e.v('crag_rock_x'),x);
 e.tap('START');put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);
 assert.equal(e.v('boss_state'),5);assert.equal(e.v('boss_reward'),1);
 assert.equal(e.v('crag_rock_timer'),0);assert.equal(e.v('bullet_count'),0);
 for(let i=11;i<21;i++)assert.equal(e.read8(at(e,'shadow_OAM')+i*4),0);
});
test('cragback: full player shot pool preserves the eight-part monster and its rock/shadow',()=>{
 const e=trial();warning(e);e.until(x=>x.v('boss_state')===2,30).idle(5);
 for(let n=0;n<16;n++)e.writeBytes(at(e,'shots')+13*n,[40,0,0,0,0,24,0,0,0,0,0,80,0]);
 put(e,'shot_count',16);e.idle(2);
 for(let i=11;i<21;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4));
 assert.equal(e.read8(at(e,'shadow_OAM')+20*4+2),146);
 assert.equal(e.read8(at(e,'shadow_OAM')+11*4+2),144);
 assert.ok(e.v('sprites_used')<=40);assert.ok(e.lastFrameCycles<FRAME_LIMIT);
 e.shot('cragback-crowded');
});
test('cragback: movement and timed jumps can win with starter pistol, camping loses',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<3600&&e.v('boss_state')!==5&&e.v('player_hp');f++) {
  const keys=['A'],x=e.v('player_x','u16');
  if((f/70|0)&1){if(x>24)keys.push('LEFT');}else if(x<120)keys.push('RIGHT');
  if(!e.v('player_air')&&((e.v('boss_state')===2&&e.v('crag_rock_timer')<10)||(e.v('boss_state')===3&&e.v('boss_timer')<10)))keys.push('B');
  e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
 }
 assert.equal(e.v('boss_state'),5,`hp=${e.v('player_hp')},boss=${e.v('boss_hp')}`);
 assert.ok(worst<FRAME_LIMIT);e.shot('cragback-victory');console.log(`Cragback victory peak=${worst}/${FRAME_LIMIT}`);
 const camp=trial();
 for(let f=0;f<3600&&camp.v('player_hp')&&camp.v('boss_state')!==5;f++)camp.hold('A');
 assert.equal(camp.v('player_hp'),0,'standing still must lose');
});


test('cragback: two bounded diagonal hops with altitude, shadow, pause and a full throw warning',()=>{
 for(const [x,y] of [[72,64],[24,40],[120,96]]) {
  const e=trial();put16(e,'actor_x',x);put16(e,'actor_y',y);
  put(e,'boss_state',0);put(e,'boss_timer',1);e.idle();
  assert.equal(e.v('boss_state'),4);assert.equal(e.v('crag_hops_left'),2);
  e.until(t=>t.v('crag_hop_timer')===16,20);
  const vx=e.v('crag_hop_vx','i16'),vy=e.v('crag_hop_vy','i16');
  e.hold('LEFT',5);assert.equal(e.v('crag_hop_vx','i16'),vx);assert.equal(e.v('crag_hop_vy','i16'),vy);
  assert.ok(e.v('crag_hop_z')>0);assert.ok(e.v('actor_x','u16')!==x||e.v('actor_y','u16')!==y);
  assert.equal(e.read8(at(e,'shadow_OAM')+11*4+2),146);e.shot('cragback-hopping');
  e.tap('START');const t=e.v('crag_hop_timer'),bx=e.v('actor_x','u16');e.idle(30);
  assert.equal(e.v('crag_hop_timer'),t);assert.equal(e.v('actor_x','u16'),bx);e.tap('START');
  for(let f=0;f<70&&e.v('boss_state')===4;f++) {
   e.idle();assert.ok(e.v('actor_x','u16')>=24&&e.v('actor_x','u16')<=120);
   assert.ok(e.v('actor_y','u16')>=40&&e.v('actor_y','u16')<=96);
   assert.ok(e.lastFrameCycles<FRAME_LIMIT);
  }
  assert.equal(e.v('crag_hops','u16'),2);assert.equal(e.v('crag_hop_z'),0);
  assert.equal(e.v('boss_state'),1);assert.equal(e.v('boss_timer'),40);
 }
});


test('cragback: airborne body does not deal invisible ground contact damage and defeat clears hop',()=>{
 const e=trial();put(e,'boss_state',0);put(e,'boss_timer',1);e.idle();
 e.until(t=>t.v('crag_hop_timer')===16,20);put(e,'shield_hp',40);
 for(let f=0;f<8;f++) {
  put16(e,'player_x',e.v('actor_x','u16'));put16(e,'player_y',e.v('actor_y','u16'));
  put(e,'player_invuln',0);put(e,'player_blink',0);e.idle();
  assert.equal(e.v('shield_hp'),40);
 }
 put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);
 assert.equal(e.v('boss_state'),5);assert.equal(e.v('crag_hop_z'),0);assert.equal(e.v('crag_hop_timer'),0);
 for(let i=11;i<21;i++)assert.equal(e.read8(at(e,'shadow_OAM')+i*4),0);
});
