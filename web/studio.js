// Standalone RogueGB Sprite Studio (8x8 to 64x64 DMG/CGB Dedicated Sprite Editor & Synthesizer)
import {
  extractGridAndPalette, flipGridX, shiftGrid, floodFillGrid, resizeGridHeight,
  resizeGridDimensions, gridHeight, parseAsciiGrid, gridToAscii, synthesizeUpFromDown, synthesizeRightFromDown,
  generateCharacterRig, compileRigToGbdk, compileArbitrarySpriteToGbdk, encodeTile8x8, CHARACTER_PRESETS, WEAPON_TEMPLATES,
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

// Check if a custom weapon was sent over from the Gemini Asset Forge (forge.html)
try {
  const rawForgeWpn = localStorage.getItem('roguegb_custom_weapon');
  if (rawForgeWpn) {
    const fw = JSON.parse(rawForgeWpn);
    if (fw && Array.isArray(fw.states) && fw.states.length >= 1) {
      const readyRows = fw.states[0]?.rows;
      const activeRows = fw.states[1]?.rows || readyRows;
      const diagRows = fw.states[2]?.rows || readyRows;
      weapons.custom_forge = {
        id: 'custom_forge',
        name: `✨ Forged: ${fw.name || 'Gemini Weapon'}`,
        category: fw.name && /sword|blade|sabre|club|axe|dagger|spear|staff/i.test(fw.name) ? 'melee' : 'small_gun',
        customPalette: Array.isArray(fw.palette) ? fw.palette : null,
        right: parseAsciiGrid(readyRows),
        swing_right: parseAsciiGrid(activeRows),
        smear_right: parseAsciiGrid(activeRows),
        fire_right: parseAsciiGrid(activeRows),
        diag_ur: parseAsciiGrid(diagRows),
      };
      const wSel = $('st-weapon');
      if (wSel) {
        const opt = document.createElement('option');
        opt.value = 'custom_forge';
        opt.textContent = weapons.custom_forge.name;
        wSel.append(opt);
      }
    }
  }
} catch {}

let spriteWidth = 16;  // 8 to 64 in 8px increments
let spriteHeight = 16; // 8 to 64 in 8px increments
let showGridLines = true;
let synthMode = '3pose';
let animKind = 'idle';
let dir = 'down';
let weaponId = new URLSearchParams(location.search).get('weapon') === 'custom_forge' && weapons.custom_forge
  ? 'custom_forge'
  : 'melee_sword';
if ($('st-weapon')) $('st-weapon').value = weaponId;
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
  undoStack.push({ target: editTarget, width: spriteWidth, height: spriteHeight, data: cloneGrid(g) });
  if (undoStack.length > 35) undoStack.shift();
}
function popUndo() {
  const item = undoStack.pop();
  if (!item) { toast('Nothing to undo'); return; }
  if (item.target !== editTarget) {
    editTarget = item.target;
    if ($('st-edit-target')) $('st-edit-target').value = editTarget;
  }
  if (item.width !== spriteWidth || item.height !== spriteHeight) {
    spriteWidth = item.width;
    spriteHeight = item.height;
    baseDown = resizeGridDimensions(baseDown, spriteWidth, spriteHeight, item.width, item.height, 'stretch');
    baseUp = resizeGridDimensions(baseUp, spriteWidth, spriteHeight, item.width, item.height, 'stretch');
    baseRight = resizeGridDimensions(baseRight, spriteWidth, spriteHeight, item.width, item.height, 'stretch');
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

const SIZES_8PX = [8, 16, 24, 32, 40, 48, 56, 64];

function populateSizeDropdowns() {
  for (const id of ['st-size-w', 'st-size-h', 'st-conv-w', 'st-conv-h']) {
    const sel = $(id);
    if (!sel) continue;
    sel.innerHTML = '';
    for (const s of SIZES_8PX) {
      const opt = document.createElement('option');
      opt.value = String(s);
      const tiles = Math.ceil(s / 8);
      opt.textContent = `${s}px (${tiles} tile${tiles > 1 ? 's' : ''})`;
      sel.append(opt);
    }
  }
}

function syncSizeTabsUI() {
  if ($('st-size-tabs')) {
    for (const b of $('st-size-tabs').querySelectorAll('button[data-w]')) {
      const isMatch = +b.dataset.w === spriteWidth && +b.dataset.h === spriteHeight;
      b.classList.toggle('on', isMatch);
    }
  }
  if ($('st-size-w')) $('st-size-w').value = String(spriteWidth);
  if ($('st-size-h')) $('st-size-h').value = String(spriteHeight);
  if ($('st-conv-w')) $('st-conv-w').value = String(convCrop.targetW || spriteWidth);
  if ($('st-conv-h')) $('st-conv-h').value = String(convCrop.targetH || spriteHeight);

  const tilesX = Math.ceil(spriteWidth / 8);
  const tilesY = Math.ceil(spriteHeight / 8);
  if ($('st-tile-badge')) {
    $('st-tile-badge').textContent = `${tilesX}×${tilesY} Tiles (${spriteWidth}×${spriteHeight})`;
  }
  const activeW = editTarget === 'weapon' ? 16 : spriteWidth;
  const activeH = editTarget === 'weapon' ? 16 : spriteHeight;
  if ($('st-editor-title')) {
    $('st-editor-title').textContent = `3. ${activeW}×${activeH} 2bpp Pixel Editor (${Math.ceil(activeW / 8)}×${Math.ceil(activeH / 8)} Tiles)`;
  }
  if ($('st-sheet-title')) {
    $('st-sheet-title').textContent = `All Generated Sequences (${spriteWidth}×${spriteHeight} GBC/DMG Sheet)`;
  }
  syncColorInputs();
}

function drawGrid(ctx, grid, ox = 0, oy = 0, palToUse = charPalette, w = null, h = null) {
  const W = w || (grid.length === 256 ? 16 : grid.length === 384 ? 16 : spriteWidth);
  const H = h || (grid.length === 256 ? 16 : grid.length === 384 ? 24 : Math.max(1, Math.round(grid.length / W)));
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const v = grid[y * W + x];
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
  const isHeroRig = spriteWidth === 16 && (spriteHeight === 16 || spriteHeight === 24);
  if (isHeroRig) {
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
    if ($('st-hw-stats')) {
      $('st-hw-stats').textContent = `16×${st.height} · ${st.anims} anims · ${st.frames}f · ${st.columns} unique 8×16 cols · ${st.romBytes} B ROM`;
    }
  } else {
    currentRig = null;
    currentCompiled = compileArbitrarySpriteToGbdk(baseDown, spriteWidth, spriteHeight, 'spr_custom', charPalette);
    const st = currentCompiled.stats;
    if ($('st-hw-stats')) {
      $('st-hw-stats').textContent = `${st.width}×${st.height} (${st.tilesX}×${st.tilesY} tiles) · ${st.uniqueTiles} unique 8×8 tiles · ${st.romBytes} B ROM`;
    }
  }
  syncColorInputs();
  syncSizeTabsUI();
  renderEditor();
  renderFrameStrip();
  renderSheet();
}

function setCanvasDimensions(newW, newH) {
  newW = Math.max(8, Math.min(64, Math.round(newW / 8) * 8));
  newH = Math.max(8, Math.min(64, Math.round(newH / 8) * 8));
  if (newW === spriteWidth && newH === spriteHeight) return;
  pushUndo();
  const stretch = $('st-size-stretch') ? $('st-size-stretch').checked : true;
  baseDown = resizeGridDimensions(baseDown, spriteWidth, spriteHeight, newW, newH, stretch ? 'stretch' : 'pad', 'bottom');
  baseUp = resizeGridDimensions(baseUp, spriteWidth, spriteHeight, newW, newH, stretch ? 'stretch' : 'pad', 'bottom');
  baseRight = resizeGridDimensions(baseRight, spriteWidth, spriteHeight, newW, newH, stretch ? 'stretch' : 'pad', 'bottom');
  baseAtkDown = null;
  baseAtkUp = null;
  baseAtkRight = null;
  spriteWidth = newW;
  spriteHeight = newH;
  rebuildRig();
  toast(`Resized canvas to ${newW}×${newH} (${stretch ? 'stretched' : 'padded'} ${Math.ceil(newW / 8)}×${Math.ceil(newH / 8)} tiles)!`);
}

function setCanvasHeight(newH) {
  setCanvasDimensions(spriteWidth, newH);
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
  if (w?.customPalette) {
    weapPalette = w.customPalette.map((c) => (c ? [...c] : null));
    return;
  }
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
  const W = editTarget === 'weapon' ? 16 : spriteWidth;
  const H = editTarget === 'weapon' ? 16 : spriteHeight;
  const activePal = editTarget === 'weapon' ? weapPalette : charPalette;
  const EDIT_CELL = Math.max(8, Math.min(28, Math.floor(400 / Math.max(W, H))));
  const cw = W * EDIT_CELL;
  const ch = H * EDIT_CELL;
  if (editCanvas.width !== cw || editCanvas.height !== ch) {
    editCanvas.width = cw;
    editCanvas.height = ch;
  }

  const showOnion = onionSkin && (editTarget === 'up' || editTarget === 'right') && baseDown.length === g.length;

  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const v = g[y * W + x];
      const px = x * EDIT_CELL;
      const py = y * EDIT_CELL;
      if (v === 0) {
        editCtx.fillStyle = (x + y) % 2 === 0 ? '#d8dfd0' : '#cad2c0';
        editCtx.fillRect(px, py, EDIT_CELL, EDIT_CELL);
        if (showOnion) {
          const ov = baseDown[y * W + x];
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

  // Subtle 1px pixel grid lines & bold 8x8 hardware tile boundary guides
  if (showGridLines) {
    editCtx.fillStyle = 'rgba(25, 35, 25, 0.16)';
    for (let x = 1; x < W; x++) editCtx.fillRect(x * EDIT_CELL, 0, 1, ch);
    for (let y = 1; y < H; y++) editCtx.fillRect(0, y * EDIT_CELL, cw, 1);

    // Bold 8x8 tile division boundaries (Game Boy hardware tiles)
    editCtx.fillStyle = 'rgba(20, 110, 65, 0.55)';
    for (let x = 8; x < W; x += 8) editCtx.fillRect(x * EDIT_CELL - 1, 0, 2, ch);
    for (let y = 8; y < H; y += 8) editCtx.fillRect(0, y * EDIT_CELL - 1, cw, 2);
  }

  // Mirror X center line indicator
  if (mirrorX) {
    const midX = Math.floor(W / 2);
    editCtx.fillStyle = 'rgba(24, 150, 230, 0.75)';
    editCtx.fillRect(midX * EDIT_CELL - 1, 0, 2, ch);
  }
}

function renderFrameStrip() {
  const strip = $('st-frame-strip');
  if (!strip) return;
  strip.innerHTML = '';

  if (currentRig) {
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
  } else {
    // Non-hero sprite (e.g. 8x8, 24x24, 32x32, 48x48, 64x64) -> show base directional poses
    const poses = [
      { name: 'Down (Front)', grid: baseDown },
      { name: 'Up (Back)', grid: baseUp },
      { name: 'Right (Side)', grid: baseRight },
      { name: 'Left (Mirror)', grid: flipGridX(baseRight) },
    ];
    poses.forEach((p, idx) => {
      const card = document.createElement('div');
      card.className = 'fcard' + (p.grid === getActiveEditGrid() ? ' on' : '');
      const c = document.createElement('canvas');
      c.width = Math.min(64, spriteWidth);
      c.height = Math.min(64, spriteHeight);
      const cx = c.getContext('2d');
      cx.fillStyle = '#d8dfd0';
      cx.fillRect(0, 0, c.width, c.height);
      drawGrid(cx, p.grid, 0, 0, charPalette, spriteWidth, spriteHeight);
      const lbl = document.createElement('span');
      lbl.textContent = p.name;
      card.append(c, lbl);
      card.onclick = () => {
        if (idx === 0) $('st-edit-target').value = 'down';
        else if (idx === 1) $('st-edit-target').value = 'up';
        else if (idx === 2) $('st-edit-target').value = 'right';
        editTarget = $('st-edit-target').value;
        renderEditor();
        renderFrameStrip();
      };
      strip.append(card);
    });
  }
}

function renderSheet() {
  if (currentRig) {
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
  } else {
    // Arbitrary size sprite sheet (Down, Up, Right)
    sheetCanvas.width = spriteWidth * 3;
    sheetCanvas.height = spriteHeight;
    sheetCtx.fillStyle = '#d8dfd0';
    sheetCtx.fillRect(0, 0, sheetCanvas.width, sheetCanvas.height);
    drawGrid(sheetCtx, baseDown, 0, 0, charPalette, spriteWidth, spriteHeight);
    drawGrid(sheetCtx, baseUp, spriteWidth, 0, charPalette, spriteWidth, spriteHeight);
    drawGrid(sheetCtx, baseRight, spriteWidth * 2, 0, charPalette, spriteWidth, spriteHeight);
  }
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

  if (!currentRig) {
    // Non-hero sprite preview: render active direction centered on the LCD stage
    const activeGrid = dir === 'up' ? baseUp : dir === 'right' ? baseRight : dir === 'left' ? flipGridX(baseRight) : baseDown;
    const ox = Math.max(0, Math.round((64 - spriteWidth) / 2));
    const oy = Math.max(0, Math.round((64 - spriteHeight) / 2));
    // Ground shadow
    prevCtx.fillStyle = '#9aa690';
    prevCtx.fillRect(ox + 2, Math.min(62, oy + spriteHeight - 2), Math.max(4, spriteWidth - 4), 2);
    drawGrid(prevCtx, activeGrid, ox, oy, charPalette, spriteWidth, spriteHeight);
    return;
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
      const snapped = snapColorToGbc(r, g, b);
      if (editTarget === 'weapon') {
        weapPalette[i] = snapped;
        if (weapPalSel) weapPalSel.value = 'preset_auto';
      } else {
        charPalette[i] = snapped;
        if (charPalSel) charPalSel.value = 'preset_auto';
      }
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
    const b = e.target.closest('button[data-w]');
    if (!b) return;
    const w = +b.dataset.w || 16;
    const h = +b.dataset.h || 16;
    setCanvasDimensions(w, h);
  });
}
if ($('st-size-w')) {
  $('st-size-w').onchange = (e) => {
    const w = +e.target.value || 16;
    setCanvasDimensions(w, spriteHeight);
  };
}
if ($('st-size-h')) {
  $('st-size-h').onchange = (e) => {
    const h = +e.target.value || 16;
    setCanvasDimensions(spriteWidth, h);
  };
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

if ($('st-tool-grid')) {
  $('st-tool-grid').onclick = () => {
    showGridLines = !showGridLines;
    $('st-tool-grid').classList.toggle('on', showGridLines);
    renderEditor();
  };
}

function floodFillGridGeneric(g, W, H, startX, startY, newShade) {
  if (startX < 0 || startX >= W || startY < 0 || startY >= H) return g;
  const target = g[startY * W + startX];
  if (target === newShade) return g;
  const stack = [[startX, startY]];
  while (stack.length) {
    const [x, y] = stack.pop();
    if (x < 0 || x >= W || y < 0 || y >= H) continue;
    if (g[y * W + x] !== target) continue;
    g[y * W + x] = newShade;
    stack.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
  }
  return g;
}

let painting = false;
let activeStrokeShade = 3;
function paintAt(e, isStart = false) {
  const g = getActiveEditGrid();
  const W = editTarget === 'weapon' ? 16 : spriteWidth;
  const H = editTarget === 'weapon' ? 16 : spriteHeight;
  const r = editCanvas.getBoundingClientRect();
  const x = Math.floor(((e.clientX - r.left) / r.width) * W);
  const y = Math.floor(((e.clientY - r.top) / r.height) * H);
  if (x < 0 || x >= W || y < 0 || y >= H) return;

  // Eyedropper tool or Alt-click picks shade under cursor
  if (editTool === 'picker' || e.altKey) {
    selectShade(g[y * W + x] & 3);
    if (editTool === 'picker') setEditTool('pencil');
    toast(`Picked shade ${brushShade}`);
    painting = false;
    return;
  }

  if (isStart) {
    pushUndo();
    const curVal = g[y * W + x] & 3;
    if (e.button === 2 || brushShade === 0) {
      activeStrokeShade = 0;
    } else if (curVal === brushShade) {
      // Tapping an already filled pixel erases it!
      activeStrokeShade = 0;
    } else {
      activeStrokeShade = brushShade;
    }
  }

  if (editTool === 'bucket' && isStart) {
    floodFillGridGeneric(g, W, H, x, y, activeStrokeShade);
    if (mirrorX) floodFillGridGeneric(g, W, H, W - 1 - x, y, activeStrokeShade);
    if (editTarget !== 'weapon') { baseAtkDown = null; baseAtkUp = null; baseAtkRight = null; }
    rebuildRig();
    painting = false;
    return;
  }

  let changed = false;
  if (g[y * W + x] !== activeStrokeShade) {
    g[y * W + x] = activeStrokeShade;
    changed = true;
  }
  if (mirrorX && g[y * W + (W - 1 - x)] !== activeStrokeShade) {
    g[y * W + (W - 1 - x)] = activeStrokeShade;
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
  const W = editTarget === 'weapon' ? 16 : spriteWidth;
  const H = editTarget === 'weapon' ? 16 : spriteHeight;
  const copy = new Uint8Array(g);
  g.fill(0);
  for (let y = 0; y < H; y++) {
    const sy = y - dy;
    if (sy < 0 || sy >= H) continue;
    for (let x = 0; x < W; x++) {
      const sx = x - dx;
      if (sx < 0 || sx >= W) continue;
      g[y * W + x] = copy[sy * W + sx];
    }
  }
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
  const W = editTarget === 'weapon' ? 16 : spriteWidth;
  const H = editTarget === 'weapon' ? 16 : spriteHeight;
  const copy = new Uint8Array(g);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      g[y * W + x] = copy[y * W + (W - 1 - x)];
    }
  }
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

// Interactive Image -> Game Boy (2bpp / GBC RGB555) Sprite Converter + 8px increments crop & dither
let activeConvImg = null;
let activeConvCanvas = null;
let activeConvData = null;
let activeConvName = 'image';
let lastConvResult = null;

// Crop state: cx, cy in [0..1] normalized source coords, zoom in [12..100] % of max fitting frame
const convCrop = {
  targetW: 16,
  targetH: 16,
  cx: 0.5,
  cy: 0.5,
  zoom: 88,
};

// Display layout cache for #st-conv-src interactive pointer events
let srcView = { ox: 0, oy: 0, dw: 200, dh: 200, scale: 1, cropRect: { x: 0, y: 0, w: 16, h: 16 } };

function syncCropRatioTabsUI() {
  const tw = convCrop.targetW || 16;
  const th = convCrop.targetH || 16;
  if ($('st-crop-ratio-tabs')) {
    for (const b of $('st-crop-ratio-tabs').querySelectorAll('button[data-w]')) {
      const match = +b.dataset.w === tw && +b.dataset.h === th;
      b.classList.toggle('on', match);
    }
  }
  if ($('st-conv-w')) $('st-conv-w').value = String(tw);
  if ($('st-conv-h')) $('st-conv-h').value = String(th);
  if ($('st-crop-zoom')) $('st-crop-zoom').value = String(Math.round(convCrop.zoom));
  if ($('st-crop-zoom-val')) $('st-crop-zoom-val').textContent = `${Math.round(convCrop.zoom)}%`;
}

/** Compute the source-image pixel rectangle { x, y, w, h } locked to target aspect ratio (targetW : targetH). */
function getLockedCropRect() {
  if (!activeConvImg) return { x: 0, y: 0, w: 16, h: 16, maxW: 16, maxH: 16 };
  const imgW = activeConvImg.width;
  const imgH = activeConvImg.height;
  const tw = convCrop.targetW || 16;
  const th = convCrop.targetH || 16;
  const aspect = tw / th;
  let maxW, maxH;
  if (imgW / imgH > aspect) {
    maxH = imgH;
    maxW = imgH * aspect;
  } else {
    maxW = imgW;
    maxH = imgW / aspect;
  }
  const z = Math.max(0.12, Math.min(1.0, convCrop.zoom / 100));
  const w = Math.max(1, maxW * z);
  const h = Math.max(1, maxH * z);
  const halfW = w / 2;
  const halfH = h / 2;
  const cxPx = Math.max(halfW, Math.min(imgW - halfW, convCrop.cx * imgW));
  const cyPx = Math.max(halfH, Math.min(imgH - halfH, convCrop.cy * imgH));
  // Keep normalized center clamped so dragging never drifts out of bounds
  convCrop.cx = imgW > 0 ? cxPx / imgW : 0.5;
  convCrop.cy = imgH > 0 ? cyPx / imgH : 0.5;
  return {
    x: cxPx - halfW,
    y: cyPx - halfH,
    w,
    h,
    maxW,
    maxH,
  };
}

/** Auto-snap the crop box tightly around the foreground subject. */
function autoFitCropToSubject() {
  if (!activeConvImg || !activeConvData) return;
  const W = activeConvImg.width;
  const H = activeConvImg.height;
  const tw = convCrop.targetW || 16;
  const th = convCrop.targetH || 16;
  // Exact match pixel art -> use 100% full frame
  if (W === tw && H === th) {
    convCrop.cx = 0.5;
    convCrop.cy = 0.5;
    convCrop.zoom = 100;
    syncCropRatioTabsUI();
    return;
  }
  const bgMode = $('st-conv-bgmode')?.value || 'auto';
  const bgTol = +($('st-conv-tol')?.value ?? 36);
  const tolSq = bgTol * bgTol * 3;
  const hasAlphaCorner =
    activeConvData[3] < 128 ||
    activeConvData[(W - 1) * 4 + 3] < 128 ||
    activeConvData[(H - 1) * W * 4 + 3] < 128;
  const bgR = activeConvData[0], bgG = activeConvData[1], bgB = activeConvData[2];
  const useCorner = bgMode === 'corner' || (bgMode === 'auto' && !hasAlphaCorner && bgTol > 0);
  const useWhite = bgMode === 'white';

  let minX = W, minY = H, maxX = -1, maxY = -1;
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const si = (y * W + x) * 4;
      const a = activeConvData[si + 3];
      if (a < 128) continue;
      const r = activeConvData[si], g = activeConvData[si + 1], b = activeConvData[si + 2];
      if (useCorner) {
        const dr = r - bgR, dg = g - bgG, db = b - bgB;
        if (dr * dr + dg * dg + db * db <= tolSq) continue;
      } else if (useWhite) {
        const thr = 255 - bgTol;
        if (r >= thr && g >= thr && b >= thr) continue;
      }
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }
  }
  if (maxX >= minX && maxY >= minY) {
    const subW = Math.max(1, maxX - minX + 1);
    const subH = Math.max(1, maxY - minY + 1);
    convCrop.cx = (minX + maxX + 1) / (2 * W);
    convCrop.cy = (minY + maxY + 1) / (2 * H);
    const { maxW, maxH } = getLockedCropRect();
    // Leave ~10% breathing margin around the detected subject
    const neededZoom = Math.max((subW * 1.12) / Math.max(1, maxW), (subH * 1.12) / Math.max(1, maxH)) * 100;
    convCrop.zoom = Math.max(12, Math.min(100, Math.round(neededZoom)));
  } else {
    convCrop.cx = 0.5;
    convCrop.cy = 0.5;
    convCrop.zoom = 100;
  }
  syncCropRatioTabsUI();
}

function openImageConverter(img, sourceName = 'image') {
  activeConvImg = img;
  activeConvName = sourceName;
  activeConvCanvas = document.createElement('canvas');
  activeConvCanvas.width = img.width;
  activeConvCanvas.height = img.height;
  const cx = activeConvCanvas.getContext('2d');
  cx.drawImage(img, 0, 0);
  activeConvData = cx.getImageData(0, 0, img.width, img.height).data;

  // If image matches a multiple of 8, suggest it, otherwise default to current studio dimensions
  if (img.width >= 8 && img.width <= 64 && img.width % 8 === 0 && img.height >= 8 && img.height <= 64 && img.height % 8 === 0) {
    convCrop.targetW = img.width;
    convCrop.targetH = img.height;
  } else {
    convCrop.targetW = spriteWidth;
    convCrop.targetH = spriteHeight;
  }

  if ($('st-conv-target') && $('st-upload-target')) {
    $('st-conv-target').value = $('st-upload-target').value || 'down';
  }

  autoFitCropToSubject();
  $('st-converter-card')?.classList.remove('hidden');
  pushUndo();
  updateConverterPreview(true);
  toast(`Loaded ${sourceName} (${img.width}×${img.height}) — drag the ${convCrop.targetW}×${convCrop.targetH} box to crop!`);
}

function updateConverterPreview(applyLive = true) {
  if (!activeConvImg || !activeConvData || !activeConvCanvas) return;
  const targetW = convCrop.targetW || 16;
  const targetH = convCrop.targetH || 16;
  const bgMode = $('st-conv-bgmode')?.value || 'auto';
  const bgTolerance = +($('st-conv-tol')?.value ?? 36);
  const shadeBias = +($('st-conv-bias')?.value ?? 0);
  const autoOutline = $('st-conv-outline') ? $('st-conv-outline').checked : false;
  const dither = $('st-conv-dither') ? $('st-conv-dither').checked : false;
  const usePal = $('st-conv-usepal') ? $('st-conv-usepal').checked : true;

  if ($('st-conv-tol-val')) $('st-conv-tol-val').textContent = String(bgTolerance);
  if ($('st-conv-bias-val')) $('st-conv-bias-val').textContent = (shadeBias > 0 ? '+' : '') + shadeBias;
  const tilesX = Math.ceil(targetW / 8);
  const tilesY = Math.ceil(targetH / 8);
  if ($('st-conv-dst-label')) $('st-conv-dst-label').textContent = `Converted ${targetW}×${targetH} (${tilesX}×${tilesY} Tiles 2bpp)`;
  syncCropRatioTabsUI();

  const rect = getLockedCropRect();
  if ($('st-crop-readout')) {
    $('st-crop-readout').textContent = `Crop (${targetW}×${targetH}): ${Math.round(rect.w)}×${Math.round(rect.h)}px at (${Math.round(rect.x)}, ${Math.round(rect.y)})`;
  }

  // Render interactive source crop canvas with dark exterior mask, 8x8 tile guides, and corner handles
  const srcCanvas = $('st-conv-src');
  if (srcCanvas) {
    const maxBox = 200;
    const scale = Math.min(maxBox / activeConvImg.width, maxBox / activeConvImg.height);
    const dw = Math.max(64, Math.round(activeConvImg.width * scale));
    const dh = Math.max(64, Math.round(activeConvImg.height * scale));
    srcCanvas.width = dw;
    srcCanvas.height = dh;
    const sctx = srcCanvas.getContext('2d');
    sctx.clearRect(0, 0, dw, dh);
    sctx.imageSmoothingEnabled = activeConvImg.width > 64;
    sctx.drawImage(activeConvImg, 0, 0, dw, dh);

    const cx0 = rect.x * scale;
    const cy0 = rect.y * scale;
    const cw = rect.w * scale;
    const ch = rect.h * scale;
    srcView = { ox: 0, oy: 0, dw, dh, scale, cropRect: { x: cx0, y: cy0, w: cw, h: ch } };

    // Dim area outside the crop box
    sctx.fillStyle = 'rgba(10, 14, 12, 0.55)';
    sctx.fillRect(0, 0, dw, cy0);
    sctx.fillRect(0, cy0 + ch, dw, dh - (cy0 + ch));
    sctx.fillRect(0, cy0, cx0, ch);
    sctx.fillRect(cx0 + cw, cy0, dw - (cx0 + cw), ch);

    // Subtle 8x8 hardware tile grid inside the crop box
    sctx.strokeStyle = 'rgba(255, 255, 255, 0.35)';
    sctx.lineWidth = 1;
    sctx.beginPath();
    for (let c = 1; c < tilesX; c++) {
      const tx = cx0 + (cw * c) / tilesX;
      sctx.moveTo(tx, cy0);
      sctx.lineTo(tx, cy0 + ch);
    }
    for (let r = 1; r < tilesY; r++) {
      const ty = cy0 + (ch * r) / tilesY;
      sctx.moveTo(cx0, ty);
      sctx.lineTo(cx0 + cw, ty);
    }
    sctx.stroke();

    // Crisp crop border
    sctx.strokeStyle = '#18d8ff';
    sctx.lineWidth = 2;
    sctx.strokeRect(cx0 + 1, cy0 + 1, Math.max(2, cw - 2), Math.max(2, ch - 2));

    // 4 Corner resize handles
    const hs = 7;
    sctx.fillStyle = '#ffe878';
    sctx.strokeStyle = '#101820';
    sctx.lineWidth = 1;
    const corners = [
      [cx0, cy0],
      [cx0 + cw - hs, cy0],
      [cx0, cy0 + ch - hs],
      [cx0 + cw - hs, cy0 + ch - hs],
    ];
    for (const [hx, hy] of corners) {
      sctx.fillRect(hx, hy, hs, hs);
      sctx.strokeRect(hx, hy, hs, hs);
    }
  }

  // Extract the exact cropped sub-image into a temporary canvas & run 2bpp GBC conversion
  const cropW = Math.max(1, Math.round(rect.w));
  const cropH = Math.max(1, Math.round(rect.h));
  const subCanvas = document.createElement('canvas');
  subCanvas.width = cropW;
  subCanvas.height = cropH;
  const subCtx = subCanvas.getContext('2d');
  subCtx.drawImage(activeConvCanvas, rect.x, rect.y, rect.w, rect.h, 0, 0, cropW, cropH);
  const subRgba = subCtx.getImageData(0, 0, cropW, cropH).data;

  const bgColor = activeConvData.length >= 4
    ? [activeConvData[0], activeConvData[1], activeConvData[2], activeConvData[3]]
    : null;
  lastConvResult = extractGridAndPalette(subRgba, cropW, cropH, targetW, targetH, {
    bgMode,
    bgTolerance,
    bgColor,
    autoCrop: false, // The interactive crop box defines the exact framing!
    shadeBias,
    autoOutline,
    dither,
  });

  // Render converted 2bpp preview canvas
  const dstCanvas = $('st-conv-dst');
  if (dstCanvas) {
    const scale = Math.max(2, Math.floor(140 / Math.max(targetW, targetH)));
    dstCanvas.width = targetW * scale;
    dstCanvas.height = targetH * scale;
    const dctx = dstCanvas.getContext('2d');
    dctx.clearRect(0, 0, dstCanvas.width, dstCanvas.height);
    const palToPreview = usePal ? lastConvResult.palette : charPalette;
    for (let y = 0; y < targetH; y++) {
      for (let x = 0; x < targetW; x++) {
        const v = lastConvResult.grid[y * targetW + x];
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
  const targetW = lastConvResult.width || convCrop.targetW || 16;
  const targetH = lastConvResult.height || convCrop.targetH || 16;
  if (targetW !== spriteWidth || targetH !== spriteHeight) {
    setCanvasDimensions(targetW, targetH, false);
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
    toast(`Applied ${targetW}×${targetH} sprite (${activeConvName}) & updated canvas!`);
  }
}

// Interactive pointer drag / corner-resize / wheel zoom on #st-conv-src
const convSrcCanvas = $('st-conv-src');
if (convSrcCanvas) {
  let dragMode = null; // 'pan' | 'resize'
  let startX = 0, startY = 0, startCx = 0.5, startCy = 0.5, startZoom = 85;

  const toCanvasCoords = (e) => {
    const r = convSrcCanvas.getBoundingClientRect();
    return {
      x: ((e.clientX - r.left) / Math.max(1, r.width)) * convSrcCanvas.width,
      y: ((e.clientY - r.top) / Math.max(1, r.height)) * convSrcCanvas.height,
    };
  };

  convSrcCanvas.addEventListener('pointerdown', (e) => {
    if (!activeConvImg) return;
    e.preventDefault();
    convSrcCanvas.setPointerCapture?.(e.pointerId);
    const pt = toCanvasCoords(e);
    const cr = srcView.cropRect;
    const cornerDist = Math.min(
      Math.hypot(pt.x - cr.x, pt.y - cr.y),
      Math.hypot(pt.x - (cr.x + cr.w), pt.y - cr.y),
      Math.hypot(pt.x - cr.x, pt.y - (cr.y + cr.h)),
      Math.hypot(pt.x - (cr.x + cr.w), pt.y - (cr.y + cr.h)),
    );
    dragMode = cornerDist <= 16 ? 'resize' : 'pan';
    startX = pt.x;
    startY = pt.y;
    startCx = convCrop.cx;
    startCy = convCrop.cy;
    startZoom = convCrop.zoom;
    // If user taps completely outside the crop box, jump the crop center there and pan
    if (
      dragMode === 'pan' &&
      (pt.x < cr.x || pt.x > cr.x + cr.w || pt.y < cr.y || pt.y > cr.y + cr.h)
    ) {
      convCrop.cx = pt.x / Math.max(1, convSrcCanvas.width);
      convCrop.cy = pt.y / Math.max(1, convSrcCanvas.height);
      startCx = convCrop.cx;
      startCy = convCrop.cy;
      updateConverterPreview(true);
    }
  });

  convSrcCanvas.addEventListener('pointermove', (e) => {
    if (!dragMode || !activeConvImg) return;
    const pt = toCanvasCoords(e);
    const dx = pt.x - startX;
    const dy = pt.y - startY;
    if (dragMode === 'pan') {
      convCrop.cx = startCx + dx / Math.max(1, convSrcCanvas.width);
      convCrop.cy = startCy + dy / Math.max(1, convSrcCanvas.height);
    } else if (dragMode === 'resize') {
      const centerPxX = convCrop.cx * convSrcCanvas.width;
      const centerPxY = convCrop.cy * convSrcCanvas.height;
      const d0 = Math.max(8, Math.hypot(startX - centerPxX, startY - centerPxY));
      const d1 = Math.max(8, Math.hypot(pt.x - centerPxX, pt.y - centerPxY));
      convCrop.zoom = Math.max(12, Math.min(100, startZoom * (d1 / d0)));
    }
    updateConverterPreview(true);
  });

  const endCropDrag = () => { dragMode = null; };
  convSrcCanvas.addEventListener('pointerup', endCropDrag);
  convSrcCanvas.addEventListener('pointercancel', endCropDrag);

  convSrcCanvas.addEventListener('wheel', (e) => {
    if (!activeConvImg) return;
    e.preventDefault();
    const delta = e.deltaY > 0 ? 5 : -5;
    convCrop.zoom = Math.max(12, Math.min(100, convCrop.zoom + delta));
    updateConverterPreview(true);
  }, { passive: false });
}

if ($('st-crop-ratio-tabs')) {
  $('st-crop-ratio-tabs').addEventListener('click', (e) => {
    const b = e.target.closest('button[data-w]');
    if (!b) return;
    convCrop.targetW = +b.dataset.w || 16;
    convCrop.targetH = +b.dataset.h || 16;
    updateConverterPreview(true);
  });
}
if ($('st-conv-w')) {
  $('st-conv-w').onchange = (e) => {
    convCrop.targetW = +e.target.value || 16;
    updateConverterPreview(true);
  };
}
if ($('st-conv-h')) {
  $('st-conv-h').onchange = (e) => {
    convCrop.targetH = +e.target.value || 16;
    updateConverterPreview(true);
  };
}
if ($('st-crop-zoom')) {
  $('st-crop-zoom').oninput = (e) => {
    convCrop.zoom = +e.target.value;
    updateConverterPreview(true);
  };
}
if ($('st-crop-autosnap')) {
  $('st-crop-autosnap').onclick = () => {
    autoFitCropToSubject();
    updateConverterPreview(true);
  };
}
if ($('st-crop-reset')) {
  $('st-crop-reset').onclick = () => {
    convCrop.cx = 0.5;
    convCrop.cy = 0.5;
    convCrop.zoom = 100;
    updateConverterPreview(true);
  };
}
const nudgeCrop = (dxNorm, dyNorm) => {
  convCrop.cx = Math.max(0, Math.min(1, convCrop.cx + dxNorm));
  convCrop.cy = Math.max(0, Math.min(1, convCrop.cy + dyNorm));
  updateConverterPreview(true);
};
if ($('st-crop-left')) $('st-crop-left').onclick = () => nudgeCrop(-0.025, 0);
if ($('st-crop-right')) $('st-crop-right').onclick = () => nudgeCrop(0.025, 0);
if ($('st-crop-up')) $('st-crop-up').onclick = () => nudgeCrop(0, -0.025);
if ($('st-crop-down')) $('st-crop-down').onclick = () => nudgeCrop(0, 0.025);

['st-conv-target', 'st-conv-bgmode', 'st-conv-tol', 'st-conv-bias', 'st-conv-outline', 'st-conv-dither', 'st-conv-usepal'].forEach((id) => {
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

populateSizeDropdowns();
syncSizeTabsUI();
syncWeaponPaletteFromWeapon();
loadPreset(Object.keys(allPresets)[0] || 'adventurer');
setInterval(stepStudio, 1000 / 60);
window.rgbStudio = { rebuildRig, loadPreset, serializeSpriteForChat, loadSpriteFromCode };
