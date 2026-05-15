# WLED-Lite — Role-Based UI Split Design

Task #5 of the [project plan](WLED-LITE-PLAN.md). Sketches the user-vs-admin partition of the web UI and the auth model that gates the admin surface. **No code changes in this pass** — implementation phases land in Tasks #6+ (effect curation), and a dedicated UI-replacement commit later.

## Decision summary

| Decision | Choice | Why |
|---|---|---|
| **Auth model** | Reuse upstream's `settingsPIN` | Already gates `/settings`, `/edit`, `/update`, `/json?cfg`, `/upload`. Zero new auth code. Lowest upstream-merge friction. The end user never sees a login prompt because they only use the simplified `/index` page, and the state-mutation endpoints don't require PIN. |
| **Default PIN** | Maintainer sets during AP-mode onboarding | Extend `welcome.htm` with a "Set admin PIN" step. The maintainer configures each device before shipping it. The PIN is per-device, never written to source, never travels in a public binary. |
| **User UI implementation** | Replace `index.htm` with a slim user UI | Upstream's `index.htm` (~390 lines) is going to conflict on every merge anyway because we'd be trimming controls from it. Easier to maintain a clean slim version than weave conditionals into a moving target. |
| **Admin UI** | Keep upstream's `/settings/*` surface as-is | The settings sub-pages are already PIN-gated and admin-shaped. We just hide / remove pages that no longer apply (e.g. 2D, pixart) — many of those are already removed by the Task #4 feature-flag deltas. |

---

## Upstream UI surface (inherited)

Captured from `wled_server.cpp`, `wled00/data/`, and `json.cpp` during this audit. See [`feature-flag-audit.md`](feature-flag-audit.md) for which flags gate which routes.

### HTTP routes

| Route | Purpose | Auth | WLED-Lite scope |
|---|---|---|---|
| `/` | Main UI (`index.htm`) | none | **USER** — replace with slim version |
| `/welcome` | First-run wizard | none | **USER** — extend with PIN-set step |
| `/liveview` | LED preview canvas | none | **ADMIN** *(curiosity feature; hide from user UI)* |
| `/settings` (+ `?p=N`) | Settings hub & sub-pages | PIN-gated | **ADMIN** |
| `/edit` | Filesystem browser | PIN-gated | **ADMIN** |
| `/update` | OTA upload | PIN + `otaLock` | **ADMIN** |
| `/u` | Usermod admin page | none, but only built with `WLED_ENABLE_USERMOD_PAGE` | **ADMIN** |
| `/dmxmap` | DMX channel map | none, gated by `WLED_ENABLE_DMX` | **ADMIN** |
| `/json` (GET) | Read state/info | none | **PUBLIC** (used by both UIs) |
| `/json` (POST state) | Update LEDs (on/off/bri/color/effect) | none | **PUBLIC** (used by user UI) |
| `/json?cfg` (POST) | Update config | PIN-gated | **ADMIN** (used by settings forms) |
| `/json/live` | Stream LED state | none | **PUBLIC** |
| `/version`, `/uptime`, `/freeheap` | Diagnostics | none | **ADMIN** *(harmless but not user-facing)* |
| `/reset` | Reboot | none | **ADMIN** *(consider adding PIN check — see open issues)* |
| `/pixart.htm`, `/pxmagic.htm`, `/pixelforge.htm` | 2D editors | feature-gated | **REMOVED** (Task #4 flag deltas already off) |
| `/liveview2D` | 2D preview | `!WLED_DISABLE_2D` | **REMOVED** (Task #4) |
| `/cpal.htm` | Palette designer | none | **ADMIN** *(useful for maintainer; not for end user)* |

### Settings sub-pages (`/settings?p=N`)

| Sub-page | Path | WLED-Lite scope |
|---|---|---|
| Hub | `settings.htm` | **ADMIN** |
| WiFi | `settings_wifi.htm` | **ADMIN** *(but `welcome.htm` exposes the initial flow for first-run)* |
| Security & Updates | `settings_sec.htm` | **ADMIN** |
| LED preferences | `settings_leds.htm` | **ADMIN** |
| GPIO pin map | `settings_pin.htm` / `settings_pininfo.htm` | **ADMIN** |
| UI preferences | `settings_ui.htm` | **ADMIN** *(theme — may surface a tiny subset on user UI later)* |
| Time & macros | `settings_time.htm` | **ADMIN** *(on/off schedule is user-facing — see open issues)* |
| Sync (MQTT, ESP-NOW, DMX, Hue) | `settings_sync.htm` | **ADMIN** |
| Usermods | `settings_um.htm` | **ADMIN** |
| 2D layout | `settings_2D.htm` | **REMOVED** (Task #4) |
| DMX map | `settings_dmx.htm` | **ADMIN** |

---

## User surface (the simplified `index.htm`)

The end user is non-technical. The user UI should look like a consumer lamp app, not a network-engineering tool. Per the plan's scope:

| Feature | Source data | UI element |
|---|---|---|
| **On / off** | `/json/state` `on` | Big toggle / power button at top |
| **Brightness** | `/json/state` `bri` | Slider 0–255 |
| **Color** | `/json/state` `seg[0].col[0]` | Color picker (reuse iro.js — already shipped) |
| **Effect picker** | `/json/eff`, `/json/state` `seg[0].fx` | Tile/list of 15–25 curated effects (Task #6 picks the set; this UI just renders whatever the firmware reports) |
| **Effect speed & intensity** | `/json/state` `seg[0].sx`, `seg[0].ix` | Two sliders below the effect picker (optional — could be hidden until effect is non-static) |
| **On/off schedule** | `/json/state` `nl` + macro/time data | "Turn on at sunset / off at 10pm" simplified picker. Underlying time/macro plumbing is admin-only; the user only sees a friendly wrapper. |

**Explicitly NOT on the user UI:**
- Presets (admin-only; admin pre-loads presets, user just sees effects)
- Multi-segment management (single segment exposed)
- Palette picker (palettes are curated in firmware per Task #4/#6; user just gets effect choices)
- Color tools (palette editor, multi-color slots, color temperature) — pick-one color only
- Sync status / network info / mDNS / NTP — all invisible
- Any settings link visible to non-PIN users
- LED count, GPIO, pin map — never visible to user
- DMX / Hue / MQTT — admin-only or removed

**Onboarding (AP mode, first run):**
The existing `welcome.htm` already routes the user through WiFi config. WLED-Lite extends it with one extra step:

1. *(existing)* Connect device to home WiFi (form submits to `/settings/wifi`)
2. **NEW** Set admin PIN — 4-digit field, "Remember this PIN. Without it, you cannot change advanced settings." Submits to `/settings/sec` (the security sub-page that owns `settingsPIN`).
3. *(existing)* "Done — to the controls" CTA → `/` (now the slim user UI).

After step 2, the admin surface is gated. The maintainer can return to settings any time via `<device>/settings` and entering the PIN.

---

## Admin surface (`/settings/*` + supporting routes)

Untouched architecturally — upstream's existing `settingsPIN` plumbing already gates all the right URLs (`wled_server.cpp:checkPin()` and the per-route checks listed in `feature-flag-audit.md`).

**What WLED-Lite changes about it:**
1. Sub-pages for features disabled by Task #4 flag deltas auto-disappear from the settings hub (the hub iterates over compiled-in features).
2. **MQTT page stays visible to admin** (per Task #4 decision — admin can configure MQTT for Home Assistant; end user never sees the page because it's behind the PIN).
3. **NTP / mDNS / sync settings stay in `settings_time.htm` / `settings_sync.htm`** — kept functional, only visible to admin (already the case because admin-gated).
4. **`welcome.htm`'s PIN-set step** writes the PIN through the same path `settings_sec.htm` does — no new API needed.
5. **Bench-flash default**: if `settingsPIN` is empty (factory state), the welcome wizard *requires* setting one before letting the user navigate to `/`. This prevents shipping unlocked devices.

**No new routes**, no new auth tokens, no cookies. The session-style behavior already exists: PIN entry sets `correctPIN=true` which times out after `PIN_TIMEOUT` ms of inactivity. That's good enough for the household scenario.

---

## Implementation phasing

This design doc is the deliverable for Task #5. Actual code lands in later commits, grouped:

| Phase | Work | Tied to plan task | Status |
|---|---|---|---|
| **A** | Curate effects + palettes (defines what the user effect picker will display) | Task #6 | Effects done; palettes deferred |
| **B** | Replace `wled00/data/index.htm` with the slim user UI sketched above | New sub-task of #5, after #6 has decided the effect list | **Done** — `index.htm`/`.js`/`.css` rewritten with the "Lantern" aesthetic (warm amber on amber-black, mobile-first). Five sections: power, brightness, color (iro.js), curated effect picker (filters `_data_RESERVED`), nightlight-as-timer. Settings link is a discrete top-right gear. Total ~20 KB uncompressed → ~6 KB after gzip in flash; firmware saved ~25 KB vs upstream's busy index. |
| **C** | Extend `wled00/data/welcome.htm` with the "Set admin PIN" step + write a small server-side check that blocks `/` until PIN is set | New sub-task of #5 | **HTML done** — `welcome.htm` rewritten with three numbered steps (WiFi → Set PIN → Use your sign), matching the Lantern aesthetic. Step 2 links to `/settings/sec` which is where the PIN field already lives. **Server-side hard-block deferred** — a brand-new device with no PIN is still reachable at `/settings` by anyone on the LAN; for personal use the welcome flow is sufficient guidance, but commercial deployments will want enforcement (see Open follow-ups). |
| **D** | Audit each remaining settings sub-page for end-user-visible language → make it admin-facing in tone (e.g. remove "tap to learn more about ABL" type aids that are aimed at first-time users) | Polish, late in the project | Open |
| **E** | (Stretch) Cosmetic theme override so the slim UI doesn't look like upstream WLED — different fonts/colors to make the brand split visible | Polish | **Done implicitly** — the "Lantern" theme in Phase B is the brand-distinguishing layer. |

**Open issues / things I deferred:**

- **Server-side hard-block on `/` until PIN is set.** Phase C currently relies on the welcome wizard to guide the maintainer through PIN setup, but doesn't enforce it. A brand-new factory-fresh device exposes `/settings` to anyone on the LAN until a PIN is set. For personal use the guidance is sufficient; for commercial deployments this should be a hard server-side check (refuse access to `/` and redirect to `/welcome` until `settingsPIN` is non-empty). Implementation lives in `wled_server.cpp` — straightforward but invasive.
- **`/reset` is unauthenticated** in upstream. Anyone on the LAN can reboot the device. Recommend a small follow-up to require PIN for `/reset` in WLED-Lite. Listed but not committed-to in this design.
- **`/liveview` is unauthenticated** and shows a real-time LED preview. Probably fine to leave public — it doesn't change state — but consider hiding it from the user UI (don't link to it).
- **WebSocket `/ws`** is unauthenticated state push/pull. Same shape as `/json` POST: state mutations OK without PIN, config mutations require PIN. No change.
- **Schedule UX** for the user UI ("turn on at sunset" etc) is a real piece of design work. The underlying macro/time engine is already in upstream; the user-friendly wrapper is what's missing. Treat as part of Phase B.
- **Default PIN suggestion**: should the welcome page suggest "1234" vs require user-picked? Leaning *require user-picked* (forced 4-digit input, no suggestion) so devices don't all ship with the same PIN if the maintainer is in a hurry.

---

## What this means for the firmware delta

Almost nothing yet — this is a design doc, not a code change. When the implementation phases land:

- New file(s): a replacement `index.htm`, possibly a slim `index.js`/`index.css` pair.
- Modified: `welcome.htm` (adds PIN step), maybe `wled_server.cpp` (blocks `/` until PIN set, optionally PIN-gates `/reset`).
- Removed: nothing else — the 2D editor data files (`pixart/`, `pxmagic/`, `pixelforge/`) stop being compiled-in by Task #4 flags but their source can stay around to keep upstream merges clean.

Re-validate this design at the start of Phase B and Phase C — between now and then, Task #6 (effect curation) may reshape what the user picker needs to render.
