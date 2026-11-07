const $ = id => document.getElementById(id);
const C = 2 * Math.PI * 52;

/* ---------- Theme & Welcome ---------- */
const root = document.documentElement;
function applyTheme(t, save) {
  root.dataset.theme = t;
  $('themeBtn').setAttribute('aria-label', t === 'dark' ? 'Switch to light mode' : 'Switch to dark mode');
  if (save) { try { localStorage.setItem('theme', t); } catch (e) {} }
}
$('themeBtn').onclick = () => applyTheme(root.dataset.theme === 'dark' ? 'light' : 'dark', true);
applyTheme(root.dataset.theme || 'light', false);

(function () {
  const h = new Date().getHours();
  $('wGreet').textContent = (h < 12 ? 'Good morning' : h < 18 ? 'Good afternoon' : 'Good evening') + '!';
  document.body.style.overflow = 'hidden';
  $('wGo').onclick = () => {
    $('welcome').classList.add('out');
    document.body.style.overflow = '';
    window.scrollTo(0, 0);
    setTimeout(() => { $('welcome').hidden = true; }, 450);
  };
})();

/* ---------- Config ---------- */
const DEFAULT_HOST = '192.168.1.100';
let espHost = (() => {
  try { const v = localStorage.getItem('espHost'); if (v) return v; } catch (e) {}
  return DEFAULT_HOST;
})();

const pingUrl = () => `http://${espHost}/ping`;
const fansUrl = () => `http://${espHost}/api/fans`;

/* ---------- Chart constants ---------- */
const BANANA_MAX = 2.0;
const MIXED_MAX  = 0.5;
const BANANA_WARN = 0.1;
const MIXED_WARN  = 0.05;
const histB = Array(60).fill(0);
const histM = Array(60).fill(0);

const xPos = i => 34 + i / 59 * 258;
const yPos = (v, max) => 138 - Math.min(v, max) / max * 128;

function drawEthChart(prefix, hist, max, warn) {
  const pts = hist.map((v, i) => `${xPos(i).toFixed(1)} ${yPos(v, max).toFixed(1)}`);
  const last = hist[hist.length - 1];
  const col = last >= warn * 2 ? 'var(--bad)' : last >= warn ? 'var(--warn)' : 'var(--ok)';

  $('ln' + prefix).setAttribute('d', 'M' + pts.join(' L'));
  $('area' + prefix).setAttribute('d', 'M' + pts.join(' L') + ' L292 138 L34 138Z');
  $('ln' + prefix).style.stroke = col;
  $('area' + prefix).style.fill = col;
  $('pt' + prefix).style.fill = col;
  $('pt' + prefix).setAttribute('cx', xPos(59));
  $('pt' + prefix).setAttribute('cy', yPos(last, max));

  const yWarn = yPos(warn, max);
  $('thr' + prefix).setAttribute('y1', yWarn);
  $('thr' + prefix).setAttribute('y2', yWarn);
  $('thrT' + prefix).setAttribute('y', yWarn - 4);
  $('thrT' + prefix).textContent = 'Fan trigger ' + warn.toFixed(2) + ' ppm';
}

/* ---------- Connection state ---------- */
let conn = 'off';
let pingTimer = null;

function setStatus(state, message) {
  const line = $('statusLine');
  if (!line) return;
  line.className = 'status-line' + (state ? ' ' + state : '');
  line.textContent = message;
}

function setConn(s, msg) {
  const prev = conn;
  conn = s;
  $('devA').className = 'a st-' + s;
  $('devS').textContent = s === 'on' ? 'Connected via Wi-Fi' : s === 'ing' ? 'Connecting…' : 'Not connected';
  $('devB').textContent = s === 'on' ? 'Disconnect' : s === 'ing' ? 'Cancel' : 'Connect';

  if (s === 'on') {
    setStatus('connected', msg || `Connected to ESP32 (${espHost})`);
    if (prev !== 'on') logEv('ESP32 connected', 'Wi-Fi link active', 'ESP32', 'Online', 'p-on');
  } else if (s === 'ing') {
    setStatus('connecting', msg || `Connecting to ${espHost}…`);
    if (prev === 'on') logEv('ESP32 disconnected', 'Retrying Wi-Fi', 'ESP32', 'Actioned', 'p-ac');
  } else {
    setStatus('idle', msg || 'Idle — tap Connect to start polling');
  }
}

async function ping() {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), 3000);
  try {
    const res = await fetch(pingUrl(), { signal: ctrl.signal, cache: 'no-store' });
    if (!res.ok) throw new Error('status ' + res.status);
    const body = await res.text();
    if (body.trim() !== 'pong') throw new Error('unexpected body');
    setConn('on');
  } catch (err) {
    setConn('ing', `Can't reach ${espHost} — retrying…`);
  } finally {
    clearTimeout(t);
  }
}

function startPolling() {
  if (pingTimer) clearInterval(pingTimer);
  setConn('ing', `Connecting to ${espHost} via Wi-Fi…`);
  pingTimer = setInterval(ping, 2000);
  ping();
}

function stopPolling() {
  if (pingTimer) clearInterval(pingTimer);
  pingTimer = null;
  setConn('off', 'Disconnected');
}

$('devA').onclick = () => {
  if (conn === 'off') startPolling();
  else stopPolling();
};

$('statusLine').addEventListener('dblclick', () => {
  const v = prompt('ESP32 IP or hostname:', espHost);
  if (v === null) return;
  const trimmed = v.trim().replace(/^https?:\/\//, '').replace(/\/$/, '');
  if (!trimmed) return;
  espHost = trimmed;
  try { localStorage.setItem('espHost', espHost); } catch (e) {}
  if (pingTimer) startPolling();
  else setConn('off', `Address set to ${espHost} — tap Connect`);
});

/* ---------- Rendering helpers ---------- */
function ring(id, p) {
  const r = $(id);
  if (!r) return;
  r.style.strokeDasharray = C;
  r.style.strokeDashoffset = C * (1 - Math.max(0, Math.min(100, p)) / 100);
}

function setChamberState(id, s) {
  const el = $(id);
  const labels = ['Normal', 'Warning', 'Purge'];
  const classes = ['state-normal', 'state-warn', 'state-high'];
  el.textContent = labels[s] || 'Normal';
  el.className = 'chamber-state ' + classes[s];
}

function setFan(id, pwm) {
  const pct = Math.round(pwm / 255 * 100);
  const el = $(id);
  el.textContent = pct + '%';
  el.className = 'pwm ' + (pct >= 70 ? 'pwm-high' : pct >= 40 ? 'pwm-warn' : 'pwm-normal');
}

function setMiniState(id, s) {
  const el = $(id);
  const labels = ['Normal', 'Elevated', 'High'];
  const classes = ['mini-normal', 'mini-warn', 'mini-high'];
  el.textContent = labels[s] || 'Normal';
  el.className = 'mini-st ' + classes[s];
}

/* ---------- Render a simulated frame ---------- */
function renderLive(d) {
  const b = d.b;
  $('bE').textContent = b.e.toFixed(2);
  $('bT').textContent = b.t.toFixed(1);
  $('bH').textContent = Math.round(b.h);
  ring('bTR', b.t / 40 * 100);
  ring('bHR', b.h);
  setChamberState('bState', b.s);
  setFan('bF1', b.pwm);
  setFan('bF2', b.pwm);

  const m = d.m;
  $('mE').textContent = m.e.toFixed(2);
  setChamberState('mState', m.s);
  setFan('mF3', m.pwm);
  setFan('mF4', m.pwm);

  d.c.forEach(crop => {
    const t = $(crop.id + 'T');
    const h = $(crop.id + 'H');
    const s = $(crop.id + 'S');
    if (t) t.textContent = crop.t.toFixed(1);
    if (h) h.textContent = Math.round(crop.h);
    if (s) setMiniState(crop.id + 'S', crop.s);
  });

  histB.push(b.e); histB.shift();
  histM.push(m.e); histM.shift();
  drawEthChart('A', histB, BANANA_MAX, BANANA_WARN);
  drawEthChart('B', histM, MIXED_MAX, MIXED_WARN);

  updateAI(b.s, m.s, b.e, m.e);
}

function updateAI(bState, mState, bE, mE) {
  const worst = Math.max(bState, mState);
  const labels = ['Normal', 'Elevated', 'High'];
  const titles = ['Normal microclimate', 'Ethylene rising: ventilating', 'High ethylene: purge'];
  const details = [
    'All environmental inputs remain within the storage profile.',
    `Banana ${bE.toFixed(2)} ppm · mixed ${mE.toFixed(2)} ppm — fans ramped to 50%.`,
    `Banana ${bE.toFixed(2)} ppm · mixed ${mE.toFixed(2)} ppm — fans at full purge.`
  ];
  $('aP').textContent = labels[worst];
  $('aT').textContent = titles[worst];
  $('aD').textContent = details[worst];
  const cf = 96.5 + Math.random() * 2.5;
  $('aC').textContent = cf.toFixed(1) + '%';
  $('aF').style.width = cf + '%';
}

/* ---------- Event Log ---------- */
const ev = [];
let showAll = false;

function logEv(a, b, c, d, e) {
  ev.unshift([new Date().toTimeString().slice(0, 8), a, b, c, d, e]);
  ev.length = Math.min(ev.length, 50);
  drawLog();
}
function drawLog() {
  $('vb').innerHTML = showAll ? 'Show<br>less' : 'View all<br>events';
  $('vb').setAttribute('aria-expanded', showAll);
  $('log').innerHTML = (showAll ? ev : ev.slice(0, 1)).map(e =>
    `<div class="ev"><div class="r"><b>${e[0]}<small>Today</small></b><span>${e[1]}</span></div>
     <p><span class="k">Automated action:</span>${e[2]}</p>
     <p><span class="k">Source:</span>${e[3]}</p>
     <span class="pill ${e[5]}">${e[4]}</span></div>`).join('');
}
$('vb').onclick = () => { showAll = !showAll; drawLog(); };

/* ---------- Simulation ---------- */
const wave = (base, amp, period, phase) =>
  base + amp * Math.sin(Date.now() / period * 2 * Math.PI + phase);

const CROP_LIMITS = {
  sitaw:     { tWarn: 7.5,  tHigh: 9.0,  hLo: 90, hHi: 95, hHighLo: 85, hHighHi: 95, eWarn: 0.05, eHigh: 0.10 },
  pomelo:    { tWarn: 12.0, tHigh: 14.0, hLo: 88, hHi: 93, hHighLo: 85, hHighHi: 95, eWarn: 0.05, eHigh: 0.50 },
  kalamansi: { tWarn: 12.0, tHigh: 14.0, hLo: 88, hHi: 93, hHighLo: 85, hHighHi: 95, eWarn: 0.05, eHigh: 0.50 },
  corn:      { tWarn: 2.0,  tHigh: 5.0,  hLo: 95, hHi: 98, hHighLo: 90, hHighHi: 98, eWarn: 0.10, eHigh: 0.50 }
};
const BANANA_LIMITS = { tWarn: 14.5, tHigh: 16.0, hLo: 90, hHi: 95, hHighLo: 85, hHighHi: 95, eWarn: 0.10, eHigh: 1.00 };
const PWM_TABLE = [51, 128, 204];

function evalState(t, h, e, L) {
  if (t > L.tHigh || h < L.hHighLo || h > L.hHighHi || e > L.eHigh) return 2;
  if (t > L.tWarn || h < L.hLo || h > L.hHi || e > L.eWarn) return 1;
  return 0;
}

function simulateLocal() {
  const bT = wave(14, 2.2, 45000, 0);
  const bH = wave(91, 5, 37000, 0.4);
  const bE = Math.max(0, wave(0.5, 0.62, 33000, 0.7));
  const mE = Math.max(0, wave(0.06, 0.08, 36000, 2.1));

  const crops = [
    { id: 'sitaw',     t: wave(6.5, 2.5, 41000, 1.1), h: wave(92, 4, 29000, 1.5) },
    { id: 'pomelo',    t: wave(10, 3, 53000, 2.3),    h: wave(90, 5, 31000, 2.7) },
    { id: 'kalamansi', t: wave(9.8, 2.8, 47000, 3.4), h: wave(90, 4.5, 35000, 3.8) },
    { id: 'corn',      t: wave(1.5, 3, 39000, 4.6),   h: wave(96.5, 2.5, 27000, 5.0) }
  ];

  const bState = evalState(bT, bH, bE, BANANA_LIMITS);
  let mState = 0;
  crops.forEach(c => {
    c.s = evalState(c.t, c.h, mE, CROP_LIMITS[c.id]);
    if (c.s > mState) mState = c.s;
  });

  return {
    b: { t: bT, h: bH, e: bE, s: bState, pwm: PWM_TABLE[bState] },
    m: { e: mE, s: mState, pwm: PWM_TABLE[mState] },
    c: crops,
    f: [PWM_TABLE[bState], PWM_TABLE[bState], PWM_TABLE[mState], PWM_TABLE[mState]]
  };
}

/* ---------- Fan push (UI -> ESP32) ---------- */
let lastPush = 0;
const pushStats = { ok: 0, fail: 0 };
window.__fanStats = pushStats;

function pushFansToESP(f4) {
  if (!Array.isArray(f4) || f4.length !== 4) return;
  if (Date.now() - lastPush < 500) return;   // max 2 pushes/sec
  lastPush = Date.now();
  const body = f4.map(v => Math.max(0, Math.min(255, Math.round(v)))).join(',');
  fetch(fansUrl(), {
    method: 'POST',
    headers: { 'Content-Type': 'text/plain' },
    body,
    cache: 'no-store'
  })
    .then(res => {
      if (!res.ok) throw new Error('status ' + res.status);
      pushStats.ok++;
    })
    .catch(() => { pushStats.fail++; });
}

/* ---------- Tick loop ---------- */
function tick() {
  const sim = simulateLocal();
  renderLive(sim);
  pushFansToESP(sim.f);
}

/* ---------- Sync ---------- */
let lastSync = null;
function doSync(isManual) {
  lastSync = new Date();
  $('sync').textContent = lastSync.toTimeString().slice(0, 8);
  const i = document.querySelector('#syncB svg');
  i.classList.remove('spin'); void i.getBoundingClientRect(); i.classList.add('spin');
  if (isManual) logEv('Telemetry synced', 'Manual sync', 'ESP32', 'Online', 'p-on');
  agoTxt();
}
function agoTxt() {
  if (!lastSync) return;
  const s = Math.round((Date.now() - lastSync) / 1000);
  $('ago').textContent = (s < 5 ? 'Just now' : s < 60 ? s + ' s ago' : Math.floor(s / 60) + ' min ago') + ' · auto';
}
$('syncB').onclick = () => doSync(true);

/* ---------- Init ---------- */
setConn('off');
drawLog();
logEv('System started', 'Portal initialised', 'System', 'Online', 'p-on');
tick();
doSync(false);
setInterval(tick, 1000);
setInterval(() => doSync(false), 300000);
setInterval(agoTxt, 1000);
