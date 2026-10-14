import { test } from 'node:test';
import { assert, bootToTitle, settle, TITLE, PLAY, RUN_OVER } from './helpers.mjs';

function enemyMenu() {
  return bootToTitle().tap('DOWN').tap('DOWN').tap('DOWN').tap('A');
}

test('playtest menu: navigation wraps, B backs out, and the campaign still launches', () => {
  const e = bootToTitle().tap('DOWN').idle(10);
  assert.equal(e.v('game_state'), TITLE);
  assert.equal(e.v('title_menu'), 1);
  e.shot('playtest-categories');
  e.tap('UP');
  assert.equal(e.v('title_choice'), 3);
  e.tap('DOWN');
  assert.equal(e.v('title_choice'), 0);
  e.tap('A').idle(10);
  assert.equal(e.v('title_menu'), 2);
  e.shot('playtest-minibosses');
  e.tap('B').tap('B').idle(10);
  assert.equal(e.v('title_menu'), 0);
  e.shot('playtest-back-home');
  settle(e.tap('START'), PLAY);
  assert.equal(e.v('play_mode'), 1);
});

test('playtest menu: lab entry and SELECT shortcut stay available', () => {
  const e = bootToTitle().tap('DOWN').tap('UP');
  settle(e.tap('A'), PLAY);
  assert.equal(e.v('play_mode'), 0);
  const shortcut = settle(bootToTitle().tap('SELECT'), PLAY);
  assert.equal(shortcut.v('play_mode'), 0);
});

test('playtest menu: all enemy trials spawn the selected ordinary enemies', () => {
  for (let kind = 0; kind < 3; kind++) {
    const e = enemyMenu();
    for (let n = 0; n < kind; n++) e.tap('DOWN');
    e.idle(10);
    if (kind === 2) e.shot('playtest-enemies');
    settle(e.tap('START'), PLAY).idle(3);
    assert.equal(e.v('play_mode'), 4);
    assert.equal(e.v('boss_kind'), 0);
    assert.equal(e.v('enemy_trial_kind'), kind);
    assert.equal(e.v('actor_count'), kind === 2 ? 4 : 1);
    const types = Array.from({length: 8}, (_, i) => e.read8(e.symbols.addr('actor_type') + i));
    assert.deepEqual(types.filter(Boolean).sort(), kind === 2 ? [1,1,2,2] : [kind + 1]);
    assert.equal(e.v('shield_hp'), 40);
    assert.equal(e.v('room_w'), 10);
    assert.equal(e.v('wave_num'), 0);
    e.shot(`enemy-trial-${kind}`);
  }
});

test('playtest menu: enemy clear has no waves and returning resets selection and encounter', () => {
  const e = settle(enemyMenu().tap('START'), PLAY);
  for (let f = 0; f < 500 && !e.v('enemy_trial_cleared'); f++) e.hold('A', 1);
  assert.equal(e.v('enemy_trial_cleared'), 1, 'starter weapon can clear the trial');
  e.idle(180);
  assert.equal(e.v('actor_count'), 0);
  assert.equal(e.v('wave_num'), 0);
  e.tap('START').tap('SELECT');
  settle(e, RUN_OVER).idle(35).tap('START');
  settle(e, TITLE);
  assert.equal(e.v('title_menu'), 0);
  e.tap('DOWN').tap('DOWN').tap('DOWN').tap('A');
  settle(e.tap('A'), PLAY);
  assert.equal(e.v('enemy_trial_cleared'), 0);
  assert.equal(e.v('actor_count'), 1);
});
