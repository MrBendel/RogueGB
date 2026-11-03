// The town (the hub): a file starts there; its lift goes down into the descent,
// an area's entrance room's south door leads back up; a run that ends returns you there; Echo's
// first contact plays once per file.
import { test } from 'node:test';
import { convert } from '../tools/tileset.mjs';
const DETAIL_TILES = convert('sands').stored.length - 172;
import { assert, bootToTitle, settle, startDescent, takeLiftDown, leaveToTown, enterTownBuilding, leaveTownBuilding, TOWN_DOORS, TITLE, PLAY, RUN_OVER, MODE_TOWN, MODE_DESCENT } from './helpers.mjs';

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const EXP_ACTIVE = 59, STORY = 88, CREDITS = 7;  // Profile offsets (save.h)
const prof = (emu, at, len = 1) => emu.readBytes(A(emu, 'profile') + at, len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);

const newFileInTown = () => settle(bootToTitle().tap('START').tap('A').tap('A'), PLAY);
/** Wait out a play -> play change (a lift). */
function arrive(emu, mode) {
  emu.until((e) => e.v('play_mode') === mode, 60, 'the mode');
  emu.until((e) => e.v('in_transition'), 60, 'the scene change');
  return settle(emu, PLAY);
}

test('a new file starts in the town; its lift goes down into a new expedition', () => {
  const emu = newFileInTown();
  assert.equal(emu.v('play_mode'), MODE_TOWN);
  takeLiftDown(emu);
  assert.equal(emu.v('play_mode'), MODE_DESCENT);
  assert.equal(emu.v('level_map'), emu.v('area_start'));
  assert.equal(prof(emu, EXP_ACTIVE), 1);
});

test("the entrance room's south door goes back up with everything: no fee, the expedition over", () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'profile') + CREDITS, [200, 0, 0, 0]);
  leaveToTown(emu);
  assert.equal(prof(emu, CREDITS, 4), 200, 'no fee');
  assert.equal(prof(emu, EXP_ACTIVE), 0);
});

test('a run that ends goes back to the town (after the fee)', () => {
  const emu = startDescent().idle(4);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 250]);  // hurt for 250
  settle(emu.idle(2), RUN_OVER).idle(40).tap('START');
  arrive(emu, MODE_TOWN);
  assert.equal(emu.v('save_slot'), 0, 'still the file');
});

test("Echo's first contact at the relay plays once per file", () => {
  const emu = newFileInTown();
  w16(emu, 'player_x', 8 * 16);
  w16(emu, 'player_y', 4 * 16);
  emu.idle(2);
  assert.equal(emu.v('dlg_active'), 1, 'she speaks');
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.equal(prof(emu, STORY) & 1, 1, 'remembered');
  // Down and back up: the town again, and she doesn't repeat herself.
  takeLiftDown(emu);
  leaveToTown(emu);
  w16(emu, 'player_x', 8 * 16);
  w16(emu, 'player_y', 4 * 16);
  emu.idle(4);
  assert.equal(emu.v('dlg_active'), 0);
});

const PAGE_SHOP = 5, GUNS = 18, GUN_SIZE = 9, GUN_NONE = 0xff, PACK_SIZE = 89, PACK = 90;
const PISTOL = [0, 0, 1, 0, 0, 0, 0, 0, 0];
/** In town with credits, talking to Marro: his shop. */
function atShop(credits) {
  const emu = newFileInTown();
  emu.writeBytes(A(emu, 'zones_done'), [0xff]);
  emu.writeBytes(A(emu, 'profile') + CREDITS, [credits & 255, (credits >> 8) & 255, 0, 0]);
  enterTownBuilding(emu);
  w16(emu, 'player_x', 3 * 16);
  w16(emu, 'player_y', 3 * 16 - 4);
  emu.idle(4).tap('A').idle(4);
  return emu;
}
// Marro's price (loot.c gun_price): (15 + 5 level) doubled a rarity step.
const priceOf = (emu, i) => {
  const at = A(emu, 'shop_stock') + i * GUN_SIZE;
  const rarity = emu.read8(at + 1), level = emu.read8(at + 2);
  return (15 + 5 * level) << rarity;
};

test("Marro's shop: three guns at your level; A buys one into a free slot", () => {
  const emu = atShop(5000);
  assert.equal(emu.v('paused'), 1);
  assert.equal(emu.v('menu_page'), PAGE_SHOP);
  emu.shot('shop');
  const price = priceOf(emu, 0);
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'paid');
  assert.notEqual(emu.read8(A(emu, 'profile') + GUNS + GUN_SIZE), GUN_NONE, 'in the second slot');
  assert.equal(emu.v('shop_sold') & 1, 1, 'sold');
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'sold once');
  emu.tap('B').idle(2);
  assert.equal(emu.v('paused'), 0, 'B leaves');
});

test('not enough credits: nothing changes', () => {
  const emu = atShop(0);
  emu.tap('A').idle(2);
  assert.equal(emu.v('shop_sold'), 0);
  assert.equal(emu.v('guns_bought', 'u16'), 0);
});

test('a full loadout and backpack: buying trades in the held gun for a quarter of its price', () => {
  const emu = atShop(5000);
  // Three more pistols, the first one held; the backpack full of them too.
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, PISTOL);
  for (let s = 0; s < 4; s++) emu.writeBytes(A(emu, 'profile') + PACK + s * GUN_SIZE, PISTOL);
  const price = priceOf(emu, 0), held = (15 + 5 * 1) << 0;
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price + (held >> 2));
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), emu.read8(A(emu, 'shop_stock')), 'the held slot has it');
});

test('a full loadout: a bought gun goes into the backpack', () => {
  const emu = atShop(5000);
  for (let s = 1; s < 4; s++) emu.writeBytes(A(emu, 'profile') + GUNS + s * GUN_SIZE, PISTOL);
  const price = priceOf(emu, 0), type = emu.read8(A(emu, 'shop_stock'));
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 5000 - price, 'no trade-in');
  assert.equal(emu.read8(A(emu, 'profile') + PACK), type, 'in the backpack');
  assert.equal(emu.read8(A(emu, 'profile') + GUNS), 0, 'the pistol still held');
});

test('a bigger backpack from Marro: 4 slots, 8 for 300, 12 for 800, then no more', () => {
  const emu = atShop(1100);
  assert.equal(prof(emu, PACK_SIZE), 4, 'a new file starts with 4');
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);  // past the three guns
  assert.equal(emu.v('shop_sel'), 3, 'the upgrade row');
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 8);
  assert.equal(prof(emu, CREDITS, 4), 800);
  emu.shot('shop-upgrade');
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 12);
  assert.equal(prof(emu, CREDITS, 4), 0);
  for (let i = 0; i < 4; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 12, 'as big as it gets: the row is gone');
});

test("selling: RIGHT turns to the backpack's guns; A sells one for a quarter of its price", () => {
  const emu = atShop(0);
  emu.writeBytes(A(emu, 'profile') + PACK + GUN_SIZE, [1, 1, 3, 0, 0, 0, 0, 0, 0]);  // a green level-3 shotgun, second slot
  emu.tap('RIGHT').idle(2);
  assert.equal(emu.v('shop_mode'), 1);
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), 0, 'an empty slot: nothing');
  emu.tap('DOWN').idle(2);
  emu.shot('shop-sell');
  emu.tap('A').idle(2);
  assert.equal(prof(emu, CREDITS, 4), ((15 + 5 * 3) << 1) >> 2);
  assert.equal(emu.read8(A(emu, 'profile') + PACK + GUN_SIZE), GUN_NONE, 'gone');
  emu.tap('LEFT').idle(2);
  assert.equal(emu.v('shop_mode'), 0, 'back to buying');
});

test('a bigger backpack carries half again as much ammo on the next expedition', () => {
  const emu = atShop(300);
  for (let i = 0; i < 3; i++) emu.tap('DOWN').idle(2);
  emu.tap('A').idle(4).tap('B').idle(4);
  assert.equal(prof(emu, PACK_SIZE), 8);
  takeLiftDown(emu);
  assert.equal(prof(emu, 198), 108, 'pistol rounds: 72 and half again');
});

const HAS_SHIELD = 17;  // Profile offset: the shield's tier
/** In town with credits, in front of a vending machine beside Marro's shop (0: guns, 1: shields). */
function atMachine(credits, k) {
  const emu = newFileInTown();
  emu.writeBytes(A(emu, 'zones_done'), [0xff]);
  emu.writeBytes(A(emu, 'profile') + CREDITS, [credits & 255, (credits >> 8) & 255, 0, 0]);
  enterTownBuilding(emu);
  w16(emu, 'player_x', (7 + k) * 16);
  w16(emu, 'player_y', 5 * 16 - 4);
  emu.idle(60).hold('UP', 4).idle(4);  // (the camera catches up)
  assert.equal(emu.v('talk_near'), 1 + k, 'the machine in reach');
  return emu;
}

test("the town's gun machine: yellow; a random gun at your level for twice a common one's price, with its card", () => {
  const emu = atMachine(500, 0);
  emu.shot('town-vending');
  emu.tap('A').idle(4);
  const price = (15 + 5 * 1) * 2;
  assert.equal(prof(emu, CREDITS, 4), 500 - price, 'paid');
  assert.equal(emu.v('drop_kind'), 2, 'a gun on the floor');
  assert.equal(emu.read8(A(emu, 'floor_gun') + 2), 1, 'at your level');
  // Down onto it: its card, and A takes it into the second slot.
  emu.hold('DOWN', 20).idle(4);
  assert.notEqual(emu.v('card_drop'), 0xff, 'its card shows');
  emu.shot('town-vending-card');
  emu.tap('A').idle(4);
  assert.notEqual(emu.read8(A(emu, 'profile') + GUNS + GUN_SIZE), GUN_NONE, 'taken');
});

test("the town's shield machine: red; the shield, then bigger ones, kept by the file", () => {
  const emu = atMachine(2000, 1);
  assert.equal(emu.v('shield_cap'), 0, 'a new file has none');
  emu.tap('A').idle(4);
  assert.equal(prof(emu, HAS_SHIELD), 1);
  assert.equal(emu.v('shield_cap'), 25, 'worn at once');
  assert.equal(prof(emu, CREDITS, 4), 2000 - 100);
  for (let i = 0; i < 3; i++) emu.tap('A').idle(4);
  assert.equal(prof(emu, HAS_SHIELD), 4);
  assert.equal(emu.v('shield_cap'), 70);
  assert.equal(prof(emu, CREDITS, 4), 2000 - 100 - 250 - 500 - 900);
  emu.tap('A').idle(4);
  assert.equal(prof(emu, CREDITS, 4), 250, 'the biggest: nothing more to buy');
  // Down into the descent: worn there too, full.
  takeLiftDown(emu);
  assert.equal(emu.v('shield_cap'), 70);
  assert.equal(emu.v('shield_hp'), 70);
});

test("the town's machines: not enough credits, nothing changes", () => {
  for (const k of [0, 1]) {
    const emu = atMachine(10, k);
    emu.tap('A').idle(4);
    assert.equal(prof(emu, CREDITS, 4), 10);
    assert.equal(prof(emu, HAS_SHIELD), 0);
    assert.equal(emu.v('drop_kind'), 0);
  }
});


test('Sands town: connected streets reach every service, with solid buildings and borders', () => {
  const emu = newFileInTown();
  assert.equal(emu.v('room_w'), 16);
  assert.equal(emu.v('room_h'), 19);
  const map = emu.readBytes(A(emu, 'room_map'), 32 * 24);
  const seen = new Set([4 * 32 + 5]), queue = [...seen];
  for (let i = 0; i < queue.length; i++) {
    const k = queue[i], x = k % 32, y = Math.floor(k / 32);
    for (const [nx, ny] of [[x-1,y],[x+1,y],[x,y-1],[x,y+1]]) {
      const n = ny * 32 + nx;
      if (nx < 0 || nx >= 16 || ny < 0 || ny >= 19 || map[n] || seen.has(n)) continue;
      seen.add(n); queue.push(n);
    }
  }
  for (const [x,y] of [[7,4],[5,9],[11,7],[11,8],[7,12],[6,10],[3,12]])
    assert.ok(seen.has(y * 32 + x), `service approach ${x},${y} is connected`);
  for (let x = 0; x < 16; x++) {
    assert.equal(map[x], 1); assert.equal(map[18 * 32 + x], 1);
  }
  assert.equal(map[7 * 32 + 5], 1, 'house roof is solid');
  for (const [x,y] of [[1,5],[1,6],[4,3],[10,2],[13,4],[14,5],[6,5],[7,5],[14,9],[13,6],[14,6],[1,9],[10,8],[10,9]])
    assert.equal(map[y * 32 + x], 1, 'visible cliff face is solid');
  for (const [x,y] of [[8,5],[10,11],[13,11]])
    assert.equal(map[y * 32 + x], 0, 'ground below cliffs is walkable');
  emu.idle(60).shot('sands-town-arrival');
  emu.writeBytes(A(emu, 'zones_done'), [0xff]);
  for (const [name,x,y] of [['market',11,8],['yard',8,15],['containers',13,17],['homes',5,9]]) {
    assert.equal(map[y*32+x],0,`preview ${name} stands on walkable ground`);
    w16(emu, 'player_x', x * 16); w16(emu, 'player_y', y * 16);
    emu.idle(90).shot(`sands-town-${name}`);
  }
});

test('Sands detail tiles survive pause, portrait dialogue, and a round trip to the descent', () => {
  const emu = newFileInTown().idle(60);
  const detail = () => {
    return [...emu.snapshot().vram.slice(0x2000 + 0x1500, 0x2000 + 0x1500 + DETAIL_TILES * 16)];
  };
  const before = detail();
  assert.ok(new Set(before).size > 4, 'second-bank art is populated');
  emu.tap('START').idle(4).tap('START').idle(4);
  assert.equal(emu.v('paused'), 0);
  assert.deepEqual(detail(), before, 'pause retains detail art');
  emu.tap('SELECT').idle(4);
  assert.deepEqual(detail(), before, 'weapon-name glyphs retain detail art');
  w16(emu, 'player_x', 8 * 16); w16(emu, 'player_y', 4 * 16);
  emu.idle(2);
  assert.equal(emu.v('dlg_active'), 1);
  for (let i = 0; i < 60 && emu.v('dlg_active'); i++) emu.tap('A');
  assert.deepEqual(detail(), before, 'portrait retains detail art');
  takeLiftDown(emu); leaveToTown(emu);
  assert.deepEqual(detail(), before, 'return reloads detail art');
});


test('three intact facades lead into furnished rooms and back to their own doorstep', () => {
  const emu = newFileInTown().idle(40);
  const stock = [...emu.readBytes(A(emu, 'shop_stock'), 27)];
  emu.writeBytes(A(emu, 'player_hp'), [31]);
  const ammo = [...emu.readBytes(A(emu, 'weapon_ammo'), 5)];
  for (let k = 0; k < 3; k++) {
    enterTownBuilding(emu, k);
    assert.equal(emu.v('room_w'), 10); assert.equal(emu.v('room_h'), 8);
    assert.equal(emu.v('player_hp'), 31, 'doors do not heal');
    assert.deepEqual([...emu.readBytes(A(emu, 'weapon_ammo'), 5)], ammo, 'doors do not refill ammo');
    emu.shot(['town-quarters','town-relay','town-market-interior'][k]);
    emu.tap('START').idle(3).tap('START').idle(3);
    leaveTownBuilding(emu);
    assert.equal(emu.v('room_w'), 16);
    assert.equal(emu.v('player_x', 'u16'), TOWN_DOORS[k][0] * 16);
    assert.equal(emu.v('player_y', 'u16'), (TOWN_DOORS[k][1] + 1) * 16 + 4);
    assert.deepEqual([...emu.readBytes(A(emu, 'shop_stock'), 27)], stock, 'stock is not rerolled by a doorway');
  }
});

test('an uncollected vending gun stays inside its shop across other room visits', () => {
  const emu = atMachine(500, 0).tap('A').idle(4);
  const gun = [...emu.readBytes(A(emu, 'floor_gun'), 9)];
  assert.equal(emu.v('drop_kind'), 2);
  leaveTownBuilding(emu);
  assert.equal(emu.v('drop_kind'), 0, 'no indoor gun leaks into the yard');
  enterTownBuilding(emu, 0);
  assert.equal(emu.v('drop_kind'), 0, 'quarters have their own floor');
  leaveTownBuilding(emu);
  enterTownBuilding(emu, 2);
  assert.equal(emu.v('drop_kind'), 2, 'purchase is still waiting');
  assert.deepEqual([...emu.readBytes(A(emu, 'floor_gun'), 9)], gun);
});


test('rescued residents stay on open ground without blocking the compact town doors', () => {
  const emu = newFileInTown();
  emu.writeBytes(A(emu, 'profile') + 242, [15]);
  enterTownBuilding(emu, 0); leaveTownBuilding(emu);
  const authored = convert('sands').maps.town;
  const kinds = [...emu.readBytes(A(emu, 'talk_kind'), emu.v('talk_count'))];
  assert.equal(kinds.filter(k => k >= 8).length, 4);
  kinds.forEach((kind,i) => {
    if (kind < 8) return;
    const x = emu.read8(A(emu, 'talk_bx') + i), y = emu.read8(A(emu, 'talk_by') + i);
    assert.equal(authored.type[y * authored.w + x], 0, 'resident stands on floor');
    assert.ok(TOWN_DOORS.every(([dx,dy]) => x !== dx || y !== dy + 1), 'doorstep stays clear');
  });
  for (let k = 0; k < 3; k++) { enterTownBuilding(emu,k); leaveTownBuilding(emu); }
  emu.shot('town-residents');
});


test('saving inside a town building resumes safely outside', () => {
  const emu = enterTownBuilding(newFileInTown(), 2);
  emu.tap('START').idle(2).tap('SELECT');
  settle(emu, TITLE);
  settle(emu.tap('START').tap('A'), PLAY);
  assert.equal(emu.v('town_inside'), 0xff);
  assert.equal(emu.v('room_w'), 16);
  assert.equal(emu.v('play_mode'), MODE_TOWN);
});


test('tower tank is walk-behind, with solid feet at its base', () => {
  const e=newFileInTown().idle(40);
  e.writeBytes(A(e,'zones_done'),[255]);
  w16(e,'player_x',80);w16(e,'player_y',188);
  e.hold('RIGHT',22).idle(4);
  assert.ok(e.v('player_x','u16')>96,'walk under tank from the side');
  e.shot('town-behind-tower');
  const ts=convert('sands');
  for(const y of [12,13]) {
    assert.equal(ts.maps.town.type[y*16+6],0);
    const b=ts.maps.town.vis[y*16+6];
    const p=ts.pieces.find(p=>b>=p.first && b<p.first+p.w*p.h);
    assert.equal(ts.def.pieces[p.piece].behind,true);
    for(let q=0;q<4;q++) assert.ok(e.read8(A(e,'room_lut')+q*256+(b|128))&128,'runtime background priority');
  }
  assert.equal(ts.maps.town.type[14*16+6],1,'tower feet remain solid');
});

test('town cliff drops require a downward jump and finish safely after releasing controls', () => {
  const e=newFileInTown().idle(40);
  e.writeBytes(A(e,'zones_done'),[255]);
  for(const x of [4*16,11*16]) {
    w16(e,'player_x',x);w16(e,'player_y',144);
    w16(e,'player_vx',0);w16(e,'player_vy',0);
    e.hold('DOWN',20).idle(2);
    assert.equal(e.v('player_y','u16'),144,'walking stops at cliff');
    e.hold('DOWN+B',3);
    assert.ok(e.v('town_drop_end'),'downward jump starts a committed drop');
    e.idle(3).shot(x===64?'town-cliff-hop-left':'town-cliff-hop-right');
    e.idle(35);
    assert.equal(e.v('town_drop_end'),0);
    assert.equal(e.v('player_y','u16'),168,'land beyond the entire cliff face');
    assert.equal(e.v('player_air'),0);
    assert.equal(e.v('player_z','u16'),0);
    e.hold('UP',20).idle(2).hold('UP+B',30).idle(10);
    assert.ok(e.v('player_y','u16')>=168,'cannot climb back through the cliff');
  }
});


test('town containers block walking onto the top or through the door face', () => {
  const e=newFileInTown().idle(40);
  e.writeBytes(A(e,'zones_done'),[255]);
  w16(e,'player_x',176);w16(e,'player_y',176);
  e.hold('DOWN',50).idle(4);
  assert.equal(e.v('player_y','u16'),176,'rear container top remains solid');
  w16(e,'player_x',144);w16(e,'player_y',256);
  e.hold('RIGHT',50).idle(4);
  assert.ok(e.v('player_x','u16')<160,'foreground door face blocks walking');
});


test('town cliff gaps stop walking, with only the stairs crossing the central wall', () => {
  const e=newFileInTown().idle(40);
  e.writeBytes(A(e,'zones_done'),[255]);
  const place=(x,y)=>{
    w16(e,'player_x',x);w16(e,'player_y',y);
    w16(e,'player_vx',0);w16(e,'player_vy',0);
    e.writeBytes(A(e,'player_xs'),[0]);e.writeBytes(A(e,'player_ys'),[0]);
  };
  for(const [label,x,y,dir,axis,bound,less] of [
    ['central face from above',112,64,'DOWN','player_y',64,true],
    ['central face from below',112,88,'UP','player_y',88,false],
    ['eastern cap from above',224,128,'DOWN','player_y',128,true],
    ['eastern cap from side',208,144,'RIGHT','player_x',216,true],
    ['upper eastern base',224,104,'UP','player_y',104,false],
    ['western cliff foot',16,152,'UP','player_y',152,false],
    ['attached ledge side',176,136,'LEFT','player_x',172,false]
  ]) {
    place(x,y);e.hold(dir,35).idle(3);
    const actual=e.v(axis,'u16');
    assert.ok(less?actual<=bound:actual>=bound,`${label}: ${actual} vs ${bound}`);
  }
  place(128,64);e.hold('DOWN',32).idle(3);
  assert.ok(e.v('player_y','u16')>=88,'actual stairs connect both sides');
});
