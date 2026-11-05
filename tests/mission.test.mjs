// Missions (mission.c, board.c): the contracts board in town posts jobs; taken
// ones count down in the descent (kills of a kind, data cores from camps, a
// miniboss) and are turned in back at the board; a debt is paid there. Carried
// cores are lost if you die; kills are kept.
// (The board's cursor moves are 2-frame presses: moving redraws the job's details,
// which takes more than a frame, so a 1-frame tap can land while it's busy.)
import { test } from 'node:test';
import { assert, bootToTitle, settle, takeLiftDown, PLAY, RUN_OVER, MODE_TOWN } from './helpers.mjs';

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const CREDITS = 7, LEVEL = 1;  // Profile offsets (save.h) ...
const MIS_KIND = 227, MIS_TARGET = 230, MIS_NEED = 233, MIS_HAVE = 236, MIS_SEED = 239, MIS_TAKEN = 241;  // ... the missions'
const CULL = 1, RETRIEVE = 2, BOUNTY = 3, DEBT = 4, RESCUE = 5, PAGE_BOARD = 7, SLIME = 1;
const prof = (emu, at, len = 1) => emu.readBytes(A(emu, 'profile') + at, len).reduce((v, x, i) => v + x * 2 ** (8 * i), 0);
const setProf = (emu, at, bytes) => emu.writeBytes(A(emu, 'profile') + at, bytes);
const job = (emu, s, kind, target, need, have = 0) => {
  setProf(emu, MIS_KIND + s, [kind]);
  setProf(emu, MIS_TARGET + s, [target]);
  setProf(emu, MIS_NEED + s, [need]);
  setProf(emu, MIS_HAVE + s, [have]);
};

const newFileInTown = () => {
  const emu = settle(bootToTitle().tap('START').tap('A').tap('A'), PLAY);
  emu.writeBytes(A(emu, 'zones_done'), [0xff]);  // (no first contact)
  return emu;
};
/** In town, at the board's terminal (block 7, 10): A opens it. */
function atBoard(emu) {
  w16(emu, 'player_x', 6 * 16);
  w16(emu, 'player_y', 10 * 16 - 4);
  emu.idle(4).tap('A').idle(4);
  assert.equal(emu.v('menu_page'), PAGE_BOARD, 'the board open');
  return emu;
}

test('the board posts four jobs; A takes one into a free slot, once', () => {
  const emu = atBoard(newFileInTown());
  emu.shot('board');
  emu.tap('A').idle(2);
  const kind = prof(emu, MIS_KIND);
  assert.ok(kind >= CULL && kind <= RESCUE, `a job taken (${kind})`);
  assert.equal(prof(emu, MIS_TAKEN) & 1, 1, 'marked taken');
  emu.tap('A').idle(2);
  assert.equal(prof(emu, MIS_KIND + 1), 0, 'not twice');
  // Three at most.
  for (const k of [1, 2, 3]) emu.hold('DOWN', 2).idle(2).tap('A').idle(2);
  assert.deepEqual([0, 1, 2].map((s) => prof(emu, MIS_KIND + s) !== 0), [true, true, true]);
  assert.equal(prof(emu, MIS_TAKEN) & 8, 0, 'the fourth stays up: no slot');
  // B leaves.
  emu.tap('B').idle(4);
  assert.equal(emu.v('paused'), 0);
});

test('offers fit how far you have been: area 1 only asks for its own kinds and boss', () => {
  const emu = newFileInTown();
  for (let seed = 1; seed < 400; seed += 37) {
    setProf(emu, MIS_SEED, [seed & 255, seed >> 8]);
    atBoard(emu);
    for (let k = 0; k < 4; k++) {
      // (read back through a take: the offer lands in slot 0)
      setProf(emu, MIS_KIND, [0]);
      setProf(emu, MIS_TAKEN, [0]);
      emu.tap('A').idle(2);
      const kind = prof(emu, MIS_KIND), target = prof(emu, MIS_TARGET);
      if (kind === CULL) assert.ok([1, 2].includes(target), `area 1's kinds (${target})`);
      if (kind === BOUNTY) assert.equal(target, 0, 'the Claimjaw');
      if (kind === DEBT) assert.ok(prof(emu, MIS_NEED) >= 3);
      emu.hold('DOWN', 2).idle(2);
    }
    emu.tap('B').idle(4);
  }
});

test('a cull job counts kills of its kind in the descent; turned in, it pays', () => {
  const emu = newFileInTown();
  job(emu, 0, CULL, SLIME, 2);
  takeLiftDown(emu);
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  for (let n = 0; n < 2; n++) {
    emu.writeBytes(A(emu, 'debug_type'), [SLIME]);
    w16(emu, 'debug_x', px + 60);
    w16(emu, 'debug_y', py - 40);
    emu.writeBytes(A(emu, 'debug_cmd'), [2]);
    emu.idle(1);
    emu.writeBytes(A(emu, 'actor_hp') + emu.v('debug_slot'), [0]);
    emu.writeBytes(A(emu, 'shots_killed'), [1]);
    emu.idle(2);
  }
  assert.equal(prof(emu, MIS_HAVE), 2, 'two counted');
  assert.equal(emu.v('mission_cull'), 0, 'done: no longer counting');
  emu.idle(70);
  assert.equal(String.fromCharCode(...emu.readBytes(A(emu, 'hud_text'), 7)), 'JOB 2/2');
});

test('at the board, a finished job pays out; a debt is paid from your credits', () => {
  const emu = newFileInTown();
  setProf(emu, CREDITS, [244, 1, 0, 0]);  // 500
  job(emu, 0, CULL, SLIME, 4, 4);
  job(emu, 1, DEBT, 0, 4);  // 200 CR
  job(emu, 2, RETRIEVE, 0, 3, 1);
  const level = prof(emu, LEVEL);
  atBoard(emu);
  for (let i = 0; i < 4; i++) emu.hold('DOWN', 2).idle(2);
  emu.tap('A').idle(2);
  assert.equal(prof(emu, MIS_KIND), 0, 'turned in');
  const paid = 500 + 4 * (8 + level);
  assert.equal(prof(emu, CREDITS, 4), paid, 'paid out');
  emu.hold('DOWN', 2).idle(2).tap('A').idle(2);
  assert.equal(prof(emu, MIS_KIND + 1), 0, 'the debt paid');
  assert.equal(prof(emu, CREDITS, 4), paid - 200);
  emu.hold('DOWN', 2).idle(2).tap('A').idle(2);
  assert.equal(prof(emu, MIS_KIND + 2), RETRIEVE, 'not done: still carried');
  emu.tap('SELECT').idle(2);
  assert.equal(prof(emu, MIS_KIND + 2), 0, 'SELECT drops it');
});

test('dying loses the data cores you carried, not the kills you counted', () => {
  const emu = newFileInTown();
  job(emu, 0, RETRIEVE, 0, 3, 2);
  job(emu, 1, CULL, SLIME, 6, 3);
  takeLiftDown(emu).idle(4);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 250]);  // hurt for 250
  settle(emu.idle(2), RUN_OVER);
  assert.equal(prof(emu, MIS_HAVE), 0, 'the cores, gone');
  assert.equal(prof(emu, MIS_HAVE + 1), 3, 'the kills, kept');
  assert.equal(prof(emu, MIS_KIND), RETRIEVE, 'the job still yours');
});

test('a retrieve job takes a data core from each camp you clear', () => {
  const emu = newFileInTown();
  job(emu, 0, RETRIEVE, 0, 3);
  takeLiftDown(emu);
  // A camp cleared (descent.c): every enemy it woke is down.
  for (let f = 0; f < 600 && !prof(emu, MIS_HAVE); f++) {
    if (emu.v('level_camps') && emu.read8(A(emu, 'camp_state')) === 0) {
      w16(emu, 'player_x', emu.read16(A(emu, 'camp_x')));
      w16(emu, 'player_y', emu.read16(A(emu, 'camp_y')));
    }
    for (let i = 0; i < 8; i++) if (emu.read8(A(emu, 'actor_type') + i)) emu.writeBytes(A(emu, 'actor_hp') + i, [0]);
    emu.writeBytes(A(emu, 'shots_killed'), [1]);
    emu.writeBytes(A(emu, 'player_hp'), [emu.v('player_hp_max')]);
    emu.idle(1);
    if (!emu.v('level_camps')) {
      // (the start map is quiet: next door)
      const links = emu.read8(A(emu, 'area_links') + emu.v('level_map'));
      const e = [1, 2, 4, 8].find((x) => links & x);
      const open = emu.readBytes(A(emu, 'level_open'), 4)[{ 1: 0, 2: 1, 4: 2, 8: 3 }[e]] * 16;
      const w = emu.v('room_w'), h = emu.v('room_h');
      const at = { 1: [open, 2 * 16 - 8], 4: [open, h * 16 - 3 * 16 + 4], 8: [2 * 16, open - 4], 2: [w * 16 - 3 * 16, open - 4] }[e];
      w16(emu, 'player_x', at[0]);
      w16(emu, 'player_y', at[1]);
      const before = emu.v('maps_entered', 'u16');
      for (let g = 0; g < 120 && emu.v('maps_entered', 'u16') === before; g++) emu.hold({ 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e], 1);
      emu.idle(30);
    }
  }
  assert.equal(prof(emu, MIS_HAVE), 1, 'a core');
});
