// Standalone RogueGB Gemini 2bpp Asset Forge (Weapons, UI/HUD, Chests, Power-Ups, Enemies)
import { GBC_PALETTES, snapColorToGbc, rgbToCgb555 } from '../harness/sprite-gen.mjs';
import {
  ASSET_CATEGORIES, parseSizeSpec, parseAssetRows, gridToAssetRows, cloneAssetGrid,
  flipAssetX, shiftAssetGrid, floodFillAssetGrid, outlineAssetGrid, STARTER_ASSETS,
  listGeminiModels, generateAssetWithGemini, compileAssetToGbdk,
} from '../harness/asset-forge.mjs';

const $ = (id) => document.getElementById(id);

let toastTimer = null;
function toast(msg) {
  const el = $('toast');
  if (!el) return;
  el.textContent = msg;
  el.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.add('hidden'), 3000);
}

function download(filename, blobOrUrl) {
  const a = document.createElement('a');
  a.href = typeof blobOrUrl === 'string' ? blobOrUrl : URL.createObjectURL(blobOrUrl);
  a.download = filename;
  a.click();
  if (typeof blobOrUrl !== 'string') setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

function rgbToHex([r, g, b]) {
  return '#' + [r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('');
}
function hexToRgb(hex) {
  const n = parseInt(hex.replace('#', ''), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

// Persistent API key & saved library in localStorage
const LS_KEY_API = 'roguegb_gemini_key';
const LS_KEY_LIB = 'roguegb_asset_library_v1';

function loadSavedLibrary() {
  try {
    const raw = localStorage.getItem(LS_KEY_LIB);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}
function saveLibraryToStorage(items) {
  try {
    localStorage.setItem(LS_KEY_LIB, JSON.stringify(items.slice(0, 40)));
  } catch {}
}

let customLibrary = loadSavedLibrary();
let activeAsset = null;
let activeStateIdx = 0;
let activeCompiled = null;
let brushShade = 3;
let editTool = 'pencil'; // 'pencil' | 'bucket' | 'picker'
let mirrorX = false;
let onionSkin = true;
let animPlay = true;
let darkBg = false;
let animTick = 0;

const undoStack = [];
function pushUndo() {
  if (!activeAsset) return;
  const st = activeAsset.states[activeStateIdx];
  if (!st) return;
  undoStack.push({
    stateIdx: activeStateIdx,
    width: activeAsset.width,
    height: activeAsset.height,
    grid: cloneAssetGrid(st.grid),
  });
  if (undoStack.length > 35) undoStack.shift();
}
function popUndo() {
  const item = undoStack.pop();
  if (!item || !activeAsset) { toast('Nothing to undo'); return; }
  const st = activeAsset.states[item.stateIdx];
  if (st && st.grid.length === item.grid.length) {
    activeStateIdx = item.stateIdx;
    st.grid.set(item.grid);
    st.rows = gridToAssetRows(st.grid, activeAsset.width, activeAsset.height);
    rebuildAsset();
    toast('Undid last edit');
  }
}

addEventListener('keydown', (e) => {
  if (e.target.matches('input, textarea, select')) return;
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    popUndo();
  }
});

const prevCanvas = $('fg-preview');
const prevCtx = prevCanvas.getContext('2d');
const editCanvas = $('fg-editor');
const editCtx = editCanvas.getContext('2d');

function hydrateAsset(raw) {
  const width = raw.width === 8 ? 8 : 16;
  const height = raw.height === 8 ? 8 : raw.height === 24 ? 24 : 16;
  const defPal = GBC_PALETTES.gbc_monk_gold.colors;
  const palette = Array.isArray(raw.palette) && raw.palette.length === 4
    ? raw.palette.map((c, i) => (i === 0 ? [0, 0, 0, 0] : c ? [...c] : [...defPal[i]]))
    : defPal.map((c) => [...c]);
  const states = (raw.states || []).map((s, idx) => {
    const grid = s.grid
      ? new Uint8Array(s.grid)
      : parseAssetRows(s.rows || [], width, height);
    return {
      name: String(s.name || `state_${idx}`),
      grid,
      rows: gridToAssetRows(grid, width, height),
    };
  });
  if (states.length === 0) {
    const empty = new Uint8Array(width * height);
    states.push({ name: 'idle', grid: empty, rows: gridToAssetRows(empty, width, height) });
  }
  return {
    id: String(raw.id || 'asset_custom'),
    name: String(raw.name || 'Custom Asset'),
    category: raw.category || 'chest',
    width,
    height,
    palette,
    states,
  };
}

function serializeActiveAsset() {
  if (!activeAsset) return '{}';
  return JSON.stringify(
    {
      id: activeAsset.id,
      name: activeAsset.name,
      category: activeAsset.category,
      width: activeAsset.width,
      height: activeAsset.height,
      palette: activeAsset.palette,
      states: activeAsset.states.map((s) => ({
        name: s.name,
        rows: gridToAssetRows(s.grid, activeAsset.width, activeAsset.height),
      })),
    },
    null,
    2,
  );
}

function syncPaletteUI() {
  if (!activeAsset) return;
  const pal = activeAsset.palette;
  const cgbHexes = [];
  for (let i = 1; i <= 3; i++) {
    const colEl = $(`fg-col-${i}`);
    if (colEl && pal[i]) colEl.value = rgbToHex(pal[i]);
    const sw = $(`fg-sw-${i}`);
    if (sw && pal[i]) sw.style.background = rgbToHex(pal[i]);
    if (pal[i]) {
      const c555 = rgbToCgb555(pal[i][0], pal[i][1], pal[i][2]);
      cgbHexes.push('0x' + c555.toString(16).toUpperCase().padStart(4, '0'));
    }
  }
  if ($('fg-cgb-readout') && cgbHexes.length === 3) {
    $('fg-cgb-readout').textContent = `CGB RGB555: ${cgbHexes.join(' · ')}`;
  }
}

function drawAssetGrid(ctx, grid, width, height, ox, oy, palette) {
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const v = grid[y * width + x];
      if (!v) continue;
      const [r, g, b] = palette[v] || [0, 0, 0];
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fillRect(ox + x, oy + y, 1, 1);
    }
  }
}

function renderEditor() {
  if (!activeAsset) return;
  const W = activeAsset.width;
  const H = activeAsset.height;
  const st = activeAsset.states[activeStateIdx] || activeAsset.states[0];
  const g = st.grid;
  const pal = activeAsset.palette;
  const cell = W === 8 ? 24 : 14;
  const cw = W * cell;
  const ch = H * cell;
  if (editCanvas.width !== cw || editCanvas.height !== ch) {
    editCanvas.width = cw;
    editCanvas.height = ch;
  }

  const prevSt = activeStateIdx > 0 ? activeAsset.states[activeStateIdx - 1] : null;
  const showOnion = onionSkin && prevSt && prevSt.grid.length === g.length;

  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const v = g[y * W + x];
      const px = x * cell;
      const py = y * cell;
      if (v === 0) {
        if (darkBg) {
          editCtx.fillStyle = (x + y) % 2 === 0 ? '#222824' : '#1a1f1c';
        } else {
          editCtx.fillStyle = (x + y) % 2 === 0 ? '#d8dfd0' : '#cad2c0';
        }
        editCtx.fillRect(px, py, cell, cell);
        if (showOnion) {
          const ov = prevSt.grid[y * W + x];
          if (ov > 0) {
            const [or, og, ob] = pal[ov] || [0, 0, 0];
            editCtx.fillStyle = `rgba(${or},${og},${ob},0.24)`;
            editCtx.fillRect(px, py, cell, cell);
          }
        }
      } else {
        const [r, gr, b] = pal[v] || [0, 0, 0];
        editCtx.fillStyle = `rgb(${r},${gr},${b})`;
        editCtx.fillRect(px, py, cell, cell);
      }
    }
  }

  // Pixel grid lines
  editCtx.fillStyle = darkBg ? 'rgba(255,255,255,0.10)' : 'rgba(25, 35, 25, 0.14)';
  for (let x = 1; x < W; x++) editCtx.fillRect(x * cell, 0, 1, ch);
  for (let y = 1; y < H; y++) editCtx.fillRect(0, y * cell, cw, 1);

  // 8x8 tile boundary lines
  editCtx.fillStyle = 'rgba(24, 180, 110, 0.55)';
  if (W > 8) editCtx.fillRect(8 * cell, 0, 1, ch);
  if (H > 8) editCtx.fillRect(0, 8 * cell, cw, 1);
  if (H > 16) editCtx.fillRect(0, 16 * cell, cw, 1);

  if (mirrorX) {
    editCtx.fillStyle = 'rgba(24, 150, 230, 0.8)';
    editCtx.fillRect((W / 2) * cell - 1, 0, 2, ch);
  }
}

function renderStateStrip() {
  const strip = $('fg-state-strip');
  if (!strip || !activeAsset) return;
  strip.innerHTML = '';
  const W = activeAsset.width;
  const H = activeAsset.height;

  activeAsset.states.forEach((st, idx) => {
    const card = document.createElement('div');
    card.className = 'fcard' + (idx === activeStateIdx ? ' on' : '');
    const c = document.createElement('canvas');
    const pad = 4;
    c.width = W + pad * 2;
    c.height = H + pad * 2;
    const cx = c.getContext('2d');
    cx.fillStyle = darkBg ? '#1e2420' : '#d8dfd0';
    cx.fillRect(0, 0, c.width, c.height);
    drawAssetGrid(cx, st.grid, W, H, pad, pad, activeAsset.palette);

    const lbl = document.createElement('span');
    lbl.textContent = `${idx + 1}. ${st.name}`;
    card.append(c, lbl);
    card.onclick = () => {
      activeStateIdx = idx;
      if ($('fg-state-name')) $('fg-state-name').value = st.name;
      renderStateStrip();
      renderEditor();
    };
    strip.append(card);
  });
}

function renderLibrary() {
  const libEl = $('fg-library');
  if (!libEl) return;
  libEl.innerHTML = '';
  const allItems = [...customLibrary, ...STARTER_ASSETS];
  allItems.forEach((item) => {
    const hydrated = hydrateAsset(item);
    const card = document.createElement('div');
    card.className = 'fcard' + (activeAsset && activeAsset.id === hydrated.id && activeAsset.name === hydrated.name ? ' on' : '');
    const c = document.createElement('canvas');
    c.width = hydrated.width + 6;
    c.height = hydrated.height + 6;
    const cx = c.getContext('2d');
    cx.fillStyle = darkBg ? '#1e2420' : '#d8dfd0';
    cx.fillRect(0, 0, c.width, c.height);
    drawAssetGrid(cx, hydrated.states[0].grid, hydrated.width, hydrated.height, 3, 3, hydrated.palette);
    const lbl = document.createElement('span');
    lbl.textContent = hydrated.name.slice(0, 14);
    card.title = `${hydrated.name} (${hydrated.width}×${hydrated.height}, ${hydrated.states.length} states)`;
    card.append(c, lbl);
    card.onclick = () => loadAssetIntoForge(hydrated);
    libEl.append(card);
  });
}

function rebuildAsset() {
  if (!activeAsset) return;
  if (activeStateIdx >= activeAsset.states.length) activeStateIdx = Math.max(0, activeAsset.states.length - 1);
  for (const st of activeAsset.states) {
    st.rows = gridToAssetRows(st.grid, activeAsset.width, activeAsset.height);
  }
  activeCompiled = compileAssetToGbdk(activeAsset);
  const st = activeCompiled.stats;
  if ($('fg-asset-title')) {
    $('fg-asset-title').textContent = `${activeAsset.name} (${activeAsset.width}×${activeAsset.height})`;
  }
  if ($('fg-hw-stats')) {
    $('fg-hw-stats').textContent = `${st.width}×${st.height} · ${st.states} states · ${st.uniqueTiles} unique 8×8 tiles · ${st.romBytes} B ROM`;
  }
  if ($('fg-editor-title')) {
    $('fg-editor-title').textContent = `3. ${activeAsset.width}×${activeAsset.height} 2bpp Pixel Editor`;
  }
  if ($('fg-state-name') && activeAsset.states[activeStateIdx]) {
    $('fg-state-name').value = activeAsset.states[activeStateIdx].name;
  }
  if ($('fg-send-studio')) {
    $('fg-send-studio').classList.toggle('hidden', activeAsset.category !== 'weapon' || activeAsset.width !== 16 || activeAsset.height !== 16);
  }
  syncPaletteUI();
  renderEditor();
  renderStateStrip();
  renderLibrary();
}

function loadAssetIntoForge(assetObj) {
  activeAsset = hydrateAsset(assetObj);
  activeStateIdx = 0;
  undoStack.length = 0;
  if ($('fg-category')) $('fg-category').value = activeAsset.category;
  if ($('fg-size')) $('fg-size').value = `${activeAsset.width}x${activeAsset.height}`;
  populateCategorySuggestions(activeAsset.category);
  rebuildAsset();
}

function populateCategorySuggestions(catId) {
  const cat = ASSET_CATEGORIES[catId] || ASSET_CATEGORIES.chest;
  const chips = $('fg-chips');
  if (!chips) return;
  chips.innerHTML = '';
  for (const text of cat.suggestions) {
    const b = document.createElement('button');
    b.className = 'small';
    b.style.padding = '3px 7px';
    b.style.fontSize = '11px';
    b.textContent = text.length > 42 ? text.slice(0, 40) + '…' : text;
    b.title = text;
    b.onclick = () => {
      if ($('fg-prompt')) $('fg-prompt').value = text;
    };
    chips.append(b);
  }
}

// Live preview animation loop
function stepForgePreview() {
  animTick++;
  if (!activeAsset) return;
  const W = activeAsset.width;
  const H = activeAsset.height;
  const stageW = Math.max(32, W + 16);
  const stageH = Math.max(32, H + 16);
  if (prevCanvas.width !== stageW || prevCanvas.height !== stageH) {
    prevCanvas.width = stageW;
    prevCanvas.height = stageH;
  }
  prevCtx.fillStyle = darkBg ? '#181c19' : '#d8dfd0';
  prevCtx.fillRect(0, 0, stageW, stageH);
  prevCtx.fillStyle = darkBg ? '#252c27' : '#c5cebc';
  for (let y = 0; y < stageH; y += 8) {
    for (let x = 0; x < stageW; x += 8) {
      prevCtx.fillRect(x, y, 1, 1);
    }
  }

  const states = activeAsset.states;
  const shownIdx = animPlay && states.length > 1
    ? Math.floor(animTick / 26) % states.length
    : activeStateIdx;
  const st = states[shownIdx] || states[0];
  const ox = Math.floor((stageW - W) / 2);
  const oy = Math.floor((stageH - H) / 2);
  drawAssetGrid(prevCtx, st.grid, W, H, ox, oy, activeAsset.palette);
}

// API Key setup & live model auto-discovery
const apiKeyInput = $('fg-api-key');
let modelRefreshTimer = null;

async function refreshAvailableModels(silent = false) {
  const apiKey = apiKeyInput?.value?.trim() || '';
  const modelSel = $('fg-model');
  if (!apiKey || !modelSel) {
    if (!silent) toast('Paste your Gemini API Key first to query available models.');
    return;
  }
  try {
    const { models, bestFlash, bestPro } = await listGeminiModels(apiKey);
    if (!models.length) return;
    const prevVal = modelSel.value;
    modelSel.innerHTML = '';

    const optAuto = document.createElement('option');
    optAuto.value = 'auto';
    optAuto.textContent = `✨ Auto (${bestFlash})`;
    modelSel.append(optAuto);

    const optAutoPro = document.createElement('option');
    optAutoPro.value = 'auto-pro';
    optAutoPro.textContent = `🧠 Auto Pro (${bestPro})`;
    modelSel.append(optAutoPro);

    for (const m of models) {
      const o = document.createElement('option');
      o.value = m.id;
      o.textContent = m.id;
      modelSel.append(o);
    }

    if ([...modelSel.options].some((o) => o.value === prevVal)) {
      modelSel.value = prevVal;
    } else {
      modelSel.value = 'auto';
    }
    if (!silent) {
      toast(`Detected ${models.length} active Gemini models (best: ${bestFlash})`);
    }
  } catch (err) {
    if (!silent) toast(err.message);
  }
}

if (apiKeyInput) {
  apiKeyInput.value = localStorage.getItem(LS_KEY_API) || '';
  if (apiKeyInput.value.trim().length > 15) {
    refreshAvailableModels(true);
  }
  apiKeyInput.oninput = () => {
    const val = apiKeyInput.value.trim();
    try { localStorage.setItem(LS_KEY_API, val); } catch {}
    clearTimeout(modelRefreshTimer);
    if (val.length > 15) {
      modelRefreshTimer = setTimeout(() => refreshAvailableModels(true), 600);
    }
  };
}
if ($('fg-refresh-models')) {
  $('fg-refresh-models').onclick = () => refreshAvailableModels(false);
}
if ($('fg-key-vis') && apiKeyInput) {
  $('fg-key-vis').onclick = () => {
    apiKeyInput.type = apiKeyInput.type === 'password' ? 'text' : 'password';
  };
}

// Category & Size controls
if ($('fg-category')) {
  $('fg-category').onchange = (e) => {
    const catId = e.target.value;
    const cat = ASSET_CATEGORIES[catId] || ASSET_CATEGORIES.chest;
    if ($('fg-size')) $('fg-size').value = cat.defaultSize;
    populateCategorySuggestions(catId);
  };
}

// Gemini Generate & Refine buttons
async function runGeminiForge(refineExisting = false) {
  const apiKey = $('fg-api-key')?.value?.trim() || '';
  const model = $('fg-model')?.value || 'auto';
  const category = $('fg-category')?.value || 'chest';
  const { width, height } = parseSizeSpec($('fg-size')?.value || '16x16');
  const prompt = $('fg-prompt')?.value?.trim() || '';
  const genBtn = $('fg-gen-btn');
  const refBtn = $('fg-refine-btn');
  const statusEl = $('fg-status');

  if (!apiKey) {
    toast('Paste your Gemini API Key first (click 🔑 Get Gemini API Key if you need one)!');
    $('fg-api-key')?.focus();
    return;
  }

  try {
    if (genBtn) genBtn.disabled = true;
    if (refBtn) refBtn.disabled = true;
    const modelLabel = model === 'auto' ? 'best available Gemini Flash model' : model;
    if (statusEl) {
      statusEl.textContent = `⏳ Asking ${modelLabel} to synthesize ${width}×${height} 2bpp pixel matrices + GBC RGB555 palette...`;
    }
    const generated = await generateAssetWithGemini({
      apiKey,
      model,
      category,
      width,
      height,
      prompt,
      existingAsset: refineExisting ? activeAsset : null,
    });
    loadAssetIntoForge(generated);
    // Auto-save newly generated Gemini asset into custom library
    customLibrary = [
      generated,
      ...customLibrary.filter((x) => x.id !== generated.id),
    ];
    saveLibraryToStorage(customLibrary);
    renderLibrary();
    const usedTag = generated.usedModel ? ` via ${generated.usedModel}` : '';
    if (statusEl) {
      statusEl.textContent = `✓ Generated "${generated.name}" (${width}×${height}, ${generated.states.length} states, 15-bit GBC palette)${usedTag}!`;
    }
    toast(`✨ Gemini generated "${generated.name}" (${width}×${height})${usedTag}!`);
  } catch (err) {
    if (statusEl) statusEl.textContent = `⚠️ ${err.message}`;
    toast(err.message);
  } finally {
    if (genBtn) genBtn.disabled = false;
    if (refBtn) refBtn.disabled = false;
  }
}

if ($('fg-gen-btn')) $('fg-gen-btn').onclick = () => runGeminiForge(false);
if ($('fg-refine-btn')) $('fg-refine-btn').onclick = () => runGeminiForge(true);

// Palette presets & custom color pickers
const palPresetSel = $('fg-pal-preset');
if (palPresetSel) {
  const autoOpt = document.createElement('option');
  autoOpt.value = 'custom';
  autoOpt.textContent = '✨ Asset / Custom GBC Palette';
  palPresetSel.append(autoOpt);
  for (const [k, v] of Object.entries(GBC_PALETTES)) {
    const o = document.createElement('option');
    o.value = k;
    o.textContent = v.name;
    palPresetSel.append(o);
  }
  palPresetSel.onchange = () => {
    const p = GBC_PALETTES[palPresetSel.value];
    if (p && activeAsset) {
      activeAsset.palette = p.colors.map((c) => [...c]);
      rebuildAsset();
    }
  };
}
for (let i = 1; i <= 3; i++) {
  const colEl = $(`fg-col-${i}`);
  if (colEl) {
    colEl.oninput = () => {
      if (!activeAsset) return;
      const [r, g, b] = hexToRgb(colEl.value);
      activeAsset.palette[i] = snapColorToGbc([r, g, b, 255]);
      if (palPresetSel) palPresetSel.value = 'custom';
      rebuildAsset();
    };
  }
}

// State management controls
if ($('fg-state-name')) {
  $('fg-state-name').oninput = (e) => {
    if (!activeAsset || !activeAsset.states[activeStateIdx]) return;
    activeAsset.states[activeStateIdx].name = e.target.value.replace(/[^a-zA-Z0-9_]/g, '_') || 'state';
    rebuildAsset();
  };
}
if ($('fg-state-dup')) {
  $('fg-state-dup').onclick = () => {
    if (!activeAsset) return;
    const cur = activeAsset.states[activeStateIdx];
    const copyGrid = cloneAssetGrid(cur.grid);
    activeAsset.states.splice(activeStateIdx + 1, 0, {
      name: `${cur.name}_2`,
      grid: copyGrid,
      rows: gridToAssetRows(copyGrid, activeAsset.width, activeAsset.height),
    });
    activeStateIdx++;
    rebuildAsset();
    toast('Cloned active state!');
  };
}
if ($('fg-state-del')) {
  $('fg-state-del').onclick = () => {
    if (!activeAsset || activeAsset.states.length <= 1) {
      toast('Asset must have at least 1 state');
      return;
    }
    activeAsset.states.splice(activeStateIdx, 1);
    activeStateIdx = Math.max(0, activeStateIdx - 1);
    rebuildAsset();
    toast('Deleted state');
  };
}
if ($('fg-anim-play')) $('fg-anim-play').onchange = (e) => { animPlay = e.target.checked; };
if ($('fg-dark-bg')) $('fg-dark-bg').onchange = (e) => { darkBg = e.target.checked; renderEditor(); renderStateStrip(); renderLibrary(); };

// Pixel Editor tools
function selectShade(s) {
  brushShade = s;
  for (const b of $('fg-palette-bar').querySelectorAll('button')) {
    b.classList.toggle('on', +b.dataset.shade === brushShade);
  }
}
$('fg-palette-bar').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-shade]');
  if (b) selectShade(+b.dataset.shade);
});

function setEditTool(tool) {
  editTool = tool;
  $('fg-tool-pencil')?.classList.toggle('on', tool === 'pencil');
  $('fg-tool-bucket')?.classList.toggle('on', tool === 'bucket');
  $('fg-tool-picker')?.classList.toggle('on', tool === 'picker');
}
if ($('fg-tool-pencil')) $('fg-tool-pencil').onclick = () => setEditTool('pencil');
if ($('fg-tool-bucket')) $('fg-tool-bucket').onclick = () => setEditTool('bucket');
if ($('fg-tool-picker')) $('fg-tool-picker').onclick = () => setEditTool('picker');
if ($('fg-tool-mirror')) {
  $('fg-tool-mirror').onclick = () => {
    mirrorX = !mirrorX;
    $('fg-tool-mirror').classList.toggle('on', mirrorX);
    renderEditor();
  };
}
if ($('fg-tool-onion')) {
  $('fg-tool-onion').onclick = () => {
    onionSkin = !onionSkin;
    $('fg-tool-onion').classList.toggle('on', onionSkin);
    renderEditor();
  };
}
if ($('fg-edit-undo')) $('fg-edit-undo').onclick = () => popUndo();

let painting = false;
let activeStrokeShade = 3;
function paintAt(e, isStart = false) {
  if (!activeAsset) return;
  const W = activeAsset.width;
  const H = activeAsset.height;
  const st = activeAsset.states[activeStateIdx];
  const g = st.grid;
  const r = editCanvas.getBoundingClientRect();
  const x = Math.floor(((e.clientX - r.left) / r.width) * W);
  const y = Math.floor(((e.clientY - r.top) / r.height) * H);
  if (x < 0 || x >= W || y < 0 || y >= H) return;

  if (editTool === 'picker' || e.altKey) {
    selectShade(g[y * W + x] & 3);
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
    floodFillAssetGrid(g, W, H, x, y, activeStrokeShade);
    if (mirrorX) floodFillAssetGrid(g, W, H, W - 1 - x, y, activeStrokeShade);
    rebuildAsset();
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
  if (changed) rebuildAsset();
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

const nudgeActive = (dx, dy) => {
  if (!activeAsset) return;
  pushUndo();
  const st = activeAsset.states[activeStateIdx];
  st.grid = shiftAssetGrid(st.grid, activeAsset.width, activeAsset.height, dx, dy);
  rebuildAsset();
};
if ($('fg-nudge-left')) $('fg-nudge-left').onclick = () => nudgeActive(-1, 0);
if ($('fg-nudge-right')) $('fg-nudge-right').onclick = () => nudgeActive(1, 0);
if ($('fg-nudge-up')) $('fg-nudge-up').onclick = () => nudgeActive(0, -1);
if ($('fg-nudge-down')) $('fg-nudge-down').onclick = () => nudgeActive(0, 1);

if ($('fg-edit-flipx')) {
  $('fg-edit-flipx').onclick = () => {
    if (!activeAsset) return;
    pushUndo();
    const st = activeAsset.states[activeStateIdx];
    st.grid = flipAssetX(st.grid, activeAsset.width, activeAsset.height);
    rebuildAsset();
  };
}
if ($('fg-edit-outline')) {
  $('fg-edit-outline').onclick = () => {
    if (!activeAsset) return;
    pushUndo();
    const st = activeAsset.states[activeStateIdx];
    st.grid = outlineAssetGrid(st.grid, activeAsset.width, activeAsset.height);
    rebuildAsset();
    toast('Added Shade 3 dark silhouette outline!');
  };
}
if ($('fg-edit-clear')) {
  $('fg-edit-clear').onclick = () => {
    if (!activeAsset) return;
    pushUndo();
    activeAsset.states[activeStateIdx].grid.fill(0);
    rebuildAsset();
  };
}

// Save to Library, Export Sheet PNG, Export GBDK C, Copy/Load JSON, Send Weapon to Studio
if ($('fg-save-lib')) {
  $('fg-save-lib').onclick = () => {
    if (!activeAsset) return;
    const snap = JSON.parse(serializeActiveAsset());
    customLibrary = [snap, ...customLibrary.filter((x) => x.id !== snap.id)];
    saveLibraryToStorage(customLibrary);
    renderLibrary();
    toast(`Saved "${snap.name}" to your browser Asset Library!`);
  };
}

if ($('fg-dl-sheet')) {
  $('fg-dl-sheet').onclick = () => {
    if (!activeAsset) return;
    const W = activeAsset.width;
    const H = activeAsset.height;
    const count = activeAsset.states.length;
    const scale = 4;
    const c = document.createElement('canvas');
    c.width = W * count * scale;
    c.height = H * scale;
    const cx = c.getContext('2d');
    activeAsset.states.forEach((st, idx) => {
      for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
          const v = st.grid[y * W + x];
          if (!v) continue;
          const [r, g, b] = activeAsset.palette[v] || [0, 0, 0];
          cx.fillStyle = `rgb(${r},${g},${b})`;
          cx.fillRect((idx * W + x) * scale, y * scale, scale, scale);
        }
      }
    });
    download(`roguegb-${activeAsset.id}-${W}x${H}.png`, c.toDataURL('image/png'));
  };
}

if ($('fg-dl-c')) {
  $('fg-dl-c').onclick = () => {
    if (!activeCompiled || !activeAsset) return;
    const combined = `${activeCompiled.header}\n${activeCompiled.source}`;
    download(`${activeAsset.id}_generated.c`, new Blob([combined], { type: 'text/plain' }));
  };
}

if ($('fg-copy-c')) {
  $('fg-copy-c').onclick = async () => {
    if (!activeCompiled) return;
    try {
      await navigator.clipboard.writeText(`${activeCompiled.header}\n${activeCompiled.source}`);
      toast('Copied GBDK 2bpp tiles + CGB palette C code to clipboard!');
    } catch {
      toast('Clipboard unavailable — use ⤓ GBDK (.h + .c)');
    }
  };
}

if ($('fg-send-studio')) {
  $('fg-send-studio').onclick = () => {
    if (!activeAsset || activeAsset.category !== 'weapon') return;
    try {
      localStorage.setItem('roguegb_custom_weapon', serializeActiveAsset());
      location.href = 'studio.html?weapon=custom_forge';
    } catch {
      toast('Could not save weapon to localStorage');
    }
  };
}

if ($('fg-copy-json')) {
  $('fg-copy-json').onclick = async () => {
    const code = serializeActiveAsset();
    if ($('fg-code-input')) $('fg-code-input').value = code;
    try {
      await navigator.clipboard.writeText(code);
      toast('Copied Asset JSON to clipboard — paste it directly into chat!');
    } catch {
      $('fg-code-drawer')?.classList.remove('hidden');
      toast('Asset JSON ready in text box below!');
    }
  };
}
if ($('fg-toggle-code')) {
  $('fg-toggle-code').onclick = () => {
    const d = $('fg-code-drawer');
    if (!d) return;
    const opening = d.classList.contains('hidden');
    d.classList.toggle('hidden');
    if (opening && $('fg-code-input')) {
      $('fg-code-input').value = serializeActiveAsset();
      $('fg-code-input').focus();
      $('fg-code-input').select();
    }
  };
}
if ($('fg-close-code')) $('fg-close-code').onclick = () => $('fg-code-drawer')?.classList.add('hidden');
if ($('fg-apply-code')) {
  $('fg-apply-code').onclick = () => {
    try {
      const parsed = JSON.parse($('fg-code-input')?.value || '{}');
      loadAssetIntoForge(parsed);
      $('fg-code-drawer')?.classList.add('hidden');
      toast(`Loaded "${parsed.name || 'custom'}" into Forge!`);
    } catch (err) {
      toast(`JSON parse error: ${err.message}`);
    }
  };
}

loadAssetIntoForge(customLibrary[0] || STARTER_ASSETS[0]);
setInterval(stepForgePreview, 1000 / 60);
window.rgbForge = { loadAssetIntoForge, serializeActiveAsset };
