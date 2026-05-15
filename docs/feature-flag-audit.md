# WLED-Lite — Feature Flag Audit

Task #4 of the [project plan](WLED-LITE-PLAN.md). Inventory of every `WLED_DISABLE_*`, `WLED_ENABLE_*`, and `USERMOD_*` flag exposed by upstream WLED, with a recommended decision against the WLED-Lite scope.

> **No code changes in this pass.** The Action column says what *should* go into `platformio.ini` (or be deferred). Application of these changes lives in a follow-up commit so this doc can be reviewed independently.

## Conventions used by upstream

- **`#ifndef WLED_DISABLE_FOO`** — the feature `FOO` is ON by default; setting `-D WLED_DISABLE_FOO` removes it.
- **`#ifdef WLED_ENABLE_FOO`** — the feature `FOO` is OFF by default; setting `-D WLED_ENABLE_FOO` adds it.
- A handful of features have **both** forms paired in `wled.h` (e.g. `WLED_ENABLE_MQTT` is auto-defined unless `WLED_DISABLE_MQTT` is set). Where this is the case, only the **DISABLE** flag needs to be set in build flags — the `_ENABLE_` half follows automatically.

The canonical flag list lives in `.github/copilot-instructions.md` and matches what's enumerated here.

## Current state of the `xiao_esp32s3_plus` build

Inherited from `[esp32_all_variants]` and `[esp32_idf_V4]`:
- `-D WLED_ENABLE_GIF` — animated GIF playback
- `-D WLED_ENABLE_DMX_INPUT` — DMX over RDM input

Inherited from board defaults (built into `custom_usermods`):
- `custom_usermods = audioreactive`

Everything else `WLED_DISABLE_*` is at its default (i.e. feature is ON), and the remaining `WLED_ENABLE_*` flags are at default (i.e. feature is OFF).

---

## `WLED_DISABLE_*` flags

The feature defaults ON; setting the flag drops it.

| Flag | Gates | Decision | Rationale | Action |
|---|---|---|---|---|
| `WLED_DISABLE_2D` | 2D matrix support (matrix layouts, X/Y mapping, several 2D-only FX, 2D web UI tools) | **DROP** | WLED-Lite targets LED strips only. Removing 2D collapses a large surface (FX.h, ws.cpp, cfg.cpp, server). | Add `-D WLED_DISABLE_2D` |
| `WLED_DISABLE_ADALIGHT` | Adalight/TPM2 serial protocol — and *also* gates Serial Improv WiFi onboarding and Serial JSON commands (see `WLED_ENABLE_ADALIGHT` block in `wled.cpp`) | **KEEP default (ON)** | The GPIO3 caveat from upstream applies to classic ESP32 hardware UART0. On ESP32-S3 with native USB-CDC, "Serial" is the USB-CDC device — no GPIO is claimed. Disabling ADALIGHT would lose Serial Improv onboarding (a friendly USB-based WiFi-credential flow for non-technical end users) without saving any pin. The Adalight pixel-protocol itself is dead weight here, but the surrounding serial-onboarding code is in scope. | none |
| `WLED_DISABLE_ALEXA` | Amazon Alexa voice control | **DROP** | Confirmed by maintainer: not in scope, easy to revisit if a user explicitly asks. Saves ~10KB and removes a network-discovery surface. | Add `-D WLED_DISABLE_ALEXA` |
| `WLED_DISABLE_BROWNOUT_DET` | ESP32 brownout detector | **KEEP default (ON)** | Stability. Only disable on hardware with known brownout false positives. | none |
| `WLED_DISABLE_ESPNOW` | ESP-NOW radio | **KEEP** | Plan explicitly requires ESP-NOW for multi-controller sync. | none |
| `WLED_DISABLE_FILESYSTEM` | LittleFS | **KEEP** | Needed for presets, config, web UI assets. | none |
| `WLED_DISABLE_HUESYNC` | Philips Hue light sync | **DROP** | Plan: "Hue sync / Art-Net / MQTT — admin only (or remove if not used)". Maintainer not using Hue. | Add `-D WLED_DISABLE_HUESYNC` |
| `WLED_DISABLE_IMPROV_WIFISCAN` | Improv-protocol WiFi onboarding (BLE/serial helper for first connect) | **KEEP** | Aligns with "WiFi onboarding (simple flow for non-technical users)" in plan scope. | none |
| `WLED_DISABLE_INFRARED` | IR remote receiver | **DROP** | Not in plan; buttons are the chosen input method. Frees the IR pin + several KB. | Add `-D WLED_DISABLE_INFRARED` |
| `WLED_DISABLE_LOXONE` | Loxone smart-home integration | **DROP** | Niche; not in scope. | Add `-D WLED_DISABLE_LOXONE` |
| `WLED_DISABLE_MQTT` | MQTT messaging | **ADMIN-ONLY (keep compiled-in)** | Maintainer decision: keep MQTT compiled-in for a future Home Assistant / smart-home bridge; hide the UI from end users in the Task #5 admin/user split. | none in this pass — Task #5 will hide the UI |
| `WLED_DISABLE_OTA` | HTTP OTA endpoint | **KEEP default (ON)** | OTA is required to push WLED-Lite updates after first USB flash. | none |
| `WLED_DISABLE_PARTICLESYSTEM1D` | 1D particle effects (subset of FX) | **UNDECIDED** | Depends on the curated 15–25 effect list from Task #6. If none of the keepers use particles, drop for flash savings. | Defer to Task #6 |
| `WLED_DISABLE_PARTICLESYSTEM2D` | 2D particle effects | **DROP** | No 2D in WLED-Lite. | Add `-D WLED_DISABLE_PARTICLESYSTEM2D` |
| `WLED_DISABLE_PIXELFORGE` | Pixelforge HTML tool served at `/pixelforge.htm` (2D content authoring) | **DROP** | 2D-only tool. | Add `-D WLED_DISABLE_PIXELFORGE` |
| `WLED_DISABLE_WEBSOCKETS` | WebSocket API used by the web UI for live state push | **KEEP default (ON)** | The user UI needs this for responsive state updates. | none |

## `WLED_ENABLE_*` flags

The feature defaults OFF; setting the flag adds it.

| Flag | Gates | Decision | Rationale | Action |
|---|---|---|---|---|
| `WLED_ENABLE_ADALIGHT` | (Auto-defined unless `WLED_DISABLE_ADALIGHT` is set) | Drop via DISABLE | See `WLED_DISABLE_ADALIGHT` row. | — |
| `WLED_ENABLE_AOTA` | ArduinoOTA network update (IDE-driven update over WiFi). Force-overrides `WLED_DISABLE_OTA`. | **KEEP OFF** | HTTP OTA is sufficient for the field-flash workflow. AOTA is mostly a developer convenience. | none |
| `WLED_ENABLE_DMX` | Legacy DMX flag (UI-only — gates DMX settings page rendering). | **KEEP** *(implied by DMX scope)* | Plan keeps DMX (admin only). The actual transport is `_DMX_INPUT` / `_DMX_OUTPUT`. Without `_DMX`, the DMX settings UI is missing. | Add `-D WLED_ENABLE_DMX` |
| `WLED_ENABLE_DMX_INPUT` | DMX receive (already enabled by `esp32_idf_V4`) | **KEEP** (already on) | DMX is in scope; input side currently enabled. | none |
| `WLED_ENABLE_DMX_OUTPUT` | DMX transmit | **ENABLE** | Maintainer decision: turn on for flexibility (controller can act as a DMX sender as well as receiver). Small flash cost; XIAO Plus has 16MB to spare. | Add `-D WLED_ENABLE_DMX_OUTPUT` |
| `WLED_ENABLE_FS_EDITOR` | `/edit` admin filesystem editor | **Already on** | `wled.h` defines this unconditionally. Useful for admin. | none |
| `WLED_ENABLE_GIF` | Animated GIF playback (already enabled by `esp32_all_variants`) | **KEEP** | Already on; nice user-facing capability. | none |
| `WLED_ENABLE_HUB75MATRIX` | HUB75 matrix panel driver | **KEEP OFF** | We removed all HUB75 envs in Task #3. | none |
| `WLED_ENABLE_JSONLIVE` | `/json/live` HTTP endpoint that streams LED state (used by external apps when WebSockets aren't available) | **KEEP OFF** | Auto-enabled only when WebSockets are disabled. We keep WebSockets, so this stays redundant. | none |
| `WLED_ENABLE_LOXONE` | (Auto-defined unless `WLED_DISABLE_LOXONE` is set) | Drop via DISABLE | See `WLED_DISABLE_LOXONE` row. | — |
| `WLED_ENABLE_MQTT` | (Auto-defined unless `WLED_DISABLE_MQTT` is set) | Drop via DISABLE (proposed) | See `WLED_DISABLE_MQTT` row. | — |
| `WLED_ENABLE_PIXART` | `/pixart.htm` admin tool (2D pixel-art import) | **KEEP OFF** | 2D-only tool. | none |
| `WLED_ENABLE_PXMAGIC` | `/pxmagic.htm` admin tool (2D pixel-magic import) | **KEEP OFF** | 2D-only tool. | none |
| `WLED_ENABLE_USERMOD_PAGE` | `/u` admin page for usermod-supplied HTML | **ENABLE** | Will host the INA219 current-cap config (Task #9), audioreactive controls, and any future usermod admin UI. Admin only. | Add `-D WLED_ENABLE_USERMOD_PAGE` |
| `WLED_ENABLE_WEBSOCKETS` | (Auto-defined unless `WLED_DISABLE_WEBSOCKETS` is set) | Keep (default on) | See `WLED_DISABLE_WEBSOCKETS` row. | — |
| `WLED_ENABLE_WPA_ENTERPRISE` | WPA2-Enterprise WiFi authentication (RADIUS, certificates) | **KEEP OFF** | Home WiFi audience. Adds ~10KB. | none |

## `USERMOD_*` flags

The `USERMOD_*` namespace contains three different kinds of identifier — only one of these is a "feature flag" in the toggle sense.

1. **`USERMOD_ID_*`** — *stable integer IDs* registered in `const.h`, used by the usermod registry to identify modules in saved presets / API responses. Not feature flags; don't change.
2. **Per-usermod compile-time config values** (`USERMOD_BATTERY_MIN_VOLTAGE`, `USERMOD_DHT_PIN`, `USERMOD_BME280`, etc.). Set by individual usermods to take build-time configuration. These are documented within each usermod's README and only matter once that usermod is selected.
3. **`USERMOD_AUTO_SAVE`, `USERMOD_BATTERY`, `USERMOD_AUTO_SAVE_ON_BOOT`, etc.** — a small number of usermods bake their *own* enable into a `-D USERMOD_FOO`. These are part of the build flags for whichever env opts in.

### Usermod *selection* is via `custom_usermods=`, not `-D` flags

The actual mechanism for picking usermods is the `custom_usermods` env directive in `platformio.ini` (consumed by `pio-scripts/load_usermods.py`). Today the XIAO env (via its parent `esp32s3dev_16MB_opi`) selects:

```
custom_usermods = audioreactive
```

| Action | Notes |
|---|---|
| **Keep** `audioreactive` | In scope per plan ("Audio-reactive functionality"). |
| **Add later** an INA219 usermod | Task #9 work. The carrier board has an INA219 on the shared I2C bus for current capping. WLED ships an `ina226_v2` usermod but **not** an INA219 module out of the box — we'll likely write a small one. |
| **Consider** `four_line_display_ALT` / OLED usermod | Carrier board can host an I2C OLED. Decide as part of the admin UI work (Task #5) — an admin status display would be a nice-to-have, not required. |
| **Skip** all other shipped usermods | Battery, BME280, DHT, BH1750, etc. are useful but not in the current scope. Easy to opt back in per device by adding to `custom_usermods`. |

The full list of upstream usermods (for future reference) lives under `usermods/` in the source tree.

---

## Proposed `platformio.ini` delta

Decisions resolved (see [Maintainer decisions](#maintainer-decisions) below). Here is the proposed addition to the `[env:xiao_esp32s3_plus]` `build_flags` block — apply in a follow-up commit, not in this audit:

```ini
;; --- WLED-Lite scope reductions ---
-D WLED_DISABLE_2D
-D WLED_DISABLE_ALEXA
-D WLED_DISABLE_HUESYNC
-D WLED_DISABLE_INFRARED
-D WLED_DISABLE_LOXONE
-D WLED_DISABLE_PARTICLESYSTEM2D
-D WLED_DISABLE_PIXELFORGE

;; --- WLED-Lite admin / scope additions ---
-D WLED_ENABLE_DMX
-D WLED_ENABLE_DMX_OUTPUT
-D WLED_ENABLE_USERMOD_PAGE
```

Notably **not** in this delta:
- `WLED_DISABLE_ADALIGHT` — kept ON to preserve USB-Serial Improv WiFi onboarding (no GPIO saving on USB-CDC boards).
- `WLED_DISABLE_MQTT` — kept ON; MQTT will remain compiled-in and hidden in Task #5's admin/user UI split.
- `WLED_DISABLE_PARTICLESYSTEM1D` — deferred to Task #6 (depends on which effects survive the curated list).

Expected payoff: ~30–50 KB of flash freed, several non-relevant settings pages disappear from the admin UI (reduces Task #5's UI-split surface area).

---

## Maintainer decisions

The four flag questions raised in the initial audit were resolved as follows:

1. **`WLED_DISABLE_ALEXA`** → **DROP** entirely. Easy to revisit if a user asks.
2. **`WLED_DISABLE_MQTT`** → **KEEP compiled-in, admin-only UI**. MQTT stays available for a future Home Assistant / smart-home bridge; Task #5 hides the settings page from end users.
3. **`WLED_ENABLE_DMX_OUTPUT`** → **ENABLE**. Both input and output on; controller can act as either a DMX sender or receiver.
4. **`WLED_DISABLE_PARTICLESYSTEM1D`** → **WAIT for Task #6**. Decide once the curated effect list is final.

A fifth decision was made during research:

5. **`WLED_DISABLE_ADALIGHT`** → **KEEP default (ON, i.e. don't disable)**. The upstream "frees the RX pin" rationale only applies to classic ESP32; on ESP32-S3 with native USB-CDC, "Serial" is USB and no GPIO is claimed. Disabling ADALIGHT would lose Serial Improv onboarding, which is a useful USB-based WiFi-credential flow for non-technical end users.

---

## Items deferred to later tasks

| Item | Defer to | Why |
|---|---|---|
| Admin-vs-user UI split for features marked **ADMIN-ONLY** | Task #5 | Auth model and route gating haven't been decided yet. |
| `WLED_DISABLE_PARTICLESYSTEM1D` decision | Task #6 | Depends on which effects survive the curated list. |
| INA219 usermod selection | Task #9 | Need to write/select the usermod first. |
| OLED admin display usermod | Task #5 | Tied to admin UI scope. |
