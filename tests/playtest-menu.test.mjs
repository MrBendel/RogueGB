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
  // The campaign: START opens the files; a new one asks for a class and starts in the town.
  settle(e.tap('START').tap('A').tap('A'), PLAY);
  assert.equal(e.v('play_mode'), 6);
});

test('playtest menu: lab entry and SELECT shortcut stay available', () => {
  const e = bootToTitle().tap('DOWN').tap('UP');
  settle(e.tap('A'), PLAY);
  assert.equal(e.v('play_mode'), 0);
  const shortcut = settle(bootToTitle().tap('SELECT'), PLAY);
  assert.equal(shortcut.v('play_mode'), 0);
});

test('playtest menu: adding a mercenary preserves the third boss entry', () => {
  const e=bootToTitle().tap('DOWN').tap('DOWN').tap('A').tap('DOWN').tap('DOWN').idle(9);
  assert.equal(e.v('title_choice'),2);
  settle(e.tap('A'),PLAY).idle(3);
  assert.equal(e.v('boss_kind'),4);
  assert.equal(e.v('play_mode'),3);
});

test('playtest menu: fourth boss entry launches Captain Flynt', () => {
  const e=bootToTitle().tap('DOWN').tap('DOWN').tap('A').tap('DOWN').tap('DOWN').tap('DOWN').idle(9);
  assert.equal(e.v('title_choice'),3);
  settle(e.tap('A'),PLAY).idle(3);
  assert.equal(e.v('boss_kind'),10);
  assert.equal(e.v('play_mode'),3);
  assert.equal(e.v('boss_hp'),36);
});

test('playtest menu: three enemy choices wrap in both directions', () => {
  const e = enemyMenu();
  for (let n = 0; n < 12; n++) {
    e.tap(n < 6 ? 'DOWN' : 'UP');
    assert.equal(e.v('title_choice'), n < 6 ? (n+1)%3 : (12-n-1)%3);
  }
  e.idle(10).shot('playtest-enemies');
  settle(e.tap('START'), PLAY).idle(3);
  assert.equal(e.v('play_mode'), 4);
  assert.equal(e.v('boss_kind'), 0);
  assert.equal(e.v('actor_count'), 1);
  assert.equal(e.v('actor_scripted'), 8);
  assert.equal(e.v('actor_hp'), 8);
  assert.equal(e.v('shield_hp'), 40);
  assert.equal(e.v('wave_num'), 0);
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
