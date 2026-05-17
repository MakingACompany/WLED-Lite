// WLED-Lite — Admin segment editor implementation.
// See wled_lite_segments.h for the API contract.

#include "wled.h"
#include "wled_lite_segments.h"

#ifdef WLED_LITE_SEGMENTS

namespace {

// HTML for the admin page, served inline. Self-contained: pulls /json/state
// to read current segments, lets admin add / edit / delete, then writes back
// via /json/state. No server-side state management here -- WLED's normal
// segment persistence (in cfg.json) handles boot restore.
//
// PIN check: the page serve itself is gated by correctPIN in the route
// handler below. State mutations from the page use the normal /json POST
// path which doesn't require PIN -- safe because the page can only be
// loaded by someone who has the PIN.
//
// Layout matches the Lantern aesthetic from the slim user UI for visual
// consistency, though it lives under /settings/* conceptually.
static const char PAGE_segments[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Segments — WLED-Lite</title>
<style>
:root{
  --bg:#150f0a;--bg-card:#221812;--bg-card-hi:#2c211a;
  --line:rgba(255,184,120,0.08);--line-strong:rgba(255,184,120,0.18);
  --text:#f4e9dd;--text-dim:#b09b87;--text-mute:#7a6957;
  --accent:#ffb878;--accent-soft:#ffd9af;--glow:255,184,120;
  --serif:ui-serif,'Charter','Iowan Old Style',Baskerville,'Times New Roman',serif;
  --sans:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,system-ui,sans-serif;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:radial-gradient(ellipse at 50% -10%,rgba(var(--glow),0.10),transparent 60%),var(--bg);color:var(--text);font-family:var(--sans);font-size:16px;line-height:1.45;padding:24px 18px 32px;-webkit-font-smoothing:antialiased}
.wrap{max-width:480px;margin:0 auto}
h1{font-family:var(--serif);font-weight:500;font-size:24px;letter-spacing:0.01em;margin:6px 0 6px}
h1 em{font-style:italic;color:var(--text-dim);font-weight:400}
p.lead{color:var(--text-dim);font-size:14px;margin:0 0 22px}
.card{background:var(--bg-card);border:1px solid var(--line);border-radius:18px;padding:22px;margin-bottom:14px}
.card__label{display:block;font-family:var(--serif);font-size:12px;text-transform:uppercase;letter-spacing:0.16em;color:var(--text-mute);margin-bottom:14px;font-weight:600}
.segrow{display:grid;grid-template-columns:1fr 70px 70px auto;gap:10px;align-items:center;background:var(--bg-card-hi);border:1px solid var(--line);border-radius:12px;padding:10px 12px;margin-bottom:8px}
.segrow input{appearance:none;-webkit-appearance:none;background:transparent;border:0;color:var(--text);font:inherit;padding:8px 6px;min-width:0;width:100%;border-bottom:1px solid var(--line);outline:none}
.segrow input:focus{border-bottom-color:rgba(var(--glow),0.6)}
.segrow input[type=number]{text-align:right;font-variant-numeric:tabular-nums}
.segrow button{appearance:none;background:transparent;border:1px solid var(--line);border-radius:10px;color:var(--text-mute);font:inherit;cursor:pointer;padding:6px 10px;min-height:36px;transition:color .2s,border-color .2s}
.segrow button:hover{color:#ff9b8a;border-color:#ff9b8a}
.segrow__hint{font-size:11px;color:var(--text-mute);letter-spacing:0.04em;text-align:center;display:block}
.btn{appearance:none;background:var(--bg-card-hi);border:1px solid var(--line);border-radius:12px;color:var(--text-dim);padding:14px 18px;min-height:48px;font-size:14px;font-weight:600;letter-spacing:0.02em;cursor:pointer;font-family:inherit;transition:background .18s,border-color .18s,color .18s;width:100%;margin-bottom:8px}
.btn:hover{color:var(--text);border-color:var(--line-strong)}
.btn--primary{background:linear-gradient(180deg,rgba(var(--glow),0.18),rgba(var(--glow),0.08));border-color:rgba(var(--glow),0.6);color:var(--accent-soft)}
.btn--primary:hover{background:linear-gradient(180deg,rgba(var(--glow),0.24),rgba(var(--glow),0.12))}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
#status{margin:6px 0 0;font-size:13px;color:var(--text-mute);min-height:1.4em}
#status.ok{color:var(--accent-soft)}
#status.err{color:#ff9b8a}
.subhead{font-size:11px;color:var(--text-mute);text-transform:uppercase;letter-spacing:0.16em;margin:0 0 4px;font-weight:600;font-family:var(--serif);display:grid;grid-template-columns:1fr 70px 70px auto;gap:10px;padding:0 12px}
.subhead span:nth-child(2),.subhead span:nth-child(3){text-align:right}
a.back{color:var(--text-dim);text-decoration:none;font-size:13px;border-bottom:1px dotted var(--text-mute);padding-bottom:1px}
a.back:hover{color:var(--accent-soft)}
</style>
</head>
<body><div class="wrap">

<a class="back" href="/settings">&larr; Back to Settings</a>
<h1>Segments <em>&middot; admin</em></h1>
<p class="lead">Define labeled regions of the LED strip. Each segment can be controlled independently from the user UI. Pixel numbers are zero-based: a 30-LED strip uses pixels 0&ndash;29.</p>

<section class="card">
  <label class="card__label">Defined segments</label>
  <div class="subhead"><span>Name</span><span>Start</span><span>Stop</span><span></span></div>
  <div id="segs"></div>
  <button id="add" class="btn">+ Add segment</button>
</section>

<section class="card">
  <div class="row2">
    <button id="cancel" class="btn">Cancel</button>
    <button id="save" class="btn btn--primary">Save</button>
  </div>
  <p id="status" role="status" aria-live="polite"></p>
</section>

</div>
<script>
let segs = [];

async function load() {
  try {
    const r = await fetch('/json/state');
    const j = await r.json();
    segs = (j.seg || []).filter(s => s && (s.stop > s.start)).map(s => ({
      id: s.id,
      n: s.n || '',
      start: s.start | 0,
      stop: s.stop | 0
    }));
    if (segs.length === 0) segs.push({ id: 0, n: '', start: 0, stop: 30 });
  } catch (e) { segs = [{ id: 0, n: '', start: 0, stop: 30 }]; }
  render();
}

function render() {
  const root = document.getElementById('segs');
  root.innerHTML = '';
  segs.forEach((s, i) => {
    const row = document.createElement('div');
    row.className = 'segrow';
    row.innerHTML =
      '<input type="text" placeholder="Name" maxlength="32" value="' + (s.n||'').replace(/"/g,'&quot;') + '" data-k="n">' +
      '<input type="number" min="0" value="' + s.start + '" data-k="start">' +
      '<input type="number" min="1" value="' + s.stop + '" data-k="stop">' +
      '<button data-rm="' + i + '" aria-label="Remove segment">&times;</button>';
    row.querySelectorAll('input').forEach(inp => {
      inp.addEventListener('input', () => {
        const k = inp.dataset.k;
        s[k] = (k === 'n') ? inp.value : (parseInt(inp.value, 10) || 0);
      });
    });
    row.querySelector('button[data-rm]').addEventListener('click', () => {
      segs.splice(i, 1);
      render();
    });
    root.appendChild(row);
  });
}

function setStatus(msg, cls) {
  const s = document.getElementById('status');
  s.textContent = msg;
  s.className = cls || '';
}

document.getElementById('add').addEventListener('click', () => {
  // Pick next available ID (0..15 is reasonable; WLED max is far higher)
  const usedIds = new Set(segs.map(s => s.id));
  let nextId = 0;
  while (usedIds.has(nextId)) nextId++;
  // Default start = max stop of existing segments
  const maxStop = segs.reduce((m, s) => Math.max(m, s.stop), 0);
  segs.push({ id: nextId, n: '', start: maxStop, stop: maxStop + 30 });
  render();
});

document.getElementById('cancel').addEventListener('click', () => {
  window.location = '/settings';
});

document.getElementById('save').addEventListener('click', async () => {
  // Validate
  for (const s of segs) {
    if (!s.n || !s.n.trim()) { setStatus('Every segment needs a name.', 'err'); return; }
    if (s.stop <= s.start) { setStatus('"' + s.n + '": stop must be greater than start.', 'err'); return; }
  }
  // Names should be unique-ish to make the user-UI pills unambiguous
  const names = segs.map(s => s.n.trim().toLowerCase());
  if (new Set(names).size !== names.length) { setStatus('Two segments share a name — they need to differ.', 'err'); return; }

  setStatus('Saving…');

  // Build the seg array. For removed indices, we send entries with start=stop=0
  // which deletes the segment in WLED. We need to know which IDs existed before
  // so we can delete the rest. Easiest: pad up to current count with zero-stop
  // entries for IDs not in our keepers.
  try {
    const before = await (await fetch('/json/state')).json();
    const existingIds = new Set(((before.seg || []).filter(s => s && (s.stop > s.start))).map(s => s.id));
    const newIds = new Set(segs.map(s => s.id));
    const payload = { seg: [] };
    // Push keeper segments
    for (const s of segs) payload.seg.push({ id: s.id, n: s.n.trim(), start: s.start, stop: s.stop, len: s.stop - s.start });
    // Push delete markers for previously-existing IDs that aren't in the new set
    for (const id of existingIds) if (!newIds.has(id)) payload.seg.push({ id, stop: 0 });

    const r = await fetch('/json/state', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
    if (!r.ok) { setStatus('Save failed (' + r.status + ').', 'err'); return; }
    setStatus('Saved. Segments active. Returning to settings…', 'ok');
    setTimeout(() => { window.location = '/settings'; }, 900);
  } catch (e) {
    setStatus('Network error — try again.', 'err');
  }
});

load();
</script>
</body>
</html>
)HTML";

void handleGet(AsyncWebServerRequest *request) {
  // PIN-gate: only serve to admin who has entered the PIN (or factory state).
  if (strlen(settingsPIN) > 0 && !correctPIN) {
    request->redirect("/settings");
    return;
  }
  AsyncWebServerResponse *resp = request->beginResponse_P(200, "text/html", (const uint8_t*)PAGE_segments, strlen_P(PAGE_segments));
  resp->addHeader("Cache-Control", "no-store");
  request->send(resp);
}

} // namespace

namespace WLEDLiteSegments {

void registerEndpoint(AsyncWebServer &server) {
  // NOT registering as /settings/segments to avoid colliding with upstream's
  // serveSettings() URL matcher. The settings.htm hub gets a link pointing
  // at this URL instead.
  server.on("/wled-lite/segments", HTTP_GET, handleGet);
}

} // namespace WLEDLiteSegments

#endif // WLED_LITE_SEGMENTS
