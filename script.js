  /* ============================================================
    IoT-Enabled Edge AI Post-Harvest Storage Systems
    Frontend UI integrated with ESP32-S3 AP HTTP Server
    ============================================================ */

  const $ = id => document.getElementById(id);
  const C = 2 * Math.PI * 52;
  const ESP_IP = (window.location.protocol.startsWith('http'))
    ? window.location.origin
    : 'http://192.168.4.1';

  /* ============================================================
  1. Welcome screen
    ============================================================ */
  (function () {
    const h = new Date().getHours();
    $('wGreet').textContent =
      (h < 12 ? 'Good morning' : h < 18 ? 'Good afternoon' : 'Good evening') + '!';

    document.body.style.overflow = 'hidden';

    $('wGo').onclick = () => {
      $('welcome').classList.add('out');
      document.body.style.overflow = '';
      window.scrollTo(0, 0);
      setTimeout(() => { $('welcome').hidden = true; }, 450);
    };
  })();

  /* ============================================================
  2. Dark mode
    ============================================================ */
  (function () {
    let d = false;
    try { d = localStorage.getItem('dark') === '1'; } catch (e) {}

    const MOON = '<path d="M21 13A9 9 0 1111 3a7 7 0 0010 10z"/>';
    const SUN  = '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>';

    function ap() {
      document.body.classList.toggle('dark', d);
      $('dmI').innerHTML = d ? SUN : MOON;
      $('dmT').textContent = d ? 'Light' : 'Dark';
      $('dm').setAttribute('aria-pressed', d);
    }

    $('dm').onclick = () => {
      d = !d;
      try { localStorage.setItem('dark', d ? '1' : '0'); } catch (e) {}
      ap();
    };

    ap();
  })();

  /* ============================================================
  3. Crop profiles
    ============================================================ */
  const P = [
    { n:'Calamansi',      d:'Citrus held cool to keep rind firm and fresh.',        t:[25,29],  h:[71,80], e:1.90, bg:'radial-gradient(circle at 30% 40%,#f2c14e,#7a8f1e 55%,#1b3d22)' },
    { n:'Corn',           d:'Sweet corn chilled fast to lock in sugars.',           t:[25,29],   h:[71,80], e:1.90, bg:'radial-gradient(circle at 70% 30%,#f0d56a,#8a9a2e 55%,#1b3d22)' },
    { n:'Pomelo',         d:'Large citrus stored with gentle airflow.',             t:[25,29],  h:[71,80], e:1.90, bg:'radial-gradient(circle at 40% 60%,#d9e08a,#5e8f3a 55%,#16351f)' },
    { n:'String Beans',   d:'Tender pods kept humid and ethylene-free.',            t:[25,29],   h:[71,80], e:1.90, bg:'radial-gradient(circle at 60% 40%,#8fcf6a,#2e7a3c 55%,#0f2c1a)' },
    { n:'Cardava Banana', d:'Green fruit prominent ridges, and a thick peel that turns green to yellow.', t:[25,29], h:[71,80], e:3.50, bg:'radial-gradient(circle at 40% 50%,#f2e07a,#c9a227 55%,#3d2f10)' }
  ];

  const ZN = { 1:'Zone 1', 2:'Zone 2' };
  const chipEls = [];
  P.forEach(p => { p.z = p.n.includes('Banana') ? 1 : 2; });

  const IMG = [
    'url(images/calamansi.jpg)',
    'url(images/corn.jpg)',
    'url(images/pomelo.jpg)',
    'url(images/string-beans.jpg)',
    'url(images/cardava-banana.jpg)'
  ];

  /* ============================================================
  4. Core state
    ============================================================ */
  let cur = 0, g = 0, auto = true, lastRun = false;

  function ring(id, p) {
    const r = $(id);
    r.style.strokeDasharray = C;
    r.style.strokeDashoffset = C * (1 - Math.max(0, Math.min(100, p)) / 100);
  }

  const hist = Array(30).fill(0);

  function draw(col, e) {
    const X = i => 34 + i / 29 * 258;
    const Y = v => 138 - Math.min(v, 2.0) / 2.0 * 128;
    const pts = hist.map((v, i) => X(i).toFixed(1) + ' ' + Y(v).toFixed(1));

    $('ln').setAttribute('d', 'M' + pts.join(' L'));
    $('area').setAttribute('d', 'M' + pts.join(' L') + ' L292 138 L34 138Z');
    $('ln').style.stroke = col;
    $('area').style.fill = col;
    $('pt').style.fill = col;
    $('pt').setAttribute('cx', X(29));
    $('pt').setAttribute('cy', Y(hist[29]));
    $('thr').setAttribute('y1', Y(e));
    $('thr').setAttribute('y2', Y(e));
    $('thrT').setAttribute('y', Y(e) - 4);
    $('thrT').textContent = 'Target max ' + e.toFixed(2) + ' ppm';
  }

  /* Generates Container 1 Fan or Container 2 Fan in Live View */
  function renderLiveFans(p) {
    const grid = $('liveFanGrid');
    const cap = $('liveFanCap');
    if (!grid || !cap) return;

    if (p.z === 1) {
      cap.textContent = 'Container 1 · Fan';
      grid.innerHTML = `
        <div class="fan-item">
          <div class="fan-info">
            <span>Container 1 Fan · Banana</span>
            <span class="fan-speed" data-fan-mode="c1">--</span>
          </div>
          <span class="fan-status" data-fan-level="c1">--</span>
        </div>
      `;
    } else {
      cap.textContent = 'Container 2 · Fan';
      grid.innerHTML = `
        <div class="fan-item">
          <div class="fan-info">
            <span>Container 2 Fan · 4 Crops</span>
            <span class="fan-speed" data-fan-mode="c2">--</span>
          </div>
          <span class="fan-status" data-fan-level="c2">--</span>
        </div>
      `;
    }

    if (RT && RT.fans) updateFans(RT.fans);
  }

  function pick(i) {
    cur = (i + P.length) % P.length;
    const p = P[cur];

    $('second').hidden = true;
    $('first').hidden = false;
    $('second').querySelector('.cap').textContent = 'Live readings · ' + p.n;

    $('hero').style.backgroundImage = IMG[cur] || p.bg;

    $('hName').textContent = p.n;
    $('hDesc').textContent = p.d;
    $('tgt').textContent =
      ZN[p.z] + ' · ' + p.t[0] + '–' + p.t[1] + ' °C · ' +
      p.h[0] + '–' + p.h[1] + '% H · normal max ' + p.e.toFixed(2) + ' ppm';

    renderLiveFans(p);

    [...$('dots').children].forEach((c, j) => { c.className = j === cur ? 'on' : ''; });
    render(lastLe);
  }

  function zv(t, h, e) {
    return '<span><small>Temp</small>' + t + '</span>' +
          '<span><small>Humidity</small>' + h + '</span>' +
          '<span><small>Ethylene</small>' + e + '</span>';
  }

  function getCropState(cropName) {
    if (!RT) return null;
    if (cropName === 'Cardava Banana') {
      return { t: RT.b.t, h: RT.b.h, e: RT.b.e, s: RT.b.s };
    }
    const idMap = { 'String Beans': 'sitaw', 'Pomelo': 'pomelo', 'Calamansi': 'kalamansi', 'Corn': 'corn' };
    const c = RT.c.find(x => x.id === idMap[cropName]);
    if (!c) return null;
    const ethVal = (c.e !== undefined && c.e !== null) ? c.e : (RT.m ? RT.m.e : null);
    return { t: c.t, h: c.h, e: ethVal, s: c.s };
  }

  function fillZones() {
    const on = live();
    P.forEach((p, i) => {
      const c = chipEls[i];
      if (!c) return;
      const v = c.querySelector('.v');
      if (!v) return;

      if (!on || !RT) {
        v.innerHTML = zv('--', '--', '--');
        return;
      }

      const d = getCropState(p.n);
      if (d && d.t !== null && d.h !== null) {
        const ethStr = (d.e !== null) ? d.e.toFixed(2) + ' ppm' : 'N/A';
        v.innerHTML = zv(
          d.t.toFixed(1) + ' °C',
          Math.round(d.h) + '%',
          ethStr
        );
      } else {
        v.innerHTML = zv('--', '--', '--');
      }
    });
  }

  P.forEach((p, i) => {
    const b = document.createElement('div');
    b.className = 'chip';
    b.setAttribute('aria-pressed', 'false');
    b.innerHTML = '<b>' + p.n + '</b><span class="v">' + zv('--', '--', '--') + '</span>';
    chipEls[i] = b;
    $('chips' + p.z).append(b);
    $('dots').append(document.createElement('i'));
  });

  /* ============================================================
  5. View switching & connection state
    ============================================================ */
  function setView(f) {
    $('first').hidden = !f;
    $('second').hidden = f;
    $('devA').tabIndex = f ? 0 : -1;
    $('devA').setAttribute('role', f ? 'button' : 'presentation');
  }

  let conn = 'off';

  function setConn(s) {
    conn = s;
    $('devA').className = 'a st-' + s;
    $('devS').textContent =
      s === 'on'  ? 'Connected to ' + ESP_IP.replace('http://', '') :
      s === 'ing' ? 'Connecting…' : 'Not connected';
    $('devB').textContent =
      s === 'on'  ? 'Disconnect' :
      s === 'ing' ? 'Cancel' : 'Connect';
    const offl = $('offl');
    if (offl) offl.hidden = (s !== 'off');
    drawLog();
  }

  function logEv(a, b, c, d, e) {
    ev.unshift([new Date().toTimeString().slice(0, 8), a, b, c, d, e, snap()]);
    ev.length = Math.min(ev.length, 50);
    drawLog();
  }

  /* ============================================================
  6. ESP32-S3 HTTP Transport (AP Mode)
    ============================================================ */
  let RT = null, btTo = null;
  let pollTimer = null;

  function live() {
    return conn === 'on' && RT && Date.now() - RT.t < 15000;
  }

  async function fetchESPData() {
    try {
      const res = await fetch(ESP_IP + '/api/data', { cache: 'no-store' });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const data = await res.json();

      data.t = Date.now();
      RT = data;

      if (conn === 'ing') window.btState('on');

      updateFans(RT.fans);
      tick();
    } catch (e) {
      if (conn === 'on') window.btState('off', 'Lost connection to ESP32-S3 AP');
    }
  }

  function startPolling() {
    stopPolling();
    fetchESPData();
    pollTimer = setInterval(fetchESPData, 1500);
  }

  function stopPolling() {
    if (pollTimer) clearInterval(pollTimer);
    pollTimer = null;
    RT = null;
  }

  window.btState = function (s, msg) {
    if (s === 'on' && conn === 'ing') {
      clearTimeout(btTo);
      setConn('on');
      logEv('ESP32-S3 AP connected', 'Wi-Fi HTTP link established', 'ESP32-S3', 'Online', 'p-on');
    } else if (s === 'off' && conn !== 'off') {
      stopPolling();
      clearTimeout(btTo);
      setConn('off');
      if (msg) $('devS').textContent = msg;
      logEv('ESP32-S3 disconnected', msg || 'Switched to offline mode', 'ESP32-S3', 'Actioned', 'p-ac');
    }
  };

  /* ============================================================
  7. DOM handlers
    ============================================================ */
  function connectToAp() {
    setConn('ing');
    startPolling();
    btTo = setTimeout(() => {
      if (conn === 'ing') {
        stopPolling();
        window.btState('off', 'Timeout connecting to ' + ESP_IP.replace('http://', ''));
      }
    }, 6000);
  }

  $('devA').onclick = () => {
    if ($('first').hidden) return;
    if (conn === 'off') {
      connectToAp();
    } else {
      stopPolling();
      window.btState('off', 'Disconnected by user');
    }
  };

  $('devA').onkeydown = e => {
    if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); $('devA').click(); }
  };

  $('ok').onclick   = () => { setView(false); window.scrollTo(0, 0); };
  $('back').onclick = () => { setView(true);  window.scrollTo(0, 0); };
  $('prev').onclick = () => pick(cur - 1);
  $('next').onclick = () => pick(cur + 1);

  /* ============================================================
  8. Event log
    ============================================================ */
  const ev = [];
  let lastT = 0, lastH = 0, lastLe = 0, initSync = true;

  function t12(s) {
    const [a, b, c] = s.split(':'), h = +a;
    return (h % 12 || 12) + ':' + b + ':' + c + ' ' + (h < 12 ? 'AM' : 'PM');
  }

  function snap() {
    return [g.toFixed(2), lastT.toFixed(1), Math.round(lastH)];
  }

  function seedR(e) {
    return ['0.00', '25.0', 85];
  }

  function rs(e) {
    const r = e[6] || (e[6] = seedR(e));
    return `<div class="rs"><div><small>Ethylene</small><b>${r[0]} ppm</b></div><div><small>Temp</small><b>${r[1]} °C</b></div><div><small>Humidity</small><b>${r[2]}%</b></div></div>`;
  }

  const SB_URL = 'YOUR_SUPABASE_URL';
  const SB_KEY = 'YOUR_SUPABASE_ANON_KEY';
  const SB_TABLE = 'event_log';
  let sbBusy = false;

  function sbOn()  { return /^https:\/\//.test(SB_URL) && SB_KEY.length > 30; }
  function sbMsg(t){ $('sbS').textContent = 'Supabase: ' + t; }

  function sbSync() {
    if (!sbOn()) { sbMsg('not configured (AP offline mode)'); return; }
    if (sbBusy) return;

    const pend = ev.filter(e => e[6] && !e._s);
    if (!pend.length) return;
    sbBusy = true;

    const rows = pend.map(e => ({
      clock: e[0], event: e[1], action: e[2], source: e[3], status: e[4],
      ethylene_ppm: Number(e[6][0]), temp_c: Number(e[6][1]), humidity_pct: Number(e[6][2])
    }));

    fetch(SB_URL + '/rest/v1/' + SB_TABLE, {
      method: 'POST',
      headers: {
        apikey: SB_KEY,
        Authorization: 'Bearer ' + SB_KEY,
        'Content-Type': 'application/json',
        Prefer: 'return=minimal'
      },
      body: JSON.stringify(rows)
    })
      .then(r => { if (!r.ok) throw new Error(r.status); pend.forEach(e => { e._s = true; }); sbMsg('synced ' + nowT()); })
      .catch(() => sbMsg('offline, will retry'))
      .then(() => { sbBusy = false; });
  }

  /* ============================================================
  9. History actions
    ============================================================ */
  let showAll = false;

  function nowT() { return t12(new Date().toTimeString().slice(0, 8)); }
  function plural(n) { return n + (n === 1 ? ' event' : ' events'); }
  function msg(t) { $('laMsg').textContent = t; }
  function csvCell(v) { return '"' + String(v).replace(/"/g, '""') + '"'; }

  function histCsv() {
    const rows = [['Time', 'Event', 'Automated action', 'Source', 'Status', 'Ethylene ppm', 'Temp C', 'Humidity %']];
    ev.forEach(e => {
      const r = e[6] || seedR(e);
      rows.push([e[0], e[1], e[2], e[3], e[4], r[0], r[1], r[2]]);
    });
    return '\ufeff' + rows.map(r => r.map(csvCell).join(',')).join('\r\n');
  }

  $('dlH').onclick = () => {
    if (!ev.length) return;
    const csv = histCsv();
    const n = ev.length;
    const nm = 'event-history-' + new Date().toISOString().slice(0, 10) + '.csv';
    try {
      const a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }));
      a.download = nm;
      document.body.appendChild(a);
      a.click();
      a.remove();
      $('csvBox').style.display = 'none';
      msg('Downloaded ' + plural(n) + ' as ' + nm);
    } catch (x) {
      $('csvTxt').value = csv;
      $('csvBox').style.display = 'block';
      $('csvTxt').select();
      msg('Download unavailable here. Copy the CSV below.');
    }
  };

  function delAsk(on) {
    $('delBox').hidden = !on;
    $('logAct').style.display = on ? 'none' : '';
  }

  $('delH').onclick = () => {
    const n = ev.length;
    if (!n) return;
    $('delTxt').textContent = 'Delete ' + plural(n) + '? This cannot be undone.';
    msg('');
    delAsk(true);
  };

  $('delOk').onclick = () => {
    const n = ev.length;
    ev.length = 0;
    showAll = false;
    $('csvBox').style.display = 'none';
    delAsk(false);
    drawLog();
    msg('Deleted ' + plural(n) + '.');
  };

  $('delBack').onclick = () => { delAsk(false); msg('Delete cancelled.'); };

  /* ============================================================
  10. Rendering & Container Fan Display (Low / Medium / Maximum)
    ============================================================ */
  function drawLog() {
    sbSync();

    const on = conn === 'on';
    $('log').style.display = on ? '' : 'none';
    $('dlH').disabled = !ev.length;
    $('delH').disabled = !ev.length;
    $('vb').style.visibility = on ? '' : 'hidden';

    if (!on) { $('log').innerHTML = ''; return; }

    $('vb').innerHTML = showAll ? 'Show<br>less' : 'View all<br>events';
    $('vb').setAttribute('aria-expanded', showAll);

    $('log').innerHTML = (showAll ? ev : ev.slice(0, 1)).map(e =>
      `<div class="ev">
        <div class="r"><b>${t12(e[0])}<small>Today</small></b><span>${e[1]}</span></div>
        <p><span class="k">Automated action:</span>${e[2]}</p>
        <p><span class="k">Source:</span>${e[3]}</p>
        ${rs(e)}
        ${e[4] === 'Online' ? '' : `<span class="pill ${e[5]}">${e[4]}</span>`}
      </div>`
    ).join('');
  }

  function updateFans(fans) {
    const modeEls  = document.querySelectorAll('[data-fan-mode]');
    const levelEls = document.querySelectorAll('[data-fan-level]');

    if (!fans) {
      modeEls.forEach(el => el.textContent = '--');
      levelEls.forEach(el => { el.textContent = '--'; el.className = 'fan-status'; });
      return;
    }

    const STATE_DESC  = ['Normal · Low Airflow', 'Warning · Medium Airflow', 'Critical · Maximum Airflow'];
    const STATE_CLASS = ['low', 'med', 'max'];

    modeEls.forEach(el => {
      const key = el.getAttribute('data-fan-mode'); // "c1" or "c2"
      const idx = fans[key];
      if (idx === undefined || idx === null) return;
      el.textContent = STATE_DESC[idx] || '--';
    });

    levelEls.forEach(el => {
      const key = el.getAttribute('data-fan-level'); // "c1" or "c2"
      const idx = fans[key];
      const lbl = fans[key + 'Label'];
      if (idx === undefined || idx === null) return;
      el.textContent = lbl || '--';
      el.className = 'fan-status ' + (STATE_CLASS[idx] || '');
    });
  }

  function render(s) {
    const lv = s || 0;
    const p = P[cur];
    const fanSpeedText = (RT && RT.fans)
      ? (p.z === 1 ? RT.fans.c1Label : RT.fans.c2Label)
      : ['Low', 'Medium', 'Maximum'][lv];

    const bx = $('aB');
    bx.classList.toggle('warn', lv === 1);
    bx.classList.toggle('bad',  lv === 2);

    $('aP').textContent = ['Normal', 'Warning', 'Critical'][lv];
    $('aT').textContent = lv === 0
      ? 'Normal microclimate maintained'
      : (lv === 2 ? 'Critical: ' : 'Warning: ') + 'Conditions outside target ranges';
    $('aD').textContent = lv === 0
      ? 'All environmental inputs remain within the ' + p.n + ' storage profile. Fan speed: ' + fanSpeedText + '.'
      : 'Container ' + p.z + ' Fan set to ' + fanSpeedText + '.';

    if (lv !== lastLe) {
      const tm2 = new Date().toTimeString().slice(0, 8);
      if (lv === 2) {
        ev.unshift([tm2, 'Critical alert (' + p.n + ')', 'Container ' + p.z + ' Fan -> Maximum', 'ESP32-S3', 'Actioned', 'p-ac', snap()]);
      } else if (lv === 1) {
        ev.unshift([tm2, 'Warning (' + p.n + ')', 'Container ' + p.z + ' Fan -> Medium', 'ESP32-S3', 'Actioned', 'p-ac', snap()]);
      } else {
        ev.unshift([tm2, 'Conditions normal (' + p.n + ')', 'Container ' + p.z + ' Fan -> Low', 'ESP32-S3', 'Resolved', 'p-re', snap()]);
      }
      lastLe = lv;
      ev.length = Math.min(ev.length, 50);
      drawLog();
    }

    $('aB').setAttribute('aria-pressed', auto);
  }

  /* ============================================================
  11. Telemetry tick
    ============================================================ */
  function tick() {
    const p = P[cur];
    fillZones();

    if (!live() || !RT) {
      ['gV', 'tV', 'hV'].forEach(i => { $(i).textContent = '--'; });
      ring('tR', 0);
      ring('hR', 0);
      hist.fill(0);
      draw('var(--mute,#8a8a8a)', p.e);

      $('gS').style.color = '';
      $('gS').textContent = 'No live data';
      $('aC').textContent = '--';
      $('aF').style.width = '0';
      $('aP').textContent = 'No data';
      $('aT').textContent = 'Waiting for ESP32-S3 AP';
      $('aD').textContent = 'Connect to PostHarvest-Storage-AI Wi-Fi to view live readings.';
      $('aB').classList.remove('warn', 'bad');

      updateFans(null);
      return;
    }

    const cropData = getCropState(p.n);
    if (!cropData) return;

    if (cropData.t === null || cropData.h === null) {
      g = (cropData.e !== null) ? cropData.e : 0;
      $('gV').textContent = (cropData.e !== null) ? g.toFixed(2) : '--';
      $('tV').textContent = '--';
      $('hV').textContent = '--';

      ring('tR', 0);
      ring('hR', 0);
      draw('var(--warn)', p.e);

      $('gS').style.color = 'var(--warn)';
      $('gS').textContent = 'SHT31 Sensor Disconnected (Fail-Safe: Normal)';

      $('aP').textContent = 'Normal (Fail-Safe)';
      $('aT').textContent = 'SHT31 Sensor Not Found';
      $('aD').textContent = 'Check SHT31 wiring. Fan defaulted to Low.';
      $('aB').classList.remove('bad');
      $('aB').classList.add('warn');

      hist.push(g);
      hist.shift();
      updateFans(RT.fans);
      return;
    }

    g = (cropData.e !== null) ? cropData.e : 0;
    const t = cropData.t;
    const h = cropData.h;
    lastT = t;
    lastH = h;

    hist.push(g);
    hist.shift();

    const s = cropData.s;
    const color = s === 2 ? 'var(--bad)' : s === 1 ? 'var(--warn)' : 'var(--ok)';

    $('gV').textContent = (cropData.e !== null) ? g.toFixed(2) : '--';
    $('tV').textContent = t.toFixed(1);
    $('hV').textContent = Math.round(h);

    ring('tR', t / 40 * 100);
    ring('hR', h);
    draw(color, p.e);

    $('gS').style.color = color;
    $('gS').textContent = (cropData.e === null) ? 'Ethylene Sensor Disconnected (Fail-Safe: Normal)'
                        : s === 2 ? 'Critical ethylene / spoilage level'
                        : s === 1 ? 'Warning: Ethylene rising'
                        : 'Ethylene level normal';

    $('aC').textContent = 'Edge ML';
    $('aF').style.width = '100%';

    render(s);
    updateFans(RT.fans);
  }

  ['tR', 'hR'].forEach(i => ring(i, 0));

  /* ============================================================
  12. Sync
    ============================================================ */
  let lastSync = null;

  function doSync(manual) {
    lastSync = new Date();
    const hm = lastSync.toTimeString().slice(0, 8);

    const i = document.querySelector('#syncB svg');
    i.classList.remove('spin');
    void i.getBoundingClientRect();
    i.classList.add('spin');

    if (manual) fetchESPData();

    try {
      localStorage.setItem('lastSync', JSON.stringify({
        t: lastSync.toISOString(), ppm: +g.toFixed(2), crop: P[cur].n
      }));
    } catch (e) {}

    if (!initSync && live()) {
      ev.unshift([
        hm,
        'Sensor telemetry synchronized',
        manual ? 'Readings refreshed from ESP32-S3' : 'Automatic refresh · readings stored locally',
        'ESP32-S3', 'Online', 'p-on', snap()
      ]);
      ev.length = Math.min(ev.length, 50);
      drawLog();
    }
    agoTxt();
  }

  function agoTxt() {
    if (!lastSync) return;
    const s = Math.round((Date.now() - lastSync) / 1000);
    $('ago').textContent = 'Synced ' + t12(lastSync.toTimeString().slice(0, 8)) + ' · ' +
      (s < 5 ? 'just now' : s < 60 ? s + ' s ago' : Math.floor(s / 60) + ' min ago');
  }

  $('syncB').onclick = () => doSync(true);
  $('vb').onclick = () => { showAll = !showAll; drawLog(); };

  /* ============================================================
  13. Boot
    ============================================================ */
  drawLog();
  pick(0);
  tick();
  doSync(false);
  initSync = false;
  setView(true);
  connectToAp();

  setInterval(tick, 2000);
  setInterval(sbSync, 15000);
  setInterval(() => doSync(false), 300000);
  setInterval(agoTxt, 1000);

  const clk = () => { $('sync').textContent = t12(new Date().toTimeString().slice(0, 8)) + ' PST'; };
  clk();
  setInterval(clk, 1000);
