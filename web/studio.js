// Standalone RogueGB Sprite Studio (16x16 & 16x24 DMG/CGB Character & Weapon Synthesizer)
import {
  extractGridAndPalette, flipGridX, shiftGrid, floodFillGrid, resizeGridHeight,
  gridHeight, parseAsciiGrid, gridToAscii, synthesizeUpFromDown, synthesizeRightFromDown,
  generateCharacterRig, compileRigToGbdk, CHARACTER_PRESETS, WEAPON_TEMPLATES,
  buildWeaponDirectionalFrames, defaultWeaponAnchor, getOracleMeleeSwipeStep,
  GBC_PALETTES, snapColorToGbc, rgbToCgb555, cloneGrid,
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
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    popUndo();
    return;
  }
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
  toastTimer = setTimeout(() => el.classList.add('hidden'), 2800);
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

let spriteHeight = 16; // 16 or 24
let synthMode = '3pose';
let animKind = 'idle';
let dir = 'down';
let weaponId = 'melee_sword';
let attackPose = false;
let editTarget = 'down';
let brushShade = 3;
let editTool = 'pencil'; // 'pencil' | 'bucket' | 'picker'
let mirrorX = false;
let onionSkin = true;
let previewMode = 'anim'; // 'anim' | 'drive'
let selectedFrame = 0;
let animTick = 0;
let customAnchor = { dx: 0, dy: 0, auto: true };

const undoStack = [];
function pushUndo() {
  const g = getActiveEditGrid();
  undoStack.push({ target: editTarget, height: spriteHeight, data: cloneGrid(g) });
  if (undoStack.length > 35) undoStack.shift();
}
function popUndo() {
  const item = undoStack.pop();
  if (!item) { toast('Nothing to undo'); return; }
  if (item.target !== editTarget) {
    editTarget = item.target;
    if ($('st-edit-target')) $('st-edit-target').value = editTarget;
  }
  const g = getActiveEditGrid();
  if (g.length === item.data.length) {
    g.set(item.data);
    rebuildRig();
    toast('Undid last edit');
  }
}

// Game Boy Color 4-color palettes (index 0 = transparent, 1 = light, 2 = mid, 3 = dark)
let charPalette = GBC_PALETTES.gbc_hero_red.colors.map((c) => (c ? [...c] : null));
let weapPalette = GBC_PALETTES.gbc_volt_blade.colors.map((c) => (c ? [...c] : null));

// Base 16x16 or 16x24 grids currently loaded in the studio
let baseDown = cloneGrid(CHARACTER_PRESETS.adventurer.down);
let baseUp = cloneGrid(CHARACTER_PRESETS.adventurer.up);
let baseRight = cloneGrid(CHARACTER_PRESETS.adventurer.right);
let baseAtkDown = null;
let baseAtkUp = null;
let baseAtkRight = null;
let currentRig = null;
let currentCompiled = null;

// Test-drive state
const drive = { x: 24, y: 20, z: 0, vz: 0, dir: 'down', walkDist: 0, rollT: 0, atkT: 0, bullets: [] };

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
  const activePal = editTarget === 'weapon' ? weapPalette : charPalette;
  const cgbHexes = [];
  for (let i = 1; i <= 3; i++) {
    const el = $(`st-col-${i}`);
    if (el && charPalette[i]) el.value = rgbToHex(charPalette[i]);
    const sw = $(`st-sw-${i}`);
    if (sw && activePal[i]) sw.style.background = rgbToHex(activePal[i]);
    if (charPalette[i]) {
      const c555 = rgbToCgb555(charPalette[i][0], charPalette[i][1], charPalette[i][2]);
      cgbHexes.push('0x' + c555.toString(16).toUpperCase().padStart(4, '0'));
    }
  }
  if ($('st-cgb-readout') && cgbHexes.length === 3) {
    $('st-cgb-readout').textContent = `CGB RGB555: ${cgbHexes.join(' · ')}`;
  }
}

function syncSizeTabsUI() {
  if ($('st-size-tabs')) {
    for (const b of $('st-size-tabs').querySelectorAll('button[data-size]')) {
      b.classList.toggle('on', +b.dataset.size === spriteHeight);
    }
  }
  const activeH = editTarget === 'weapon' ? 16 : spriteHeight;
  if ($('st-editor-title')) {
    $('st-editor-title').textContent = `3. 16×${activeH} 2bpp Pixel Editor`;
  }
  if ($('st-sheet-title')) {
    $('st-sheet-title').textContent = `All Generated Sequences (16×${spriteHeight} GBC/DMG Sheet)`;
  }
  syncColorInputs();
}

function drawGrid(ctx, grid, ox = 0, oy = 0, palToUse = charPalette) {
  const H = gridHeight(grid);
  for (let y = 0; y < H; y++) {
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
  const charH = currentRig?.height || spriteHeight;
  if (w.category === 'melee' && (aKind === 'attack' || isAttack)) {
    const step = getOracleMeleeSwipeStep(wf, d, fIdx % 3, charH);
    if (!step) return null;
    if (!customAnchor.auto) return { grid: step.grid, dx: customAnchor.dx, dy: customAnchor.dy, behind: step.behind };
    return step;
  }
  const grid = wf[d]?.[isAttack ? 1 : 0] || null;
  if (!grid) return null;
  const def = defaultWeaponAnchor(d, aKind, fIdx, w.category || 'small_gun', isAttack, charH);
  const dx = customAnchor.auto ? def.dx : customAnchor.dx;
  const dy = customAnchor.auto ? def.dy : customAnchor.dy;
  return { grid, dx, dy, behind: def.behind };
}

function rebuildRig() {
  spriteHeight = gridHeight(baseDown);
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
  $('st-hw-stats').textContent = `16×${st.height} · ${st.anims} anims · ${st.frames}f · ${st.columns} unique 8×16 cols · ${st.romBytes} B ROM`;
  syncColorInputs();
  syncSizeTabsUI();
  renderEditor();
  renderFrameStrip();
  renderSheet();
}

function setCanvasHeight(newH) {
  if (newH === spriteHeight) return;
  pushUndo();
  const stretch = $('st-size-stretch') ? $('st-size-stretch').checked : true;
  baseDown = resizeGridHeight(baseDown, newH, stretch);
  baseUp = resizeGridHeight(baseUp, newH, stretch);
  baseRight = resizeGridHeight(baseRight, newH, stretch);
  baseAtkDown = null;
  baseAtkUp = null;
  baseAtkRight = null;
  spriteHeight = newH;
  rebuildRig();
  toast(`Converted character canvas to 16×${newH} (${stretch ? 'stretched proportions' : 'padded/cropped bottom'})!`);
}

function loadPreset(id) {
  const p = allPresets[id] || CHARACTER_PRESETS.adventurer;
  synthMode = p.mode || '3pose';
  document.querySelectorAll('input[name="st-synth-mode"]').forEach((r) => { r.checked = r.value === synthMode; });
  baseDown = new Uint8Array(p.down);
  spriteHeight = gridHeight(baseDown);
  baseUp = p.up ? new Uint8Array(p.up) : synthesizeUpFromDown(baseDown);
  baseRight = p.right ? new Uint8Array(p.right) : synthesizeRightFromDown(baseDown);
  baseAtkDown = p.attack_down ? p.attack_down.map((g) => new Uint8Array(g)) : null;
  baseAtkUp = p.attack_up ? p.attack_up.map((g) => new Uint8Array(g)) : null;
  baseAtkRight = p.attack_right ? p.attack_right.map((g) => new Uint8Array(g)) : null;
  if (p.palette) {
    charPalette = p.palette.map((c) => (c ? [...c] : null));
    if ($('st-char-pal')) $('st-char-pal').value = 'preset_auto';
  }
  undoStack.length = 0;
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

const EDIT_CELL = 14;
function renderEditor() {
  const g = getActiveEditGrid();
  const H = gridHeight(g);
  const W = 16;
  const activePal = editTarget === 'weapon' ? weapPalette : charPalette;
  const cw = W * EDIT_CELL;
  const ch = H * EDIT_CELL;
  if (editCanvas.width !== cw || editCanvas.height !== ch) {
    editCanvas.width = cw;
    editCanvas.height = ch;
  }

  const showOnion = onionSkin && (editTarget === 'up' || editTarget === 'right') && gridHeight(baseDown) === H;

  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const v = g[y * 16 + x];
      const px = x * EDIT_CELL;
      const py = y * EDIT_CELL;
      if (v === 0) {
        editCtx.fillStyle = (x + y) % 2 === 0 ? '#d8dfd0' : '#cad2c0';
        editCtx.fillRect(px, py, EDIT_CELL, EDIT_CELL);
        if (showOnion) {
          const ov = baseDown[y * 16 + x];
          if (ov > 0) {
            const [or, og, ob] = charPalette[ov] || [0, 0, 0];
            editCtx.fillStyle = `rgba(${or},${og},${ob},0.22)`;
            editCtx.fillRect(px, py, EDIT_CELL, EDIT_CELL);
          }
        }
      } else {
        const [r, gr, b] = activePal[v] || [0, 0, 0];
        editCtx.fillStyle = `rgb(${r},${gr},${b})`;
        editCtx.fillRect(px, py, EDIT_CELL, EDIT_CELL);
      }
    }
  }

  // Subtle 1px pixel grid lines
  editCtx.fillStyle = 'rgba(25, 35, 25, 0.14)';
  for (let x = 1; x < W; x++) editCtx.fillRect(x * EDIT_CELL, 0, 1, ch);
  for (let y = 1; y < H; y++) editCtx.fillRect(0, y * EDIT_CELL, cw, 1);

  // 8x8 / 8x16 hardware tile boundary guides
  editCtx.fillStyle = 'rgba(20, 90, 55, 0.45)';
  editCtx.fillRect(8 * EDIT_CELL, 0, 1, ch);
  editCtx.fillRect(0, 8 * EDIT_CELL, cw, 1);
  if (H > 16) {
    editCtx.fillStyle = 'rgba(200, 60, 20, 0.55)';
    editCtx.fillRect(0, 16 * EDIT_CELL, cw, 1);
  }

  // Mirror X center line indicator
  if (mirrorX) {
    editCtx.fillStyle = 'rgba(24, 150, 230, 0.75)';
    editCtx.fillRect(8 * EDIT_CELL - 1, 0, 2, ch);
  }
}

function renderFrameStrip() {
  const strip = $('st-frame-strip');
  strip.innerHTML = '';
  const key = `${animKind}_${dir}`;
  const frames = currentRig.anims[key] || currentRig.anims.idle_down;
  if (selectedFrame >= frames.length) selectedFrame = 0;
  const charH = currentRig.height || 16;
  const cardH = charH > 16 ? 40 : 32;
  const oy = Math.floor((cardH - charH) / 2);

  frames.forEach((fGrid, idx) => {
    const card = document.createElement('div');
    card.className = 'fcard' + (idx === selectedFrame ? ' on' : '');
    const c = document.createElement('canvas');
    c.width = 32;
    c.height = cardH;
    if (charH > 16) c.style.height = '60px';
    const cx = c.getContext('2d');
    cx.fillStyle = '#d8dfd0';
    cx.fillRect(0, 0, 32, cardH);
    const wStep = resolveWeaponStep(dir, animKind, idx, attackPose);
    if (customAnchor.auto && idx === selectedFrame && wStep) {
      $('st-anchor-dx').value = wStep.dx;
      $('st-anchor-dy').value = wStep.dy;
    }
    if (wStep && wStep.behind) drawGrid(cx, wStep.grid, 8 + wStep.dx, oy + wStep.dy, weapPalette);
    drawGrid(cx, fGrid, 8, oy, charPalette);
    if (wStep && !wStep.behind) drawGrid(cx, wStep.grid, 8 + wStep.dx, oy + wStep.dy, weapPalette);

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
  const cellH = currentRig.height || 16;
  sheetCanvas.width = cols * 16;
  sheetCanvas.height = rows * cellH;
  sheetCtx.fillStyle = '#d8dfd0';
  sheetCtx.fillRect(0, 0, sheetCanvas.width, sheetCanvas.height);
  entries.forEach(([, frames], r) => {
    frames.forEach((g, c) => {
      drawGrid(sheetCtx, g, c * 16, r * cellH, charPalette);
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

  const charH = currentRig?.height || 16;
  const baseOy = charH > 16 ? 20 : 24;
  const shadowFootOff = charH - 2;

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
    const oy = baseOy + dashDy + jumpY + dashHopY;

    // Ground shadow follows the dash position
    prevCtx.fillStyle = '#9aa690';
    prevCtx.fillRect(ox + 2, baseOy + shadowFootOff + dashDy, 12, 2);

    // Dash afterimage trail on mid-dash frames (frames 1 & 2)
    if (animKind === 'roll' && (selectedFrame === 1 || selectedFrame === 2)) {
      const trailX = ox - (dir === 'right' ? 4 : dir === 'left' ? -4 : 0);
      const trailY = oy - (dir === 'down' ? 4 : dir === 'up' ? -4 : 0);
      prevCtx.globalAlpha = 0.35;
      drawGrid(prevCtx, frames[0], trailX, trailY, charPalette);
      prevCtx.globalAlpha = 1.0;
    }

    const isSwinging = animKind === 'attack' || attackPose;
    if (wStep && wStep.behind) {
      drawGrid(prevCtx, wStep.grid, ox + wStep.dx, oy + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, ox + wStep.dx, oy + wStep.dy, isSwinging);
    }
    drawGrid(prevCtx, fGrid, ox, oy, charPalette);
    if (wStep && !wStep.behind) {
      drawGrid(prevCtx, wStep.grid, ox + wStep.dx, oy + wStep.dy, weapPalette);
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
        drive.bullets.push({ x: drive.x + 8, y: drive.y + Math.round(charH * 0.5), vx, vy });
      }
    }
    const rollVx = drive.rollT > 0 && !dx && !dy ? (drive.dir === 'right' ? 1 : drive.dir === 'left' ? -1 : 0) : dx;
    const rollVy = drive.rollT > 0 && !dx && !dy ? (drive.dir === 'down' ? 1 : drive.dir === 'up' ? -1 : 0) : dy;
    const speedMul = drive.rollT > 0 ? 1.8 : (isMelee && drive.atkT > 0 ? 0.35 : 1);
    drive.x = Math.max(2, Math.min(46, drive.x + rollVx * speedMul));
    drive.y = Math.max(2, Math.min(62 - charH, drive.y + rollVy * speedMul));
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
    prevCtx.fillRect(px + 2, Math.round(drive.y) + shadowFootOff, 12, 2);
    if (wStep && wStep.behind) {
      drawGrid(prevCtx, wStep.grid, px + wStep.dx, py + wStep.dy, weapPalette);
      drawVoltSparks(prevCtx, px + wStep.dx, py + wStep.dy, isAtk);
    }
    drawGrid(prevCtx, fGrid, px, py, charPalette);
    if (wStep && !wStep.behind) {
      drawGrid(prevCtx, wStep.grid, px + wStep.dx, py + wStep.dy, weapPalette);
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

if ($('st-size-tabs')) {
  $('st-size-tabs').addEventListener('click', (e) => {
    const b = e.target.closest('button[data-size]');
    if (!b) return;
    setCanvasHeight(+b.dataset.size);
  });
}

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
  $('st-drive-touch')?.classList.add('hidden');
};
$('st-mode-drive').onclick = () => {
  previewMode = 'drive';
  $('st-mode-drive').classList.add('on');
  $('st-mode-anim').classList.remove('on');
  $('st-drive-hint').classList.remove('hidden');
  $('st-drive-touch')?.classList.remove('hidden');
};

if ($('st-drive-touch')) {
  for (const btn of $('st-drive-touch').querySelectorAll('button[data-btn]')) {
    const bit = BTN[btn.dataset.btn] || 0;
    const press = (e) => {
      e.preventDefault();
      keyMask |= bit;
      btn.classList.add('on');
      if (navigator.vibrate) navigator.vibrate(8);
    };
    const release = (e) => {
      e.preventDefault();
      keyMask &= ~bit;
      btn.classList.remove('on');
    };
    btn.addEventListener('pointerdown', press);
    btn.addEventListener('pointerup', release);
    btn.addEventListener('pointercancel', release);
    btn.addEventListener('pointerleave', release);
  }
}

$('st-edit-target').onchange = (e) => {
  editTarget = e.target.value;
  syncSizeTabsUI();
  renderEditor();
};
function selectShade(shade) {
  brushShade = shade;
  for (const el of $('st-palette-bar').querySelectorAll('button')) {
    el.classList.toggle('on', +el.dataset.shade === brushShade);
  }
}
$('st-palette-bar').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-shade]');
  if (!b) return;
  selectShade(+b.dataset.shade);
});

function setEditTool(tool) {
  editTool = tool;
  $('st-tool-pencil')?.classList.toggle('on', tool === 'pencil');
  $('st-tool-bucket')?.classList.toggle('on', tool === 'bucket');
  $('st-tool-picker')?.classList.toggle('on', tool === 'picker');
}
if ($('st-tool-pencil')) $('st-tool-pencil').onclick = () => setEditTool('pencil');
if ($('st-tool-bucket')) $('st-tool-bucket').onclick = () => setEditTool('bucket');
if ($('st-tool-picker')) $('st-tool-picker').onclick = () => setEditTool('picker');
if ($('st-tool-mirror')) {
  $('st-tool-mirror').onclick = () => {
    mirrorX = !mirrorX;
    $('st-tool-mirror').classList.toggle('on', mirrorX);
    renderEditor();
  };
}
if ($('st-tool-onion')) {
  $('st-tool-onion').onclick = () => {
    onionSkin = !onionSkin;
    $('st-tool-onion').classList.toggle('on', onionSkin);
    renderEditor();
  };
}
if ($('st-edit-undo')) {
  $('st-edit-undo').onclick = () => popUndo();
}

let painting = false;
let activeStrokeShade = 3;
function paintAt(e, isStart = false) {
  const g = getActiveEditGrid();
  const H = gridHeight(g);
  const r = editCanvas.getBoundingClientRect();
  const x = Math.floor(((e.clientX - r.left) / r.width) * 16);
  const y = Math.floor(((e.clientY - r.top) / r.height) * H);
  if (x < 0 || x >= 16 || y < 0 || y >= H) return;

  // Eyedropper tool or Alt-click picks shade under cursor
  if (editTool === 'picker' || e.altKey) {
    selectShade(g[y * 16 + x] & 3);
    if (editTool === 'picker') setEditTool('pencil');
    toast(`Picked shade ${brushShade}`);
    painting = false;
    return;
  }

  if (isStart) {
    pushUndo();
    activeStrokeShade = e.button === 2 ? 0 : brushShade;
  }

  if (editTool === 'bucket' && isStart) {
    floodFillGrid(g, x, y, activeStrokeShade);
    if (mirrorX) floodFillGrid(g, 15 - x, y, activeStrokeShade);
    if (editTarget !== 'weapon') { baseAtkDown = null; baseAtkUp = null; baseAtkRight = null; }
    rebuildRig();
    painting = false;
    return;
  }

  let changed = false;
  if (g[y * 16 + x] !== activeStrokeShade) {
    g[y * 16 + x] = activeStrokeShade;
    changed = true;
  }
  if (mirrorX && g[y * 16 + (15 - x)] !== activeStrokeShade) {
    g[y * 16 + (15 - x)] = activeStrokeShade;
    changed = true;
  }
  if (changed) {
    if (editTarget !== 'weapon') { baseAtkDown = null; baseAtkUp = null; baseAtkRight = null; }
    rebuildRig();
  }
}
editCanvas.addEventListener('contextmenu', (e) => e.preventDefault());
editCanvas.addEventListener('pointerdown', (e) => {
  e.preventDefault();
  painting = true;
  editCanvas.setPointerCapture?.(e.pointerId);
  paintAt(e, true);
});
editCanvas.addEventListener('pointermove', (e) => { if (painting) paintAt(e, false); });
editCanvas.addEventListener('pointerup', () => { painting = false; });
editCanvas.addEventListener('pointercancel', () => { painting = false; });

function nudgeActiveGrid(dx, dy) {
  pushUndo();
  const g = getActiveEditGrid();
  g.set(shiftGrid(g, dx, dy));
  if (editTarget !== 'weapon') { baseAtkDown = null; baseAtkUp = null; baseAtkRight = null; }
  rebuildRig();
}
if ($('st-nudge-left')) $('st-nudge-left').onclick = () => nudgeActiveGrid(-1, 0);
if ($('st-nudge-right')) $('st-nudge-right').onclick = () => nudgeActiveGrid(1, 0);
if ($('st-nudge-up')) $('st-nudge-up').onclick = () => nudgeActiveGrid(0, -1);
if ($('st-nudge-down')) $('st-nudge-down').onclick = () => nudgeActiveGrid(0, 1);

$('st-edit-flipx').onclick = () => {
  pushUndo();
  const g = getActiveEditGrid();
  g.set(flipGridX(g));
  rebuildRig();
};
$('st-edit-clear').onclick = () => {
  pushUndo();
  getActiveEditGrid().fill(0);
  rebuildRig();
};
$('st-edit-synth-dirs').onclick = () => {
  pushUndo();
  baseUp = synthesizeUpFromDown(baseDown);
  baseRight = synthesizeRightFromDown(baseDown);
  baseAtkDown = null;
  baseAtkUp = null;
  baseAtkRight = null;
  rebuildRig();
  toast('Synthesized Up, Right & Attack poses from Down!');
};

// Interactive Image -> Game Boy (2bpp / GBC RGB555) Sprite Converter state
let activeConvImg = null;
let activeConvData = null;
let activeConvName = 'image';
let lastConvResult = null;

function openImageConverter(img, sourceName = 'image') {
  activeConvImg = img;
  activeConvName = sourceName;
  const c = document.createElement('canvas');
  c.width = img.width;
  c.height = img.height;
  const cx = c.getContext('2d');
  cx.drawImage(img, 0, 0);
  activeConvData = cx.getImageData(0, 0, img.width, img.height).data;

  // Auto-detect 16x24 if image aspect ratio is clearly taller than square (>= 1.3:1)
  const aspect = img.height / Math.max(1, img.width);
  const autoH = img.height === 24 ? 24 : img.height === 16 ? 16 : aspect >= 1.3 ? 24 : spriteHeight;
  if ($('st-conv-size')) $('st-conv-size').value = String(autoH);
  if ($('st-conv-target') && $('st-upload-target')) {
    $('st-conv-target').value = $('st-upload-target').value || 'down';
  }
  // Default Auto-Crop ON for larger images, OFF for exact 16x16 or 16x24 pixel art
  if ($('st-conv-crop')) {
    $('st-conv-crop').checked = img.width > 20 || img.height > 28;
  }

  $('st-converter-card')?.classList.remove('hidden');
  pushUndo();
  updateConverterPreview(true);
  toast(`Loaded ${sourceName} (${img.width}×${img.height}) into Sprite Converter!`);
}

function updateConverterPreview(applyLive = true) {
  if (!activeConvImg || !activeConvData) return;
  const targetH = +($('st-conv-size')?.value || spriteHeight) === 24 ? 24 : 16;
  const bgMode = $('st-conv-bgmode')?.value || 'auto';
  const bgTolerance = +($('st-conv-tol')?.value ?? 36);
  const shadeBias = +($('st-conv-bias')?.value ?? 0);
  const autoCrop = $('st-conv-crop') ? $('st-conv-crop').checked : true;
  const autoOutline = $('st-conv-outline') ? $('st-conv-outline').checked : false;
  const usePal = $('st-conv-usepal') ? $('st-conv-usepal').checked : true;

  if ($('st-conv-tol-val')) $('st-conv-tol-val').textContent = String(bgTolerance);
  if ($('st-conv-bias-val')) $('st-conv-bias-val').textContent = (shadeBias > 0 ? '+' : '') + shadeBias;
  if ($('st-conv-dst-label')) $('st-conv-dst-label').textContent = `Converted 16×${targetH} (2bpp)`;

  // Render source thumbnail
  const srcCanvas = $('st-conv-src');
  if (srcCanvas) {
    srcCanvas.width = 96;
    srcCanvas.height = targetH === 24 ? 144 : 96;
    const sctx = srcCanvas.getContext('2d');
    sctx.clearRect(0, 0, srcCanvas.width, srcCanvas.height);
    sctx.imageSmoothingEnabled = activeConvImg.width > 64;
    const scale = Math.min(srcCanvas.width / activeConvImg.width, srcCanvas.height / activeConvImg.height);
    const dw = Math.round(activeConvImg.width * scale);
    const dh = Math.round(activeConvImg.height * scale);
    sctx.drawImage(
      activeConvImg,
      Math.floor((srcCanvas.width - dw) / 2),
      Math.floor((srcCanvas.height - dh) / 2),
      dw,
      dh,
    );
  }

  // Run 2bpp quantization + GBC RGB555 palette extraction
  lastConvResult = extractGridAndPalette(activeConvData, activeConvImg.width, activeConvImg.height, targetH, {
    bgMode,
    bgTolerance,
    autoCrop,
    shadeBias,
    autoOutline,
  });

  // Render converted 2bpp preview canvas
  const dstCanvas = $('st-conv-dst');
  if (dstCanvas) {
    const scale = 6;
    dstCanvas.width = 16 * scale;
    dstCanvas.height = targetH * scale;
    const dctx = dstCanvas.getContext('2d');
    dctx.clearRect(0, 0, dstCanvas.width, dstCanvas.height);
    const palToPreview = usePal ? lastConvResult.palette : charPalette;
    for (let y = 0; y < targetH; y++) {
      for (let x = 0; x < 16; x++) {
        const v = lastConvResult.grid[y * 16 + x];
        if (!v) continue;
        const [r, g, b] = palToPreview[v] || [0, 0, 0];
        dctx.fillStyle = `rgb(${r},${g},${b})`;
        dctx.fillRect(x * scale, y * scale, scale, scale);
      }
    }
  }

  if (applyLive) {
    applyConvertedSpriteToRig(false);
  }
}

function applyConvertedSpriteToRig(showToast = true) {
  if (!lastConvResult) return;
  const targetH = gridHeight(lastConvResult.grid);
  if (targetH !== spriteHeight) {
    baseDown = resizeGridHeight(baseDown, targetH, true);
    baseUp = resizeGridHeight(baseUp, targetH, true);
    baseRight = resizeGridHeight(baseRight, targetH, true);
    spriteHeight = targetH;
  }
  const target = $('st-conv-target')?.value || $('st-upload-target')?.value || 'down';
  const usePal = $('st-conv-usepal') ? $('st-conv-usepal').checked : true;
  if (target === 'up') {
    baseUp = cloneGrid(lastConvResult.grid);
  } else if (target === 'right') {
    baseRight = cloneGrid(lastConvResult.grid);
  } else {
    baseDown = cloneGrid(lastConvResult.grid);
    if (usePal) {
      charPalette = lastConvResult.palette.map((c) => (c ? [...c] : null));
      if (charPalSel) charPalSel.value = 'preset_auto';
    }
    if (synthMode === '1pose' || target === 'down') {
      baseUp = synthesizeUpFromDown(baseDown);
      baseRight = synthesizeRightFromDown(baseDown);
    }
  }
  baseAtkDown = null;
  baseAtkUp = null;
  baseAtkRight = null;
  rebuildRig();
  if (showToast) {
    toast(`Applied converted 16×${spriteHeight} sprite (${activeConvName}) & synthesized all animations!`);
  }
}

['st-conv-size', 'st-conv-target', 'st-conv-bgmode', 'st-conv-tol', 'st-conv-bias', 'st-conv-crop', 'st-conv-outline', 'st-conv-usepal'].forEach((id) => {
  const el = $(id);
  if (el) el.oninput = () => updateConverterPreview(true);
});
if ($('st-conv-apply')) {
  $('st-conv-apply').onclick = () => {
    applyConvertedSpriteToRig(true);
    $('st-converter-card')?.classList.add('hidden');
  };
}
if ($('st-conv-close')) {
  $('st-conv-close').onclick = () => $('st-converter-card')?.classList.add('hidden');
}

function handleFileUploadInput(e) {
  const f = e.target.files?.[0];
  if (!f) return;
  const img = new Image();
  img.onload = () => openImageConverter(img, f.name);
  img.src = URL.createObjectURL(f);
  e.target.value = '';
}
if ($('st-upload-png')) $('st-upload-png').onchange = handleFileUploadInput;
if ($('st-upload-hero')) $('st-upload-hero').onchange = handleFileUploadInput;
if ($('st-upload-another')) $('st-upload-another').onchange = handleFileUploadInput;

// Drag-and-drop image anywhere onto the Studio page
addEventListener('dragover', (e) => e.preventDefault());
addEventListener('drop', (e) => {
  const f = e.dataTransfer?.files?.[0];
  if (!f || !f.type.startsWith('image/')) return;
  e.preventDefault();
  const img = new Image();
  img.onload = () => openImageConverter(img, f.name);
  img.src = URL.createObjectURL(f);
});

if ($('st-paste-btn')) {
  $('st-paste-btn').onclick = async () => {
    try {
      if (navigator.clipboard?.read) {
        const items = await navigator.clipboard.read();
        for (const item of items) {
          const imgType = item.types.find((t) => t.startsWith('image/'));
          if (imgType) {
            const blob = await item.getType(imgType);
            const img = new Image();
            img.onload = () => openImageConverter(img, 'Clipboard Image');
            img.src = URL.createObjectURL(blob);
            return;
          }
        }
      }
      if (navigator.clipboard?.readText) {
        const text = await navigator.clipboard.readText();
        if (text && (text.trim().startsWith('{') || /^[.0123\s"']+$/m.test(text))) {
          loadSpriteFromCode(text);
          return;
        }
      }
      toast('No image found on clipboard — tap 🖼️ Upload Image → Sprite');
    } catch {
      toast('Tap 🖼️ Upload Image → Sprite or press Ctrl+V to paste');
    }
  };
}

// Global Ctrl+V clipboard paste listener (supports pasted images OR sprite JSON/ASCII)
addEventListener('paste', (e) => {
  if (e.target.matches('input, textarea')) return;
  const items = e.clipboardData?.items || [];
  for (const item of items) {
    if (item.type.startsWith('image/')) {
      e.preventDefault();
      const blob = item.getAsFile();
      if (!blob) return;
      const img = new Image();
      img.onload = () => openImageConverter(img, 'Clipboard Image');
      img.src = URL.createObjectURL(blob);
      return;
    }
  }
});

// Chat-friendly Sprite Code Export & Import
function serializeSpriteForChat() {
  const obj = {
    name: currentRig?.name || 'custom_hero',
    size: `16x${spriteHeight}`,
    mode: synthMode,
    palette: charPalette,
    down: gridToAscii(baseDown),
  };
  if (synthMode === '3pose') {
    obj.up = gridToAscii(baseUp);
    obj.right = gridToAscii(baseRight);
  }
  return JSON.stringify(obj, null, 2);
}

function loadSpriteFromCode(rawText) {
  const txt = (rawText || '').trim();
  if (!txt) { toast('Paste Sprite JSON or 16-char ASCII rows first'); return; }
  pushUndo();
  try {
    if (txt.startsWith('{')) {
      const obj = JSON.parse(txt);
      if (!obj.down) throw new Error('Missing "down" grid in JSON');
      baseDown = parseAsciiGrid(obj.down);
      spriteHeight = gridHeight(baseDown);
      synthMode = obj.mode || (obj.up || obj.right ? '3pose' : '1pose');
      document.querySelectorAll('input[name="st-synth-mode"]').forEach((r) => { r.checked = r.value === synthMode; });
      baseUp = obj.up ? parseAsciiGrid(obj.up) : synthesizeUpFromDown(baseDown);
      baseRight = obj.right ? parseAsciiGrid(obj.right) : synthesizeRightFromDown(baseDown);
      if (Array.isArray(obj.palette) && obj.palette.length === 4) {
        charPalette = obj.palette.map((c) => (c ? [...c] : null));
        if (charPalSel) charPalSel.value = 'preset_auto';
      }
      baseAtkDown = null; baseAtkUp = null; baseAtkRight = null;
      rebuildRig();
      toast(`Loaded "${obj.name || 'custom'}" (16×${spriteHeight}) from Sprite JSON!`);
      return;
    }
    // Otherwise parse raw 16-char ASCII lines (. 0 1 2 3)
    const lines = txt
      .split(/\r?\n/)
      .map((l) => l.trim().replace(/^["']|["'],?$/g, ''))
      .filter((l) => l.length === 16 && /^[.0123]+$/.test(l));
    if (lines.length !== 16 && lines.length !== 24) {
      throw new Error(`Expected 16 or 24 rows of 16 chars (.0123), found ${lines.length}`);
    }
    baseDown = parseAsciiGrid(lines);
    spriteHeight = gridHeight(baseDown);
    baseUp = synthesizeUpFromDown(baseDown);
    baseRight = synthesizeRightFromDown(baseDown);
    baseAtkDown = null; baseAtkUp = null; baseAtkRight = null;
    rebuildRig();
    toast(`Loaded 16×${spriteHeight} ASCII sprite into Studio!`);
  } catch (err) {
    toast(`Load error: ${err.message}`);
  }
}

if ($('st-copy-chat')) {
  $('st-copy-chat').onclick = async () => {
    const code = serializeSpriteForChat();
    if ($('st-code-input')) $('st-code-input').value = code;
    try {
      await navigator.clipboard.writeText(code);
      toast('Copied Sprite JSON to clipboard — paste it directly into chat!');
    } catch {
      $('st-code-drawer')?.classList.remove('hidden');
      toast('Sprite JSON ready in text box below — copy it into chat!');
    }
  };
}
if ($('st-toggle-import')) {
  $('st-toggle-import').onclick = () => {
    const drawer = $('st-code-drawer');
    if (!drawer) return;
    const opening = drawer.classList.contains('hidden');
    drawer.classList.toggle('hidden');
    if (opening && $('st-code-input')) {
      $('st-code-input').value = serializeSpriteForChat();
      $('st-code-input').focus();
      $('st-code-input').select();
    }
  };
}
if ($('st-close-code')) {
  $('st-close-code').onclick = () => $('st-code-drawer')?.classList.add('hidden');
}
if ($('st-apply-code')) {
  $('st-apply-code').onclick = () => {
    loadSpriteFromCode($('st-code-input')?.value || '');
    $('st-code-drawer')?.classList.add('hidden');
  };
}

$('st-dl-sheet').onclick = () => {
  const c = document.createElement('canvas');
  c.width = sheetCanvas.width * 2;
  c.height = sheetCanvas.height * 2;
  const cx = c.getContext('2d');
  cx.imageSmoothingEnabled = false;
  cx.drawImage(sheetCanvas, 0, 0, c.width, c.height);
  download(`roguegb-${currentRig.name}-16x${spriteHeight}-sheet.png`, c.toDataURL('image/png'));
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
window.rgbStudio = { rebuildRig, loadPreset, serializeSpriteForChat, loadSpriteFromCode };
