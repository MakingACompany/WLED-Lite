// WLED-Lite — slim user UI.
// Talks to the firmware via /json/si (state read + write) and /ws (live push).
// Reuses /iro.js for the color picker.

(function () {
  'use strict';

  // ---------- state + transport ----------
  let cpick = null;
  let ws = null;
  let wsReady = false;
  let reconnectAttempts = 0;
  let lastColorSendAt = 0;
  let suppressColorPush = false; // when applying remote state, don't echo back
  let effectsLoaded = false;

  const $ = (id) => document.getElementById(id);
  const statusEl = $('status');
  const powerBtn = $('power');
  const powerState = $('power-state');
  const briSlider = $('bri');
  const briVal = $('bri-val');
  const effectsEl = $('effects');
  const timerEl = $('timer');
  const devName = $('dev-name');

  // ---------- API ----------
  function send(payload) {
    if (wsReady && ws.readyState === 1) {
      try { ws.send(JSON.stringify(payload)); return; } catch (e) {}
    }
    fetch('/json/si', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
      keepalive: true
    }).catch(() => {});
  }

  // Debounced send factory: returns a function that throttles to one call per `ms`.
  function debouncer(ms) {
    let t = null, pending = null;
    return (payload) => {
      pending = payload;
      if (t) return;
      t = setTimeout(() => {
        t = null;
        const p = pending; pending = null;
        if (p) send(p);
      }, ms);
    };
  }
  const briSend = debouncer(120);
  const colorSend = debouncer(120);

  // ---------- WebSocket ----------
  function connectWS() {
    try {
      ws = new WebSocket('ws://' + location.host + '/ws');
    } catch (e) {
      scheduleReconnect();
      return;
    }
    ws.onopen = () => {
      wsReady = true;
      reconnectAttempts = 0;
      statusEl.hidden = true;
    };
    ws.onmessage = (e) => {
      if (typeof e.data !== 'string') return; // ignore binary liveview frames
      let msg;
      try { msg = JSON.parse(e.data); } catch (_) { return; }
      if (msg.state) applyState(msg.state);
      if (msg.info && msg.info.name) devName.textContent = msg.info.name;
    };
    ws.onclose = () => {
      wsReady = false;
      scheduleReconnect();
    };
    ws.onerror = () => { try { ws.close(); } catch (_) {} };
  }

  function scheduleReconnect() {
    reconnectAttempts++;
    if (reconnectAttempts > 1) statusEl.hidden = false;
    const delay = Math.min(8000, 500 * Math.pow(1.6, Math.min(reconnectAttempts, 8)));
    setTimeout(connectWS, delay);
  }

  // ---------- Apply remote state -> UI ----------
  function applyState(s) {
    // power
    if (typeof s.on === 'boolean') {
      powerBtn.setAttribute('aria-pressed', String(s.on));
      powerState.textContent = s.on ? 'On' : 'Off';
    }
    // brightness
    if (typeof s.bri === 'number') {
      briSlider.value = s.bri;
      briVal.textContent = Math.round(s.bri / 2.55) + '%';
    }
    // segment 0 = the user-facing segment
    const seg = (s.seg && (s.seg[0] || (Array.isArray(s.seg) === false ? s.seg : null))) || null;
    if (seg) {
      // primary color -> color picker (without echoing back)
      const c0 = seg.col && seg.col[0];
      if (cpick && c0 && c0.length >= 3) {
        suppressColorPush = true;
        try { cpick.color.rgb = { r: c0[0], g: c0[1], b: c0[2] }; } catch (_) {}
        suppressColorPush = false;
      }
      // effect highlight
      if (typeof seg.fx === 'number') {
        const rows = effectsEl.querySelectorAll('.effect');
        rows.forEach((el) => {
          el.classList.toggle('is-on', +el.dataset.id === seg.fx);
        });
      }
    }
    // nightlight = our "turn off in N hours" timer
    if (s.nl) {
      let activeMin = 0;
      if (s.nl.on && typeof s.nl.dur === 'number') activeMin = s.nl.dur;
      timerEl.querySelectorAll('.timer__btn').forEach((b) => {
        b.classList.toggle('is-on', +b.dataset.min === activeMin);
      });
    }
  }

  // ---------- Effects ----------
  function renderEffects(list) {
    effectsEl.innerHTML = '';
    list.forEach((entry, idx) => {
      // Handle both upstream response shapes:
      //   ["Solid","Blink","Reserved",...]
      //   [[0,{name:"Solid"}],...]
      let id = idx, name = '';
      if (typeof entry === 'string') {
        name = entry;
      } else if (Array.isArray(entry) && entry.length >= 2) {
        id = entry[0];
        name = (entry[1] && entry[1].name) || '';
      } else if (entry && entry.name) {
        name = entry.name;
      }
      if (!name) return;
      // Filter out firmware-side _data_RESERVED slots ("RSVD") -- these are
      // effects the WLED_LITE_FX_TRIM keepers list removed from the curated
      // set. Also catch any legacy "Reserved" or "-..." placeholders.
      if (name === 'RSVD' || name === 'Reserved' || name === '-' || name[0] === '-') return;
      // strip @param-spec used by upstream FX data strings (e.g. "Blink@!,Duty cycle;...")
      const at = name.indexOf('@');
      if (at >= 0) name = name.substring(0, at);
      const li = document.createElement('li');
      li.className = 'effect';
      li.dataset.id = String(id);
      li.setAttribute('role', 'option');
      li.tabIndex = 0;
      li.textContent = name;
      li.addEventListener('click', () => selectEffect(id));
      li.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectEffect(id); }
      });
      effectsEl.appendChild(li);
    });
    effectsLoaded = true;
  }

  function selectEffect(id) {
    effectsEl.querySelectorAll('.effect').forEach((el) =>
      el.classList.toggle('is-on', +el.dataset.id === id)
    );
    send({ on: true, seg: { fx: id } });
  }

  async function fetchEffects() {
    try {
      const r = await fetch('/json/eff');
      if (r.ok) {
        const data = await r.json();
        if (Array.isArray(data)) renderEffects(data);
      }
    } catch (_) {}
  }

  // ---------- Timer (nightlight wrapper) ----------
  function wireTimer() {
    timerEl.addEventListener('click', (e) => {
      const btn = e.target.closest('.timer__btn');
      if (!btn) return;
      const min = +btn.dataset.min;
      timerEl.querySelectorAll('.timer__btn').forEach((b) => b.classList.remove('is-on'));
      btn.classList.add('is-on');
      if (min === 0) {
        send({ nl: { on: false } });
      } else {
        // mode 1 = fade out; tbri 0 = target off
        send({ nl: { on: true, dur: min, mode: 1, tbri: 0 } });
      }
    });
  }

  // ---------- Daily schedule ----------
  function wireSchedule() {
    const onIn  = $('sched-on');
    const offIn = $('sched-off');
    const save  = $('sched-save');
    const clear = $('sched-clear');
    const stat  = $('sched-status');
    if (!save || !clear) return;

    const setStatus = (msg, cls) => {
      stat.textContent = msg;
      stat.classList.remove('is-ok', 'is-error');
      if (cls) stat.classList.add(cls);
    };

    const postSchedule = async (body) => {
      try {
        const r = await fetch('/wled-lite/schedule', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(body)
        });
        const data = await r.json().catch(() => ({}));
        if (!r.ok || !data.ok) {
          setStatus(data.error || ('Request failed (' + r.status + ')'), 'is-error');
          return false;
        }
        return true;
      } catch (e) {
        setStatus('Network error — try again.', 'is-error');
        return false;
      }
    };

    save.addEventListener('click', async () => {
      const on  = onIn.value;
      const off = offIn.value;
      if (!/^\d{2}:\d{2}$/.test(on) || !/^\d{2}:\d{2}$/.test(off)) {
        setStatus('Enter both times in HH:MM format.', 'is-error');
        return;
      }
      setStatus('Saving…');
      const ok = await postSchedule({ on, off });
      if (ok) setStatus('Schedule saved. Daily ' + on + ' → ' + off + '.', 'is-ok');
    });

    clear.addEventListener('click', async () => {
      setStatus('Clearing…');
      const ok = await postSchedule({ clear: true });
      if (ok) setStatus('Schedule cleared.', 'is-ok');
    });
  }

  // ---------- Power + Brightness ----------
  function wirePower() {
    powerBtn.addEventListener('click', () => {
      const wasOn = powerBtn.getAttribute('aria-pressed') === 'true';
      send({ on: !wasOn });
    });
  }

  function wireBrightness() {
    briSlider.addEventListener('input', () => {
      const v = +briSlider.value;
      briVal.textContent = Math.round(v / 2.55) + '%';
      briSend({ on: true, bri: v });
    });
  }

  // ---------- Color picker ----------
  function wirePicker() {
    if (typeof iro === 'undefined') return;
    cpick = new iro.ColorPicker('#picker', {
      width: 260,
      layoutDirection: 'horizontal',
      wheelLightness: false,
      wheelAngle: 270,
      wheelDirection: 'clockwise',
      layout: [{ component: iro.ui.Wheel }]
    });
    cpick.on('color:change', (color) => {
      if (suppressColorPush) return;
      const c = color.rgb;
      colorSend({ on: true, seg: { col: [[c.r, c.g, c.b]] } });
    });
  }

  // ---------- Bootstrap ----------
  async function init() {
    wirePower();
    wireBrightness();
    wireTimer();
    wireSchedule();
    wirePicker();

    // Initial state fetch (HTTP, in case WS isn't ready yet)
    try {
      const r = await fetch('/json/si');
      if (r.ok) {
        const json = await r.json();
        if (json.state) applyState(json.state);
        if (json.info && json.info.name) devName.textContent = json.info.name;
      }
    } catch (_) {}

    fetchEffects();
    connectWS();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
