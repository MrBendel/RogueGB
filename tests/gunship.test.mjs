import { test } from 'node:test';
import { assert, bootToTitle, settle, PLAY, RUN_OVER, FRAME_LIMIT } from './helpers.mjs';
const at=(e,n)=>e.symbols.addr(n);
const put=(e,n,v,i=0)=>e.write8(at(e,n)+i,v);
const put16=(e,n,v,i=0)=>e.writeBytes(at(e,n)+i*2,[v&255,v>>8]);
const read=(e,n,i=0)=>e.read8(at(e,n)+i);
function trial() {
  const e=bootToTitle().tap('DOWN').tap('DOWN').tap('A').tap('DOWN').idle(9);
  e.shot('gunship-menu');
  return settle(e.tap('A'),PLAY).idle(3);
}

test('gunship: selectable large body, guarded core and bounded sprite allocation',()=>{
  const e=trial();
  assert.equal(e.v('boss_kind'),3);
  assert.equal(e.v('play_mode'),5);
  assert.equal(e.v('boss_hp'),40);
  assert.equal(e.v('boss_state'),6);
  assert.equal(e.v('actor_count'),1);
  assert.equal(read(e,'actor_guard'),1);
  e.shot('gunship-cruise');
  const oam=at(e,'shadow_OAM');
  const tiles=[];
  for(let i=15;i<40;i++) if(e.read8(oam+i*4)) tiles.push(e.read8(oam+i*4+2));
  for(const tile of [96,98,100,102,104,110,112,114]) assert.ok(tiles.includes(tile),`ship column ${tile}`);
  e.hold('A',40);
  assert.equal(read(e,'actor_hp'),40);
  assert.notEqual(e.v('aim_target'),0);
});

test('gunship: marked bombing point stays fixed and can be dodged',()=>{
  const e=trial();
  e.until(x=>x.v('gunship_bomb_timer')>0,60);
  const x=e.v('gunship_bomb_x'),y=e.v('gunship_bomb_y');
  assert.equal(x,72); assert.equal(y,96);
  e.idle(2).shot('gunship-bomb-warning');
  e.hold('RIGHT',25);
  assert.equal(e.v('gunship_bomb_x'),x);
  assert.equal(e.v('gunship_bomb_y'),y);
  e.idle().until(x=>x.v('gunship_bomb_live')===1,40);
  e.idle(3).shot('gunship-bomb-blast');
  assert.equal(e.v('shield_hp'),40,'leaving the marked floor avoids damage');
  const hit=trial();
  hit.until(x=>x.v('gunship_bomb_live')===1,100).idle(2);
  assert.equal(hit.v('shield_hp'),28,'a marked explosion deals 12 damage');
});

test('gunship: laser warning is harmless; live sweep hurts and opens the core',()=>{
  const e=trial();
  put16(e,'player_x',24); put16(e,'player_y',24); // clear of bombing floor
  e.until(x=>x.v('boss_state')===7,220);
  put16(e,'player_x',30); put16(e,'player_y',80);
  put(e,'shield_hp',40); put(e,'player_hp',60);
  e.idle(20).shot('gunship-laser-warning');
  assert.equal(e.v('shield_hp'),40);
  assert.equal(read(e,'actor_guard'),1);
  e.until(x=>x.v('boss_state')===8,60);
  assert.equal(read(e,'actor_guard'),0);
  e.idle(12).shot('gunship-laser-live');
  assert.ok(e.v('shield_hp')<40,'crossing the active beam hurts');
  e.until(x=>x.v('boss_state')===0,110);
  assert.equal(read(e,'actor_guard'),0);
  assert.ok(e.v('boss_timer')>=118);
  e.idle(2).shot('gunship-core-open');
});

test('gunship: pause freezes hazards; half health shortens the next laser warning',()=>{
  const e=trial();
  e.until(x=>x.v('gunship_bomb_timer')>0,60).tap('START');
  const t=e.v('boss_timer'),b=e.v('gunship_bomb_timer');
  e.idle(90);
  assert.equal(e.v('boss_timer'),t); assert.equal(e.v('gunship_bomb_timer'),b);
  e.tap('START');
  put(e,'actor_hp',20);
  put16(e,'player_x',24); put16(e,'player_y',24);
  e.until(x=>x.v('boss_state')===7,220);
  assert.equal(e.v('boss_phase'),1);
  assert.equal(e.v('boss_timer'),48);
});

test('gunship: victory removes large body and all hazards without repeat rewards',()=>{
  const e=trial();
  e.until(x=>x.v('gunship_bomb_timer')>0,60);
  put(e,'actor_hp',0); put(e,'shots_killed',1);
  e.idle(3);
  assert.equal(e.v('boss_state'),5); assert.equal(e.v('boss_reward'),1);
  assert.equal(e.v('actor_count'),0);
  const oam=at(e,'shadow_OAM');
  for(let i=15;i<40;i++) assert.equal(e.read8(oam+i*4),0,`sprite ${i} cleared`);
  const hp=e.v('player_hp'), shield=e.v('shield_hp');
  e.idle(100);
  assert.equal(e.v('boss_reward'),1);
  assert.equal(e.v('player_hp'),hp); assert.ok(e.v('shield_hp')>=shield);
  e.shot('gunship-victory');
});

test('gunship: normal movement and starter pistol can win within hardware budgets',()=>{
  const e=trial(),points=[[120,96],[120,24],[24,24],[24,96]];
  let p=0,worst=0,peakSprites=0;
  for(let f=0;f<6500 && e.v('boss_state')!==5;f++) {
    const [x,y]=points[p];
    const dx=x-e.v('player_x','u16'),dy=y-e.v('player_y','u16');
    if(Math.abs(dx)<4 && Math.abs(dy)<4) p=(p+1)&3;
    const keys=['A'];
    if(Math.abs(dx)>=4) keys.push(dx>0?'RIGHT':'LEFT');
    if(Math.abs(dy)>=4) keys.push(dy>0?'DOWN':'UP');
    e.hold(keys,1);
    worst=Math.max(worst,e.lastFrameCycles);
    peakSprites=Math.max(peakSprites,e.v('sprites_used'));
    assert.ok(e.v('player_hp')>0,'patrol survives with normal health and shields');
    assert.ok(e.v('sprites_used')<=40);
  }
  assert.equal(e.v('boss_state'),5,'starter pistol finishes the fight');
  assert.ok(e.v('shots_hit','u16')>0);
  assert.ok(worst<FRAME_LIMIT,`${worst}/${FRAME_LIMIT}`);
  console.log(`gunship: worst=${worst}/${FRAME_LIMIT}, sprites=${peakSprites}`);
});

test('gunship: all laser segments remain visible with a full player-shot pool',()=>{
  const e=trial();
  put16(e,'player_x',24); put16(e,'player_y',24);
  e.until(x=>x.v('boss_state')===8,260);
  const shots=at(e,'shots');
  for(let i=0;i<16;i++) {
    const x=24+(i%4)*24,y=64+Math.floor(i/4)*8;
    e.writeBytes(shots+i*13,[120,0,0,0,0,x,0,0,0,0,0,y,0]);
  }
  put(e,'shot_count',16);
  e.idle(3);
  const oam=at(e,'shadow_OAM');
  const ys=[];
  for(let i=15;i<25;i++) if(e.read8(oam+i*4+2)===116 && e.read8(oam+i*4)) ys.push(e.read8(oam+i*4)-16);
  assert.deepEqual(ys,[48,64,80,96]);
  assert.ok(e.v('sprites_used')<=40);
  assert.ok(e.lastFrameCycles<FRAME_LIMIT);
  e.shot('gunship-crowded-laser');
});
