/* ==========================================
   1. CAMERA & CONTROLS LOGIC
   ========================================== */
const els = {
  ip: document.getElementById("ipInput"), connect: document.getElementById("connectBtn"),
  disconnect: document.getElementById("disconnectBtn"), snapshot: document.getElementById("snapshotBtn"),
  fullscreen: document.getElementById("fullscreenBtn"), togglePanel: document.getElementById("togglePanelBtn"),
  stream: document.getElementById("stream"), placeholder: document.getElementById("placeholder"),
  badge: document.getElementById("statusBadge"), streamHint: document.getElementById("streamUrlHint"),
  panel: document.getElementById("controlPanel"), viewer: document.getElementById("viewer"),
};
const state = { ip: "", connected: false, pollTimer: null, streamLive: false };

function baseUrl() { return `http://${state.ip}`; }
function streamUrl() { return `http://${state.ip}:81/stream?t=${Date.now()}`; }
function setBadge(text, kind) {
  els.badge.textContent = text;
  els.badge.className = "badge " + (kind === "live" ? "badge-live" : kind === "error" ? "badge-error" : "badge-idle");
}
function setConnectedUI(on) {
  if (els.connect) els.connect.disabled = on;
  if (els.disconnect) els.disconnect.disabled = !on;
  if (els.snapshot) els.snapshot.disabled = !on;
  if (els.fullscreen) els.fullscreen.disabled = !on;
}
async function fetchStatus() { const r = await fetch(`${baseUrl()}/status`, { cache: "no-store" }); return r.json(); }
async function setControl(name, val) { await fetch(`${baseUrl()}/control?var=${encodeURIComponent(name)}&val=${encodeURIComponent(val)}`, { cache: "no-store" }); }
function setSelect(id, val) { const el = document.getElementById(id); if (el) el.value = String(val); }
function setRange(id, val) { const el = document.getElementById(id); if (!el) return; el.value = val; const label = document.getElementById(id + "Val"); if (label) label.textContent = val; }
function setCheckbox(id, val) { const el = document.getElementById(id); if (el) el.checked = !!val; }
function populateUI(s) {
  setSelect("framesize", s.framesize); setRange("quality", s.quality); setRange("brightness", s.brightness);
  setRange("contrast", s.contrast); setRange("saturation", s.saturation); setSelect("special_effect", s.special_effect);
  setSelect("wb_mode", s.wb_mode); setSelect("gainceiling", s.gainceiling); setCheckbox("hmirror", s.hmirror);
  setCheckbox("vflip", s.vflip); setCheckbox("awb", s.awb); setCheckbox("aec", s.aec); setCheckbox("agc", s.agc);
  setCheckbox("dcw", s.dcw); setCheckbox("colorbar", s.colorbar);
}
async function connectCamera() {
  const ip = els.ip.value.trim();
  if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip)) { setBadge("Invalid IP", "error"); return; }
  state.ip = ip; state.connected = true; state.streamLive = false;
  setBadge("Connecting…", "idle"); if (els.placeholder) els.placeholder.style.display = "none";
  if (els.streamHint) els.streamHint.textContent = streamUrl(); setConnectedUI(true);
  if (els.stream) {
    els.stream.onload = () => { state.streamLive = true; setBadge("Live", "live"); };
    els.stream.onerror = () => { state.streamLive = false; setBadge("Stream lost", "error"); };
    els.stream.src = streamUrl();
  }
  try { populateUI(await fetchStatus()); } catch (e) { console.warn("status fetch failed", e); }
  clearInterval(state.pollTimer);
  state.pollTimer = setInterval(async () => { if (!state.connected) return; try { await fetchStatus(); } catch {} }, 5000);
}
function disconnectCamera() {
  clearInterval(state.pollTimer); state.pollTimer = null; state.connected = false; state.streamLive = false;
  if (els.stream) { els.stream.onload = null; els.stream.onerror = null; els.stream.removeAttribute("src"); els.stream.src = ""; }
  if (els.placeholder) els.placeholder.style.display = ""; if (els.streamHint) els.streamHint.textContent = "http://<ip>:81/stream";
  setBadge("Idle", "idle"); setConnectedUI(false);
}
async function takeSnapshot() {
  try {
    const r = await fetch(`${baseUrl()}/capture`, { cache: "no-store" }); const blob = await r.blob();
    const url = URL.createObjectURL(blob); const a = document.createElement("a");
    a.href = url; a.download = `esp32-${new Date().toISOString().replace(/[:.]/g, "-")}.jpg`;
    document.body.appendChild(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  } catch { setBadge("Snapshot failed", "error"); }
}
function goFullscreen() { if (document.fullscreenElement) document.exitFullscreen(); else if (els.viewer) els.viewer.requestFullscreen?.(); }
function bindCameraControls() {
  if (els.connect) els.connect.addEventListener("click", connectCamera);
  if (els.disconnect) els.disconnect.addEventListener("click", disconnectCamera);
  if (els.snapshot) els.snapshot.addEventListener("click", takeSnapshot);
  if (els.fullscreen) els.fullscreen.addEventListener("click", goFullscreen);
  if (els.ip) els.ip.addEventListener("keydown", (e) => { if (e.key === "Enter") connectCamera(); });
  if (els.togglePanel && els.panel) els.togglePanel.addEventListener("click", () => { els.panel.classList.toggle("hidden"); els.togglePanel.textContent = els.panel.classList.contains("hidden") ? "Show Controls" : "Hide Controls"; });
  ["framesize", "special_effect", "wb_mode", "gainceiling"].forEach((id) => { const el = document.getElementById(id); if (el) el.addEventListener("change", async () => { if (state.connected) await setControl(id, el.value); }); });
  ["quality", "brightness", "contrast", "saturation"].forEach((id) => {
    const el = document.getElementById(id); const label = document.getElementById(id + "Val");
    if (el) { el.addEventListener("input", () => { if (label) label.textContent = el.value; }); el.addEventListener("change", async () => { if (state.connected) await setControl(id, el.value); }); }
  });
  ["hmirror", "vflip", "awb", "aec", "agc", "dcw", "colorbar"].forEach((id) => { const el = document.getElementById(id); if (el) el.addEventListener("change", async () => { if (state.connected) await setControl(id, el.checked ? 1 : 0); }); });
}

/* ==========================================
   2. XIAOZHI AI VOICE CHAT LOGIC
   ========================================== */
let aiWs = null;
let audioContext = null;
let mediaStream = null;
let voiceAiBound = false;

function logToChat(sender, text) {
  const aiChatLog = document.getElementById("aiChatLog");
  if (!aiChatLog) return;
  const div = document.createElement("div");
  div.style.marginBottom = "5px";
  div.innerHTML = `<strong style="color: ${sender === 'AI' ? '#3ea6ff' : '#31d67b'};">[${sender}]:</strong> ${text}`;
  aiChatLog.appendChild(div);
  aiChatLog.scrollTop = aiChatLog.scrollHeight;
}

async function connectVoiceAI() {
  if (aiWs) return;
  const aiStatusEl = document.getElementById("aiStatus");
  const startAiBtn = document.getElementById("startAiBtn");
  const stopAiBtn = document.getElementById("stopAiBtn");

  if (aiStatusEl) aiStatusEl.textContent = "Status: Connecting...";
  aiWs = new WebSocket('ws://localhost:8081');

  aiWs.onopen = async () => {
    console.log("WebSocket connected to bridge.");
    if (startAiBtn) startAiBtn.disabled = true;
    if (stopAiBtn) stopAiBtn.disabled = false;

    try {
      mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
      audioContext = new (window.AudioContext || window.webkitAudioContext)();
      logToChat("System", "Microphone active.");

      // CORRECTED HELLO PAYLOAD (includes required features block)
      const helloMessage = {
        "type": "hello",
        "version": 1,
        "features": {
          "mcp": true,
          "aec": true
        },
        "transport": "websocket",
        "audio_params": {
          "format": "opus",
          "sample_rate": 16000,
          "channels": 1,
          "frame_duration": 60
        }
      };
      aiWs.send(JSON.stringify(helloMessage));
      console.log("Hello message sent. Waiting for server session_id...");
      if (aiStatusEl) aiStatusEl.textContent = "Status: Waiting for server handshake...";
      
    } catch (err) {
      console.error("Microphone error:", err);
      if (err.name === 'NotAllowedError' || err.name === 'NotFoundError') {
        logToChat("System", "Microphone access denied.");
      } else {
        logToChat("System", "Error during handshake.");
      }
    }
  };

  aiWs.onmessage = (event) => {
    if (event.data instanceof Blob) {
      // Audio data from AI (Opus stream)
    } else {
      try {
        const msg = JSON.parse(event.data);
        console.log("Message from AI:", msg);
        
        // WAIT for server's hello response before sending listen
        if (msg.type === "hello" && msg.transport === "websocket") {
          console.log("Server acknowledged hello. Session ID:", msg.session_id);
          if (aiStatusEl) aiStatusEl.textContent = "Status: Connected! Listening...";
          
          // CORRECTED LISTEN MESSAGE (includes state and mode)
          const listenMessage = {
            "session_id": msg.session_id,
            "type": "listen",
            "state": "start",
            "mode": "manual"
          };
          aiWs.send(JSON.stringify(listenMessage));
          console.log("Listen command sent.");
        }
        
        if (msg.type === "text" && msg.text) logToChat("AI", msg.text);
        if (msg.type === "stt" && msg.text) logToChat("You", msg.text);
        if (msg.type === "llm" && msg.text) logToChat("AI", msg.text);
      } catch (e) { console.log("Raw message:", event.data); }
    }
  };

  aiWs.onclose = () => {
    console.log("Voice AI disconnected.");
    if (aiStatusEl) aiStatusEl.textContent = "Status: Disconnected";
    if (startAiBtn) startAiBtn.disabled = false;
    if (stopAiBtn) stopAiBtn.disabled = true;
    aiWs = null;
    disconnectVoiceAI();
  };

  aiWs.onerror = (err) => {
    console.error("WebSocket Error:", err);
    if (aiStatusEl) aiStatusEl.textContent = "Status: Error connecting to bridge.";
  };
}

function disconnectVoiceAI() {
  if (mediaStream) { mediaStream.getTracks().forEach(track => track.stop()); mediaStream = null; }
  if (audioContext) { audioContext.close(); audioContext = null; }
  if (aiWs) { aiWs.close(); aiWs = null; }
  
  const aiStatusEl = document.getElementById("aiStatus");
  const startAiBtn = document.getElementById("startAiBtn");
  const stopAiBtn = document.getElementById("stopAiBtn");
  if (aiStatusEl) aiStatusEl.textContent = "Status: Disconnected";
  if (startAiBtn) startAiBtn.disabled = false;
  if (stopAiBtn) stopAiBtn.disabled = true;
}

/* ==========================================
   3. INITIALIZATION
   ========================================== */
function init() {
  bindCameraControls();
  setConnectedUI(false);
  const savedIp = localStorage.getItem("esp32.ip");
  if (savedIp && els.ip) els.ip.value = savedIp;
  if (els.ip) els.ip.addEventListener("change", () => { localStorage.setItem("esp32.ip", els.ip.value.trim()); });

  if (voiceAiBound) return;
  voiceAiBound = true;

  const startAiBtn = document.getElementById("startAiBtn");
  const stopAiBtn = document.getElementById("stopAiBtn");
  if (startAiBtn) startAiBtn.addEventListener("click", connectVoiceAI);
  if (stopAiBtn) stopAiBtn.addEventListener("click", disconnectVoiceAI);
}

document.addEventListener("DOMContentLoaded", init);