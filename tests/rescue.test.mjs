// Rescue jobs (rescue.c, mission.c MIS_RESCUE): someone from town is held beside
// the first camp of one of the named area's field maps. Cleared, A frees them, or
// they follow you to the area's entrance, or you hold the camp while they cut loose.
// Turned in, they stand in town.
// (The board's cursor moves are 2-frame presses: moving redraws the job's details,
// which takes more than a frame, so a 1-frame tap can land while it's busy.)
import { test } from 'node:test';
import { assert, bootToTitle, settle, takeLiftDown, PLAY, RUN_OVER } from './helpers.mjs';

const A = (emu, n) => emu.symbols.addr(n);
const w16 = (emu, n, v) => emu.writeBytes(A(emu, n), [v & 255, v >> 8]);
const CREDITS = 7, EXP_MAP = 63;  // Profile offsets (save.h) ...
const MIS_KIND = 227, MIS_TARGET = 230, MIS_NEED = 233, MIS_HAVE = 236, MIS_SEED = 239, MIS_TAKEN = 241, RESCUED = 242;
const RESCUE = 5, FREE = 0, ESCORT = 1, HOLD = 2, PIP = 3, PAGE_BOARD = 6;
const RS_NONE = 0, RS_HELD = 1, RS_FOLLOW = 2, RS_HOLD = 3, TALK_PERSON = 8;
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

const keepAlive = (emu) => emu.writeBytes(A(emu, 'player_hp'), [emu.v('player_hp_max')]);
const killAll = (emu) => {
  for (let i = 0; i < 8; i++) if (emu.read8(A(emu, 'actor_type') + i)) emu.writeBytes(A(emu, 'actor_hp') + i, [0]);
  emu.writeBytes(A(emu, 'shots_killed'), [1]);
};
const closeDialog = (emu) => {
  for (let k = 0; k < 20 && emu.v('dlg_active'); k++) emu.tap('A').idle(2);
  assert.equal(emu.v('dlg_active'), 0, 'the dialogue closed');
};

/** A file with a rescue job (area 1), down in the map they're held in (saved, quit, continued there). */
function atCaptiveMap(how) {
  const emu = newFileInTown();
  job(emu, 0, RESCUE, PIP | (how << 4), 1);
  takeLiftDown(emu);
  assert.equal(emu.v('rescue_slot'), 0, 'held in this area');
  const map = emu.v('rescue_map');
  assert.notEqual(map, 0xff);
  setProf(emu, EXP_MAP, [map]);
  emu.tap('START').idle(2).tap('SELECT');
  settle(emu, 0, 'the title');
  settle(emu.tap('START').tap('A'), PLAY, 'continued');
  assert.equal(emu.v('level_map'), map);
  assert.equal(emu.v('rescue_state'), RS_HELD, 'they stand here');
  return emu;
}

/** Their talker: its block. */
function captive(emu) {
  const n = emu.v('talk_count');
  for (let i = 0; i < n; i++)
    if (emu.read8(A(emu, 'talk_kind') + i) === TALK_PERSON + PIP) return [emu.read8(A(emu, 'talk_bx') + i), emu.read8(A(emu, 'talk_by') + i)];
  return null;
}

/** Stand below them (in reach). */
function besideCaptive(emu) {
  const [bx, by] = captive(emu);
  w16(emu, 'player_x', bx * 16);
  w16(emu, 'player_y', by * 16 + 12);
  emu.idle(2);
}

/** Their camp cleared (woken by standing beside them, every enemy down). */
function clearCamp(emu) {
  besideCaptive(emu);
  for (let f = 0; f < 300 && emu.read8(A(emu, 'camp_state')) !== 2; f++) {
    killAll(emu);
    keepAlive(emu);
    emu.idle(1);
  }
  assert.equal(emu.read8(A(emu, 'camp_state')), 2, 'the camp cleared');
  emu.idle(30);
}

test('free: guarded until the camp is cleared; then A frees them and the job is done', () => {
  const emu = atCaptiveMap(FREE);
  emu.shot('rescue-held');
  besideCaptive(emu);
  if (emu.read8(A(emu, 'level_camps'))) {
    emu.tap('A').idle(2);
    assert.equal(prof(emu, MIS_HAVE), 0, 'still guarded');
    clearCamp(emu);
    besideCaptive(emu);
  }
  emu.tap('A').idle(2);
  assert.equal(emu.v('dlg_active'), 1, 'their thanks');
  closeDialog(emu);
  assert.equal(prof(emu, MIS_HAVE), 1, 'rescued');
  assert.equal(captive(emu), null, 'gone home');
  assert.equal(emu.v('rescue_state'), RS_NONE);
});

test('escort: they follow you, and enemies can bring them down (back to where they were held)', () => {
  const emu = atCaptiveMap(ESCORT);
  clearCamp(emu);
  besideCaptive(emu);
  emu.tap('A').idle(2);
  closeDialog(emu);
  assert.equal(emu.v('rescue_state'), RS_FOLLOW, 'following');
  assert.equal(captive(emu), null, 'no longer a talker');
  // They keep up.
  const px = emu.v('player_x', 'u16'), py = emu.v('player_y', 'u16');
  for (let f = 0; f < 40; f++) emu.hold('RIGHT', 1);
  emu.idle(60);
  const fx = emu.v('rescue_x', 'u16'), fy = emu.v('rescue_y', 'u16');
  const dx = Math.abs(fx - emu.v('player_x', 'u16')), dy = Math.abs(fy - emu.v('player_y', 'u16'));
  assert.ok(dx + dy < 40, `close behind (${dx}, ${dy}; the player moved from ${px}, ${py})`);
  emu.shot('rescue-follow');
  // An enemy on top of them, at their last hit point.
  emu.writeBytes(A(emu, 'rescue_hp'), [1]);
  emu.writeBytes(A(emu, 'debug_type'), [1]);
  w16(emu, 'debug_x', fx);
  w16(emu, 'debug_y', fy);
  emu.writeBytes(A(emu, 'debug_cmd'), [2]);
  emu.idle(4);
  assert.equal(emu.v('rescue_state'), RS_NONE, 'down');
  assert.equal(prof(emu, MIS_HAVE), 0, 'not rescued');
  assert.equal(prof(emu, MIS_KIND), RESCUE, 'the job still yours');
});

test('escort: brought into the area\'s entrance, they are safe', () => {
  const emu = atCaptiveMap(ESCORT);
  clearCamp(emu);
  besideCaptive(emu);
  emu.tap('A').idle(2);
  closeDialog(emu);
  // The way back, map by map (the area's links: a breadth-first path).
  const start = emu.v('area_start'), links = emu.readBytes(A(emu, 'area_links'), 12);
  const step = { 1: -4, 2: 1, 4: 4, 8: -1 };
  const prev = { [emu.v('level_map')]: null }, queue = [emu.v('level_map')];
  while (queue.length) {
    const m = queue.shift();
    for (const e of [1, 2, 4, 8]) if (links[m] & e && !((m + step[e]) in prev)) prev[m + step[e]] = [m, e], queue.push(m + step[e]);
  }
  const path = [];
  for (let m = start; prev[m]; m = prev[m][0]) path.unshift(prev[m][1]);
  for (const e of path) {
    const open = emu.readBytes(A(emu, 'level_open'), 4)[{ 1: 0, 2: 1, 4: 2, 8: 3 }[e]] * 16;
    const w = emu.v('room_w'), h = emu.v('room_h');
    const at = { 1: [open, 2 * 16 - 8], 4: [open, h * 16 - 3 * 16 + 4], 8: [2 * 16, open - 4], 2: [w * 16 - 3 * 16, open - 4] }[e];
    w16(emu, 'player_x', at[0]);
    w16(emu, 'player_y', at[1]);
    const before = emu.v('maps_entered', 'u16');
    for (let g = 0; g < 120 && emu.v('maps_entered', 'u16') === before; g++) {
      keepAlive(emu);
      emu.writeBytes(A(emu, 'rescue_hp'), [8]);
      emu.hold({ 1: 'UP', 2: 'RIGHT', 4: 'DOWN', 8: 'LEFT' }[e], 1);
    }
    emu.idle(30);
    closeDialog(emu);
  }
  assert.equal(emu.v('level_map'), start, 'in the entrance');
  assert.equal(prof(emu, MIS_HAVE), 1, 'safe');
  assert.equal(emu.v('rescue_state'), RS_NONE);
});

test('hold: the clock runs while you stay near; enemies come; then they are free', () => {
  const emu = atCaptiveMap(HOLD);
  clearCamp(emu);
  besideCaptive(emu);
  emu.tap('A').idle(2);
  closeDialog(emu);
  assert.equal(emu.v('rescue_state'), RS_HOLD, 'holding');
  emu.idle(5 * 60);
  const left = emu.v('rescue_timer', 'u16');
  assert.ok(left < 20 && left >= 14, `counting down (${left})`);
  assert.ok(emu.v('actor_count') > 0, 'enemies come at the camp');
  emu.shot('rescue-hold');
  // Away from them, the clock stops.
  const px = emu.v('player_x', 'u16');
  w16(emu, 'player_x', px > 200 ? px - 120 : px + 120);
  for (let f = 0; f < 120; f++) keepAlive(emu), emu.idle(1);
  assert.equal(emu.v('rescue_timer', 'u16'), left, 'paused while away');
  besideCaptive(emu);
  for (let f = 0; f < 22 * 60 && !prof(emu, MIS_HAVE); f++) {
    keepAlive(emu);
    if (f % 30 === 0) killAll(emu);
    if (emu.v('dlg_active')) emu.tap('A');
    emu.idle(1);
  }
  assert.equal(prof(emu, MIS_HAVE), 1, 'free');
});

test('dying loses a rescue (they\'re held again); the board pays for one done, and they move to town', () => {
  const emu = newFileInTown();
  job(emu, 0, RESCUE, PIP, 1, 1);
  takeLiftDown(emu).idle(4);
  emu.writeBytes(A(emu, 'debug_cmd'), [4, 250]);
  settle(emu.idle(2), RUN_OVER);
  assert.equal(prof(emu, MIS_HAVE), 0, 'lost');
  // Done, at the board.
  const town = newFileInTown();
  setProf(town, CREDITS, [0, 0, 0, 0]);
  job(town, 0, RESCUE, PIP | (ESCORT << 4), 2, 1);
  w16(town, 'player_x', 6 * 16);
  w16(town, 'player_y', 10 * 16 - 4);
  town.idle(4).tap('A').idle(4);
  assert.equal(town.v('menu_page'), PAGE_BOARD);
  for (let i = 0; i < 4; i++) town.hold('DOWN', 2).idle(2);
  const lv = prof(town, 1), c = 80 + 40 * 2 + 10 * lv;
  town.tap('A').idle(2);
  assert.equal(prof(town, MIS_KIND), 0, 'turned in');
  assert.equal(prof(town, RESCUED), 1 << PIP, 'rescued');
  assert.equal(prof(town, CREDITS, 4), c + (c >> 1), 'paid (an escort: half again)');
  // In town from the next visit on.
  town.tap('B').idle(4);
  takeLiftDown(town);
  town.writeBytes(A(town, 'debug_cmd'), [4, 250]);  // (the quick way back up)
  settle(town.idle(2), RUN_OVER).idle(40).tap('START');
  town.until((e) => e.v('play_mode') === 6, 60, 'the town');
  town.until((e) => e.v('in_transition'), 60, 'the scene change');
  settle(town, PLAY, 'back in town');
  const kinds = town.readBytes(A(town, 'talk_kind'), town.v('talk_count'));
  assert.ok(kinds.includes(TALK_PERSON + PIP), `Pip in town (${kinds})`);
  town.shot('rescue-town');
});

test('the board only asks you to rescue someone not yet rescued or on a job', () => {
  const emu = newFileInTown();
  setProf(emu, RESCUED, [0b0111]);  // all but Pip
  let seen = 0;
  for (let seed = 1; seed < 2000 && seen < 3; seed += 53) {
    setProf(emu, MIS_SEED, [seed & 255, seed >> 8]);
    setProf(emu, MIS_TAKEN, [0]);
    for (let k = 0; k < 4; k++) {
      setProf(emu, MIS_KIND, [0]);
      w16(emu, 'player_x', 6 * 16);
      w16(emu, 'player_y', 10 * 16 - 4);
      emu.idle(4).tap('A').idle(4);
      for (let j = 0; j < 7; j++) emu.hold('UP', 2).idle(2);  // (the cursor stays where it was)
      for (let j = 0; j < k; j++) emu.hold('DOWN', 2).idle(2);
      emu.tap('A').idle(2).tap('B').idle(4);
      if (prof(emu, MIS_KIND) === RESCUE) {
        seen++;
        assert.equal(prof(emu, MIS_TARGET) & 15, PIP, 'only Pip is left');
        assert.equal(prof(emu, MIS_NEED), 1, 'an area reached (a new file: the first)');
      }
    }
  }
  assert.ok(seen > 0, 'rescues are offered');
});
