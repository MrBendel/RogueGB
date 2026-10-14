// CPU profiler: runs a scenario and shows, for the most expensive frames, which
// functions the cycles went to (sampling the program counter on every instruction).
//   node harness/profile.mjs walk    walk the lab: left column, bottom row
//   node harness/profile.mjs arena   fight in the arena
//   node harness/profile.mjs shoot   the same fight, holding A (auto-aim, the player's shots);
//                                    add --weapon=N to fire weapon N (1 shotgun, 2 rifle, 3 sniper, 4 sword),
//                                    --top=N to list N functions per frame
//   node harness/profile.mjs stress  arena with the pools kept full (8 enemies, 20 bullets)
// Functions without an exported symbol (static C functions, asm local labels) are
// counted under the nearest symbol before them.
import fs from 'node:fs';
import { loadRom } from './node.mjs';
const emu = loadRom();
// Symbols for bank 0 (0x0000-0x3FFF) and the switchable banks (0x4000-0x7FFF,
// which the .noi lists as bank << 16 | address).
const syms = fs.readFileSync('build/roguegb.noi', 'utf8').split(/\r?\n/).map((l) => /^DEF (\S+) 0x([0-9A-F]+)/i.exec(l)).filter(Boolean)
  .map((m) => ({ name: m[1], addr: parseInt(m[2], 16) }))
  .filter((s) => (s.addr & 0xFFFF) < 0x8000 && !/^_rR[AO]M/.test(s.name) && (/^[_.][A-Za-z_]/.test(s.name) || /^F[a-z_]+\$/.test(s.name)))
  .sort((a, b) => a.addr - b.addr);
const fnAt = (key) => { let lo = 0, hi = syms.length - 1, best = '?'; while (lo <= hi) { const mid = (lo + hi) >> 1; if (syms[mid].addr <= key) { best = syms[mid].name; lo = mid + 1; } else hi = mid - 1; } return best.replace(/\$0\$0$/, ''); };
// In 0x4000-0x7FFF, which code it is depends on the bank switched in.
const fn = (pc) => fnAt(pc >= 0x4000 && pc < 0x8000 ? (emu.read8(0xFF90) << 16) | pc : pc);
const m = { RIGHT: 1, LEFT: 2, UP: 4, DOWN: 8, A: 16, B: 32, SELECT: 64, START: 128 };
const mask = (s) => s.split('+').filter(Boolean).reduce((a, d) => a | m[d], 0);
const step = (b, n = 1) => { emu.setButtons(mask(b)); emu.run(n); };
const v = (n) => emu.read16(emu.symbols.addr(n));
step('', 30); step('SELECT'); step('', 42);  // SELECT on the title: the test lab (START begins a descent)
// --weapon=N: switch to weapon N first (SELECT cycles: 0 pistol, 1 shotgun, 2 rifle, 3 sniper, 4 sword).
const weaponArg = process.argv.find((a) => a.startsWith('--weapon='));
for (let k = 0; k < (weaponArg ? Number(weaponArg.slice(9)) : 0); k++) { step('SELECT'); step(''); }
const scenario = process.argv[2] || 'walk';
// --top=N: how many functions to list per frame (7 by default).
const topN = Number((process.argv.find((a) => a.startsWith('--top=')) || '--top=7').slice(6));
const toArena = () => { step('LEFT', 80); step('DOWN', 200); while (v('player_x') < 148) step('RIGHT'); step('', 10); while (v('player_y') < 256) step('DOWN'); };
// Strafe left and right, jumping now and then; `shoot` holds A throughout.
// The sword (weapon 4) swings once per press, so it taps A every 8 frames instead.
const tapping = weaponArg && Number(weaponArg.slice(9)) === 4;
const fight = (n, shoot = false) => Array.from({ length: n }, (_, f) => (f % 45 === 0 ? 'LEFT+B' : f % 90 < 45 ? 'LEFT' : 'RIGHT') + (shoot && (!tapping || f % 8 === 0) ? '+A' : ''));
// Before some frames, poke memory (the stress scenario refills the pools).
let before = () => {};
const pokedAt = new Set();
const plan = scenario === 'arena' || scenario === 'shoot'
  ? (toArena(), fight(600, scenario === 'shoot'))
  : scenario === 'stress'
    ? (toArena(), step('DOWN', 24), (before = (f) => {
        // Every 30 frames: refill to 8 enemies + 20 bullets around the player, and
        // top up HP so the run doesn't end.
        if (f % 30 === 0) {
          // The refill spawns a whole wave in one frame and overruns, so the next
          // frame starts late too: leave both out.
          pokedAt.add(f);
          pokedAt.add(f + 1);
          emu.write8(emu.symbols.addr('debug_cmd'), 1);
          emu.write8(emu.symbols.addr('player_hp'), emu.read8(emu.symbols.addr('player_hp_max')));
        }
      }), fight(600))
    : [...Array(80).fill('LEFT'), ...Array(200).fill('DOWN'), ...Array(320).fill('RIGHT')];
const vs = emu.symbols.addr('vsync');
const frames = [];
plan.forEach((b, idx) => {
  before(idx);
  emu.setButtons(mask(b));
  emu.gb.next_frame();
  const h = new Map(); let cyc = 0, r = emu.gb.registers(), pc = r.pc; r.free();
  while ((pc < vs || pc >= vs + 0x10) && cyc < 200000) { const c = emu.gb.clock(); cyc += c; const k = fn(pc); h.set(k, (h.get(k) || 0) + c); r = emu.gb.registers(); pc = r.pc; r.free(); }
  // Frames where the scenario poked memory (e.g. a debug refill spawning a whole
  // wave) aren't representative, so they're left out of the ranking.
  if (pokedAt.has(idx)) { emu.frame++; emu._snap = null; return; }
  frames.push({ idx, b, cyc, at: `x ${v('player_x')} y ${v('player_y')} fall ${emu.read8(emu.symbols.addr('player_fall_timer'))} state ${emu.read8(emu.symbols.addr('game_state'))}`, top: [...h.entries()].sort((a, b2) => b2[1] - a[1]).slice(0, topN).map(([k, c]) => `${k}:${c}`) });
  emu.frame++; emu._snap = null;
});
const sorted = [...frames].sort((a, b) => b.cyc - a.cyc);
for (const f of sorted.slice(0, 3)) console.log(f.idx, f.b, f.cyc, f.at, '\n    ' + f.top.join('\n    '));
const med = [...frames].sort((a, b) => a.cyc - b.cyc)[frames.length >> 1];
const BUDGET = 131328;  // a Color frame at double speed (140,448) less vblank: see README, Frame budget
console.log(`frames over budget (${BUDGET}): ${frames.filter((f) => f.cyc > BUDGET).length} of ${frames.length}`);
console.log('median frame', med.cyc, '\n    ' + med.top.join('\n    '));
