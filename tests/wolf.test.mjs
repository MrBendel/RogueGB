import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v);
const put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(delay=9){
 const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('DOWN').tap('DOWN').idle(delay);
 assert.equal(e.v('title_choice'),2);e.shot('wolf-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
function threat(e){
 put(e,'wolf_cooldown',0);put(e,'wolf_timer',100);put(e,'wolf_state',0);
 e.writeBytes(at(e,'shots'),[90,0,0,0,0,e.v('actor_x','u16')+4,0,0,255,255,0,e.v('actor_y','u16')+30,0]);
 put(e,'shot_count',1);
}
test('wolf: closes distance, circles nearby, and warns an aimed three-glob fan',()=>{
 const e=trial();assert.equal(e.v('actor_scripted'),10);assert.equal(e.v('actor_hp'),30);
 assert.equal(e.v('play_actor_oam'),15);
 const y=e.v('actor_y','u16');e.idle(12);assert.ok(e.v('actor_y','u16')>y);
 e.shot('wolf-run');e.until(x=>x.v('wolf_state')===4);
 e.shot('wolf-spit-warning');const aim=e.v('wolf_aim');
 e.hold('RIGHT',8);assert.equal(e.v('wolf_aim'),aim);
 e.until(x=>x.v('wolf_state')===5,25);
 assert.equal(e.v('wolf_spits','u16'),1);assert.equal(e.v('bullets_fired','u16'),3);
 assert.equal(e.v('enemy_bullet_damage'),18);e.idle(8);e.shot('wolf-acid-fan');
 const shots=e.v('bullets_fired','u16');e.idle(12);
 assert.equal(e.v('bullets_fired','u16'),shots,'recovery has no attacks');
});
test('wolf: incoming fire causes a committed lateral leap and vulnerable landing',()=>{
 const e=trial();threat(e);
 e.until(x=>x.v('wolf_state')===1,12);e.shot('wolf-crouch');
 assert.equal(e.v('wolf_leap_y','i8'),0);
 const dx=e.v('wolf_leap_x','i8'),start=e.v('actor_x','u16');assert.ok(dx);
 e.until(x=>x.v('wolf_state')===2,8).idle(5);e.shot('wolf-leap');
 assert.equal(e.v('actor_x','u16')-start,dx*15);
 assert.ok(e.v('wolf_z')>=7);assert.equal(e.v('actor_guard'),0);
 const o=at(e,'shadow_OAM'),cy=e.v('cam_y','u16'),wy=e.v('actor_y','u16');
 assert.equal(e.read8(o+11*4),wy+18-cy,'shadow remains on the floor');
 assert.equal(e.read8(o+15*4),wy+16-cy-e.v('wolf_z'),'body rises above the floor');
 e.hold('LEFT',3);assert.equal(e.v('wolf_leap_x','i8'),dx,'does not track the player midair');
 e.until(x=>x.v('wolf_state')===3,12);assert.equal(e.v('wolf_z'),0);
 assert.ok(e.v('wolf_cooldown')>60);
 const landed=e.v('actor_x','u16');e.idle(8);assert.equal(e.v('actor_x','u16'),landed);
 // Clear the old shot, re-center to leave both sides free, then trigger another.
 e.writeBytes(at(e,'shots'),new Array(16*13).fill(0));put(e,'shot_count',0);
 put16(e,'actor_x',72);put16(e,'actor_y',32);threat(e);
 e.until(x=>x.v('wolf_state')===1,12);assert.equal(e.v('wolf_leap_x','i8'),-dx);
});
test('wolf: pause freezes leaps and walls stop substeps without coordinate wrap',()=>{
 const e=trial();threat(e);e.until(x=>x.v('wolf_state')===2,20).idle(3);
 e.tap('START');const timer=e.v('wolf_timer'),x=e.v('actor_x','u16'),z=e.v('wolf_z');
 e.idle(60);assert.equal(e.v('wolf_timer'),timer);assert.equal(e.v('actor_x','u16'),x);assert.equal(e.v('wolf_z'),z);
 e.tap('START');put16(e,'actor_x',16);put16(e,'actor_y',32);
 put(e,'wolf_state',2);put(e,'wolf_timer',12);put(e,'wolf_leap_x',255);put(e,'wolf_leap_y',0);
 e.idle(5);assert.ok(e.v('actor_x','u16')>=14);assert.equal(e.v('wolf_state'),3);
});
test('wolf: a departing shot does not bait a dodge, and cooldown stops repeated leaps',()=>{
 const e=trial();threat(e);
 // Same nearby shot, now travelling away from the wolf.
 e.writeBytes(at(e,'shots')+7,[0,1,0]);e.idle(10);
 assert.equal(e.v('wolf_jumps','u16'),0);assert.equal(e.v('wolf_state'),0);
 threat(e);put(e,'wolf_cooldown',50);e.idle(10);
 assert.equal(e.v('wolf_jumps','u16'),0);assert.equal(e.v('wolf_state'),0);
});
test('wolf: moving and jumping with the starter pistol can win at normal health',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<2400&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++){
  const x=e.v('player_x','u16'),keys=['A'];
  if((f/65|0)&1){if(x>20)keys.push('LEFT');}else if(x<122)keys.push('RIGHT');
  if(f%55===0)keys.push('B');
  e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
 }
 assert.equal(e.v('enemy_trial_cleared'),1,`hp=${e.v('player_hp')} enemy=${e.v('actor_hp')}`);
 assert.ok(worst<FRAME_LIMIT);e.idle(2);assert.equal(e.v('bullet_count'),0);
 assert.equal(e.read8(at(e,'shadow_OAM')+11*4),0);e.shot('wolf-clear');
 console.log(`wolf victory peak=${worst}/${FRAME_LIMIT}`);
});
test('wolf: stationary players take aimed acid damage',()=>{
 const e=trial();e.until(x=>!x.v('player_hp'),1800);
 assert.equal(e.v('death_cause'),13);assert.ok(e.v('wolf_spits','u16')>=3);
});
test('wolf: standing still holding fire loses the starting-position duel',()=>{
 // Different frame phases previously exposed gaps in quantized projectile aim.
 for(const delay of [0,3,9,30]) {
  const e=trial(delay);
  for(let f=0;f<1800&&e.v('player_hp')&&!e.v('enemy_trial_cleared');f++)e.hold('A');
  assert.equal(e.v('player_hp'),0,`start phase ${delay}`);assert.equal(e.v('enemy_trial_cleared'),0);
  assert.equal(e.v('death_cause'),13);
 }
});
test('wolf: sustained fight bounds projectile pool, sprites and frame budget',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<1200;f++){
  put(e,'player_hp',60);put(e,'shield_hp',40);put(e,'actor_hp',100);
  e.hold('A');worst=Math.max(worst,e.lastFrameCycles);
  assert.ok(e.v('bullet_count')<=20);assert.ok(e.v('sprites_used')<=40);
  assert.ok(e.v('actor_x','u16')<144);assert.ok(e.v('actor_y','u16')<112);
 }
 assert.ok(e.v('wolf_jumps','u16')>0);assert.ok(e.v('wolf_spits','u16')>3);
 assert.ok(worst<FRAME_LIMIT);console.log(`wolf stress peak=${worst}/${FRAME_LIMIT}`);
});
