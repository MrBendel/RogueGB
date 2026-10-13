// Placeholder sound effects (sfx.c, issue #13): the right sound for each event,
// and the sound hardware actually making noise.
import { test } from 'node:test';
import { assert, startRun, bootToTitle } from './helpers.mjs';

const SFX = {
  PISTOL: 0, SHOTGUN: 1, HURT: 8, SHIELD_HIT: 9, SHIELD_BREAK: 10, SHIELD_FULL: 11, RELOAD: 12, RELOADED: 13,
  JUMP: 16, START: 20,
};
const addr = (emu, n) => emu.symbols.addr(n);
const w8 = (emu, n, v) => emu.write8(addr(emu, n), v);
const w16 = (emu, n, v) => emu.writeBytes(addr(emu, n), [v & 255, v >> 8]);
function hurt(emu, dmg) {
  w8(emu, 'debug_type', dmg);
  w16(emu, 'debug_x', emu.v('player_x', 'u16') - 8);
  w16(emu, 'debug_y', emu.v('player_y', 'u16') + 12);
  w8(emu, 'debug_cmd', 4);
  return emu.idle(2);
}
/** The audio produced while running n frames: the loudest sample. */
function loudness(emu, n) {
  emu.audio();  // drop what's buffered
  emu.idle(n);
  const { samples } = emu.audio();
  let peak = 0;
  for (const s of samples) peak = Math.max(peak, Math.abs(s - (samples[0] ?? 0)));
  return peak;
}

test('sound is on, and a shot makes noise', () => {
  const emu = startRun().idle(30);
  assert.equal(emu.read8(0xff26) & 0x80, 0x80, 'NR52: sound on');
  const quiet = loudness(emu, 10);
  emu.hold('A', 1);
  assert.equal(emu.v('sfx_last'), SFX.PISTOL);
  const loud = loudness(emu, 6);
  assert.ok(loud > quiet, `louder while firing (${quiet} -> ${loud})`);
});

test('each event plays its own sound', () => {
  const emu = startRun().idle(30);
  emu.hold('B', 1).idle(1);
  assert.equal(emu.v('sfx_last'), SFX.JUMP);
  emu.idle(60);
  hurt(emu, 5);
  assert.equal(emu.v('sfx_last'), SFX.HURT);
  emu.idle(80);
  emu.tap('SELECT').idle(20);  // the shotgun
  emu.hold('A', 1).idle(1);
  assert.equal(emu.v('sfx_last'), SFX.SHOTGUN);
  emu.idle(50).hold('SELECT', 2).hold('SELECT+B', 2).idle(1);
  assert.equal(emu.v('sfx_last'), SFX.RELOAD);
  emu.until((e) => e.v('sfx_last') === SFX.RELOADED, 120, 'the reload to finish');
});

test('shield sounds: a hit, a break, and a chime when it is full again', () => {
  const emu = startRun().idle(30);
  w8(emu, 'debug_cmd', 3);  // equip the first shield
  emu.idle(2);
  hurt(emu, 5);
  assert.equal(emu.v('sfx_last'), SFX.SHIELD_HIT);
  emu.idle(30);
  hurt(emu, 50);
  assert.equal(emu.v('sfx_last'), SFX.SHIELD_BREAK, 'the break wins over the hurt');
  emu.until((e) => e.v('sfx_last') === SFX.SHIELD_FULL, 400, 'the shield to recharge');
});

test('pressing START on the title plays the start sound', () => {
  const emu = bootToTitle();
  emu.tap('START');
  assert.equal(emu.v('sfx_last'), SFX.START);
});
