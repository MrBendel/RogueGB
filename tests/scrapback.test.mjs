import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT,RUN_OVER,TITLE} from './helpers.mjs';
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
 const e=trial();let worst=0;
 for(let f=0;f<3600&&e.v('boss_state')!==5&&e.v('player_hp');f++) {
  const state=e.v('boss_state'),x=e.v('player_x','u16'),keys=['A'];
  if((f/70|0)&1){if(x>22)keys.push('LEFT');}else if(x<122)keys.push('RIGHT');
  // Respond to the visible threats: jump as the boss approaches a wall or
  // before crossing an active pool, rather than blindly jumping at launch.
  let jump=false;
  if(state===2) {
   const bx=e.v('actor_x','u16'),by=e.v('actor_y','u16');
   const vx=e.v('actor_vx','i16')/256*3,vy=e.v('actor_vy','i16')/256*3;
   const impact=Math.min(vx?((vx>0?124:20)-bx)/vx:999,vy?((vy>0?92:20)-by)/vy:999);
   jump=impact<9;
  }
  for(let i=0;i<e.v('scrap_pool_count');i++)if(e.read8(at(e,'scrap_pool_painted')+i)===2) {
   const px=e.read8(at(e,'scrap_pool_x')+i),py=e.read8(at(e,'scrap_pool_y')+i);
   if(Math.abs(x-px)<22&&Math.abs(e.v('player_y','u16')-py)<22)jump=true;
  }
  if(jump&&!e.v('player_air'))keys.push('B');
  e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
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

const poolTile=(e,i=0)=>0x9800+(e.read8(at(e,'scrap_pool_y')+i)>>3)*32+(e.read8(at(e,'scrap_pool_x')+i)>>3);
function seededPool(e) {
 put(e,'boss_state',0);put(e,'boss_timer',100);
 put(e,'scrap_pool_count',1);put(e,'scrap_pool_x',32);put(e,'scrap_pool_y',64);
 put(e,'scrap_pool_age',0);put(e,'scrap_pool_painted',0);
 put16(e,'player_x',32);put16(e,'player_y',64);
 put(e,'player_invuln',0);put(e,'player_blink',0);put(e,'shield_hp',40);
}
test('scrapback acid: charge deposits a bounded trail which lasts through recovery and fades',()=>{
 const e=trial();e.until(x=>x.v('boss_state')===2).idle(6);e.shot('scrapback-acid-fizz');
 assert.ok(e.v('scrap_pool_count')>=1);assert.equal(e.v('scrap_pool_painted'),1);
 e.until(x=>x.v('boss_state')===0,100).idle(8);
 const count=e.v('scrap_pool_count');assert.ok(count>=2&&count<=4);
 assert.equal(e.v('scrap_pool_painted'),2);e.shot('scrapback-acid-trail');
 e.until(x=>x.v('boss_timer')===12,110).idle(4);e.shot('scrapback-acid-fade');
 assert.equal(e.v('scrap_pool_painted'),3);
 e.until(x=>x.v('boss_state')===9,20).idle(6);
 for(let i=0;i<count;i++) {
  assert.equal(e.read8(at(e,'scrap_pool_painted')+i),0);
  assert.ok(e.read8(poolTile(e,i))<144,'floor art is restored before the next charge');
 }
});
test('scrapback acid: fizz is safe, active acid hurts, jump clears it, pause freezes it',()=>{
 const e=trial();seededPool(e);e.idle(12);
 assert.equal(e.v('shield_hp'),40);assert.equal(e.v('scrap_pool_painted'),1);
 e.tap('START');const age=e.v('scrap_pool_age');e.idle(60);assert.equal(e.v('scrap_pool_age'),age);
 e.tap('START').idle(15);assert.equal(e.v('shield_hp'),32);assert.equal(e.v('scrap_acid_hits','u16'),1);
 put(e,'player_invuln',0);put(e,'player_blink',0);put(e,'shield_hp',40);
 put16(e,'player_x',32);put16(e,'player_y',64);put16(e,'player_z',4096);put(e,'player_air',1);put16(e,'player_vz',0);
 e.idle(3);assert.equal(e.v('shield_hp'),40,'airborne crossing avoids acid');
 put(e,'boss_timer',10);put16(e,'player_z',0);put(e,'player_air',0);put16(e,'player_vz',0);
 e.idle(3);assert.equal(e.v('shield_hp'),40,'fading acid is harmless');
});
test('scrapback acid: full shot pool cannot hide hazards, and defeat restores the floor',()=>{
 const e=trial();seededPool(e);const floor=e.read8(poolTile(e));
 put(e,'scrap_pool_age',30);put16(e,'player_x',112);e.idle(3);
 for(let n=0;n<16;n++)e.writeBytes(at(e,'shots')+13*n,[40,0,0,0,0,24,0,0,0,0,0,80,0]);
 put(e,'shot_count',16);e.idle(3);assert.ok([148,152,156].includes(e.read8(poolTile(e))));
 assert.ok(e.lastFrameCycles<FRAME_LIMIT);e.shot('scrapback-acid-crowded');
 put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(8);
 assert.equal(e.v('boss_state'),5);assert.equal(e.read8(poolTile(e)),floor);
 assert.equal(e.v('scrap_pool_painted'),0);
});

function impact(e,air=false,near=false) {
 put16(e,'actor_x',123);put16(e,'actor_y',32);
 put16(e,'actor_vx',256);put16(e,'actor_vy',0);put(e,'actor_xs',0);put(e,'actor_ys',0);
 put(e,'boss_state',2);put(e,'boss_timer',90);
 put16(e,'player_x',near?112:40);put16(e,'player_y',near?32:80);put16(e,'player_vx',0);put16(e,'player_vy',0);
 put(e,'player_air',0);put16(e,'player_z',0);put16(e,'player_vz',0);
 put(e,'player_blink',0);put(e,'player_invuln',0);put(e,'cam_shake',0);
 if(air)e.hold('B');else e.idle();
 assert.equal(e.v('boss_state'),0);
}
test('scrapback impact: sustained shake, bounded recoil and grounded knockdown',()=>{
 const e=trial();impact(e);
 assert.ok(e.v('cam_shake')>=22&&e.v('cam_shake')<=24);
 assert.equal(e.v('scrap_rebound_timer'),8);assert.equal(e.v('player_stun'),24);
 const bx=e.v('actor_x','u16'),x=e.v('player_x','u16'),y=e.v('player_y','u16'),shots=e.v('shots_fired','u16');
 e.hold('RIGHT+A+B',8);
 assert.equal(e.v('actor_x','u16'),bx-8);assert.equal(e.v('scrap_rebound_timer'),0);
 assert.equal(e.v('player_x','u16'),x);assert.equal(e.v('player_y','u16'),y);
 assert.equal(e.v('player_air'),0);assert.equal(e.v('shots_fired','u16'),shots);
 e.shot('scrapback-knockdown');
 const rebound=e.v('actor_x','u16');e.hold('RIGHT+A',8);
 assert.equal(e.v('actor_x','u16'),rebound,'rebound settles instead of sliding forever');
 e.hold('RIGHT',15);assert.equal(e.v('player_stun'),0);assert.ok(e.v('player_x','u16')>x);
});
test('scrapback impact: jumping avoids stun; pause freezes stun and rebound; acid cannot chain-hit',()=>{
 const air=trial();impact(air,true);assert.equal(air.v('player_stun'),0);
 const contact=trial();impact(contact,false,true);
 assert.equal(contact.v('shield_hp'),20,'physical charge contact resolves before knockdown protection');
 assert.equal(contact.v('player_stun'),24);
 const e=trial();impact(e);e.tap('START');const t=e.v('player_stun'),r=e.v('scrap_rebound_timer');
 e.idle(60);assert.equal(e.v('player_stun'),t);assert.equal(e.v('scrap_rebound_timer'),r);e.tap('START');
 put(e,'scrap_pool_count',1);put(e,'scrap_pool_x',40);put(e,'scrap_pool_y',80);
 put(e,'scrap_pool_age',30);put(e,'scrap_pool_painted',2);put(e,'shield_hp',40);
 e.idle(5);assert.equal(e.v('shield_hp'),40,'knockdown protects against unavoidable acid damage');
 e.until(x=>!x.v('player_stun'),40).tap('B');
 assert.equal(e.v('player_air'),1);assert.equal(e.v('shield_hp'),40,'getting up allows a jump before acid can hit');
});
test('scrapback impact: leaving a stunned fight resets player control on the next encounter',()=>{
 const e=trial();impact(e);e.tap('START').tap('SELECT');
 settle(e,RUN_OVER).idle(35).tap('START');settle(e,TITLE);
 settle(e.tap('SELECT'),PLAY).idle(3);assert.equal(e.v('player_stun'),0);
 const x=e.v('player_x','u16');e.hold('RIGHT',10);assert.ok(e.v('player_x','u16')>x);
});


test('scrapback acid: three bubbling frames loop, pause freezes animation, palettes fade toward floor',()=>{
 const e=trial();seededPool(e);put(e,'scrap_pool_age',30);put16(e,'player_x',112);
 const frames=new Set();let worst=0;
 for(let n=0;n<32;n++) {
  e.idle();if(n>3)frames.add(e.read8(poolTile(e)));
  worst=Math.max(worst,e.lastFrameCycles);
 }
 assert.deepEqual([...frames].sort(),[148,152,156]);
 // Distinct pixel data, not just different tile indices.
 const art=[148,152,156].map(t=>Array.from({length:64},(_,i)=>e.read8(0x8000+t*16+i)).join(','));
 assert.equal(new Set(art).size,3);
 e.tap('START');const tick=e.v('scrap_acid_tick');e.idle(45);
 assert.equal(e.v('scrap_acid_tick'),tick);e.tap('START');
 for(const [timer,palette] of [[13,4],[9,3],[5,2]]) {
  put(e,'boss_timer',timer);e.idle(2);
  assert.equal(e.v('scrap_pool_visual')&7,palette);
  assert.equal(e.v('scrap_pool_painted'),3);
  e.shot(`scrapback-acid-fade-${palette}`);
 }
 const floor=e.v('pal_base_bg','u16');
 const distance=c=>[0,5,10].reduce((n,shift)=>n+Math.abs(((c>>shift)&31)-((floor>>shift)&31)),0);
 for(let color=1;color<4;color++) {
  let previous=100;
  for(const palette of [5,4,3,2]) {
   const d=distance((e.read8(at(e,'pal_base_bg')+(palette*4+color)*2)|(e.read8(at(e,'pal_base_bg')+(palette*4+color)*2+1)<<8)));
   assert.ok(d<previous);previous=d;
  }
 }
 assert.ok(worst<FRAME_LIMIT);
});


test('scrapback impact: screen-sized arena visibly shakes and settles without moving world positions',()=>{
 const e=trial();impact(e);
 const px=e.v('player_x','u16'),py=e.v('player_y','u16');
 const offsets=new Set();let moved=false,lateMovement=false;
 for(let f=0;f<28;f++) {
  const x=e.v('cam_x','u16'),y=e.v('cam_y','u16');
  offsets.add(`${x},${y}`);moved ||= x!==0||y!==0;
  if(f>=8 && (e.read8(0xff43)||e.read8(0xff42)))lateMovement=true;
  assert.ok(x<=3&&y<=3,'impact shake stays within the drawn border');
  assert.equal(e.v('scroll_x'),x);assert.equal(e.v('scroll_y'),y);
  assert.equal(e.read8(e.symbols.addr('shadow_OAM')+3*4+1),px+8-x,'player shares background shake');
  if(x||y)e.shot('scrapback-visible-shake');
  e.idle();
 }
 assert.ok(lateMovement,'hardware scroll keeps shaking beyond the old eight-frame duration');
 assert.ok(moved,'impact actually offsets the rendered view');assert.ok(offsets.size>1);
 assert.equal(e.v('cam_x','u16'),0);assert.equal(e.v('cam_y','u16'),0);
 assert.equal(e.v('player_x','u16'),px);assert.equal(e.v('player_y','u16'),py);
 // Respect the existing shake-strength option in compact arenas too.
 put(e,'cam_shake_max',0);impact(e);e.idle(3);
 assert.equal(e.v('cam_x','u16'),0);assert.equal(e.v('cam_y','u16'),0);
});
