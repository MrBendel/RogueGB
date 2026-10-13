// Health and the shield layer (health.c, issue #10) and the HUD bars (hud.c,
// issue #11). Damage comes from debug command 4 in the lab (no enemies there),
// so every hit is exactly where and when the test says.
import { test } from 'node:test';
import { assert, matchGolden, startRun } from './helpers.mjs';

const HEALTH_START = 60;
const SHIELD_CAP = 40, SHIELD_DELAY = 150, SHIELD_RATE = 2;  // health.h: the first shield
const SHIELD_IFRAMES = 20;  // player.c
const HUD_FX_SHIELD = 1, HUD_FX_WHITE = 2, HUD_FX_LOW = 4;

const addr = (emu, name) => emu.symbols.addr(name);
const w8 = (emu, name, v) => emu.write8(addr(emu, name), v);
const w16 = (emu, name, v) => emu.writeBytes(addr(emu, name), [v & 0xff, (v >> 8) & 0xff]);

/** Run debug command cmd (play.c) on the next frame. */
function debug(emu, cmd) {
  w8(emu, 'debug_cmd', cmd);
  return emu.idle(1);
}
const giveShield = (emu) => debug(emu, 3);
/** A hit of dmg from just left of the player. */
function hurt(emu, dmg) {
  w8(emu, 'debug_type', dmg);
  w16(emu, 'debug_x', emu.v('player_x', 'u16') - 8);
  w16(emu, 'debug_y', emu.v('player_y', 'u16') + 12);
  return debug(emu, 4);
}
/** Wait out any invulnerability so the next hit lands. */
const recover = (emu) => emu.until((e) => !e.v('player_invuln') && !e.v('player_blink'), 200, 'invulnerability to end');

const run = () => startRun().idle(2);
// A bar change shows on screen a couple of frames later (queued, written in the
// next vblank, then drawn): screenshots wait a few frames.

test('a run starts with full health and no shield', () => {
  const emu = run();
  assert.equal(emu.v('player_hp'), HEALTH_START);
  assert.equal(emu.v('player_hp_max'), HEALTH_START);
  assert.equal(emu.v('shield_cap'), 0);
  assert.equal(emu.v('hud_shield_len'), 0, 'no shield bar');
  assert.equal(emu.v('hud_hp_len'), 64, '60 health: 8 cells of bar');
  assert.equal(emu.v('hud_hp_px'), 64, 'full');
  matchGolden(emu, 'hud-no-shield');
});

test('the shield takes a hit: no health lost, no knockback, a short grace', () => {
  const emu = giveShield(run());
  assert.equal(emu.v('shield_hp'), SHIELD_CAP);
  hurt(emu, 10);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP - 10);
  assert.equal(emu.v('player_hp'), HEALTH_START);
  assert.equal(emu.v('shield_hits', 'u16'), 1);
  assert.equal(emu.v('player_hits', 'u16'), 0);
  assert.equal(emu.v('player_blink'), 0, 'no blink');
  assert.equal(emu.v('player_vx', 'i16'), 0, 'no knockback');
  assert.ok(emu.v('player_invuln') > 0 && emu.v('player_invuln') <= SHIELD_IFRAMES, 'short invulnerability');
  assert.equal(emu.v('hud_fx') & HUD_FX_SHIELD, HUD_FX_SHIELD, 'the player tinted shield blue');
  // Invulnerable: a second hit right away does nothing.
  hurt(emu, 10);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP - 10);
});

test('a hit bigger than the shield breaks it and the rest spills into health', () => {
  const emu = giveShield(run());
  w8(emu, 'shield_hp', 4);
  hurt(emu, 10);
  assert.equal(emu.v('shield_hp'), 0);
  assert.equal(emu.v('player_hp'), HEALTH_START - 6);
  assert.equal(emu.v('player_hits', 'u16'), 1);
  assert.ok(emu.v('player_blink') > 0, 'blinking');
  assert.ok(emu.v('player_vx', 'i16') > 0, 'knocked back, away from the hit');
  assert.ok(emu.v('shield_broke') > 0, 'the break shows');
  const fx = [];
  for (let f = 0; f < 24; f++) fx.push(emu.idle(1).v('hud_fx') & 3);
  assert.ok(fx.includes(HUD_FX_WHITE) && fx.includes(HUD_FX_SHIELD), `the break flashes: ${fx.join('')}`);
  assert.equal(fx.at(-1), 0, 'and ends');
});

test('the shield recharges after its delay, a point every SHIELD_RATE frames', () => {
  const emu = giveShield(run());
  hurt(emu, 30);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP - 30);
  emu.idle(SHIELD_DELAY - 2);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP - 30, 'nothing during the delay');
  emu.idle(2 + 10 * SHIELD_RATE);
  const hp = emu.v('shield_hp');
  assert.ok(hp >= SHIELD_CAP - 20 && hp <= SHIELD_CAP - 19, `about 10 points back after 20 frames (${hp})`);
  emu.idle(30 * SHIELD_RATE);
  assert.equal(emu.v('shield_hp'), SHIELD_CAP, 'full, and no further');
});

test('a hit during the recharge restarts the delay', () => {
  const emu = giveShield(run());
  hurt(emu, 30);
  emu.idle(SHIELD_DELAY + 10);
  const part = emu.v('shield_hp');
  assert.ok(part > SHIELD_CAP - 30, 'recharging');
  hurt(emu, 1);
  emu.idle(SHIELD_DELAY - 2);
  assert.equal(emu.v('shield_hp'), part - 1, 'waiting again');
});

test('health regenerates only with a regen rate (none by default)', () => {
  const emu = run();
  hurt(emu, 20);
  recover(emu);
  emu.idle(120);
  assert.equal(emu.v('player_hp'), HEALTH_START - 20, 'no regen');
  w8(emu, 'health_regen', 4);
  emu.idle(40);
  assert.equal(emu.v('player_hp'), HEALTH_START - 10, 'a point every 4 frames');
});

test('screen: HUD with a full shield', () => {
  matchGolden(giveShield(run()).idle(4), 'hud-shield-full');
});

test('screen: HUD with the shield partly drained and health partial', () => {
  const emu = giveShield(run());
  w8(emu, 'shield_hp', 17);
  w8(emu, 'player_hp', 37);
  w8(emu, 'shield_wait', 250);
  emu.idle(2);
  assert.equal(emu.v('hud_shield_px'), 17);
  assert.equal(emu.v('hud_hp_px'), Math.floor((37 * 64) / 60));
  matchGolden(emu.idle(4), 'hud-shield-partial');
});

test('screen: HUD with the shield broken and health low (pulsing)', () => {
  const emu = giveShield(run());
  w8(emu, 'shield_hp', 0);
  w8(emu, 'shield_wait', 250);
  w8(emu, 'player_hp', 9);
  const fx = [];
  for (let f = 0; f < 40; f++) fx.push(emu.idle(1).v('hud_fx') & HUD_FX_LOW);
  assert.ok(fx.includes(0) && fx.includes(HUD_FX_LOW), 'the health bar pulses at low health');
  emu.until((e) => e.v('hud_fx') & HUD_FX_LOW, 40, 'the pulse');
  matchGolden(emu.idle(4), 'hud-shield-broken-low');
});

test('bars grow with max values up to 88 px, then scale', () => {
  const emu = run();
  w8(emu, 'player_hp_max', 80);
  w8(emu, 'player_hp', 80);
  emu.idle(2);
  assert.equal(emu.v('hud_hp_len'), 80);
  w8(emu, 'player_hp_max', 200);
  w8(emu, 'player_hp', 100);
  emu.idle(2);
  assert.equal(emu.v('hud_hp_len'), 88, 'as long as it gets');
  assert.equal(emu.v('hud_hp_px'), 44, 'half');
  w8(emu, 'player_hp', 1);
  emu.idle(2);
  assert.equal(emu.v('hud_hp_px'), 1, 'never empty while alive');
  w8(emu, 'player_hp', 200);
  giveShield(emu);
  w8(emu, 'shield_cap', 120);
  w8(emu, 'shield_hp', 120);
  emu.idle(2);
  matchGolden(emu.idle(4), 'hud-grown');
});

test('the pause panel takes the window and the HUD comes back after', () => {
  const emu = giveShield(run());
  w8(emu, 'shield_hp', 17);
  w8(emu, 'shield_wait', 250);
  emu.idle(2);
  emu.tap('START').idle(2);
  assert.equal(emu.v('paused'), 1);
  emu.tap('START').idle(2);
  assert.equal(emu.v('hud_shield_px'), 17);
  matchGolden(emu.idle(4), 'hud-after-pause');
});

test('CPU budget: the HUD redrawing every frame (shield recharging, health regenerating)', () => {
  const emu = giveShield(run());
  w8(emu, 'shield_hp', 0);
  w8(emu, 'shield_wait', 0);
  w8(emu, 'shield_rate', 1);
  w8(emu, 'player_hp_max', 200);
  w8(emu, 'player_hp', 10);
  w8(emu, 'health_regen', 1);
  let worst = 0;
  for (let f = 0; f < 60; f++) {
    emu.hold(f < 30 ? 'RIGHT' : 'LEFT', 1);
    worst = Math.max(worst, emu.lastFrameCycles);
  }
  assert.ok(emu.v('shield_hp') >= 30 && emu.v('player_hp') >= 60, 'both bars moved every frame');
  assert.ok(worst < 38000, `worst frame ${worst} cycles (about a quarter frame, as for walking: camera.test.mjs)`);
});

// --- The HUD's layout (hud.c): window map tiles, all in VRAM bank 1 ---
const WIN = 0x9c00, RIGHT_X = 13;
const T_LINE = 14, T_SLOT = 16, T_SLOT_ICON = 18, T_GUN = 20, T_DIGIT = 24, T_FONT = 64;
const winTile = (emu, x, y) => emu.snapshot().vram[WIN - 0x8000 + y * 32 + x];
const winAttr = (emu, x, y) => emu.snapshot().vram[0x2000 + WIN - 0x8000 + y * 32 + x];

test('HUD: before a shield, its row shows an empty dashed slot', () => {
  const emu = run().idle(4);
  assert.equal(winTile(emu, 0, 0), T_SLOT_ICON);
  assert.equal(winTile(emu, 1, 0), T_SLOT);
  assert.equal(winTile(emu, 12, 0), T_LINE, 'then just the top line');
  for (let x = 0; x < 20; x++) assert.ok(winAttr(emu, x, 0) & 0x08, `column ${x}: a bank-1 HUD tile`);
});

test('HUD: the held weapon as an icon and its rounds in tall digits; a switch shows the name a moment', () => {
  const emu = run().idle(4);
  assert.equal(winTile(emu, RIGHT_X + 1, 0), T_GUN);
  assert.equal(winTile(emu, RIGHT_X + 2, 1), T_GUN + 3);
  // 12 rounds: "1" and "2", top and bottom halves.
  assert.equal(winTile(emu, RIGHT_X + 5, 0), T_DIGIT + 2);
  assert.equal(winTile(emu, RIGHT_X + 6, 1), T_DIGIT + 4 + 1);
  emu.tap('SELECT').idle(4);
  const name = Array.from({ length: 7 }, (_, i) => String.fromCharCode(winTile(emu, RIGHT_X + i, 0) - T_FONT + 32)).join('');
  assert.equal(name, 'SHOTGUN');
  emu.idle(60);
  assert.equal(winTile(emu, RIGHT_X + 1, 0), T_GUN, 'then the icon again');
  assert.equal(winTile(emu, RIGHT_X + 6, 0), T_DIGIT + 5 * 2, '5 shells');
});

test('sprites never draw over the HUD (they are switched off from its first line)', () => {
  const hudRows = (emu) => {
    const fb = emu.frameBuffer();
    return Buffer.from(fb.subarray(128 * 160 * 3, 144 * 160 * 3)).toString('hex');
  };
  const row = (emu, y) => Buffer.from(emu.frameBuffer().subarray(y * 160 * 3, (y + 1) * 160 * 3)).toString('hex');
  const emu = run().idle(60);
  const clean = hudRows(emu), above = row(emu, 124);
  // A slime half over the HUD: its 16 px from screen line 120 to 135.
  const x = emu.v('cam_x', 'u16') + 40, y = emu.v('cam_y', 'u16') + 120;
  w8(emu, 'debug_type', 1);
  w16(emu, 'debug_x', x);
  w16(emu, 'debug_y', y);
  w8(emu, 'debug_cmd', 2);
  emu.idle(1);
  const slot = emu.v('debug_slot');
  assert.ok(slot < 8, 'spawned');
  emu.write8(emu.symbols.addr('actor_timer') + slot, 254);  // keep it still
  emu.idle(4);
  assert.notEqual(row(emu, 124), above, 'it shows just above the HUD ...');
  // ... and not on it.
  assert.equal(hudRows(emu), clean);
  matchGolden(emu, 'hud-sprite-clipped');
});
