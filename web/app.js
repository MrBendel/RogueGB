// RogueGB playtest page. Runs the same Boytacean core and harness code as the
// headless tests, so recordings made here replay exactly in `harness/cli.mjs`.
import initCore, * as B from '../harness/vendor/boytacean/boytacean.js';
import {
  Emu, Symbols, Recorder, BTN, readTyped, prepareReplay, toBase64, fromBase64, SCREEN_W, SCREEN_H,
} from '../harness/emu.mjs';
import {
  quantizeRgbaToGrid16, flipGridX, synthesizeUpFromDown, synthesizeRightFromDown,
  generateCharacterRig, compileRigToGbdk, CHARACTER_PRESETS, WEAPON_TEMPLATES,
  buildWeaponDirectionalFrames, defaultWeaponAnchor, DMG_PALETTES, cloneGrid,
} from '../harness/sprite-gen.mjs';

const FRAME_MS = 1000 / 59.7275;
const $ = (id) => document.getElementById(id);
const store = {
  get(k, d) { try { const v = localStorage.getItem('rgb.' + k); return v ? JSON.parse(v) : d; } catch { return d; } },
  set(k, v) { try { localStorage.setItem('rgb.' + k, JSON.stringify(v)); } catch { /* storage unavailable */ } },
};

// ---------------------------------------------------------------------------
// Backend: the local dev server (tools/serve.mjs) by default. The hosted
// playtest bundle swaps in its own backend via window.RGB_BACKEND.
const localBackend = {
  name: 'local',
  async loadRom() {
    const r = await fetch('/build/roguegb.gb', { cache: 'no-store' });
    if (!r.ok) throw new Error('No ROM yet. Run "node tools/build.mjs" (or fix the build errors).');
    const rom = new Uint8Array(await r.arrayBuffer());
    let sym = '';
    const s = await fetch('/build/roguegb.noi', { cache: 'no-store' });
    if (s.ok) sym = await s.text();
    return { rom, sym, label: 'build/roguegb.gb' };
  },
  subscribe(onEvent) {
    const es = new EventSource('/events');
    es.onmessage = (e) => onEvent(JSON.parse(e.data));
    es.onerror = () => setPill('dev server disconnected', 'err');
  },
  async listReplays() {
    const r = await fetch('/api/replays');
    return r.ok ? r.json() : [];
  },
  async getReplay(id) {
    const r = await fetch('/api/replays/' + encodeURIComponent(id));
    return r.json();
  },
  async saveReplay(replay, kind) {
    const r = await fetch('/api/replays', {
      method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ kind, replay }),
    });
    if (!r.ok) throw new Error(await r.text());
    return (await r.json()).id;
  },
};
const backend = window.RGB_BACKEND || localBackend;

// ---------------------------------------------------------------------------
// State
let emu = null;
let symbols = new Symbols('');
let paused = false;
let speed = 1;
let fastForward = false;
let rewinding = false;
let acc = 0;
let last = performance.now();
let recorder = null;
let player = null; // { input, length, name }
let quickState = null;
let fpsCount = 0;
let fpsT = performance.now();

const rewindBuf = []; // { frame, state }
const REWIND_MAX = 600; // states, one every 2 frames => ~20 s
const checkpoints = []; // { frame, state } every 600 frames, for bug reports
let history = []; // [absFrame, mask] input changes since oldest checkpoint
let lastMask = -1;

const canvas = $('screen');
const ctx2d = canvas.getContext('2d');
const img = ctx2d.createImageData(SCREEN_W, SCREEN_H);

// ---------------------------------------------------------------------------
// Input
const keyMap = {
  ArrowUp: 'UP', ArrowDown: 'DOWN', ArrowLeft: 'LEFT', ArrowRight: 'RIGHT',
  KeyW: 'UP', KeyS: 'DOWN', KeyA: 'LEFT', KeyD: 'RIGHT',
  KeyX: 'A', KeyK: 'A', KeyZ: 'B', KeyJ: 'B',
  Enter: 'START', ShiftLeft: 'SELECT', ShiftRight: 'SELECT',
};
const held = { key: 0, touch: 0, pad: 0 };

addEventListener('keydown', (e) => {
  if (e.target.matches('input, textarea, select')) return;
  const b = keyMap[e.code];
  if (b) { held.key |= BTN[b]; e.preventDefault(); return; }
  if (e.repeat && e.code !== 'KeyN') return;
  switch (e.code) {
    case 'KeyP': togglePause(); break;
    case 'KeyN': stepOnce(); break;
    case 'KeyR': rewinding = true; break;
    case 'Tab': fastForward = true; e.preventDefault(); break;
    case 'KeyM': toggleAudio(); break;
    case 'F2': e.preventDefault(); resetEmu(); break;
    case 'F5': e.preventDefault(); quickSave(); break;
    case 'F9': e.preventDefault(); quickLoad(); break;
    case 'F12': e.preventDefault(); screenshot(); break;
  }
});
addEventListener('keyup', (e) => {
  const b = keyMap[e.code];
  if (b) held.key &= ~BTN[b];
  if (e.code === 'KeyR') rewinding = false;
  if (e.code === 'Tab') fastForward = false;
});
addEventListener('blur', () => {
  held.key = 0;
  held.touch = 0;
  activePointers.clear();
  syncTouchVisuals();
  rewinding = false;
  fastForward = false;
});

const hapticLabel = $('haptic-label');
function haptic(ms = 12) {
  try {
    if (navigator.vibrate) navigator.vibrate(ms);
    else hapticLabel?.click();
  } catch { /* ignore */ }
}

const touchpad = $('touchpad');
const dpadEl = $('dpad');
const touchBtns = [...document.querySelectorAll('#touchpad [data-btn]')];
const actionBtns = touchBtns.filter((el) => !dpadEl.contains(el));
const activePointers = new Map(); // pointerId -> { zone: 'dpad' | 'buttons', mask: number }

function computePointerMask(zone, clientX, clientY) {
  let mask = 0;
  if (zone === 'dpad') {
    const r = dpadEl.getBoundingClientRect();
    const dx = clientX - (r.left + r.width / 2);
    const dy = clientY - (r.top + r.height / 2);
    const dist = Math.hypot(dx, dy);
    if (dist > 10 && dist < Math.max(r.width, r.height) * 0.95) {
      const ang = Math.atan2(dy, dx);
      const c = Math.cos(ang);
      const s = Math.sin(ang);
      if (c > 0.3827) mask |= BTN.RIGHT;
      if (c < -0.3827) mask |= BTN.LEFT;
      if (s > 0.3827) mask |= BTN.DOWN;
      if (s < -0.3827) mask |= BTN.UP;
    }
  } else {
    for (const el of actionBtns) {
      const r = el.getBoundingClientRect();
      const pad = 8;
      if (clientX >= r.left - pad && clientX <= r.right + pad && clientY >= r.top - pad && clientY <= r.bottom + pad) {
        mask |= BTN[el.dataset.btn];
      }
    }
  }
  return mask;
}

function syncTouchVisuals() {
  for (const el of touchBtns) {
    el.classList.toggle('held', !!(held.touch & BTN[el.dataset.btn]));
  }
}

function updateTouchState() {
  const prev = held.touch;
  let next = 0;
  for (const p of activePointers.values()) next |= p.mask;
  held.touch = next;
  if (next !== prev) {
    syncTouchVisuals();
    const pressedNew = (next & ~prev) !== 0;
    haptic(pressedNew ? 14 : 5);
  }
}

touchpad.addEventListener('pointerdown', (e) => {
  e.preventDefault();
  const zone = dpadEl.contains(e.target) ? 'dpad' : 'buttons';
  const mask = computePointerMask(zone, e.clientX, e.clientY);
  activePointers.set(e.pointerId, { zone, mask });
  touchpad.setPointerCapture?.(e.pointerId);
  updateTouchState();
});

touchpad.addEventListener('pointermove', (e) => {
  const p = activePointers.get(e.pointerId);
  if (!p) return;
  e.preventDefault();
  p.mask = computePointerMask(p.zone, e.clientX, e.clientY);
  updateTouchState();
});

function endPointer(e) {
  if (!activePointers.has(e.pointerId)) return;
  e.preventDefault();
  activePointers.delete(e.pointerId);
  updateTouchState();
}
touchpad.addEventListener('pointerup', endPointer);
touchpad.addEventListener('pointercancel', endPointer);
touchpad.addEventListener('lostpointercapture', endPointer);

function pollGamepad() {
  held.pad = 0;
  for (const gp of navigator.getGamepads?.() || []) {
    if (!gp) continue;
    const p = (i) => gp.buttons[i]?.pressed;
    const ax = gp.axes[0] || 0;
    const ay = gp.axes[1] || 0;
    if (p(12) || ay < -0.5) held.pad |= BTN.UP;
    if (p(13) || ay > 0.5) held.pad |= BTN.DOWN;
    if (p(14) || ax < -0.5) held.pad |= BTN.LEFT;
    if (p(15) || ax > 0.5) held.pad |= BTN.RIGHT;
    // Positional, Nintendo-style: right face button = A, bottom = B.
    if (p(1) || p(3)) held.pad |= BTN.A;
    if (p(0) || p(2)) held.pad |= BTN.B;
    if (p(9)) held.pad |= BTN.START;
    if (p(8)) held.pad |= BTN.SELECT;
  }
}
const liveMask = () => held.key | held.touch | held.pad;

// ---------------------------------------------------------------------------
// Audio
let audioCtx = null;
let audioOn = false;
let audioNext = 0;
function toggleAudio() {
  audioOn = !audioOn;
  if (audioOn && !audioCtx) audioCtx = new AudioContext();
  if (audioCtx) audioOn ? audioCtx.resume() : audioCtx.suspend();
  $('b-audio').textContent = audioOn ? '🔊 Sound' : '🔇 Sound';
  $('b-audio').classList.toggle('on', audioOn);
}
function pumpAudio() {
  const { samples, rate, channels } = emu.audio();
  if (!audioOn || !audioCtx || speed !== 1 || fastForward || !samples.length) return;
  const frames = Math.floor(samples.length / channels);
  const buf = audioCtx.createBuffer(2, frames, rate);
  const L = buf.getChannelData(0);
  const R = buf.getChannelData(1);
  for (let i = 0; i < frames; i++) {
    L[i] = samples[i * channels] / 32768;
    R[i] = samples[i * channels + (channels > 1 ? 1 : 0)] / 32768;
  }
  const src = audioCtx.createBufferSource();
  src.buffer = buf;
  src.connect(audioCtx.destination);
  const now = audioCtx.currentTime;
  if (audioNext < now || audioNext > now + 0.25) audioNext = now + 0.05;
  src.start(audioNext);
  audioNext += buf.duration;
}

// ---------------------------------------------------------------------------
// Emulation
async function loadFromBackend() {
  try {
    const { rom, sym, label } = await backend.loadRom();
    start(rom, sym, label);
  } catch (e) {
    showOverlay(e.message);
    setPill('no ROM', 'err');
  }
}

function start(rom, symText = '', label = 'ROM') {
  emu?.gb?.free();
  emu = new Emu(B, rom, { mode: 'auto' });
  symbols = new Symbols(symText);
  emu.symbols = symbols;
  resetBuffers();
  recorder = null;
  player = null;
  updateRecUi();
  hideOverlay();
  fillSymbols();
  setPill(`${emu.info.title || label} · ${emu.mode.toUpperCase()} · ${emu.info.crc32}`, 'ok');
  document.title = `${emu.info.title || 'RogueGB'} · playtest`;
}

function resetBuffers() {
  rewindBuf.length = 0;
  checkpoints.length = 0;
  history = [];
  lastMask = -1;
}

function resetEmu() {
  if (!emu) return;
  emu.reset();
  resetBuffers();
  player = null;
  if (recorder) { recorder = null; toast('Recording discarded (reset)'); }
  updateRecUi();
}

function tick() {
  let mask;
  if (player) {
    const f = emu.frame - player.startFrame;
    if (f >= player.length) {
      toast(`Replay "${player.name}" finished at frame ${f}`);
      player = null;
      updateRecUi();
      paused = true;
      updatePauseUi();
      return;
    }
    mask = player.input(f);
  } else mask = liveMask();

  if (emu.frame % 600 === 0) {
    checkpoints.push({ frame: emu.frame, state: emu.saveState() });
    if (checkpoints.length > 3) {
      checkpoints.shift();
      const from = checkpoints[0].frame;
      history = history.filter(([f]) => f >= from);
    }
  }
  if (mask !== lastMask) { history.push([emu.frame, mask]); lastMask = mask; }
  recorder?.sample(mask);
  emu.setButtons(mask);
  emu.step();
  if (emu.frame % 2 === 0) {
    rewindBuf.push({ frame: emu.frame, state: emu.saveState() });
    if (rewindBuf.length > REWIND_MAX) rewindBuf.shift();
  }
  pumpAudio();
}

function rewindStep() {
  const s = rewindBuf.pop();
  if (!s) return;
  emu.loadState(s.state);
  emu.frame = s.frame;
  // Anything recorded after this point no longer happened.
  history = history.filter(([f]) => f < s.frame);
  while (checkpoints.length && checkpoints.at(-1).frame > s.frame) checkpoints.pop();
  lastMask = -1;
  if (recorder) { recorder = null; updateRecUi(); toast('Recording stopped (rewind)'); }
  player = null;
}

function render() {
  const fb = emu.frameBuffer();
  const d = img.data;
  for (let i = 0, j = 0; i < fb.length; i += 3, j += 4) {
    d[j] = fb[i]; d[j + 1] = fb[i + 1]; d[j + 2] = fb[i + 2]; d[j + 3] = 255;
  }
  ctx2d.putImageData(img, 0, 0);
}

function loop(now) {
  requestAnimationFrame(loop);
  pollGamepad();
  const dt = Math.min(now - last, 100);
  last = now;
  if (!emu) return;
  if (rewinding) {
    rewindStep();
    render();
  } else if (!paused) {
    acc += dt * speed * (fastForward ? 4 : 1);
    let n = 0;
    while (acc >= FRAME_MS && n < 16) { tick(); acc -= FRAME_MS; n++; fpsCount++; }
    if (n === 16) acc = 0;
    if (n) render();
  }
  if (now - fpsT >= 500) {
    $('st-fps').textContent = `${Math.round((fpsCount * 1000) / (now - fpsT))} fps`;
    fpsCount = 0;
    fpsT = now;
    $('st-frame').textContent = `frame ${emu.frame}`;
    $('st-speed').textContent = rewinding ? '◀◀ rewind' : fastForward ? '4x ff' : `${speed}x`;
  }
  if (emu.frame % 6 === 0 || paused) updateWatches();
}

function togglePause() { paused = !paused; updatePauseUi(); }
function updatePauseUi() {
  $('b-pause').textContent = paused ? '▶ Play' : '⏸ Pause';
  $('b-pause').classList.toggle('on', paused);
}
function stepOnce() {
  if (!emu) return;
  paused = true;
  updatePauseUi();
  tick();
  render();
  $('st-frame').textContent = `frame ${emu.frame}`;
  updateWatches();
}

function quickSave() {
  if (!emu) return;
  quickState = { frame: emu.frame, state: emu.saveState() };
  toast(`State saved at frame ${emu.frame}`);
}
function quickLoad() {
  if (!emu || !quickState) return toast('No saved state');
  emu.loadState(quickState.state);
  emu.frame = quickState.frame;
  rewindBuf.length = 0;
  history = history.filter(([f]) => f < quickState.frame);
  while (checkpoints.length && checkpoints.at(-1).frame > quickState.frame) checkpoints.pop();
  lastMask = -1;
  render();
  toast(`State loaded (frame ${emu.frame})`);
}

function screenshotDataUrl(scale = 4) {
  const c = document.createElement('canvas');
  c.width = SCREEN_W * scale;
  c.height = SCREEN_H * scale;
  const x = c.getContext('2d');
  x.imageSmoothingEnabled = false;
  x.drawImage(canvas, 0, 0, c.width, c.height);
  return c.toDataURL('image/png');
}
function screenshot() {
  if (!emu) return;
  download(`roguegb-f${emu.frame}.png`, screenshotDataUrl());
}
// data: a Blob or a data: URL string
async function download(name, data) {
  if (backend.download) {
    // Hosted page: plain <a download> links are blocked, the viewer saves instead.
    const blob = typeof data === 'string' ? await (await fetch(data)).blob() : data;
    try { await backend.download(name, blob); } catch (e) { if (e?.code !== 'declined') toast(`Couldn't save: ${e.message || e.code}`); }
    return;
  }
  const a = document.createElement('a');
  a.href = typeof data === 'string' ? data : URL.createObjectURL(data);
  a.download = name;
  a.click();
}

// ---------------------------------------------------------------------------
// Recording, replays and bug reports
const sessionReplays = [];

function toggleRecord() {
  if (!emu) return;
  if (!recorder) {
    player = null;
    recorder = new Recorder(emu, { fromState: true });
    toast('Recording… press again to stop');
  } else {
    const replay = recorder.finish({ note: $('note').value.trim(), kind: 'recording' });
    recorder = null;
    keepReplay(replay, 'recording');
  }
  updateRecUi();
}

function bugReport() {
  if (!emu) return;
  const cp = checkpoints[0];
  const startFrame = cp ? cp.frame : 0;
  const events = history.filter(([f]) => f >= startFrame).map(([f, m]) => [f - startFrame, m]);
  if (!events.length || events[0][0] !== 0) events.unshift([0, 0]);
  const replay = {
    version: 1,
    kind: 'bug',
    rom: { title: emu.info.title, crc32: emu.info.crc32 },
    mode: emu.mode,
    length: emu.frame - startFrame,
    start_state: cp ? toBase64(cp.state) : null,
    events,
    note: $('note').value.trim() || '(no description)',
    created: new Date().toISOString(),
    end_state: toBase64(emu.saveState()),
    screenshot: screenshotDataUrl(2),
    watches: Object.fromEntries(watchList.map((w) => [w.name, safeRead(w)])),
  };
  keepReplay(replay, 'bug');
  $('note').value = '';
}

async function keepReplay(replay, kind) {
  const name = `${kind} · ${replay.length}f${replay.note ? ' · ' + replay.note : ''}`;
  const entry = { name, replay, id: null };
  sessionReplays.unshift(entry);
  renderReplays();
  try {
    entry.id = await backend.saveReplay(replay, kind);
    toast(kind === 'bug' ? `Bug report saved (${entry.id})` : `Recording saved (${entry.id})`);
  } catch (e) {
    toast(`Kept in this tab only: ${e.message}`);
  }
  renderReplays();
  refreshSavedReplays();
}

function playReplay(replay, name) {
  if (!emu) return;
  recorder = null;
  const input = prepareReplay(emu, replay);
  resetBuffers();
  player = { input, length: replay.length, name, startFrame: emu.frame };
  paused = false;
  updatePauseUi();
  updateRecUi();
  toast(`Playing "${name}" (${replay.length} frames)`);
}

let savedReplays = [];
async function refreshSavedReplays() {
  try { savedReplays = await backend.listReplays(); } catch { savedReplays = []; }
  renderReplays();
}

function renderReplays() {
  const ul = $('replays');
  ul.innerHTML = '';
  const sessionIds = new Set(sessionReplays.map((e) => e.id).filter(Boolean));
  const rows = [
    ...sessionReplays.map((e) => ({ name: e.name, get: async () => e.replay, saved: !!e.id })),
    ...savedReplays.filter((r) => !sessionIds.has(r.id)).map((r) => ({
      name: r.name || r.id, get: () => backend.getReplay(r.id), saved: true,
    })),
  ].slice(0, 40);
  for (const r of rows) {
    const li = document.createElement('li');
    const n = document.createElement('span');
    n.className = 'name';
    n.textContent = (r.saved ? '' : '◌ ') + r.name;
    n.title = r.name;
    const play = document.createElement('button');
    play.textContent = '▶';
    play.title = 'Play';
    play.onclick = async () => playReplay(await r.get(), r.name);
    const dl = document.createElement('button');
    dl.textContent = '⤓';
    dl.title = 'Download JSON';
    dl.onclick = async () => {
      const blob = new Blob([JSON.stringify(await r.get())], { type: 'application/json' });
      download(r.name.replace(/[^\w.-]+/g, '_').slice(0, 60) + '.json', blob);
    };
    li.append(n, play, dl);
    ul.append(li);
  }
}

function updateRecUi() {
  $('b-rec').textContent = recorder ? '■ Stop' : '● Record';
  $('b-rec').classList.toggle('recording', !!recorder);
  $('st-rec').classList.toggle('hidden', !recorder);
  $('st-play').classList.toggle('hidden', !player);
}

// ---------------------------------------------------------------------------
// Watches
let watchList = store.get('watches', [
  // Movement lab: positions and velocities are 8.8 fixed point (divide by 256 for pixels).
  { name: 'player_vx', type: 'i16' }, { name: 'player_vy', type: 'i16' },
  { name: 'player_x', type: 'u16' }, { name: 'player_y', type: 'u16' }, { name: 'move_preset', type: 'u8' },
]);
function fillSymbols() {
  const dl = $('symbols');
  dl.innerHTML = '';
  for (const s of symbols.variables()) {
    const o = document.createElement('option');
    o.value = s.name.slice(1);
    o.label = '0x' + s.addr.toString(16).toUpperCase();
    dl.append(o);
  }
  renderWatches();
}
function safeRead(w) {
  try { return readTyped(emu, symbols.addr(w.name), w.type); } catch { return '?'; }
}
function renderWatches() {
  const tb = $('watches').tBodies[0];
  tb.innerHTML = '';
  watchList.forEach((w, i) => {
    const tr = document.createElement('tr');
    const s = symbols.get(w.name);
    tr.innerHTML = `<td></td><td class="muted"></td><td class="val">–</td><td class="x"><button title="Remove">✕</button></td>`;
    tr.cells[0].textContent = w.name;
    tr.cells[1].textContent = s ? `${w.type} @${s.addr.toString(16).toUpperCase()}` : `${w.type} (missing)`;
    tr.querySelector('button').onclick = () => { watchList.splice(i, 1); store.set('watches', watchList); renderWatches(); };
    tb.append(tr);
  });
  updateWatches();
}
function updateWatches() {
  if (!emu) return;
  const rows = $('watches').tBodies[0].rows;
  watchList.forEach((w, i) => { if (rows[i]) rows[i].cells[2].textContent = String(safeRead(w)); });
}
function addWatch() {
  const name = $('watch-name').value.trim().replace(/^_/, '');
  if (!name) return;
  watchList.push({ name, type: $('watch-type').value });
  store.set('watches', watchList);
  $('watch-name').value = '';
  renderWatches();
}

// ---------------------------------------------------------------------------
// UI plumbing
function setPill(text, cls = '') {
  const p = $('build-status');
  p.textContent = text;
  p.className = 'pill ' + cls;
}
function showOverlay(html, isLog = false) {
  const o = $('overlay');
  o.classList.remove('hidden');
  o.innerHTML = '';
  if (isLog) { const pre = document.createElement('pre'); pre.textContent = html; o.append(pre); }
  else o.textContent = html;
}
function hideOverlay() { $('overlay').classList.add('hidden'); }
let toastTimer = 0;
function toast(msg) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.add('hidden'), 2600);
}

$('b-pause').onclick = () => { haptic(10); togglePause(); };
$('b-step').onclick = () => { haptic(10); stepOnce(); };
$('b-reset').onclick = () => { haptic(10); resetEmu(); };
$('sel-speed').onchange = (e) => { speed = +e.target.value; };
$('b-audio').onclick = () => { haptic(10); toggleAudio(); };
$('b-shot').onclick = () => { haptic(10); screenshot(); };
$('b-rec').onclick = () => { haptic(12); toggleRecord(); };
$('b-report').onclick = () => { haptic(12); bugReport(); };
$('b-save').onclick = () => { haptic(10); quickSave(); };
$('b-load').onclick = () => { haptic(10); quickLoad(); };
$('b-watch').onclick = addWatch;
$('watch-name').addEventListener('keydown', (e) => { if (e.key === 'Enter') addWatch(); });
if (backend.name !== 'local') $('chk-restart').closest('.row').classList.add('hidden');
$('chk-restart').checked = store.get('restartOnBuild', true);
$('chk-restart').onchange = (e) => store.set('restartOnBuild', e.target.checked);

const mobileMq = matchMedia('(max-width: 820px)');
function syncMobilePanels(e) {
  for (const d of document.querySelectorAll('.panel details:not(#build-log-box)')) {
    if (e.matches) d.removeAttribute('open');
    else if (d.querySelector('summary')?.textContent !== 'Controls') d.setAttribute('open', '');
  }
}
syncMobilePanels(mobileMq);
mobileMq.addEventListener?.('change', syncMobilePanels);

$('file-replay').onchange = async (e) => {
  const f = e.target.files[0];
  if (f) playReplay(JSON.parse(await f.text()), f.name);
  e.target.value = '';
};
$('file-rom').onchange = async (e) => {
  const f = e.target.files[0];
  if (f) start(new Uint8Array(await f.arrayBuffer()), '', f.name);
  e.target.value = '';
};

function onBackendEvent(ev) {
  if (ev.type === 'building') setPill('building…');
  if (ev.type === 'build') {
    const box = $('build-log-box');
    if (!ev.ok) {
      setPill('build failed', 'err');
      box.classList.remove('hidden');
      $('build-log').textContent = ev.log || '';
      showOverlay(ev.log || 'build failed', true);
      return;
    }
    box.classList.toggle('hidden', !/warning/i.test(ev.log || ''));
    $('build-log').textContent = ev.log || '';
    if (!emu || ev.crc32 !== emu.info.crc32) {
      if (!emu || $('chk-restart').checked) {
        loadFromBackend().then(() => toast(`Rebuilt in ${ev.ms} ms, restarted`));
      } else {
        setPill(`new build ready (${ev.crc32}): press Reset to load`, 'ok');
        $('b-reset').onclick = () => { $('b-reset').onclick = resetEmu; loadFromBackend(); };
      }
    }
  }
}

// ---------------------------------------------------------------------------
// PWA install prompt & service worker
let deferredInstallPrompt = null;
const installBtn = $('b-install');
addEventListener('beforeinstallprompt', (e) => {
  e.preventDefault();
  deferredInstallPrompt = e;
  installBtn?.classList.remove('hidden');
});
addEventListener('appinstalled', () => {
  deferredInstallPrompt = null;
  installBtn?.classList.add('hidden');
  toast('RogueGB installed!');
});
if (installBtn) {
  installBtn.onclick = async () => {
    if (!deferredInstallPrompt) return;
    haptic(10);
    deferredInstallPrompt.prompt();
    const { outcome } = await deferredInstallPrompt.userChoice;
    if (outcome === 'accepted') installBtn.classList.add('hidden');
    deferredInstallPrompt = null;
  };
}
if (!window.claude && 'serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1')) {
  navigator.serviceWorker.register('./sw.js').catch(() => {});
}

// ---------------------------------------------------------------------------
// Sprite Studio (Procedural 16x16 Character & Weapon Animation Synthesizer)
const studio = (() => {
  const pal = DMG_PALETTES.dmg_olive;
  const allPresets = { ...(window.RGB_ARTWORK_PRESETS || {}), ...CHARACTER_PRESETS };
  const weapons = structuredClone ? structuredClone(WEAPON_TEMPLATES) : JSON.parse(JSON.stringify(WEAPON_TEMPLATES));
  for (const w of Object.values(weapons)) {
    w.right = new Uint8Array(Object.values(w.right));
    if (w.swing_right) w.swing_right = new Uint8Array(Object.values(w.swing_right));
    if (w.fire_right) w.fire_right = new Uint8Array(Object.values(w.fire_right));
  }

  let activeTab = 'playtest';
  let synthMode = '3pose';
  let animKind = 'idle';
  let dir = 'down';
  let weaponId = 'small_pistol';
  let attackPose = false;
  let editTarget = 'down';
  let brushShade = 3;
  let previewMode = 'anim'; // 'anim' | 'drive'
  let selectedFrame = 0;
  let animTick = 0;
  let customAnchor = { dx: 0, dy: 0, auto: true };

  // Base 16x16 grids currently loaded in the studio
  let baseDown = cloneGrid(CHARACTER_PRESETS.adventurer.down);
  let baseUp = cloneGrid(CHARACTER_PRESETS.adventurer.up);
  let baseRight = cloneGrid(CHARACTER_PRESETS.adventurer.right);
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

  function drawGrid16(ctx, grid, ox = 0, oy = 0) {
    for (let y = 0; y < 16; y++) {
      for (let x = 0; x < 16; x++) {
        const v = grid[y * 16 + x];
        if (!v) continue;
        const [r, g, b] = pal[v];
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(ox + x, oy + y, 1, 1);
      }
    }
  }

  function getWeaponFrame(d, isAttack) {
    if (weaponId === 'none' || !weapons[weaponId]) return null;
    const wf = buildWeaponDirectionalFrames(weapons[weaponId]);
    return wf[d]?.[isAttack ? 1 : 0] || null;
  }

  function getAnchor(d, aKind, fIdx, isAttack) {
    const cat = weapons[weaponId]?.category || 'small_gun';
    const def = defaultWeaponAnchor(d, aKind, fIdx, cat, isAttack);
    if (customAnchor.auto) return def;
    return { dx: customAnchor.dx, dy: customAnchor.dy, behind: def.behind };
  }

  function rebuildRig() {
    currentRig = generateCharacterRig({
      name: $('st-preset')?.value || 'custom',
      down: baseDown,
      up: synthMode === '3pose' ? baseUp : null,
      right: synthMode === '3pose' ? baseRight : null,
    });
    if (synthMode === '1pose') {
      baseUp = cloneGrid(currentRig.bases.up);
      baseRight = cloneGrid(currentRig.bases.right);
    }
    currentCompiled = compileRigToGbdk(currentRig, 'spr_hero');
    const st = currentCompiled.stats;
    $('st-hw-stats').textContent = `${st.anims} anims · ${st.frames}f · ${st.columns} unique 8×16 cols · ${st.romBytes} B ROM`;
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
    rebuildRig();
  }

  function getActiveEditGrid() {
    if (editTarget === 'up') return baseUp;
    if (editTarget === 'right') return baseRight;
    if (editTarget === 'weapon') {
      const w = weapons[weaponId];
      if (!w) return baseDown;
      return attackPose ? (w.swing_right || w.fire_right || w.right) : w.right;
    }
    return baseDown;
  }

  function renderEditor() {
    const g = getActiveEditGrid();
    editCtx.clearRect(0, 0, 16, 16);
    for (let y = 0; y < 16; y++) {
      for (let x = 0; x < 16; x++) {
        const v = g[y * 16 + x];
        if (v === 0) {
          editCtx.fillStyle = (x + y) % 2 === 0 ? '#cdd6b0' : '#bfc9a2';
        } else {
          const [r, gr, b] = pal[v];
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
    const wGrid = animKind === 'roll' ? null : getWeaponFrame(dir, attackPose);

    frames.forEach((fGrid, idx) => {
      const card = document.createElement('div');
      card.className = 'fcard' + (idx === selectedFrame ? ' on' : '');
      const c = document.createElement('canvas');
      c.width = 24;
      c.height = 24;
      const cx = c.getContext('2d');
      cx.fillStyle = '#c4cfa1';
      cx.fillRect(0, 0, 24, 24);
      const anc = getAnchor(dir, animKind, idx, attackPose);
      if (customAnchor.auto && idx === selectedFrame) {
        $('st-anchor-dx').value = anc.dx;
        $('st-anchor-dy').value = anc.dy;
      }
      if (anc.behind && wGrid) drawGrid16(cx, wGrid, 4 + anc.dx, 4 + anc.dy);
      drawGrid16(cx, fGrid, 4, 4);
      if (!anc.behind && wGrid) drawGrid16(cx, wGrid, 4 + anc.dx, 4 + anc.dy);

      const lbl = document.createElement('span');
      lbl.textContent = `f${idx}`;
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
    sheetCtx.fillStyle = '#c4cfa1';
    sheetCtx.fillRect(0, 0, sheetCanvas.width, sheetCanvas.height);
    entries.forEach(([, frames], r) => {
      frames.forEach((g, c) => {
        drawGrid16(sheetCtx, g, c * 16, r * 16);
      });
    });
  }

  function stepStudio() {
    if (activeTab !== 'studio') return;
    animTick++;
    prevCtx.fillStyle = '#c4cfa1';
    prevCtx.fillRect(0, 0, 64, 64);
    // Subtle DMG tile grid background
    prevCtx.fillStyle = '#b6c292';
    for (let y = 0; y < 64; y += 8) {
      for (let x = 0; x < 64; x += 8) {
        prevCtx.fillRect(x, y, 1, 1);
      }
    }

    if (previewMode === 'anim') {
      const key = `${animKind}_${dir}`;
      const frames = currentRig.anims[key] || currentRig.anims.idle_down;
      const period = animKind === 'idle' ? 24 : animKind === 'run' ? 8 : animKind === 'roll' ? 5 : 14;
      if (animTick % period === 0) {
        selectedFrame = (selectedFrame + 1) % frames.length;
        renderFrameStrip();
      }
      const fGrid = frames[selectedFrame] || frames[0];
      const wGrid = animKind === 'roll' ? null : getWeaponFrame(dir, attackPose);
      const anc = getAnchor(dir, animKind, selectedFrame, attackPose);
      const jumpY = animKind === 'jump' ? (selectedFrame === 1 ? -5 : selectedFrame === 2 ? -2 : 0) : 0;
      // Shadow
      prevCtx.fillStyle = '#8b956d';
      prevCtx.fillRect(26, 38, 12, 2);
      if (anc.behind && wGrid) drawGrid16(prevCtx, wGrid, 24 + anc.dx, 24 + jumpY + anc.dy);
      drawGrid16(prevCtx, fGrid, 24, 24 + jumpY);
      if (!anc.behind && wGrid) drawGrid16(prevCtx, wGrid, 24 + anc.dx, 24 + jumpY + anc.dy);
    } else {
      // Interactive Test Drive mode
      const m = liveMask();
      let dx = 0, dy = 0;
      if (m & BTN.LEFT) { dx = -1; drive.dir = 'left'; }
      if (m & BTN.RIGHT) { dx = 1; drive.dir = 'right'; }
      if (m & BTN.UP) { dy = -1; drive.dir = 'up'; }
      if (m & BTN.DOWN) { dy = 1; drive.dir = 'down'; }
      if ((m & BTN.B) && drive.z === 0) drive.vz = 2.2;
      if ((m & BTN.SELECT) && drive.rollT === 0) drive.rollT = 16;
      if ((m & BTN.A) && drive.atkT === 0) {
        drive.atkT = 10;
        if (weapons[weaponId]?.category?.includes('gun')) {
          const vx = drive.dir === 'right' ? 2.5 : drive.dir === 'left' ? -2.5 : 0;
          const vy = drive.dir === 'down' ? 2.5 : drive.dir === 'up' ? -2.5 : 0;
          drive.bullets.push({ x: drive.x + 8, y: drive.y + 8, vx, vy });
        }
      }
      const speedMul = drive.rollT > 0 ? 1.6 : 1;
      drive.x = Math.max(2, Math.min(46, drive.x + dx * speedMul));
      drive.y = Math.max(2, Math.min(46, drive.y + dy * speedMul));
      if (drive.z > 0 || drive.vz !== 0) {
        drive.z = Math.max(0, drive.z + drive.vz);
        drive.vz -= 0.18;
        if (drive.z <= 0) { drive.z = 0; drive.vz = 0; }
      }
      if (drive.rollT > 0) drive.rollT--;
      if (drive.atkT > 0) drive.atkT--;

      let aKind = 'idle';
      if (drive.rollT > 0) aKind = 'roll';
      else if (drive.z > 0) aKind = 'jump';
      else if (dx || dy) { aKind = 'run'; drive.walkDist++; }

      const frames = currentRig.anims[`${aKind}_${drive.dir}`];
      const fIdx = aKind === 'run'
        ? Math.floor(drive.walkDist / 5) % frames.length
        : aKind === 'roll'
          ? Math.floor((16 - drive.rollT) / 4) % frames.length
          : aKind === 'jump'
            ? (drive.vz > 0 ? 1 : 2)
            : Math.floor(animTick / 24) % frames.length;
      const fGrid = frames[fIdx] || frames[0];
      const isAtk = drive.atkT > 4;
      const wGrid = aKind === 'roll' ? null : getWeaponFrame(drive.dir, isAtk);
      const anc = getAnchor(drive.dir, aKind, fIdx, isAtk);
      const px = Math.round(drive.x);
      const py = Math.round(drive.y - drive.z);

      prevCtx.fillStyle = '#8b956d';
      prevCtx.fillRect(px + 2, Math.round(drive.y) + 14, 12, 2);
      if (anc.behind && wGrid) drawGrid16(prevCtx, wGrid, px + anc.dx, py + anc.dy);
      drawGrid16(prevCtx, fGrid, px, py);
      if (!anc.behind && wGrid) drawGrid16(prevCtx, wGrid, px + anc.dx, py + anc.dy);

      // Bullets
      prevCtx.fillStyle = '#1f2420';
      drive.bullets = drive.bullets.filter((b) => {
        b.x += b.vx; b.y += b.vy;
        prevCtx.fillRect(Math.round(b.x), Math.round(b.y), 2, 2);
        return b.x >= 0 && b.x < 64 && b.y >= 0 && b.y < 64;
      });
    }
  }

  function setTab(tab) {
    activeTab = tab;
    $('tab-playtest').classList.toggle('on', tab === 'playtest');
    $('tab-studio').classList.toggle('on', tab === 'studio');
    $('playtest-view').classList.toggle('hidden', tab !== 'playtest');
    $('studio-view').classList.toggle('hidden', tab !== 'studio');
    if (tab === 'studio' && !paused) togglePause();
  }

  $('tab-playtest').onclick = () => setTab('playtest');
  $('tab-studio').onclick = () => setTab('studio');

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
  if (backend.name === 'local') {
    fetch('/api/artwork-presets').then((r) => (r.ok ? r.json() : null)).then((data) => {
      if (data) { Object.assign(allPresets, data); populatePresets(); loadPreset( Object.keys(data)[0] || 'adventurer'); }
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

  $('st-weapon').onchange = (e) => { weaponId = e.target.value; renderEditor(); renderFrameStrip(); };
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
    rebuildRig();
    toast('Synthesized Up and Right poses from Down!');
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
      const grid = quantizeRgbaToGrid16(data, img.width, img.height);
      const target = $('st-upload-target').value;
      if (target === 'up') baseUp = grid;
      else if (target === 'right') baseRight = grid;
      else {
        baseDown = grid;
        if (synthMode === '1pose') {
          baseUp = synthesizeUpFromDown(baseDown);
          baseRight = synthesizeRightFromDown(baseDown);
        }
      }
      rebuildRig();
      toast(`Imported ${f.name} into ${target} (16×16 4-shade DMG)`);
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
      toast('Copied GBDK C header + source to clipboard!');
    } catch {
      toast('Clipboard unavailable — use Download GBDK (.h + .c)');
    }
  };

  loadPreset(Object.keys(allPresets)[0] || 'adventurer');
  setInterval(stepStudio, 1000 / 60);
  return { setTab, rebuildRig };
})();

// ---------------------------------------------------------------------------
await (window.RGB_WASM ? initCore({ module_or_path: window.RGB_WASM }) : initCore());
renderWatches();
await loadFromBackend();
backend.subscribe?.(onBackendEvent);
refreshSavedReplays();
requestAnimationFrame(loop);
// Expose for debugging from the browser console / automation.
window.rgb = { get emu() { return emu; }, symbols: () => symbols, playReplay, bugReport, toggleRecord, studio };
