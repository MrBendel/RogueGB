// CPU profiler: runs a scenario and shows, for the most expensive frames, which
// functions the cycles went to (sampling the program counter on every instruction).
//   node harness/profile.mjs walk    walk the lab: left column, bottom row
//   node harness/profile.mjs arena   fight in the arena
//   node harness/profile.mjs stress  arena with the pools kept full (8 enemies, 20 bullets)
// Functions without an exported symbol (static C functions, asm local labels) are
// counted under the nearest symbol before them.
import fs from 'node:fs';
import { loadRom } from './node.mjs';
const emu = loadRom();
const syms = fs.readFileSync('build/roguegb.noi', 'utf8').split(/\r?\n/).map((l) => /^DEF (\S+) 0x([0-9A-F]+)/i.exec(l)).filter(Boolean)
  .map((m) => ({ name: m[1], addr: parseInt(m[2], 16) })).filter((s) => s.addr < 0x8000 && (/^[_.][A-Za-z_]/.test(s.name) || /^F[a-z_]+\$/.test(s.name)))
  .sort((a, b) => a.addr - b.addr);
const fn = (pc) => { let lo = 0, hi = syms.length - 1, best = '?'; while (lo <= hi) { const mid = (lo + hi) >> 1; if (syms[mid].addr <= pc) { best = syms[mid].name; lo = mid + 1; } else hi = mid - 1; } return best.replace(/\$0\$0$/, ''); };
const m = { RIGHT: 1, LEFT: 2, UP: 4, DOWN: 8, A: 16, B: 32, SELECT: 64, START: 128 };
const mask = (s) => s.split('+').filter(Boolean).reduce((a, d) => a | m[d], 0);
const step = (b, n = 1) => { emu.setButtons(mask(b)); emu.run(n); };
const v = (n) => emu.read16(emu.symbols.addr(n));
step('', 30); step('START'); step('', 42);
const scenario = process.argv[2] || 'walk';
const toArena = () => { step('LEFT', 80); step('DOWN', 200); while (v('player_x') < 148) step('RIGHT'); step('', 10); while (v('player_y') < 256) step('DOWN'); };
const fight = (n) => Array.from({ length: n }, (_, f) => (f % 45 === 0 ? 'LEFT+B' : f % 90 < 45 ? 'LEFT' : 'RIGHT'));
// Before some frames, poke memory (the stress scenario refills the pools).
let before = () => {};
const pokedAt = new Set();
const plan = scenario === 'arena'
  ? (toArena(), fight(600))
  : scenario === 'stress'
    ? (toArena(), step('DOWN', 24), (before = (f) => {
        // Every 30 frames: refill to 8 enemies + 20 bullets around the player, and
        // top up HP so the run doesn't end.
        if (f % 30 === 0) {
          pokedAt.add(f);
          emu.write8(emu.symbols.addr('debug_cmd'), 1);
          emu.write8(emu.symbols.addr('player_hp'), 6);
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
  frames.push({ idx, b, cyc, at: `x ${v('player_x')} y ${v('player_y')} fall ${emu.read8(emu.symbols.addr('player_fall_timer'))} state ${emu.read8(emu.symbols.addr('game_state'))}`, top: [...h.entries()].sort((a, b2) => b2[1] - a[1]).slice(0, 7).map(([k, c]) => `${k}:${c}`) });
  emu.frame++; emu._snap = null;
});
const sorted = [...frames].sort((a, b) => b.cyc - a.cyc);
for (const f of sorted.slice(0, 3)) console.log(f.idx, f.b, f.cyc, f.at, '\n    ' + f.top.join('\n    '));
const med = [...frames].sort((a, b) => a.cyc - b.cyc)[frames.length >> 1];
const BUDGET = 65664;  // see README, Frame budget
console.log(`frames over budget (${BUDGET}): ${frames.filter((f) => f.cyc > BUDGET).length} of ${frames.length}`);
console.log('median frame', med.cyc, '\n    ' + med.top.join('\n    '));
