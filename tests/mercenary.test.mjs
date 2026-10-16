import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,TITLE,RUN_OVER,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v);
const put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,v>>8]);
function trial(){
 const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('DOWN').idle(9);
 assert.equal(e.v('title_choice'),1);e.shot('mercenary-menu');
 return settle(e.tap('A'),PLAY).idle(3);
}
const healthy=e=>{put(e,'player_hp',60);put(e,'shield_hp',40);};
test('mercenary: supplied walk frames, aimed rifle, six-round bursts and reload',()=>{
 const e=trial();assert.equal(e.v('actor_scripted'),9);assert.equal(e.v('actor_hp'),28);
 const x=e.v('actor_x','u16');e.idle(12);assert.notEqual(e.v('actor_x','u16'),x);
 assert.ok(e.read8(at(e,'shadow_OAM')+16*4));e.shot('mercenary-strafe');
 e.until(x=>x.v('merc_state')===1);e.shot('mercenary-aim');
 e.until(x=>x.v('merc_state')===2);
 const aim=e.v('merc_locked_aim');e.hold('RIGHT',20);
 assert.equal(e.v('merc_locked_aim'),aim,'burst commits to its warned direction');
 e.until(x=>x.v('merc_state')===3);assert.equal(e.v('merc_shots','u16'),6);
 e.shot('mercenary-reload');const shots=e.v('merc_shots','u16');
 assert.ok(e.read8(at(e,'shadow_OAM')+19*4),'reload meter is visible');
 e.idle(28);assert.equal(e.v('merc_shots','u16'),shots,'reload creates a firing gap');
});
test('mercenary: reacts to approaching shots, commits a roll, then recovers without immunity',()=>{
 const e=trial();put(e,'merc_dodge_cooldown',0);put(e,'merc_timer',100);
 // Slow projectile approaching from below, visible before the dodge decision.
 e.writeBytes(at(e,'shots'),[90,0,0,0,0,e.v('actor_x','u16')+4,0,0,255,255,0,e.v('actor_y','u16')+24,0]);
 put(e,'shot_count',1);
 e.until(x=>x.v('merc_state')===4,15);e.shot('mercenary-dodge-warn');
 const dx=e.v('merc_roll_x','i8'),dy=e.v('merc_roll_y','i8');
 e.until(x=>x.v('merc_state')===5,15).idle(5).shot('mercenary-roll');
 assert.equal(e.v('actor_guard'),0);assert.ok(e.v('actor_t0')>=144);
 const pos=e.v('actor_x','u16');e.hold('RIGHT',3);
 assert.equal(e.v('merc_roll_x','i8'),dx);assert.equal(e.v('merc_roll_y','i8'),dy);
 if(dx)assert.notEqual(e.v('actor_x','u16'),pos);
 e.until(x=>x.v('merc_state')===6,20);assert.ok(e.v('merc_timer')>=16);
 assert.ok(e.v('merc_dodge_cooldown')>90);
 const x=e.v('actor_x','u16'),y=e.v('actor_y','u16');e.idle(8);
 assert.equal(e.v('actor_x','u16'),x);assert.equal(e.v('actor_y','u16'),y);
});
test('mercenary: pause freezes AI and movement, boundaries constrain fast rolls',()=>{
 const e=trial();e.tap('START');const t=e.v('merc_timer'),x=e.v('actor_x','u16');
 e.idle(60);assert.equal(e.v('merc_timer'),t);assert.equal(e.v('actor_x','u16'),x);e.tap('START');
 put16(e,'actor_x',16);put16(e,'actor_y',32);put(e,'merc_state',5);put(e,'merc_timer',14);
 put(e,'merc_roll_x',255);put(e,'merc_roll_y',0);e.idle(14);
 assert.ok(e.v('actor_x','u16')>=14);assert.equal(e.v('merc_state'),6);
});
test('mercenary: movement and pistol can win with normal health and bounded CPU',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<2400&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++){
  const x=e.v('player_x','u16');const keys=['A'];
  if((f/75|0)&1){if(x>20)keys.push('LEFT');}else if(x<122)keys.push('RIGHT');
  if(f%55===0)keys.push('B');
  e.hold(keys);worst=Math.max(worst,e.lastFrameCycles);
 }
 assert.equal(e.v('enemy_trial_cleared'),1,`hp=${e.v('player_hp')} enemy=${e.v('actor_hp')}`);
 assert.ok(worst<FRAME_LIMIT);e.idle(2);
 assert.equal(e.v('bullet_count'),0);
 for(let i=15;i<23;i++)assert.equal(e.read8(at(e,'shadow_OAM')+i*4),0);
 console.log(`mercenary victory peak=${worst}/${FRAME_LIMIT}`);
});
test('mercenary: standing still without fighting is punished',()=>{
 const e=trial();e.until(x=>!x.v('player_hp'),1500);
 assert.equal(e.v('player_hp'),0);assert.ok(e.v('merc_shots','u16')>=12);
});
test('mercenary: holding fire without dodging loses the starting-position duel',()=>{
 const e=trial();
 for(let f=0;f<1800&&e.v('player_hp')&&!e.v('enemy_trial_cleared');f++)e.hold('A');
 assert.equal(e.v('player_hp'),0,'standing and holding fire must not solve this duel');
 assert.equal(e.v('enemy_trial_cleared'),0);
 assert.equal(e.v('death_cause'),12);
});
test('mercenary: long fight keeps pools and frame time bounded',()=>{
 const e=trial();let worst=0;
 for(let f=0;f<1200;f++){
  healthy(e);put(e,'actor_hp',100);e.hold('A');worst=Math.max(worst,e.lastFrameCycles);
  assert.ok(e.v('bullet_count')<=20);assert.ok(e.v('sprites_used')<=40);
 }
 assert.ok(e.v('merc_rolls','u16')>0);assert.ok(worst<FRAME_LIMIT);
 console.log(`mercenary stress peak=${worst}/${FRAME_LIMIT}`);
});
test('mercenary: returning to Acid Skimmer resets projectile settings and art',()=>{
 const e=trial();assert.equal(e.v('enemy_bullet_damage'),16);
 e.tap('START').tap('SELECT');settle(e,RUN_OVER).idle(35).tap('START');settle(e,TITLE);
 e.tap('DOWN').tap('DOWN').tap('DOWN').tap('A').idle(9);
 settle(e.tap('A'),PLAY).idle(3);
 assert.equal(e.v('actor_scripted'),8);assert.equal(e.v('actor_hp'),8);
 assert.equal(e.v('enemy_bullet_damage'),8);assert.equal(e.v('enemy_bullet_cause'),3);
 assert.equal(e.v('merc_shots','u16'),0);
});
