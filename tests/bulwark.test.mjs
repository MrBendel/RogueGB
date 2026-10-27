import {test} from 'node:test';
import {assert,bootToTitle,settle,PLAY,FRAME_LIMIT} from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n),put=(e,n,v)=>e.write8(at(e,n),v),put16=(e,n,v)=>e.writeBytes(at(e,n),[v&255,(v>>8)&255]);
function trial(){const e=bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A').tap('UP').idle(9);assert.equal(e.v('title_choice'),5);e.shot('bulwark-menu');return settle(e.tap('A'),PLAY).idle(3);}
function shot(e,x,y,vx,vy,n=0){e.writeBytes(at(e,'shots')+13*n,[60,vx&255,(vx>>8)&255,vx<0?255:0,0,x,0,vy&255,(vy>>8)&255,vy<0?255:0,0,y,0]);put(e,'shot_count',n+1);put(e,'shot_damage',2);}
function fixed(e,face=1,state=0){put16(e,'actor_x',72);put16(e,'actor_y',60);put(e,'bulwark_state',state);put(e,'bulwark_timer',200);put(e,'bulwark_turn',200);put(e,'bulwark_face',face);}
test('bulwark: directional shield blocks frontal shots in all four directions, including fast volleys',()=>{
 const dirs=[[1,0],[0,1],[-1,0],[0,-1]];
 for(let face=0;face<4;face++) {
  const e=trial();fixed(e,face);const [dx,dy]=dirs[face];
  for(let n=0;n<3;n++)shot(e,76+dx*22,60+dy*22,-dx*3072,-dy*3072,n);
  e.idle(2);assert.equal(e.v('actor_hp'),24);assert.equal(e.v('bulwark_blocks','u16'),3);assert.equal(e.v('shot_count'),0);e.shot(`bulwark-shield-${face}`);
 }
});
test('bulwark: side and rear shots hurt; lowering the shield exposes the front',()=>{
 for(const kind of ['rear','side','open']) {
  const e=trial();fixed(e,1,kind==='open'?3:0);
  if(kind==='rear')shot(e,76,48,0,512);
  else if(kind==='side')shot(e,60,60,512,0);
  else shot(e,76,72,0,-512);
  e.idle(8);assert.ok(e.v('actor_hp')<24,kind);assert.equal(e.v('bulwark_blocks','u16'),0);
 }
});
test('bulwark: warning locks aim, three-round burst opens shield, pause freezes recovery',()=>{
 const e=trial();e.until(t=>t.v('bulwark_state')===1,90);e.shot('bulwark-warning');
 const vx=e.v('actor_vx','i16'),vy=e.v('actor_vy','i16');e.hold('RIGHT',12);assert.equal(e.v('actor_vx','i16'),vx);assert.equal(e.v('actor_vy','i16'),vy);
 e.until(t=>t.v('bulwark_state')===2,30).idle(17);assert.equal(e.v('bulwark_shots','u16'),3);e.shot('bulwark-burst');
 assert.equal(e.read8(at(e,'shadow_OAM')+11*4),0);e.until(t=>t.v('bulwark_state')===3,15);
 e.tap('START');const t=e.v('bulwark_timer');e.idle(30);assert.equal(e.v('bulwark_timer'),t);
});
test('bulwark: close bash is warned, jump avoids damage, defeat clears projectiles and shield',()=>{
 for(const air of [false,true]) {
  const e=trial();put16(e,'actor_x',e.v('player_x','u16')-18);put16(e,'actor_y',e.v('player_y','u16')-12);put(e,'bulwark_timer',1);e.idle();
  assert.equal(e.v('bulwark_state'),4);assert.equal(e.v('shield_hp'),40);e.until(t=>t.v('bulwark_state')===5,25);
  if(air)e.hold('B',12);else e.idle(12);assert.equal(e.v('shield_hp'),air?40:24);assert.equal(e.v('bulwark_state'),3);
  put(e,'actor_hp',0);put(e,'shots_killed',1);e.idle(4);assert.equal(e.v('enemy_trial_cleared'),1);assert.equal(e.v('bullet_count'),0);assert.equal(e.read8(at(e,'shadow_OAM')+11*4),0);
 }
});
test('bulwark: circling and shooting can win with the starter pistol within hardware budgets',()=>{
 const e=trial();let peak=0,waypoint=0;const route=[[120,96],[120,32],[24,32],[24,96]];
 for(let f=0;f<5000&&!e.v('enemy_trial_cleared')&&e.v('player_hp');f++) {
  const keys=['A'],x=e.v('player_x','u16'),y=e.v('player_y','u16'),[tx,ty]=route[waypoint];
  if(Math.abs(x-tx)<5&&Math.abs(y-ty)<5)waypoint=(waypoint+1)%4;
  if(x<tx-3)keys.push('RIGHT');else if(x>tx+3)keys.push('LEFT');if(y<ty-3)keys.push('DOWN');else if(y>ty+3)keys.push('UP');
  if((e.v('bulwark_state')===2||e.v('bulwark_state')===5)&&!e.v('player_air'))keys.push('B');
  e.hold(keys);peak=Math.max(peak,e.lastFrameCycles);assert.ok(e.v('sprites_used')<=40);
 }
 assert.equal(e.v('enemy_trial_cleared'),1,`hp=${e.v('player_hp')} enemy=${e.v('actor_hp')}`);assert.ok(peak<FRAME_LIMIT,`peak ${peak}`);e.shot('bulwark-victory');console.log(`Bulwark peak ${peak}/${FRAME_LIMIT}`);
});

test('bulwark: shield also blocks frontal sword swings while rear swings land',()=>{
 for(const rear of [false,true]) {
  const e=trial();for(let n=0;n<4;n++)e.tap('SELECT');
  fixed(e,1);put16(e,'player_x',72);put16(e,'player_y',rear?42:78);
  for(let f=0;f<45&&e.v('actor_hp');f++) {
   put16(e,'actor_x',72);put16(e,'actor_y',60);e.hold('A');
  }
  if(rear)assert.ok(e.v('actor_hp')<24,'rear sword hit');else assert.equal(e.v('actor_hp'),24,'front sword blocked');
 }
});
