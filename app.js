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

/* ---------- Crops ---------- */
const P = [
  { n:'Calamansi', d:'Citrus held cool to keep rind firm and fresh.', t:[8,10], h:[85,90], e:330, img:'images/calamansi.jpg', bg:'radial-gradient(circle at 30% 40%,#f2c14e,#7a8f1e 55%,#1b3d22)' },
  { n:'Corn', d:'Sweet corn chilled fast to lock in sugars.', t:[0,2], h:[95,98], e:330, img:'images/corn.jpg', bg:'radial-gradient(circle at 70% 30%,#f0d56a,#8a9a2e 55%,#1b3d22)' },
  { n:'Pomelo', d:'Large citrus stored with gentle airflow.', t:[7,10], h:[85,90], e:330, img:'images/pomelo.jpg', bg:'radial-gradient(circle at 40% 60%,#d9e08a,#5e8f3a 55%,#16351f)' },
  { n:'String Beans', d:'Tender pods kept humid and ethylene-free.', t:[4,7], h:[90,95], e:330, img:'images/string-beans.jpg', bg:'radial-gradient(circle at 60% 40%,#8fcf6a,#2e7a3c 55%,#0f2c1a)' },
  { n:'Cardaba Banana', d:'Green fruit monitored for controlled, even ripening.', t:[13,15], h:[90,95], e:330, img:'images/banana.jpg', bg:'linear-gradient(160deg,#c9d95a,#2e7a3c 60%,#0f2c1a)' }
];

let cur = 0;
let g1 = 0, g2 = 0; // Simulated fallback
const hist1 = Array(30).fill(0);
const hist2 = Array(30).fill(0);
let lastRun1 = false, lastRun2 = false;

/* ---------- Event Log ---------- */
const ev = [
  ['14:02:48','Telemetry synchronized','Readings stored locally','ESP32-S3','Online','p-on'],
  ['13:46:12','Humidity reached equilibrium','Ventilation returned to standby','SHT31','Resolved','p-re']
];
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

/* ---------- Chart / Gauge Rendering ---------- */
function ring(id, p) {
  const r = $(id);
  if (!r) return;
  r.style.strokeDasharray = C;
  r.style.strokeDashoffset = C * (1 - Math.max(0, Math.min(100, p)) / 100);
}
function drawChart(col, e, hist, suffix) {
  const X = i => 34 + i / 29 * 258, Y = v => 138 - Math.min(v, 400) / 400 * 128;
  const pts = hist.map((v, i) => X(i).toFixed(1) + ' ' + Y(v).toFixed(1));
  $('ln' + suffix).setAttribute('d', 'M' + pts.join(' L'));
  $('area' + suffix).setAttribute('d', 'M' + pts.join(' L') + ' L292 138 L34 138Z');
  $('ln' + suffix).style.stroke = col; $('area' + suffix).style.fill = col; $('pt' + suffix).style.fill = col;
  $('pt' + suffix).setAttribute('cx', X(29)); $('pt' + suffix).setAttribute('cy', Y(hist[29]));
  $('thr' + suffix).setAttribute('y1', Y(e)); $('thr' + suffix).setAttribute('y2', Y(e));
  $('thrT' + suffix).setAttribute('y', Y(e) - 4); $('thrT' + suffix).textContent = 'Fan at ' + e + ' ppm';
}

/* ---------- Crop Picker ---------- */
function pick(i) {
  cur = (i + P.length) % P.length;
  const p = P[cur];
  $('hero').style.backgroundImage = `url("${p.img}"), ${p.bg}`;
  $('hName').textContent = p.n; $('hDesc').textContent = p.d;
  
  const tgtStr = `${p.t[0]}–${p.t[1]} °C · ${p.h[0]}–${p.h[1]}% RH · fan at ${p.e} ppm`;
  $('tgt1').textContent = tgtStr;
  $('tgt2').textContent = tgtStr;

  [...$('chips').children].forEach((c, j) => c.setAttribute('aria-pressed', j === cur));
  const ch = $('chips'), sc = ch.children[cur];
  if (sc) ch.scrollTo({ left: sc.getBoundingClientRect().left - ch.getBoundingClientRect().left + ch.scrollLeft - (ch.clientWidth - sc.offsetWidth) / 2, behavior: 'smooth' });
  [...$('dots').children].forEach((c, j) => c.className = j === cur ? 'on' : '');
  render();
}
P.forEach((p, i) => {
  const b = document.createElement('button');
  b.className = 'chip'; b.textContent = p.n; b.onclick = () => pick(i);
  $('chips').append(b); $('dots').append(document.createElement('i'));
});

function setView(first) {
  $('first').hidden = !first; $('second').hidden = first;
  $('devA').tabIndex = first ? 0 : -1;
  $('devA').setAttribute('role', first ? 'button' : 'presentation');
}
$('ok').onclick = () => { setView(false); window.scrollTo(0, 0); };
$('back').onclick = () => { setView(true); window.scrollTo(0, 0); };
$('prev').onclick = () => pick(cur - 1);
$('next').onclick = () => pick(cur + 1);

/* ---------- Bluetooth LE ---------- */
const SERVICE_UUID = '6e400001-b5a8-f393-e0a9-e50e24dcca9e';
const TX_CHAR_UUID = '6e400003-b5a8-f393-e0a9-e50e24dcca9e';
const DEVICE_NAME  = 'ESP32-Storage';

let conn = 'off', bleDev = null, bleBuf = '', live = null;
let keepAliveTimer = null;
let lastBleTime = 0;
const decoder = new TextDecoder();

function setStatus(state, message) {
  const line = $('statusLine');
  if (!line) return;
  line.className = 'status-line' + (state ? ' ' + state : '');
  line.textContent = message;
}
function setConn(s, msg) {
  conn = s;
  $('devA').className = 'a st-' + s;
  $('devS').textContent = s === 'on' ? 'Connected via Bluetooth LE' : s === 'ing' ? 'Connecting…' : 'Not connected';
  $('devB').textContent = s === 'on' ? 'Disconnect' : s === 'ing' ? 'Cancel' : 'Connect';
  if (s === 'on') setStatus('connected', msg || `Connected to ${DEVICE_NAME}`);
  else if (s === 'ing') setStatus('connecting', msg || `Connecting…`);
  else setStatus('idle', msg || 'Idle — tap Connect to scan');
}

function onBleData(e) { onBleText(decoder.decode(e.target.value)); }

function onBleText(text) {
  bleBuf += text;
  let nl;
  while ((nl = bleBuf.indexOf('\n')) >= 0) {
    const line = bleBuf.slice(0, nl).trim();
    bleBuf = bleBuf.slice(nl + 1);
    
    // Parse the dual-container payload: T1:...,H1:...,E1:...,T2:...,H2:...,E2:...
    const m = line.match(/T1:(-?[\d.]+),H1:(-?[\d.]+),E1:(-?[\d.]+),T2:(-?[\d.]+),H2:(-?[\d.]+),E2:(-?[\d.]+)/);
    if (m) {
      live = {
        t1: parseFloat(m[1]), h1: parseFloat(m[2]), e1: parseFloat(m[3]),
        t2: parseFloat(m[4]), h2: parseFloat(m[5]), e2: parseFloat(m[6]),
        at: Date.now()
      };
      lastBleTime = Date.now();
      
      // Instantly update UI with real data
      updateUI(live);
    }
  }
}

if (window.AndroidBle) {
  window.NativeBLE = {
    onState(msg) { setConn('ing', msg); },
    onConnected(name) { setConn('on', `Connected to ${name}`); logEv('ESP32 connected', 'BLE established', 'ESP32', 'Online', 'p-on'); },
    onDisconnected: onBleLost,
    onError(msg) { stopKeepAlive(); setConn('off', msg); setStatus('error', msg); },
    onData: onBleText
  };
}

function startKeepAlive(char) {
  stopKeepAlive();
  keepAliveTimer = setInterval(async () => {
    try {
      if (!bleDev || !bleDev.gatt.connected) { stopKeepAlive(); return; }
      await char.readValue();
    } catch (e) { stopKeepAlive(); }
  }, 5000);
}
function stopKeepAlive() {
  if (keepAliveTimer) { clearInterval(keepAliveTimer); keepAliveTimer = null; }
}

function onBleLost() {
  stopKeepAlive();
  live = null; bleBuf = ''; bleDev = null;
  if (conn === 'on') logEv('ESP32 disconnected', 'Switched to offline mode', 'ESP32', 'Actioned', 'p-ac');
  setConn('off', 'Disconnected — tap Connect to retry');
  // Clear real data UI
  $('tV1').textContent = '--'; $('hV1').textContent = '--'; ring('tR1', 0); ring('hR1', 0);
  $('tV2').textContent = '--'; $('hV2').textContent = '--'; ring('tR2', 0); ring('hR2', 0);
}

async function bleConnect() {
  if (window.AndroidBle) {
    setConn('ing', `Scanning…`); window.AndroidBle.connect(); return;
  }
  if (!navigator.bluetooth) {
    setConn('off', 'Bluetooth not supported — use Chrome/Edge'); return;
  }
  setConn('ing', 'Scanning for ESP32…');
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }, { namePrefix: 'ESP32' }],
      optionalServices: [SERVICE_UUID]
    });
    setConn('ing', `Connecting to ${device.name || DEVICE_NAME}…`);
    device.addEventListener('gattserverdisconnected', onBleLost);
    const server  = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    const tx      = await service.getCharacteristic(TX_CHAR_UUID);
    await tx.startNotifications();
    tx.addEventListener('characteristicvaluechanged', onBleData);
    bleDev = device;
    startKeepAlive(tx);
    setConn('on', `Connected to ${device.name || DEVICE_NAME}`);
    logEv('ESP32 connected', 'BLE session established', 'ESP32', 'Online', 'p-on');
  } catch (err) {
    console.warn(err);
    setConn('off', 'Connection failed or cancelled');
  }
}

$('devA').onclick = () => {
  if ($('first').hidden) return;
  if (conn === 'off') bleConnect();
  else if (conn === 'on' && window.AndroidBle) window.AndroidBle.disconnect();
  else if (conn === 'on' && bleDev && bleDev.gatt.connected) bleDev.gatt.disconnect();
  else if (conn === 'ing') {
    if (window.AndroidBle) window.AndroidBle.disconnect();
    stopKeepAlive();
    setConn('off', 'Cancelled');
  }
};

/* ---------- Core Logic ---------- */
function updateUI(d) {
  // Container 1
  $('tV1').textContent = d.t1.toFixed(1);
  $('hV1').textContent = Math.round(d.h1);
  ring('tR1', d.t1 / 50 * 100);
  ring('hR1', d.h1);
  $('gV1').textContent = Math.round(d.e1);
  
  const col1 = d.e1 >= P[cur].e * 1.4 ? 'var(--bad)' : d.e1 >= P[cur].e ? 'var(--warn)' : 'var(--ok)';
  drawChart(col1, P[cur].e, hist1, '1');
  $('gS1').style.color = col1;
  $('gS1').textContent = d.e1 >= P[cur].e * 1.4 ? 'High ethylene' : d.e1 >= P[cur].e ? 'Ethylene rising' : 'Normal';

  // Container 2
  $('tV2').textContent = d.t2.toFixed(1);
  $('hV2').textContent = Math.round(d.h2);
  ring('tR2', d.t2 / 50 * 100);
  ring('hR2', d.h2);
  $('gV2').textContent = Math.round(d.e2);
  
  const col2 = d.e2 >= P[cur].e * 1.4 ? 'var(--bad)' : d.e2 >= P[cur].e ? 'var(--warn)' : 'var(--ok)';
  drawChart(col2, P[cur].e, hist2, '2');
  $('gS2').style.color = col2;
  $('gS2').textContent = d.e2 >= P[cur].e * 1.4 ? 'High ethylene' : d.e2 >= P[cur].e ? 'Ethylene rising' : 'Normal';
}

function tick() {
  const p = P[cur];
  
  // If not connected or haven't received BLE data in 15s, run simulation
  if (conn !== 'on' || Date.now() - lastBleTime > 15000) {
    g1 = Math.max(0, Math.min(500, g1 + (Math.random() - .46) * 35));
    g2 = Math.max(0, Math.min(500, g2 + (Math.random() - .46) * 35));
    
    // Simulate temp/hum fallback
    const sim = {
      t1: 28.0 + Math.random(), h1: 85.0 + Math.random(), e1: g1,
      t2: 28.5 + Math.random(), h2: 84.0 + Math.random(), e2: g2
    };
    updateUI(sim);
  }

  const fresh = conn === 'on' && live && Date.now() - lastBleTime <= 15000;
  if (fresh) { g1 = live.e1; g2 = live.e2; }
  hist1.push(g1); hist1.shift();
  hist2.push(g2); hist2.shift();

  // Global Inference State (based on highest ethylene)
  const maxE = Math.max(live ? live.e1 : g1, live ? live.e2 : g2);
  const run = maxE >= p.e;
  const lv = maxE >= p.e * 1.4 ? 2 : maxE >= p.e ? 1 : 0;

  $('aP').textContent = ['Normal', 'Elevated', 'High'][lv];
  $('aT').textContent = lv === 0 ? 'Normal microclimate' : lv === 1 ? 'Ethylene rising: ventilating' : 'High ethylene: purge';
  $('aD').textContent = lv === 0 ? 'All inputs within profile.' : `Max ethylene is ${Math.round(maxE)} ppm (limit ${p.e}).`;

  if (run !== lastRun1 || run !== lastRun2) {
    lastRun1 = run; lastRun2 = run;
    logEv(run ? 'Ventilation triggered' : 'Ventilation standby', run ? 'Purge fan ON (auto)' : 'Fan OFF', 'System', run ? 'Actioned' : 'Resolved', run ? 'p-ac' : 'p-re');
  }
  
  const cf = 97.4 + Math.random() * 1.6;
  $('aC').textContent = cf.toFixed(1) + '%';
  $('aF').style.width = cf + '%';
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
pick(0);
tick();
doSync(false);
setView(true);
setInterval(tick, 2000);
setInterval(() => doSync(false), 300000);
setInterval(agoTxt, 1000);