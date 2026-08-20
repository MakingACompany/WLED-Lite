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
  // Segment routing. `null` = apply to all segments. Otherwise the target
  // segment id (number). Discovered from /json/state on load + every push.
  let segments = [];     // [{id, n}], filtered to entries with stop > start
  let activeSegId = null;
  // When the segment count crosses NEST_THRESHOLD, the row collapses into
  // [All] [Letters], with individual pills shown only when the user has
  // drilled into "Letters" or selected a specific letter.
  let expandedLetters = false;
  const NEST_THRESHOLD = 5;

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
  // Rewrites the `seg` field of a payload so the change targets the segment
  // the user has selected via the segment pill (or all segments when "All"
  // is selected). Brightness `bri` stays global -- it's a per-device value
  // in WLED's model, not per-segment.
  function routedPayload(payload) {
    if (!payload || !payload.seg) return payload;
    if (segments.length < 2) return payload;        // single-segment device: leave it alone
    const segPatch = payload.seg;
    if (activeSegId === null) {
      // "All" -> broadcast to every known segment id
      payload.seg = segments.map(s => Object.assign({ id: s.id }, segPatch));
    } else {
      payload.seg = [Object.assign({ id: activeSegId }, segPatch)];
    }
    return payload;
  }

  function send(payload) {
    payload = routedPayload(payload);
    if (wsReady && ws.readyState === 1) {
      try { ws.send(JSON.stringify(payload)); return; } catch (e) {}
    }
    return fetch('/json/si', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
      keepalive: true
    }).catch(() => {});
  }

  // Debounced send factory: returns a function that throttles to one call per `ms`.
  function debouncer(ms) {
    let t = null, pending = null;
    const fn = (payload) => {
      pending = payload;
      if (t) return;
      t = setTimeout(() => {
        t = null;
        const p = pending; pending = null;
        if (p) send(p);
      }, ms);
    };
    // Sends whatever's pending right now instead of waiting out the rest of
    // the delay -- callers that are about to snapshot live state (preset
    // save/update) need the device to have already applied the latest
    // slider value, not whatever it had `ms` ago.
    fn.flush = () => {
      if (t) { clearTimeout(t); t = null; }
      const p = pending; pending = null;
      return p ? send(p) : undefined;
    };
    return fn;
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
    // Discover segments and (re)render the selector if needed.
    if (Array.isArray(s.seg)) {
      const next = s.seg
        .filter(x => x && typeof x.id === 'number' && (x.stop > x.start))
        .map(x => ({ id: x.id, n: (x.n && x.n.trim()) || ('Segment ' + (x.id + 1)) }));
      // Re-render only if the segment set actually changed (avoid layout thrash)
      const sig = next.map(x => x.id + ':' + x.n).join('|');
      if (sig !== segments.map(x => x.id + ':' + x.n).join('|')) {
        segments = next;
        renderSegPills();
      }
    }
    // Pick which segment's state drives the displayed color / effect.
    const activeSeg = (activeSegId === null
      ? (s.seg && s.seg[0])
      : (Array.isArray(s.seg) ? s.seg.find(x => x.id === activeSegId) : null));
    const seg = activeSeg || (Array.isArray(s.seg) ? s.seg[0] : s.seg) || null;
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

  // ---------- Segment pills ----------
  // Render a segment name with trailing digits as a visual subscript.
  // "B1" -> "B<sub>1</sub>", "Top12" -> "Top<sub>12</sub>". Names without a
  // letter+digit suffix ("L", "1B", "Top") render as plain escaped text.
  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => (
      { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]
    ));
  }
  function formatSegLabel(name) {
    const m = String(name).match(/^(.*?\D)(\d+)$/);
    if (m) return escapeHtml(m[1]) + '<sub>' + escapeHtml(m[2]) + '</sub>';
    return escapeHtml(name);
  }

  function refetchState() {
    fetch('/json/si').then(r => r.json())
      .then(j => j && j.state && applyState(j.state))
      .catch(() => {});
  }

  function renderSegPills() {
    const card   = $('seg-card');
    const top    = $('seg-pills-top');
    const expand = $('seg-pills-expand');
    if (!card || !top || !expand) return;

    if (segments.length < 2) {
      card.hidden = true;
      activeSegId = null;
      expandedLetters = false;
      expand.hidden = true;
      return;
    }
    card.hidden = false;
    if (activeSegId !== null && !segments.find(s => s.id === activeSegId)) {
      activeSegId = null;
    }

    const nested      = segments.length >= NEST_THRESHOLD;
    const lettersOpen = nested && (expandedLetters || activeSegId !== null);

    top.innerHTML    = '';
    expand.innerHTML = '';

    const mkPill = (parent, opts) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'seg-pill' + (opts.on ? ' is-on' : '');
      b.dataset.segid = opts.key;
      if (opts.html) b.innerHTML = opts.html;
      else b.textContent = opts.label;
      b.setAttribute('role', 'tab');
      b.setAttribute('aria-selected', String(!!opts.on));
      if (opts.ariaExpanded !== undefined) {
        b.setAttribute('aria-expanded', String(opts.ariaExpanded));
      }
      b.addEventListener('click', opts.onClick);
      parent.appendChild(b);
    };

    // "All" -- always visible. Tapping it returns to global control and
    // collapses the letters drawer.
    mkPill(top, {
      key: 'all',
      label: 'All',
      on: activeSegId === null,
      onClick: () => {
        activeSegId = null;
        expandedLetters = false;
        renderSegPills();
        refetchState();
      }
    });

    if (nested) {
      // "Letters" toggle pill. Highlights when a specific letter is active
      // (or the drawer is manually open). Tapping it drills in.
      mkPill(top, {
        key: 'letters',
        html: 'Letters ' + (lettersOpen ? '▾' : '▸'),
        on: lettersOpen,
        ariaExpanded: lettersOpen,
        onClick: () => {
          if (activeSegId !== null) {
            // Already drilled into a specific letter. Tap collapses + resets.
            activeSegId = null;
            expandedLetters = false;
            refetchState();
          } else {
            expandedLetters = !expandedLetters;
          }
          renderSegPills();
        }
      });
      expand.hidden = !lettersOpen;
      if (lettersOpen) {
        segments.forEach(s => mkPill(expand, {
          key: String(s.id),
          html: formatSegLabel(s.n),
          on: s.id === activeSegId,
          onClick: () => {
            activeSegId = s.id;
            expandedLetters = true;
            renderSegPills();
            refetchState();
          }
        }));
      }
    } else {
      expand.hidden = true;
      segments.forEach(s => mkPill(top, {
        key: String(s.id),
        html: formatSegLabel(s.n),
        on: s.id === activeSegId,
        onClick: () => {
          activeSegId = s.id;
          renderSegPills();
          refetchState();
        }
      }));
    }
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
    if (!powerBtn) return;
    powerBtn.addEventListener('click', () => {
      const wasOn = powerBtn.getAttribute('aria-pressed') === 'true';
      send({ on: !wasOn });
    });
  }

  function wireBrightness() {
    if (!briSlider) return;
    briSlider.addEventListener('input', () => {
      const v = +briSlider.value;
      briVal.textContent = Math.round(v / 2.55) + '%';
      briSend({ on: true, bri: v });
    });
  }

  // ---------- Color picker ----------
  function wirePicker() {
    if (typeof iro === 'undefined' || !$('picker')) return;
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

  // ---------- Effect Tuning (Speed & Intensity) ----------
  const fxSpeed = $('fx-speed');
  const fxSpeedVal = $('fx-speed-val');
  const fxIntensity = $('fx-intensity');
  const fxIntensityVal = $('fx-intensity-val');

  const fxSpeedSend = debouncer(120);
  const fxIntensitySend = debouncer(120);

  function wireFxControls() {
    if (fxSpeed) {
      fxSpeed.addEventListener('input', () => {
        const v = +fxSpeed.value;
        fxSpeedVal.textContent = Math.round(v / 2.55) + '%';
        fxSpeedSend({ seg: { sx: v } });
      });
    }
    if (fxIntensity) {
      fxIntensity.addEventListener('input', () => {
        const v = +fxIntensity.value;
        fxIntensityVal.textContent = Math.round(v / 2.55) + '%';
        fxIntensitySend({ seg: { ix: v } });
      });
    }
  }

  // ---------- Palettes ----------
  const palettesEl = $('palettes');
  const CURATED_PALETTES = [
    [0, 'Default'],
    [2, 'Rainbow'],
    [3, 'Sunset'],
    [4, 'Sunset 2'],
    [5, 'Beach'],
    [6, 'Sunburst'],
    [11, 'Breeze'],
    [35, 'Ocean'],
    [36, 'Forest'],
    [49, 'Party'],
    [52, 'Aurora'],
    [54, 'Fire'],
    [56, 'Sakura'],
    [60, 'Neon']
  ];

  function renderPalettes() {
    if (!palettesEl) return;
    palettesEl.innerHTML = '';
    CURATED_PALETTES.forEach(([id, name]) => {
      const li = document.createElement('li');
      li.className = 'effect';
      li.dataset.id = String(id);
      li.setAttribute('role', 'option');
      li.tabIndex = 0;
      li.textContent = name;
      li.addEventListener('click', () => selectPalette(id));
      palettesEl.appendChild(li);
    });
  }

  function selectPalette(id) {
    if (!palettesEl) return;
    palettesEl.querySelectorAll('.effect').forEach((el) =>
      el.classList.toggle('is-on', +el.dataset.id === id)
    );
    send({ seg: { pal: id } });
  }

  // ---------- Presets & Boot Preset ----------
  const presetPills = $('preset-pills');
  const presetNameIn = $('preset-name');
  const presetSaveBtn = $('preset-save');
  const presetUpdateBtn = $('preset-update');
  let currentPresets = {};
  let activePresetId = 0;

  async function fetchPresets() {
    try {
      const r = await fetch('/presets.json');
      if (r.ok) {
        currentPresets = await r.json();
        renderPresets();
      }
    } catch (_) {}
  }

  function renderPresets() {
    if (!presetPills) return;
    presetPills.innerHTML = '';

    // Sort preset keys numerically (1, 2, 3...) so cycle order is crystal clear!
    const validKeys = Object.keys(currentPresets)
      .map(k => parseInt(k, 10))
      .filter(k => !isNaN(k) && k > 0 && currentPresets[k] && typeof currentPresets[k] === 'object' && currentPresets[k].n)
      .sort((a, b) => a - b);

    if (validKeys.length === 0) {
      presetPills.innerHTML = '<span style="font-size:13px; color:var(--text-mute);">No saved presets yet — device will start Off/Black</span>';
      if (presetUpdateBtn) presetUpdateBtn.hidden = true;
      return;
    }

    validKeys.forEach((id, index) => {
      const p = currentPresets[id];
      const name = p.n || ('Preset ' + id);

      // Pill container
      const isDefault = (index === 0);
      const pill = document.createElement('div');
      pill.className = 'preset-pill' + (id === activePresetId ? ' is-on' : '') + (isDefault ? ' is-default' : '');
      if (isDefault) pill.title = 'Default startup preset';
      pill.dataset.id = String(id);

      // Move Up badge (if not first)
      if (index > 0) {
        const upBtn = document.createElement('span');
        upBtn.className = 'preset-pill__del';
        upBtn.textContent = '▲';
        upBtn.title = 'Move up in cycle order';
        upBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          swapPresets(validKeys[index - 1], id);
        });
        pill.appendChild(upBtn);
      }

      // Pill text (click to select/apply)
      const labelSpan = document.createElement('span');
      labelSpan.textContent = (index + 1) + '. ' + name;
      labelSpan.style.cursor = 'pointer';
      labelSpan.addEventListener('click', () => {
        selectPreset(id, name);
      });
      pill.appendChild(labelSpan);

      // Move Down badge (if not last)
      if (index < validKeys.length - 1) {
        const downBtn = document.createElement('span');
        downBtn.className = 'preset-pill__del';
        downBtn.textContent = '▼';
        downBtn.title = 'Move down in cycle order';
        downBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          swapPresets(id, validKeys[index + 1]);
        });
        pill.appendChild(downBtn);
      }

      // Delete X badge
      const delBadge = document.createElement('span');
      delBadge.className = 'preset-pill__del';
      delBadge.textContent = '✕';
      delBadge.title = 'Delete preset';
      delBadge.addEventListener('click', (e) => {
        e.stopPropagation();
        deletePreset(id);
      });
      pill.appendChild(delBadge);

      presetPills.appendChild(pill);
    });
  }

  async function swapPresets(idA, idB) {
    const pA = currentPresets[idA];
    const pB = currentPresets[idB];
    if (!pA || !pB) return;

    // 1. Immediately swap in local memory & re-render UI for instant smooth feedback
    const cloneA = JSON.parse(JSON.stringify(pA));
    const cloneB = JSON.parse(JSON.stringify(pB));
    currentPresets[idA] = cloneB;
    currentPresets[idB] = cloneA;
    renderPresets();

    // 2. Sequentially send psave commands with a delay so LittleFS flash locks don't collide
    const payloadA = Object.assign({}, cloneA, { psave: idB, ib: true, sb: true });
    const payloadB = Object.assign({}, cloneB, { psave: idA, ib: true, sb: true });

    try {
      await fetch('/json/si', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payloadA)
      });
      await new Promise(r => setTimeout(r, 450));

      await fetch('/json/si', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payloadB)
      });
      await new Promise(r => setTimeout(r, 450));
    } catch (_) {}

    fetchPresets();
  }

  function selectPreset(id, name) {
    activePresetId = id;
    if (presetNameIn) presetNameIn.value = name || '';
    if (presetUpdateBtn) presetUpdateBtn.hidden = false;

    presetPills.querySelectorAll('.preset-pill').forEach(el => {
      el.classList.toggle('is-on', +el.dataset.id === id);
    });

    send({ ps: id });
  }

  function deletePreset(id) {
    if (activePresetId === id) {
      activePresetId = 0;
      if (presetNameIn) presetNameIn.value = '';
      if (presetUpdateBtn) presetUpdateBtn.hidden = true;
    }
    send({ pdel: id });
    setTimeout(fetchPresets, 400);
    setTimeout(fetchPresets, 1200);
  }

  // Slider/color changes are debounced (see debouncer() above) so dragging
  // doesn't flood the device with requests -- but that means right after a
  // drag there can still be a pending send the device hasn't applied yet.
  // Saving a preset snapshots the device's *current* live state, so any
  // pending send has to land first or the preset captures a stale value
  // (exactly the "Update doesn't stick" symptom).
  async function flushPendingSliders() {
    await Promise.all(
      [briSend, colorSend, fxSpeedSend, fxIntensitySend].map((d) => Promise.resolve(d.flush()))
    );
  }

  function wirePresets() {
    if (presetSaveBtn && presetNameIn) {
      // Save New Preset (assign next numeric ID)
      presetSaveBtn.addEventListener('click', async () => {
        const name = presetNameIn.value.trim() || 'Custom Preset';
        let nextId = 1;
        for (let i = 1; i <= 250; i++) {
          if (!currentPresets[i] || !currentPresets[i].n) {
            nextId = i;
            break;
          }
        }
        await flushPendingSliders();
        send({ psave: nextId, n: name, ib: true, sb: true });
        presetNameIn.value = '';
        activePresetId = nextId;
        setTimeout(fetchPresets, 400);
        setTimeout(fetchPresets, 1200);
      });
    }

    if (presetUpdateBtn && presetNameIn) {
      // Overwrite/Update Active Preset
      presetUpdateBtn.addEventListener('click', async () => {
        if (!activePresetId) return;
        const name = presetNameIn.value.trim() || ('Preset ' + activePresetId);
        await flushPendingSliders();
        send({ psave: activePresetId, n: name, ib: true, sb: true });
        setTimeout(fetchPresets, 400);
        setTimeout(fetchPresets, 1200);
      });
    }
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
    // boot preset
    if (s.def && typeof s.def.ps === 'number') {
      activeBootPreset = s.def.ps;
      if (bootPresetSel) bootPresetSel.value = String(s.def.ps);
    }
    // active preset
    if (typeof s.ps === 'number' && s.ps > 0) {
      activePresetId = s.ps;
      if (presetPills) {
        presetPills.querySelectorAll('.preset-pill').forEach((el) => {
          el.classList.toggle('is-on', +el.dataset.id === s.ps);
        });
      }
    }
    // Discover segments and (re)render the selector if needed.
    if (Array.isArray(s.seg)) {
      const next = s.seg
        .filter(x => x && typeof x.id === 'number' && (x.stop > x.start))
        .map(x => ({ id: x.id, n: (x.n && x.n.trim()) || ('Segment ' + (x.id + 1)) }));
      const sig = next.map(x => x.id + ':' + x.n).join('|');
      if (sig !== segments.map(x => x.id + ':' + x.n).join('|')) {
        segments = next;
        renderSegPills();
      }
    }
    // Pick which segment's state drives displayed color / effect / speed / intensity / palette.
    const activeSeg = (activeSegId === null
      ? (s.seg && s.seg[0])
      : (Array.isArray(s.seg) ? s.seg.find(x => x.id === activeSegId) : null));
    const seg = activeSeg || (Array.isArray(s.seg) ? s.seg[0] : s.seg) || null;
    if (seg) {
      // primary color
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
      // speed & intensity
      if (typeof seg.sx === 'number' && fxSpeed) {
        fxSpeed.value = seg.sx;
        fxSpeedVal.textContent = Math.round(seg.sx / 2.55) + '%';
      }
      if (typeof seg.ix === 'number' && fxIntensity) {
        fxIntensity.value = seg.ix;
        fxIntensityVal.textContent = Math.round(seg.ix / 2.55) + '%';
      }
      // palette highlight
      if (typeof seg.pal === 'number' && palettesEl) {
        palettesEl.querySelectorAll('.effect').forEach((el) => {
          el.classList.toggle('is-on', +el.dataset.id === seg.pal);
        });
      }
    }
    // nightlight
    if (s.nl) {
      let activeMin = 0;
      if (s.nl.on && typeof s.nl.dur === 'number') activeMin = s.nl.dur;
      timerEl.querySelectorAll('.timer__btn').forEach((b) => {
        b.classList.toggle('is-on', +b.dataset.min === activeMin);
      });
    }
  }

  // ---------- Bootstrap ----------
  async function init() {
    wirePower();
    wireBrightness();
    wireFxControls();
    renderPalettes();
    wirePresets();
    wireTimer();
    wireSchedule();
    wirePicker();

    // Initial state fetch
    try {
      const r = await fetch('/json/si');
      if (r.ok) {
        const json = await r.json();
        if (json.state) applyState(json.state);
        if (json.info && json.info.name) devName.textContent = json.info.name;
      }
    } catch (_) {}

    fetchEffects();
    fetchPresets();
    connectWS();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
