import { test } from 'node:test';
import { convert } from '../tools/tileset.mjs';
import { assert, boot, bootToTitle, legacyTownFixture, settle, PLAY, TITLE, RUN_OVER, FRAME_LIMIT } from './helpers.mjs';
const A=(e,n)=>e.symbols.addr(n);
const w8=(e,n,v)=>e.write8(A(e,n),v);
const w16=(e,n,v)=>e.writeBytes(A(e,n),[v&255,v>>8]);
const profile=(e,off)=>e.read8(A(e,'profile')+off);
const fresh=()=>settle(bootToTitle().tap('START').tap('A').tap('A'),PLAY);
function close(e){for(let i=0;i<60&&e.v('dlg_active');i++)e.tap('A').idle(1);assert.equal(e.v('dlg_active'),0);return e;}
function wake(e=fresh()){
  e.until(e=>e.v('dlg_active'),100,'unknown entity contact');
  return close(e);
}
function shield(e=wake()){
  e.hold('UP',1).until(e=>profile(e,17)===1,100,'first shield');
  e.idle(1).until(e=>e.v('dlg_active'),30,'shield explanation');
  return close(e);
}
function resume(e){
  const r=boot();r.setSram(e.sram());settle(r,TITLE);
  return settle(r.tap('START').tap('A'),PLAY).idle(3);
}
function place(e,x,y){w16(e,'player_x',x);w16(e,'player_y',y);w16(e,'player_vx',0);w16(e,'player_vy',0);}
function north(e,done){place(e,144,16);e.hold('UP',1).until(done,130,'north road');return e.idle(45);}

test('opening: wake prone in a quiet desert, hear the unknown entity, then find the shield',()=>{
  const e=fresh();
  assert.equal(e.v('world_id'),1);assert.equal(e.v('level_map'),9);
  assert.equal(e.v('shield_cap'),0);assert.equal(profile(e,17),0);
  assert.equal(profile(e,14),0,'surface opening does not count as underground depth');
  assert.ok(e.v('opening_timer')>0);assert.equal(e.v('player_stun'),1);
  const x=e.v('player_x','u16'),py=e.v('player_y','u16'),map=e.readBytes(A(e,'room_map'),768);
  for(const dx of [3,12])for(const dy of [8,15])assert.equal(map[((py+dy)>>4)*32+((x+dx)>>4)],0,'wake feet clear of cliffs');
  e.shot('opening-wake');
  const y=e.v('player_y','u16');e.hold('UP+A',15);
  assert.equal(e.v('player_y','u16'),y,'wake pose holds briefly');
  assert.equal(e.v('actor_count'),0,'safe starting pocket');
  e.idle(1).until(e=>e.v('dlg_active'),100,'first contact');
  e.tap('A').idle(2);e.shot('opening-first-contact');close(e);
  assert.ok(profile(e,88)&4,'contact remembered');
  assert.equal(e.v('player_stun'),0);
  shield(e);
  assert.equal(e.v('shield_cap'),25);assert.equal(e.v('shield_hp'),25);
  assert.equal(profile(e,17),1,'permanent tier-one shield');
  assert.equal(e.v('chest_open_bits')&1,1);
  assert.equal(e.v('actor_count'),0,'shield explanation precedes combat');
  e.shot('opening-first-shield');
  e.tap('START').idle(4);e.shot('opening-objective-map');e.tap('START').idle(4);
});

test('opening art: the shield sits on the only pass from the wake pocket toward town',()=>{
  const m=convert('sands').maps.wake;
  assert.equal(m.w,20);assert.equal(m.h,24);
  const q=[[9,21]],seen=new Set(['9,21']);
  for(let i=0;i<q.length;i++)for(const [dx,dy]of [[1,0],[-1,0],[0,1],[0,-1]]){
    const x=q[i][0]+dx,y=q[i][1]+dy,k=`${x},${y}`;
    // Remove the shield's 24px pickup region: there must be no bypass.
    if(x<0||y<0||x>=20||y>=24||m.type[y*20+x]||seen.has(k)||(Math.abs(x-9)<=1&&Math.abs(y-18)<=1))continue;
    seen.add(k);q.push([x,y]);
  }
  assert.ok(!seen.has('9,0'),'cannot walk around the shield on the way north');
  for(const [x,y]of [[7,12],[10,11],[12,13],[7,5],[11,5],[14,7]])assert.equal(m.type[y*20+x],0,'thrower spawn on floor');
});

test('opening: two groups of three rocklings attack; their rocks and fight fit the hardware budget',()=>{
  const e=shield();place(e,160,144);w8(e,'player_invuln',255);e.idle(5);
  assert.equal(e.v('actor_count'),6);
  assert.ok([...e.readBytes(A(e,'actor_type'),8)].filter(Boolean).every(v=>v===2),'rocklings use the ordinary shooter actor slots');
  assert.ok([...e.readBytes(A(e,'actor_elite'),8)].every(v=>v===0),'no random elites in the first fight');
  e.idle(40);e.writeBytes(A(e,'actor_hp'),new Uint8Array(8).fill(255));
  let peak=0;
  for(let f=0;f<240;f++){
    w8(e,'player_invuln',255);e.hold('A',1);peak=Math.max(peak,e.lastFrameCycles);
    assert.ok(e.v('actor_count')<=6);assert.ok(e.v('sprites_used')<=40);
  }
  assert.ok(e.v('bullets_fired','u16')>0,'throwers actually attack');
  assert.ok(peak<FRAME_LIMIT,`opening peak=${peak}`);
  assert.equal(e.v('rockling_on'),1);
  assert.ok([...e.readBytes(A(e,'actor_scripted'),6)].every(v=>v===13),'Cragback-style rock controllers');
  e.shot('opening-rockling-groups');
});

test('opening: the shield pass and north road can be traversed without moving the player by debug writes',()=>{
  const e=shield();
  e.hold('RIGHT',1).until(e=>e.v('player_x','u16')>=160,60,'walk beside the supply case');
  e.idle(5);
  for(let f=0;f<650&&e.v('level_map')===9;f++){
    w8(e,'player_invuln',255);e.hold('UP',1);
  }
  e.shot('opening-walk-route');
  assert.equal(e.v('level_map'),5,`walked from shield to Crossroads: ${e.v('player_x','u16')},${e.v('player_y','u16')}`);
  assert.equal(profile(e,17),1);
});

test('opening: shield and contact survive reload and death; first arrival completes the introduction',()=>{
  const e=shield();
  const r=resume(e);
  assert.equal(r.v('level_map'),9);assert.equal(r.v('shield_cap'),25);
  assert.equal(r.v('opening_timer'),0);assert.equal(r.v('dlg_active'),0,'no repeated contact');
  w8(r,'debug_cmd',4);w8(r,'debug_type',250);
  settle(r.idle(2),RUN_OVER).idle(40).tap('START');
  settle(r,PLAY).idle(3);
  assert.equal(r.v('play_mode'),1);assert.equal(r.v('level_map'),9,'death cannot skip to town');
  assert.equal(r.v('shield_cap'),25,'found shield retained');
  assert.equal(r.v('opening_timer'),0,'retry skips the wake-up');
  north(r,e=>e.v('level_map')===5);
  north(r,e=>e.v('play_mode')===6);settle(r,PLAY);
  r.until(e=>e.v('dlg_active'),50,'town arrival message');r.tap('A').idle(2);r.shot('opening-town-arrival');close(r);
  assert.equal(profile(r,88)&2,0,'opening finished');assert.ok(profile(r,88)&1,'old first-contact scene suppressed');
  assert.equal(r.v('shield_cap'),25);
  const home=resume(r);assert.equal(home.v('play_mode'),6);assert.equal(home.v('opening_timer'),0);
});

test('opening: established profiles keep their town start without receiving a free shield',()=>{
  const e=legacyTownFixture(fresh());
  e.tap('START').tap('SELECT');settle(e,TITLE); // persist the established-profile fixture
  const r=resume(e);
  assert.equal(r.v('play_mode'),6);assert.equal(profile(r,88)&2,0);
  assert.equal(profile(r,17),0);assert.equal(r.v('opening_timer'),0);
});

function loneRockling() {
  const e=shield();place(e,160,144);w8(e,'player_invuln',255);e.idle(45);
  for(let i=1;i<8;i++)e.write8(A(e,'actor_type')+i,0);
  w8(e,'actor_count',1);w16(e,'actor_x',120);w16(e,'actor_y',120);
  w8(e,'actor_state',0);w8(e,'actor_timer',1);w8(e,'actor_scripted',13);
  w8(e,'player_invuln',0);w8(e,'player_blink',0);w8(e,'player_stun',0);
  e.idle(1);return e;
}
test('rocklings: warn, lock a target, lob a rock, and pause safely',()=>{
  const e=loneRockling();
  assert.equal(e.v('actor_state'),1);
  const tx=e.v('actor_vx','u16'),ty=e.v('actor_vy','u16');
  e.idle(2).shot('opening-rockling-windup');
  e.tap('START');const t=e.v('actor_timer');e.idle(40);
  assert.equal(e.v('actor_timer'),t,'paused wind-up');e.tap('START');
  e.hold('RIGHT',20).idle(1);
  assert.equal(e.v('actor_vx','u16'),tx);assert.equal(e.v('actor_vy','u16'),ty,'target does not home');
  e.until(e=>e.v('actor_state')===2,50,'rock released');e.idle(12);
  e.shot('opening-rockling-flight');
  const hp=e.v('shield_hp');e.until(e=>e.v('actor_state')===3,40,'rock lands');
  assert.equal(e.v('shield_hp'),hp,'moving away avoids the landing');
  e.idle(2).shot('opening-rockling-impact');
});
test('rocklings: a landing hurts grounded players, jumping clears it, and a kill cancels the throw',()=>{
  for(const airborne of [false,true]) {
    const e=loneRockling();
    w8(e,'actor_state',2);w8(e,'actor_timer',1);
    if(airborne){w8(e,'player_air',1);w16(e,'player_z',16*256);}
    const hp=e.v('shield_hp');e.idle(1);
    assert.equal(e.v('shield_hp'),airborne?hp:hp-12);
  }
  const e=loneRockling();w8(e,'actor_type',0);w8(e,'actor_count',0);
  const hp=e.v('shield_hp'),fired=e.v('bullets_fired','u16');e.idle(90);
  assert.equal(e.v('shield_hp'),hp);assert.equal(e.v('bullets_fired','u16'),fired);
});
test('rocklings: landing coordinates work below the first 256 pixels of the desert',()=>{
  const e=loneRockling();place(e,160,264);w16(e,'actor_y',224);
  w8(e,'actor_state',0);w8(e,'actor_timer',1);e.idle(1);
  assert.equal(e.v('actor_vy','u16'),276,'world-space landing point does not wrap');
});
