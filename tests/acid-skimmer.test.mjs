import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n);
const put=(e,n,v,i=0)=>e.write8(at(e,n)+i,v);
const put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,v>>8]);
function trial(){
 const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('UP').idle(9);
 assert.equal(e.v('title_choice'),9);e.shot('acid-skimmer-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
function alive(e){put(e,'player_hp',60);put(e,'shield_hp',40);}

test('acid skimmer: menu selection, hover animation and a shadow',()=>{
 const e=trial();assert.equal(e.v('enemy_trial_kind'),9);assert.equal(e.v('actor_hp'),8);
 assert.equal(e.v('actor_scripted'),8);assert.equal(e.v('actor_guard'),0);
 const tiles=new Set();for(let f=0;f<20;f++){e.idle();tiles.add(e.v('actor_t0'));}
 assert.deepEqual([...tiles].sort((a,b)=>a-b),[96,100]);
 assert.equal(e.read8(at(e,'shadow_OAM')+15*4+2),118);
 e.shot('acid-skimmer-hover');
});

test('acid skimmer: locks a harmless warning, visibly spits and leaves a persistent pool',()=>{
 const e=trial();e.until(x=>x.v('acid_state')===1);
 const x=e.v('acid_target_x'),y=e.v('acid_target_y');
 e.idle(2).shot('acid-skimmer-warning');
 e.hold('RIGHT',25);assert.equal(e.v('acid_target_x'),x);assert.equal(e.v('acid_target_y'),y);
 assert.equal(e.v('shield_hp'),40);
 e.idle().until(x=>x.v('acid_state')===2).idle(10).shot('acid-skimmer-spit');
 assert.equal(e.v('acid_spits','u16'),1);
 e.until(x=>x.v('acid_pool_life')>0);
 assert.equal(e.v('acid_pool_life'),180);
 assert.equal(e.v('acid_pool_x'),x);assert.equal(e.v('acid_pool_y'),y);
 e.idle(2).shot('acid-skimmer-pool');assert.equal(e.v('shield_hp'),40,'sidestep avoids acid');
 put16(e,'player_x',x);put16(e,'player_y',y);e.idle();
 assert.equal(e.v('shield_hp'),34,'pool deals six damage');
 e.idle(10);assert.equal(e.v('shield_hp'),34,'pool has a damage tick cooldown');
});

test('acid skimmer: airborne players clear acid; pause freezes spit and pool timers',()=>{
 const e=trial();e.until(x=>x.v('acid_state')===2);
 e.tap('START');const t=e.v('acid_timer'),gx=e.v('acid_glob_x','i16');e.idle(90);
 assert.equal(e.v('acid_timer'),t);assert.equal(e.v('acid_glob_x','i16'),gx);
 e.tap('START').idle().until(x=>x.v('acid_pool_life')>0);
 alive(e);put(e,'acid_hit_timer',0);put(e,'player_invuln',0);put(e,'player_blink',0);
 put16(e,'player_x',e.v('acid_pool_x'));put16(e,'player_y',e.v('acid_pool_y'));
 put16(e,'player_z',4096);put(e,'player_air',1);put16(e,'player_vz',0);
 e.idle();assert.equal(e.v('shield_hp'),40,'height clears the pool');
 e.tap('START');const life=e.v('acid_pool_life');e.idle(60);assert.equal(e.v('acid_pool_life'),life);
});

test('acid skimmer: pools expire, stay bounded and remain visible with full shots',()=>{
 const e=trial();
 for(let f=0;f<400;f++){alive(e);e.idle();}
 assert.ok(e.v('acid_spits','u16')>=3);
 for(let i=0;i<2;i++)assert.ok(e.read8(at(e,'acid_pool_life')+i)<=180);
 put(e,'acid_state',0);put(e,'acid_timer',250);
 put(e,'acid_pool_life',3,0);put(e,'acid_pool_life',0,1);e.idle(3);
 assert.equal(e.v('acid_pool_life'),0);
 put(e,'acid_pool_life',100);put(e,'acid_pool_x',64);put(e,'acid_pool_y',80);
 for(let n=0;n<16;n++)e.writeBytes(at(e,'shots')+13*n,[40,0,0,0,0,24,0,0,0,0,0,64,0]);
 put(e,'shot_count',16);e.idle(2);
 for(let i=16;i<18;i++)assert.ok(e.read8(at(e,'shadow_OAM')+i*4),'pool keeps its reserved sprite');
 e.shot('acid-skimmer-crowded');
});

test('acid skimmer: ordinary pistol fire wins, clears acid, and fits the frame budget',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<1800&&!e.v('enemy_trial_cleared');f++){
  const x=e.v('player_x','u16');const keys=['A'];
  if((f/90|0)&1){if(x>24)keys.push('LEFT');}else if(x<120)keys.push('RIGHT');
  e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
  assert.ok(e.v('player_hp'));assert.ok(e.v('sprites_used')<=40);
 }
 assert.equal(e.v('enemy_trial_cleared'),1);assert.equal(e.v('acid_pool_life'),0);
 assert.equal(e.read8(at(e,'acid_pool_life')+1),0);
 for(let i=15;i<23;i++)assert.equal(e.read8(at(e,'shadow_OAM')+i*4),0);
 assert.ok(worst<FRAME_LIMIT);console.log(`acid skimmer peak=${worst}/${FRAME_LIMIT}`);
});
