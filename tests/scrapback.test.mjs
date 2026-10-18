import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v);
const put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(){
 const e=bootToTitle().tap('DOWN').tap('A').tap('DOWN').idle(9);
 assert.equal(e.v('title_choice'),1);e.shot('scrapback-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
test('scrapback: miniboss has a larger body, armor, warning and exposed wall crash',()=>{
 const e=trial();assert.equal(e.v('boss_kind'),5);assert.equal(e.v('boss_hp'),24);
 assert.equal(e.v('actor_guard'),1);assert.equal(e.v('play_actor_oam'),15);
 for(let i=11;i<17;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4));
 e.shot('scrapback-stalk');e.until(x=>x.v('boss_state')===1);e.shot('scrapback-warning');
 e.until(x=>x.v('boss_state')===2);e.hold('RIGHT',15);e.shot('scrapback-charge');
 e.until(x=>x.v('boss_state')===0,95);assert.equal(e.v('actor_guard'),0);
 assert.equal(e.v('scrap_crashes','u16'),1);assert.ok(e.v('boss_timer')>=98);
 e.idle(2);e.shot('scrapback-exposed');const hp=e.v('actor_hp');
 e.hold('A',85);assert.ok(e.v('actor_hp')<hp,'starter pistol punishes the crash');
});
test('scrapback: arbitrary-angle charges preserve their slope and never track a dodge',()=>{
 const e=trial();put16(e,'actor_x',40);put16(e,'actor_y',32);
 put16(e,'player_x',109);put16(e,'player_y',79);put(e,'boss_state',1);put(e,'boss_timer',2);
 e.idle(3);assert.equal(e.v('boss_state'),2);
 const vx=e.v('actor_vx','i16'),vy=e.v('actor_vy','i16');
 assert.ok(vx>0&&vy>0&&vx!==vy,'not snapped to cardinal or 45-degree directions');
 assert.ok(Math.abs(vx*47-vy*69)<100,'launch slope aims at the actual player position');
 const x=e.v('actor_x','u16'),y=e.v('actor_y','u16');
 e.hold('LEFT',6);
 assert.equal(e.v('actor_vx','i16'),vx);assert.equal(e.v('actor_vy','i16'),vy);
 assert.ok(e.v('actor_x','u16')>x&&e.v('actor_y','u16')>y);
 assert.equal(e.v('scrap_target_x'),109);assert.equal(e.v('scrap_target_y'),79);
});
test('scrapback: the closed shell rejects shots and a crash opens real projectile damage',()=>{
 const e=trial();
 const shoot=()=>{
  e.writeBytes(at(e,'shots'),[12,0,0,0,0,e.v('actor_x','u16')+4,0,0,0,0,0,e.v('actor_y','u16'),0]);
  put(e,'shot_count',1);put(e,'shot_damage',1);e.idle(3);
 };
 shoot();assert.equal(e.v('actor_hp'),24);
 put(e,'shots',0);put(e,'shot_count',0);
 e.until(x=>x.v('boss_state')===0,180);shoot();assert.equal(e.v('actor_hp'),23);
});
test('scrapback: charges in all quadrants stop inside the full body footprint',()=>{
 for(const [x,y] of [[20,20],[124,20],[20,92],[124,92]]) {
  const e=trial();put16(e,'actor_x',72);put16(e,'actor_y',56);
  put16(e,'player_x',x);put16(e,'player_y',y);put(e,'boss_state',1);put(e,'boss_timer',1);
  e.idle(2);e.until(x=>x.v('boss_state')===0,100);
  assert.ok(e.v('actor_x','u16')>=20&&e.v('actor_x','u16')<=124);
  assert.ok(e.v('actor_y','u16')>=20&&e.v('actor_y','u16')<=92);
 }
});
test('scrapback: pause freezes a charge and defeat clears the larger sprite',()=>{
 const e=trial();e.until(x=>x.v('boss_state')===2).idle(2).tap('START');
 const x=e.v('actor_x','u16'),y=e.v('actor_y','u16'),t=e.v('boss_timer');e.idle(60);
 assert.equal(e.v('actor_x','u16'),x);assert.equal(e.v('actor_y','u16'),y);assert.equal(e.v('boss_timer'),t);
 e.tap('START');put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);
 assert.equal(e.v('boss_state'),5);assert.equal(e.v('boss_reward'),1);
 for(let i=11;i<17;i++)assert.equal(e.read8(at(e,'shadow_OAM')+i*4),0);
});
test('scrapback: moving and jumping can win with normal health and the starter pistol',()=>{
 const e=trial();let worst=0,last=0;
 for(let f=0;f<3600&&e.v('boss_state')!==5&&e.v('player_hp');f++) {
  const state=e.v('boss_state'),x=e.v('player_x','u16'),keys=['A'];
  if((f/70|0)&1){if(x>22)keys.push('LEFT');}else if(x<122)keys.push('RIGHT');
  if(state===2&&last!==2)keys.push('B');
  last=state;e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
 }
 assert.equal(e.v('boss_state'),5,`player=${e.v('player_hp')} boss=${e.v('boss_hp')}`);
 assert.ok(worst<FRAME_LIMIT);e.shot('scrapback-victory');
 console.log(`Scrapback victory peak=${worst}/${FRAME_LIMIT}`);
});
test('scrapback: standing still and holding fire cannot win',()=>{
 const e=trial();
 for(let f=0;f<2400&&e.v('player_hp')&&e.v('boss_state')!==5;f++)e.hold('A');
 assert.equal(e.v('player_hp'),0);assert.notEqual(e.v('boss_state'),5);assert.equal(e.v('death_cause'),14);
});
