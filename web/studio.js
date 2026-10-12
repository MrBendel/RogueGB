// Standalone RogueGB Sprite Studio (16x16 DMG/CGB Character & Weapon Synthesizer)
import {
  extractGridAndPalette, flipGridX, synthesizeUpFromDown, synthesizeRightFromDown,
  generateCharacterRig, compileRigToGbdk, CHARACTER_PRESETS, WEAPON_TEMPLATES,
  buildWeaponDirectionalFrames, defaultWeaponAnchor, getOracleMeleeSwipeStep,
  GBC_PALETTES, snapColorToGbc, cloneGrid,
} from '../harness/sprite-gen.mjs';

const $ = (id) => document.getElementById(id);

const BTN = { RIGHT: 1, LEFT: 2, UP: 4, DOWN: 8, A: 16, B: 32, SELECT: 64, START: 128 };
const KEYMAP = {
  ArrowRight: BTN.RIGHT, KeyD: BTN.RIGHT,
  ArrowLeft: BTN.LEFT, KeyA: BTN.LEFT,
  ArrowUp: BTN.UP, KeyW: BTN.UP,
  ArrowDown: BTN.DOWN, KeyS: BTN.DOWN,
  KeyX: BTN.A, KeyK: BTN.A,
  KeyZ: BTN.B, KeyJ: BTN.B, Space: BTN.B,
  ShiftLeft: BTN.SELECT, ShiftRight: BTN.SELECT,
  Enter: BTN.START,
};
let keyMask = 0;
addEventListener('keydown', (e) => {
  if (e.target.matches('input, textarea, select')) return;
  if (KEYMAP[e.code]) { keyMask |= KEYMAP[e.code]; e.preventDefault(); }
});
addEventListener('keyup', (e) => {
  if (KEYMAP[e.code]) keyMask &= ~KEYMAP[e.code];
});

let toastTimer = null;
function toast(msg) {
  const el = $('toast');
  if (!el) return;
  el.textContent = msg;
  el.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.add('hidden'), 2600);
}

function download(filename, blobOrUrl) {
  const a = document.createElement('a');
  a.href = typeof blobOrUrl === 'string' ? blobOrUrl : URL.createObjectURL(blobOrUrl);
  a.download = filename;
  a.click();
  if (typeof blobOrUrl !== 'string') setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

const allPresets = { ...(window.RGB_ARTWORK_PRESETS || {}), ...CHARACTER_PRESETS };
const weapons = structuredClone ? structuredClone(WEAPON_TEMPLATES) : JSON.parse(JSON.stringify(WEAPON_TEMPLATES));
for (const w of Object.values(weapons)) {
  w.right = new Uint8Array(Object.values(w.right));
  if (w.swing_right) w.swing_right = new Uint8Array(Object.values(w.swing_right));
  if (w.fire_right) w.fire_right = new Uint8Array(Object.values(w.fire_right));
  if (w.diag_ur) w.diag_ur = new Uint8Array(Object.values(w.diag_ur));
  if (w.smear_right) w.smear_right = new Uint8Array(Object.values(w.smear_right));
}

let synthMode = '3pose';
let animKind = 'idle';
let dir = 'down';
let weaponId = 'melee_sword';
let attackPose = false;
let editTarget = 'down';
let brushShade = 3;
let previewMode = 'anim'; // 'anim' | 'drive'
let selectedFrame = 0;
let animTick = 0;
let customAnchor = { dx: 0, dy: 0, auto: true };

// Game Boy Color 4-color palettes (index 0 = transparent, 1 = light, 2 = mid, 3 = dark)
let charPalette = GBC_PALETTES.gbc_hero_red.colors.map((c) => (c ? [...c] : null));
let weapPalette = GBC_PALETTES.gbc_volt_blade.colors.map((c) => (c ? [...c] : null));

// Base 16x16 grids currently loaded in the studio
let baseDown = cloneGrid(CHARACTER_PRESETS.adventurer.down);
let baseUp = cloneGrid(CHARACTER_PRESETS.adventurer.up);
let baseRight = cloneGrid(CHARACTER_PRESETS.adventurer.right);
let baseAtkDown = null;
let baseAtkUp = null;
let baseAtkRight = null;
let currentRig = null;
let currentCompiled = null;

// Test-drive state
const drive = { x: 24, y: 24, z: 0, vz: 0, dir: 'down', walkDist: 0, rollT: 0, atkT: 0, bullets: [] };

const prevCanvas = $('st-preview');
const prevCtx = prevCanvas.getContext('2d');
const editCanvas = $('st-editor');
const editCtx = editCanvas.getContext('2d');
const sheetCanvas = $('st-sheet');
const sheetCtx = sheetCanvas.getContext('2d');

function rgbToHex([r, g, b]) {
  return '#' + [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('');
}
function hexToRgb(hex) {
  const n = parseInt(hex.replace('#', ''), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
function syncColorInputs() {
  for (let i = 1; i <= 3; i++) {
    const el = $(`st-col-${i}`);
    if (el && charPalette[i]) el.value = rgbToHex(charPalette[i]);
  }
}

function drawGrid16(ctx, grid, ox = 0, oy = 0, palToUse = charPalette) {
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const v = grid[y * 16 + x];
      if (!v) continue;
      const [r, g, b] = palToUse[v] || charPalette[v] || [0, 0, 0];
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fillRect(ox + x, oy + y, 1, 1);
    }
  }
}

function resolveWeaponStep(d, aKind, fIdx, isAttack) {
  if (weaponId === 'none' || !weapons[weaponId] || aKind === 'roll') return null;
  const w = weapons[weaponId];
  const wf = buildWeaponDirectionalFrames(w);
  if (w.category === 'melee' && (aKind === 'attack' || isAttack)) {
    const step = getOracleMeleeSwipeStep(wf, d, fIdx % 3);
    if (!step) return null;
    if (!customAnchor.auto) return { grid: step.grid, dx: customAnchor.dx, dy: customAnchor.dy, behind: step.behind };
    return step;
  }
  const grid = wf[d]?.[isAttack ? 1 : 0] || null;
  if (!grid) return null;
  const def = defaultWeaponAnchor(d, aKind, fIdx, w.category || 'small_gun', isAttack);
  const dx = customAnchor.auto ? def.dx : customAnchor.dx;
  const dy = customAnchor.auto ? def.dy : customAnchor.dy;
  return { grid, dx, dy, behind: def.behind };
}

function rebuildRig() {
  currentRig = generateCharacterRig({
    name: $('st-preset')?.value || 'custom',
    palette: charPalette,
    down: baseDown,
    up: synthMode === '3pose' ? baseUp : null,
    right: synthMode === '3pose' ? baseRight : null,
    attack_down: synthMode === '3pose' ? baseAtkDown : null,
    attack_up: synthMode === '3pose' ? baseAtkUp : null,
    attack_right: synthMode === '3pose' ? baseAtkRight : null,
  });
  if (synthMode === '1pose') {
    baseUp = cloneGrid(currentRig.bases.up);
    baseRight = cloneGrid(currentRig.bases.right);
  }
  currentCompiled = compileRigToGbdk(currentRig, 'spr_hero', weapPalette);
  const st = currentCompiled.stats;
  $('st-hw-stats').textContent = `${st.anims} anims · ${st.frames}f · ${st.columns} unique 8×16 cols · ${st.romBytes} B ROM`;
  syncColorInputs();
  renderEditor();
  renderFrameStrip();
  renderSheet();
}

function loadPreset(id) {
  const p = allPresets[id] || CHARACTER_PRESETS.adventurer;
  synthMode = p.mode || '3pose';
  document.querySelectorAll('input[name="st-synth-mode"]').forEach((r) => { r.checked = r.value === synthMode; });
  baseDown = new Uint8Array(p.down);
  baseUp = p.up ? new Uint8Array(p.up) : synthesizeUpFromDown(baseDown);
  baseRight = p.right ? new Uint8Array(p.right) : synthesizeRightFromDown(baseDown);
  baseAtkDown = p.attack_down ? p.attack_down.map((g) => new Uint8Array(g)) : null;
  baseAtkUp = p.attack_up ? p.attack_up.map((g) => new Uint8Array(g)) : null;
  baseAtkRight = p.attack_right ? p.attack_right.map((g) => new Uint8Array(g)) : null;
  if (p.palette) {
    charPalette = p.palette.map((c) => (c ? [...c] : null));
    if ($('st-char-pal')) $('st-char-pal').value = 'preset_auto';
  }
  rebuildRig();
}

function syncWeaponPaletteFromWeapon() {
  const w = weapons[weaponId];
  const pKey = w?.palette || 'gbc_volt_blade';
  if (GBC_PALETTES[pKey]) {
    weapPalette = GBC_PALETTES[pKey].colors.map((c) => (c ? [...c] : null));
    if ($('st-weap-pal')) $('st-weap-pal').value = pKey;
  }
}

function getActiveEditGrid() {
  if (editTarget === 'up') return baseUp;
  if (editTarget === 'right') return baseRight;
  if (editTarget === 'weapon') {
    const w = weapons[weaponId];
    if (!w) return baseDown;
    return attackPose ? (w.smear_right || w.swing_right || w.fire_right || w.right) : w.right;
  }
  return baseDown;
}

function renderEditor() {
  const g = getActiveEditGrid();
  const activePal = editTarget === 'weapon' ? weapPalette : charPalette;
  editCtx.clearRect(0, 0, 16, 16);
  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      const v = g[y * 16 + x];
      if (v === 0) {
        editCtx.fillStyle = (x + y) % 2 === 0 ? '#d8dfd0' : '#cad2c0';
      } else {
        const [r, gr, b] = activePal[v] || [0, 0, 0];
        editCtx.fillStyle = `rgb(${r},${gr},${b})`;
      }
      editCtx.fillRect(x, y, 1, 1);
    }
  }
}

function renderFrameStrip() {
  const strip = $('st-frame-strip');
  strip.innerHTML = '';
  const key = `${animKind}_${dir}`;
  const frames = currentRig.anims[key] || currentRig.anims.idle_down;
  if (selectedFrame >= frames.length) selectedFrame = 0;

  frames.forEach((fGrid, idx) => {
    const card = document.createElement('div');
    card.className = 'fcard' + (idx === selectedFrame ? ' on' : '');
    const c = document.createElement('canvas');
    c.width = 32;
    c.height = 32;
    const cx = c.getContext('2d');
    cx.fillStyle = '#d8dfd0';
    cx.fillRect(0, 0, 32, 32);
    const wStep = resolveWeaponStep(dir, animKind, idx, attackPose);
    if (customAnchor.auto && idx === selectedFrame && wStep) {
      $('st-anchor-dx').value = wStep.dx;
      $('st-anchor-dy').value = wStep.dy;
    }
    if (wStep && wStep.behind) drawGrid16(cx, wStep.grid, 8 + wStep.dx, 8 + wStep.dy, weapPalette);
    drawGrid16(cx, fGrid, 8, 8, charPalette);
    if (wStep && !wStep.behind) drawGrid16(cx, wStep.grid, 8 + wStep.dx, 8 + wStep.dy, weapPalette);

    const lbl = document.createElement('span');
    lbl.textContent = animKind === 'attack' ? ['90°', '45°', '0°'][idx] || `f${idx}` : `f${idx}`;
    card.append(c, lbl);
    card.onclick = () => { selectedFrame = idx; renderFrameStrip(); };
    strip.append(card);
  });
}

function renderSheet() {
  const entries = Object.entries(currentRig.anims);
  const cols = 4;
  const rows = entries.length;
  sheetCanvas.width = cols * 16;
  sheetCanvas.height = rows * 16;
  sheetCtx.fillStyle = '#d8dfd0';
  sheetCtx.fillRect(0, 0, sheetCanvas.width, sheetCanvas.height);
  entries.forEach(([, frames], r) => {
    frames.forEach((g, c) => {
      drawGrid16(sheetCtx, g, c * 16, r * 16, charPalette);
    });
  });
}

function drawVoltSparks(ctx, wx, wy, isSwinging) {
  if (weaponId !== 'melee_sword') return;
  const c1 = weapPalette[1] || [255, 255, 224];
  const c2 = weapPalette[2] || [24, 216, 255];
  const count = isSwinging ? 5 : 2;
  for (let i = 0; i < count; i++) {
    const h = (animTick * 7 + i * 19) & 31;
    const sx = wx + 3 + (h % 10);
    const sy = wy + 3 + ((h >> 1) % 10);
    const [r, g, b] = (i + animTick) % 2 === 0 ? c1 : c2;
    ctx.fillStyle = `rgb(${r},${g},${b})`;
    ctx.fillRect(sx, sy, 1, 1);
    if (isSwinging && i % 2 === 0) {
      ctx.fillRect(sx + ((h & 1) ? 1 : -1), sy + ((h & 2) ? 1 : -1), 1, 1);
    }
  }
}

function stepStudio() {
  animTick++;
  prevCtx.fillStyle = '#d8dfd0';
  prevCtx.fillRect(0, 0, 64, 64);
  // Subtle tile grid background
  prevCtx.fillStyle = '#c5cebc';
  for (let y = 0; y < 64; y += 8) {
    for (let x = 0; x < 64; x += 8) {
      prevCtx.fillRect(x, y, 1, 1);
    }
  }

  if (previewMode === 'anim') {
    const key = `${animKind}_${dir}`;
    const frames = currentRig.anims[key] || currentRig.anims.idle_down;
    const period = animKind === 'idle' ? 24 : animKind === 'run' ? 8 : animKind === 'attack' ? 7 : animKind === 'roll' ? 5 : 14;
    if (animTick % period === 0) {
      selectedFrame = (selectedFrame + 1) % frames.length;
      renderFrameStrip();
    }
    const fGrid = frames[selectedFrame] || frames[0];
    const wStep = resolveWeaponStep(dir, animKind, selectedFrame, attackPose);
    const jumpY = animKind === 'jump' ? (selectedFrame === 1 ? -5 : selectedFrame === 2 ? -2 : 0) : 0;

    // Enter the Gungeon-style directional dodge-dash travel in Roll preview
    const dashStep = animKind === 'roll' ? ([-6, -1, 4, 7][selectedFrame] || 0) : 0;
    const dashDx = dir === 'right' ? dashStep : dir === 'left' ? -dashStep : 0;
    const dashDy = dir === 'down' ? dashStep : dir === 'up' ? -dashStep : 0;
    const dashHopY = animKind === 'roll' && (selectedFrame === 1 || selectedFrame === 2) ? -2 : 0;
    const ox = 24 + dashDx;
    const oy = 24 + dashDy + jumpY + dashHopY;

    // Ground shadow follows the dash position
    prevCtx.fillStyle = '#9aa690';
    prevCtx.fillRect(ox + 2, 38 + dashDy, 12, 2);

    // Dash afterimage trail on mid-dash frames (frames 1 & 2)
    if (animKind === 'roll' && (selectedFrame === 1 || selectedFrame === 2)) {
      const trailX = ox - (dir === 'right' ? 4 : dir === 'left' ? -4 : 0);
      const trailY = oy - (dir === 'down' ? 4 : dir === 'up' ? -4 : 0);
      prevCtx.globalAlpha = 0.35;
      drawGrid16(prevCtx, frames[0], trailX, trailY, charPalette);
      prevCtx.globalAlpha = 1.0;
    }

    const isSwinging = animKind === 'attack' || attackPose;
    if (wStep && wStep.behind) {
      drawGrid16(prevCtx, wStep.grid, ox + wStep.dx, oy + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, ox + wStep.dx, oy + wStep.dy, isSwinging);
    }
    drawGrid16(prevCtx, fGrid, ox, oy, charPalette);
    if (wStep && !wStep.behind) {
      drawGrid16(prevCtx, wStep.grid, ox + wStep.dx, oy + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, ox + wStep.dx, oy + wStep.dy, isSwinging);
    }
  } else {
    // Interactive Test Drive mode
    const m = keyMask;
    let dx = 0, dy = 0;
    if (m & BTN.LEFT) { dx = -1; drive.dir = 'left'; }
    if (m & BTN.RIGHT) { dx = 1; drive.dir = 'right'; }
    if (m & BTN.UP) { dy = -1; drive.dir = 'up'; }
    if (m & BTN.DOWN) { dy = 1; drive.dir = 'down'; }
    if ((m & BTN.B) && drive.z === 0) drive.vz = 2.2;
    if ((m & BTN.SELECT) && drive.rollT === 0) drive.rollT = 16;
    const isMelee = weapons[weaponId]?.category === 'melee';
    if ((m & BTN.A) && drive.atkT === 0) {
      drive.atkT = isMelee ? 15 : 10;
      if (weapons[weaponId]?.category?.includes('gun')) {
        const vx = drive.dir === 'right' ? 2.5 : drive.dir === 'left' ? -2.5 : 0;
        const vy = drive.dir === 'down' ? 2.5 : drive.dir === 'up' ? -2.5 : 0;
        drive.bullets.push({ x: drive.x + 8, y: drive.y + 8, vx, vy });
      }
    }
    // During a dodge-roll, propel the character along their facing direction even if D-pad is released
    const rollVx = drive.rollT > 0 && !dx && !dy ? (drive.dir === 'right' ? 1 : drive.dir === 'left' ? -1 : 0) : dx;
    const rollVy = drive.rollT > 0 && !dx && !dy ? (drive.dir === 'down' ? 1 : drive.dir === 'up' ? -1 : 0) : dy;
    const speedMul = drive.rollT > 0 ? 1.8 : (isMelee && drive.atkT > 0 ? 0.35 : 1);
    drive.x = Math.max(2, Math.min(46, drive.x + rollVx * speedMul));
    drive.y = Math.max(2, Math.min(46, drive.y + rollVy * speedMul));
    if (drive.z > 0 || drive.vz !== 0) {
      drive.z = Math.max(0, drive.z + drive.vz);
      drive.vz -= 0.18;
      if (drive.z <= 0) { drive.z = 0; drive.vz = 0; }
    }
    if (drive.rollT > 0) drive.rollT--;
    if (drive.atkT > 0) drive.atkT--;

    let aKind = 'idle';
    if (drive.rollT > 0) aKind = 'roll';
    else if (isMelee && drive.atkT > 0) aKind = 'attack';
    else if (drive.z > 0) aKind = 'jump';
    else if (dx || dy) { aKind = 'run'; drive.walkDist++; }

    const frames = currentRig.anims[`${aKind}_${drive.dir}`] || currentRig.anims[`idle_${drive.dir}`];
    const fIdx = aKind === 'attack'
      ? Math.min(2, Math.floor((15 - drive.atkT) / 5))
      : aKind === 'run'
        ? Math.floor(drive.walkDist / 5) % frames.length
        : aKind === 'roll'
          ? Math.floor((16 - drive.rollT) / 4) % frames.length
          : aKind === 'jump'
            ? (drive.vz > 0 ? 1 : 2)
            : Math.floor(animTick / 24) % frames.length;
    const fGrid = frames[fIdx] || frames[0];
    const isAtk = isMelee ? drive.atkT > 0 : drive.atkT > 4;
    const wStep = resolveWeaponStep(drive.dir, aKind, fIdx, isAtk);
    const rollHop = aKind === 'roll' && (fIdx === 1 || fIdx === 2) ? 2 : 0;
    const px = Math.round(drive.x);
    const py = Math.round(drive.y - drive.z - rollHop);

    prevCtx.fillStyle = '#9aa690';
    prevCtx.fillRect(px + 2, Math.round(drive.y) + 14, 12, 2);
    if (wStep && wStep.behind) {
      drawGrid16(prevCtx, wStep.grid, px + wStep.dx, py + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, px + wStep.dx, py + wStep.dy, isAtk);
    }
    drawGrid16(prevCtx, fGrid, px, py, charPalette);
    if (wStep && !wStep.behind) {
      drawGrid16(prevCtx, wStep.grid, px + wStep.dx, py + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, px + wStep.dx, py + wStep.dy, isAtk);
    }

    // Bullets
    prevCtx.fillStyle = '#1f2420';
    drive.bullets = drive.bullets.filter((b) => {
      b.x += b.vx; b.y += b.vy;
      prevCtx.fillRect(Math.round(b.x), Math.round(b.y), 2, 2);
      return b.x >= 0 && b.x < 64 && b.y >= 0 && b.y < 64;
    });
  }
}

// Populate GBC palette selectors
const charPalSel = $('st-char-pal');
const weapPalSel = $('st-weap-pal');
if (charPalSel && weapPalSel) {
  const autoOpt = document.createElement('option');
  autoOpt.value = 'preset_auto';
  autoOpt.textContent = '✨ Preset / Custom GBC';
  charPalSel.append(autoOpt);
  for (const [k, v] of Object.entries(GBC_PALETTES)) {
    const o1 = document.createElement('option');
    o1.value = k;
    o1.textContent = v.name;
    charPalSel.append(o1);
    const o2 = document.createElement('option');
    o2.value = k;
    o2.textContent = v.name;
    weapPalSel.append(o2);
  }
  weapPalSel.value = 'gbc_volt_blade';
  charPalSel.onchange = () => {
    const p = GBC_PALETTES[charPalSel.value];
    if (p) {
      charPalette = p.colors.map((c) => (c ? [...c] : null));
      rebuildRig();
    }
  };
  weapPalSel.onchange = () => {
    const p = GBC_PALETTES[weapPalSel.value];
    if (p) {
      weapPalette = p.colors.map((c) => (c ? [...c] : null));
      rebuildRig();
    }
  };
}
for (let i = 1; i <= 3; i++) {
  const colInput = $(`st-col-${i}`);
  if (colInput) {
    colInput.oninput = () => {
      const [r, g, b] = hexToRgb(colInput.value);
      charPalette[i] = snapColorToGbc(r, g, b);
      if (charPalSel) charPalSel.value = 'preset_auto';
      rebuildRig();
    };
  }
}

// Populate character presets dropdown
const presetSel = $('st-preset');
function populatePresets() {
  presetSel.innerHTML = '';
  for (const [k, v] of Object.entries(allPresets)) {
    const opt = document.createElement('option');
    opt.value = k;
    opt.textContent = v.name;
    presetSel.append(opt);
  }
}
populatePresets();
if (!window.RGB_ARTWORK_PRESETS) {
  fetch('/api/artwork-presets').then((r) => (r.ok ? r.json() : null)).then((data) => {
    if (data) { Object.assign(allPresets, data); populatePresets(); loadPreset(Object.keys(data)[0] || 'adventurer'); }
  }).catch(() => {});
}
presetSel.onchange = () => loadPreset(presetSel.value);

document.querySelectorAll('input[name="st-synth-mode"]').forEach((r) => {
  r.onchange = () => { synthMode = r.value; rebuildRig(); };
});

$('st-anim-tabs').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-anim]');
  if (!b) return;
  animKind = b.dataset.anim;
  selectedFrame = 0;
  for (const el of $('st-anim-tabs').querySelectorAll('button')) el.classList.toggle('on', el === b);
  renderFrameStrip();
});

$('st-dir-tabs').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-dir]');
  if (!b) return;
  dir = b.dataset.dir;
  for (const el of $('st-dir-tabs').querySelectorAll('button')) el.classList.toggle('on', el === b);
  renderFrameStrip();
});

$('st-weapon').onchange = (e) => {
  weaponId = e.target.value;
  syncWeaponPaletteFromWeapon();
  rebuildRig();
};
$('st-attack').onchange = (e) => { attackPose = e.target.checked; renderEditor(); renderFrameStrip(); };
$('st-anchor-dx').oninput = (e) => { customAnchor.auto = false; $('st-anchor-reset').checked = false; customAnchor.dx = +e.target.value; renderFrameStrip(); };
$('st-anchor-dy').oninput = (e) => { customAnchor.auto = false; $('st-anchor-reset').checked = false; customAnchor.dy = +e.target.value; renderFrameStrip(); };
$('st-anchor-reset').onchange = (e) => { customAnchor.auto = e.target.checked; renderFrameStrip(); };

$('st-mode-anim').onclick = () => {
  previewMode = 'anim';
  $('st-mode-anim').classList.add('on');
  $('st-mode-drive').classList.remove('on');
  $('st-drive-hint').classList.add('hidden');
};
$('st-mode-drive').onclick = () => {
  previewMode = 'drive';
  $('st-mode-drive').classList.add('on');
  $('st-mode-anim').classList.remove('on');
  $('st-drive-hint').classList.remove('hidden');
};

$('st-edit-target').onchange = (e) => { editTarget = e.target.value; renderEditor(); };
$('st-palette-bar').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-shade]');
  if (!b) return;
  brushShade = +b.dataset.shade;
  for (const el of $('st-palette-bar').querySelectorAll('button')) el.classList.toggle('on', el === b);
});

let painting = false;
function paintAt(e) {
  const r = editCanvas.getBoundingClientRect();
  const x = Math.floor(((e.clientX - r.left) / r.width) * 16);
  const y = Math.floor(((e.clientY - r.top) / r.height) * 16);
  if (x < 0 || x >= 16 || y < 0 || y >= 16) return;
  const g = getActiveEditGrid();
  if (g[y * 16 + x] !== brushShade) {
    g[y * 16 + x] = brushShade;
    if (editTarget !== 'weapon') { baseAtkDown = null; baseAtkUp = null; baseAtkRight = null; }
    rebuildRig();
  }
}
editCanvas.addEventListener('pointerdown', (e) => { e.preventDefault(); painting = true; editCanvas.setPointerCapture?.(e.pointerId); paintAt(e); });
editCanvas.addEventListener('pointermove', (e) => { if (painting) paintAt(e); });
editCanvas.addEventListener('pointerup', () => { painting = false; });
editCanvas.addEventListener('pointercancel', () => { painting = false; });

$('st-edit-flipx').onclick = () => {
  const g = getActiveEditGrid();
  g.set(flipGridX(g));
  rebuildRig();
};
$('st-edit-clear').onclick = () => {
  getActiveEditGrid().fill(0);
  rebuildRig();
};
$('st-edit-synth-dirs').onclick = () => {
  baseUp = synthesizeUpFromDown(baseDown);
  baseRight = synthesizeRightFromDown(baseDown);
  baseAtkDown = null;
  baseAtkUp = null;
  baseAtkRight = null;
  rebuildRig();
  toast('Synthesized Up, Right & Attack poses from Down!');
};

$('st-upload-png').onchange = (e) => {
  const f = e.target.files[0];
  if (!f) return;
  const img = new Image();
  img.onload = () => {
    const c = document.createElement('canvas');
    c.width = img.width;
    c.height = img.height;
    const cx = c.getContext('2d');
    cx.drawImage(img, 0, 0);
    const data = cx.getImageData(0, 0, img.width, img.height).data;
    const { grid, palette } = extractGridAndPalette(data, img.width, img.height);
    const target = $('st-upload-target').value;
    if (target === 'up') baseUp = grid;
    else if (target === 'right') baseRight = grid;
    else {
      baseDown = grid;
      charPalette = palette;
      if (charPalSel) charPalSel.value = 'preset_auto';
      if (synthMode === '1pose') {
        baseUp = synthesizeUpFromDown(baseDown);
        baseRight = synthesizeRightFromDown(baseDown);
      }
    }
    baseAtkDown = null;
    baseAtkUp = null;
    baseAtkRight = null;
    rebuildRig();
    toast(`Imported ${f.name} into ${target} with extracted GBC 4-color palette!`);
  };
  img.src = URL.createObjectURL(f);
  e.target.value = '';
};

$('st-dl-sheet').onclick = () => {
  const c = document.createElement('canvas');
  c.width = sheetCanvas.width * 2;
  c.height = sheetCanvas.height * 2;
  const cx = c.getContext('2d');
  cx.imageSmoothingEnabled = false;
  cx.drawImage(sheetCanvas, 0, 0, c.width, c.height);
  download(`roguegb-${currentRig.name}-sheet.png`, c.toDataURL('image/png'));
};

$('st-dl-c').onclick = () => {
  const combined = `${currentCompiled.header}\n${currentCompiled.source}`;
  download('spr_hero_generated.c', new Blob([combined], { type: 'text/plain' }));
};

$('st-copy-c').onclick = async () => {
  try {
    await navigator.clipboard.writeText(`${currentCompiled.header}\n${currentCompiled.source}`);
    toast('Copied GBDK C header + source (with CGB palettes) to clipboard!');
  } catch {
    toast('Clipboard unavailable — use Download GBDK (.h + .c)');
  }
};

syncWeaponPaletteFromWeapon();
loadPreset(Object.keys(allPresets)[0] || 'adventurer');
setInterval(stepStudio, 1000 / 60);
window.rgbStudio = { rebuildRig, loadPreset };
