import { test } from 'node:test';
import { convert } from '../tools/tileset.mjs';
import { assert, legacyTownFixture, boot, bootToTitle, settle, takeLiftDown, PLAY, TITLE, RUN_OVER, FRAME_LIMIT } from './helpers.mjs';

const A = (e,n) => e.symbols.addr(n);
const w8 = (e,n,v) => e.write8(A(e,n),v);
const w16 = (e,n,v) => e.writeBytes(A(e,n),[v&255,v>>8]);
const pos = (e,x,y) => {
  w16(e,'player_x',x);w16(e,'player_y',y);
  w16(e,'player_vx',0);w16(e,'player_vy',0);
  w8(e,'player_xs',0);w8(e,'player_ys',0);
};
function town() {
  const e=legacyTownFixture(settle(bootToTitle().tap('START').tap('A').tap('A'),PLAY)).idle(40);
  w8(e,'zones_done',255);
  return e;
}
function depart(e=town()) {
  pos(e,7*16,17*16);
  e.hold('DOWN',1).until(e=>e.v('world_id')===1 && e.v('play_mode')===1,150,'desert departure');
  return settle(e,PLAY).idle(35);
}
function cross(e,dir,map) {
  pos(e,dir==='RIGHT'? 19*16-8 : dir==='LEFT'? 8:9*16,dir==='UP'?16:8*16);
  e.hold(dir,1).until(e=>e.v('level_map')===map,130,'surface map transition');
  return e.idle(45);
}
function returnTown(e) {
  pos(e,9*16,16);
  e.hold('UP',1).until(e=>e.v('play_mode')===6,130,'town return');
  return settle(e,PLAY).idle(35);
}
function assertFeet(e) {
  const x=e.v('player_x','u16'),y=e.v('player_y','u16'),map=e.readBytes(A(e,'room_map'),768);
  for(const px of [x+3,x+12])for(const py of [y+8,y+15])
    assert.equal(map[(py>>4)*32+(px>>4)],0,`arrival hitbox at ${px},${py} is clear`);
}

function walkTo(e,tx,ty) {
  const w=e.v('room_w'),h=e.v('room_h'),m=e.readBytes(A(e,'room_map'),768);
  const start=((e.v('player_y','u16')+12)>>4)*32+((e.v('player_x','u16')+8)>>4);
  const goal=ty*32+tx,q=[start],prev=new Map([[start,null]]);
  for(let i=0;i<q.length&&!prev.has(goal);i++){
    const x=q[i]%32,y=q[i]>>5;
    for(const [nx,ny]of [[x-1,y],[x+1,y],[x,y-1],[x,y+1]]){
      const n=ny*32+nx;
      if(nx<0||ny<0||nx>=w||ny>=h||m[n]||prev.has(n))continue;
      prev.set(n,q[i]);q.push(n);
    }
  }
  assert.ok(prev.has(goal),'walkable route exists');
  const path=[];for(let p=goal;p!==null;p=prev.get(p))path.unshift(p);
  for(const p of path){
    const x=(p%32)*16,y=(p>>5)*16-4;
    for(let f=0;f<60;f++){
      const dx=x-e.v('player_x','u16'),dy=y-e.v('player_y','u16');
      if(Math.abs(dx)<=1&&Math.abs(dy)<=1)break;
      e.hold(Math.abs(dx)>1?(dx>0?'RIGHT':'LEFT'):(dy>0?'DOWN':'UP'),1);
    }
    assert.ok(Math.abs(x-e.v('player_x','u16'))<=1&&Math.abs(y-e.v('player_y','u16'))<=1,`walk to ${p%32},${p>>5}`);
    e.idle(5);
  }
}

test('desert: the whole town-to-Rustyard route and return can be walked without teleporting',()=>{
  const e=town();
  walkTo(e,7,18);e.shot('desert-town-road');
  e.hold('DOWN',1).until(e=>e.v('world_id')===1&&e.v('play_mode')===1,100,'walk out of town');
  settle(e,PLAY).idle(35);
  walkTo(e,18,8);
  e.hold('RIGHT',1).until(e=>e.v('level_map')===6,100,'walk into Rustyard').idle(45);
  assertFeet(e);
  e.hold('LEFT',1).until(e=>e.v('level_map')===5,100,'walk back to Crossroads').idle(45);
  walkTo(e,9,1);
  e.hold('UP',1).until(e=>e.v('play_mode')===6,100,'walk back home');
  settle(e,PLAY).idle(30);assertFeet(e);
});

test('desert art: exits, cache and camp slots connect without increasing the tile budget',()=>{
  const ts=convert('sands');
  assert.ok(ts.stored.length<=220);assert.ok(ts.blocks.length<=128);
  for(const [name,start,targets] of [
    ['crossroads',[9,0],[[19,8],[3,10],[9,6],[9,10]]],
    ['rustyard',[0,8],[[11,6],[15,9],[11,11],[15,11]]],
  ]) {
    const m=ts.maps[name],q=[start],seen=new Set([start.join(',')]);
    for(let i=0;i<q.length;i++)for(const [dx,dy]of [[-1,0],[1,0],[0,-1],[0,1]]){
      const [x,y]=[q[i][0]+dx,q[i][1]+dy],key=`${x},${y}`;
      if(x<0||y<0||x>=m.w||y>=m.h||m.type[y*m.w+x]||seen.has(key))continue;
      seen.add(key);q.push([x,y]);
    }
    for(const p of targets)assert.ok(seen.has(p.join(',')),`${name}: ${p} reachable`);
    // Every walkable border belongs to a real connection.
    for(let y=0;y<m.h;y++)for(let x=0;x<m.w;x++)if((!x||!y||x===m.w-1||y===m.h-1)&&!m.type[y*m.w+x])
      assert.ok(name==='crossroads' ? ((!y||y===15)&&x>=8&&x<12)||(x===19&&y>=7&&y<11) : !x&&y>=7&&y<11);
  }
});

test('desert: town road, both surface transitions, clear arrivals and the unchanged town lift',()=>{
  const e=town();w8(e,'player_hp',23);
  depart(e);assert.equal(e.v('level_map'),5);assert.equal(e.v('player_hp'),23);assertFeet(e);
  assert.equal(e.v('level_camps'),0,'crossroads is a safe orientation map');
  e.idle(20).shot('desert-crossroads-entry');
  cross(e,'RIGHT',6);assertFeet(e);
  assert.equal(String.fromCharCode(...e.readBytes(A(e,'hud_text'),7)),'RUSTYRD','region label survives the banked HUD call');
  assert.equal(e.v('level_camps'),1);assert.equal(e.v('camp_state'),0,'arrival outside camp wake range');
  e.shot('desert-rustyard-entry');
  cross(e,'LEFT',5);assertFeet(e);
  returnTown(e);assertFeet(e);
  assert.equal(e.v('player_x','u16'),112);assert.equal(e.v('player_y','u16'),272);
  assert.equal(e.v('player_hp'),23,'round trip does not heal');
  takeLiftDown(e);assert.equal(e.v('world_id'),0,'lift still starts generated lunar descent');
  assert.ok(e.v('area_maps')>=6);
});

test('desert: pause map, weapon labels and loot card preserve the second-bank scenery',()=>{
  const e=depart();
  const detail=()=>[...e.snapshot().vram.slice(0x3500,0x3500+42*16)];
  const before=detail();
  e.tap('START').idle(4);e.shot('desert-overworld-map');e.tap('START').idle(4);
  assert.deepEqual(detail(),before);
  e.tap('SELECT').idle(4);assert.deepEqual(detail(),before);
  pos(e,3*16,9*16+8);e.idle(20);
  assert.equal(e.v('chest_open_bits')&1,1,'cache opens');
  e.until(e=>e.v('card_drop')!==255,50,'cache gun card');e.shot('desert-cache-loot');
  e.tap('A').idle(4);pos(e,8*16,12*16);e.idle(15);
  const guns=[...e.readBytes(A(e,'profile')+18,36)];
  assert.deepEqual(detail(),before,'closing card keeps ground detail');
  cross(e,'RIGHT',6);cross(e,'LEFT',5);
  assert.equal(e.v('chest_open_bits')&1,1,'cache stays open after backtracking');
  returnTown(e);
  assert.deepEqual([...e.readBytes(A(e,'profile')+18,36)],guns,'salvaged guns return home');
});

test('desert: guarded Rustyard camp wakes, fits the budget and remains cleared after backtracking',()=>{
  const e=cross(depart(),'RIGHT',6);
  pos(e,15*16,11*16);w8(e,'player_invuln',255);e.idle(5);
  assert.equal(e.v('chest_open_bits'),0,'guarded reward stays closed');
  assert.equal(e.v('camp_state'),1);
  let peak=0;
  for(let f=0;f<200;f++){
    w8(e,'player_invuln',255);e.idle(1);
    peak=Math.max(peak,e.lastFrameCycles);
    assert.ok(e.v('actor_count')<=8);assert.ok(e.v('sprites_used')<=40);
  }
  assert.ok(peak<FRAME_LIMIT,`surface fight peak=${peak}`);
  e.shot('desert-rustyard-camp');
  // Clear the squad and its queued reinforcements through the ordinary death
  // path, then exercise the real chest gate and state persistence.
  for(let f=0;f<700 && e.v('camp_state')!==2;f++){
    w8(e,'player_invuln',255);
    for(let a=0;a<8;a++)if(e.read8(A(e,'actor_type')+a))e.write8(A(e,'actor_hp')+a,0);
    w8(e,'shots_killed',1);
    e.idle(1);
  }
  assert.equal(e.v('camp_state'),2,'all reinforcements exhausted');
  pos(e,15*16,10*16+8);e.idle(20);
  assert.equal(e.v('chest_open_bits')&1,1,'reward opens after victory');
  cross(e,'LEFT',5);cross(e,'RIGHT',6);
  assert.equal(e.v('camp_state'),2);assert.equal(e.v('chest_open_bits')&1,1);
  assert.equal(e.v('actor_count'),0);
});

test('desert: battery save resumes the authored region; death returns to town',()=>{
  const e=cross(depart(),'RIGHT',6);
  const again=boot();again.setSram(e.sram());settle(again,TITLE);
  settle(again.tap('START').tap('A'),PLAY).idle(35);
  assert.equal(again.v('world_id'),1);assert.equal(again.v('level_map'),6);assertFeet(again);
  w8(again,'debug_cmd',4);w8(again,'debug_type',250);
  settle(again.idle(2),RUN_OVER).idle(40).tap('START');
  settle(again,PLAY).idle(30);
  assert.equal(again.v('play_mode'),6);assert.equal(again.v('town_inside'),255);
});
