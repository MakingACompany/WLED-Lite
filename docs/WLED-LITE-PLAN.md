# WLED-Lite — Project Plan

A simplified, hardware-focused fork of [WLED](https://github.com/wled/WLED) (EUPL-1.2). This document is the canonical roadmap and project memory for WLED-Lite, written so it can be picked up from any machine.

---

## Goal

A WLED variant that a **non-technical end user** (the maintainer's 75-year-old father, friends, family) can use to control LED strips, while still giving the **admin** (maintainer) full access to advanced features like DMX, pin/button configuration, feature toggles, and multi-controller sync.

Two faces of the same firmware:
- **User UI** — pick a color, pick an effect, brightness, on/off schedule, WiFi onboarding. NTP, mDNS, sync details are invisible/automatic.
- **Admin UI** — DMX, pin/button bindings, feature toggles, current-cap config, sync grouping, advanced network.

---

## Hardware target

**Primary MCU:** Seeed Studio XIAO ESP32-S3 Plus.

The maintainer is standardizing on this module for multiple projects (lighted sign, ventilation controller, etc.), so WLED-Lite is narrowed to ESP32-S3 only. All other targets (ESP8266, classic ESP32, ESP32-S2, ESP32-C3, HUB75 matrix panels, board-specific WROOM-2 dev kits) have been removed from `platformio.ini`. If S2/C3/C5/ATTiny or LED-matrix support is ever needed again, the corresponding env blocks can be lifted back from the upstream `wled/WLED` repo.

The retained S3 envs are:
- `xiao_esp32s3_plus` — primary target, extends `esp32s3dev_16MB_opi`
- `esp32s3dev_16MB_opi` — 16MB flash + OPI PSRAM, also the parent of `xiao_esp32s3_plus`
- `esp32s3dev_8MB_opi` — 8MB flash + OPI PSRAM
- `esp32s3_4M_qspi` — 4MB flash + QSPI PSRAM (closest match to the smaller build that flashed successfully on prior bring-up attempts)

Keeping a few S3 envs in parallel gives the maintainer something to A/B against when bringing up the XIAO Plus, since the obvious 16MB-OPI config did not boot on a prior attempt.

A custom carrier board is being designed around the module with:
- Up to **8 LED strip outputs** on independent GPIO pins
- **I2C bus** shared by various components, including but not limited to: INA219 current sensor, OLED display, I2C PWM driver (for computer fans in ventilation projects), other sensors
- **Configurable buttons** (short / long press), e.g. for the first deployment — a lighted sign:
  - btn1 short = effect cycle; btn1 long = enter brightness mode (subsequent short presses cycle brightness)
  - btn2 short = mode switch; btn2 long = off
- **USB-C** programming and debug must keep working — flashing and serial debug over USB are mandatory for bench bring-up.

---

## Scope

### Keep
- Color picker, effect picker (curated subset), brightness, on/off schedule
- WiFi onboarding (simple flow for non-technical users), including Access Point initial configuration
- DMX (admin only)
- Multi-controller sync — both ESP-NOW and WiFi
- Current capping — math-based (existing ABL) and INA219 I2C measured feedback
- I2C device support (multiple devices on shared bus)
- Configurable buttons with short/long press behaviors
- Up to 8 strips per controller
- USB programming/debug
- Server settings, user administration, feature enable/disable (admin only)
- Audio-reactive functionality - the maintainer has a project that may involve audio reactive effects and changes.

### Drop / simplify / make admin-only
- Most MCU board targets (keep only ESP32-S3 variants — XIAO Plus is primary; ESP8266, classic ESP32, ESP32-S2, ESP32-C3, HUB75 matrix, WROOM-2 dev kits all removed; lift back from upstream if needed)
- ~80% of effects — aim for 15–25 curated effects, not the full ~180
- ~90% of effect palettes - aim for 10-20 color palettes, not the full selection.
- NTP / mDNS visibility — keep functionality, hide config from user view
- Hue sync / Art-Net / MQTT — admin only (or remove if not used)
- Complex network tuning surface

---

## Architecture decisions

### Rebrand via build flags, not source edits
WLED already exposes rebrand macros:
- `WLED_BRAND` (defaults to `"WLED"`, used by `DEFAULT_AP_SSID = WLED_BRAND "-AP"`)
- `WLED_PRODUCT_NAME` (defaults to `"FOSS"`)
- `WLED_RELEASE_NAME` (gates OTA compatibility — see caveat below)
- `WLED_VERSION`
- `WLED_REPO`

These will be set in build_flags of our env block in `platformio.ini`. **Do not edit `wled00/const.h` or `wled00/wled_metadata.cpp`** — that would create permanent upstream-merge conflicts.

**OTA caveat:** `WLED_RELEASE_NAME` is embedded in firmware at a fixed offset and OTA refuses cross-release updates. Pick it once (`"XIAO_ESP32S3_LITE"` is the current proposal) and don't change it — devices already deployed cannot OTA to firmware with a different release name; they'd need a USB reflash.

### Upstream tracking
- `origin` → `git@github.com:MakingACompany/WLED-Lite.git` (via HTTPS + PAT for this machine)
- `upstream` → `https://github.com/wled/WLED.git` (read-only)
- Strategy: periodically `git fetch upstream && git merge upstream/main`. Most conflicts will be in files we've trimmed (board defs, FX list, UI). See task #11 for the documented workflow.
- Fork point: WLED 17.0.0-dev, commit `8e94cf5b`.

### License
WLED is **EUPL v1.2 or later** (copyleft). WLED-Lite must:
- Retain the EUPL LICENSE file
- Preserve original copyright and contributor notices
- Be redistributed under EUPL when distributed
- Cannot be relicensed to MIT/proprietary

`package.json` license was previously incorrectly "ISC" — this has been corrected to `EUPL-1.2` (the LICENSE file itself was always correct).

---

## Status

### Completed
| # | Task | Notes |
|---|------|-------|
| 1 | Set up fork remotes and initial push | `origin` → `MakingACompany/WLED-Lite`; `upstream` → `wled/WLED`. Initial push at `8e94cf5b`. |
| 2 | Rebrand package.json | name=wled-lite, version=0.1.0-lite, license=EUPL-1.2, URLs updated. Commit `0bc6f68d`. |
| 3 | Trim platformio.ini + WLED-Lite build flags | Cut from ~765 → ~260 lines. Kept only S3 envs: `xiao_esp32s3_plus` (new, primary), `esp32s3dev_16MB_opi` (parent + parity smoke test), `esp32s3dev_8MB_opi`, `esp32s3_4M_qspi`. Removed all ESP8266/classic-ESP32/S2/C3/WROOM-2/HUB75 envs and their shared sections. New env adds rebrand flags (`WLED_BRAND`, `WLED_PRODUCT_NAME`, `WLED_RELEASE_NAME=XIAO_ESP32S3_LITE`, `WLED_REPO`) and keeps USB-CDC enabled. Pre-built binaries for all 4 S3 envs committed under `firmware/`, plus Windows re-compile docs at `docs/dev-setup-windows.md`. On-device verification (USB-CDC programming, AP SSID `WLED-Lite-AP`, About page) deferred to first hardware bring-up. Commit `4dbabf27`. |
| 4 | Audit feature flags, decide keep/drop/admin-only | See [`docs/feature-flag-audit.md`](feature-flag-audit.md). Inventory + decision matrix for all `WLED_DISABLE_*` / `WLED_ENABLE_*` / `USERMOD_*` flags. Maintainer decisions captured: drop Alexa, drop Hue / Loxone / IR, drop 2D + pixelforge + 2D particles, drop pixart/pxmagic; keep MQTT compiled-in (hide UI in Task #5); enable DMX in+out + usermod page; keep Adalight (Improv onboarding); defer particle-1D to Task #6. No code changes in this commit — the `platformio.ini` delta block in the audit lands in a separate follow-up. Commit `b24742e4`. |
| 4a | Apply Task #4 feature-flag delta to xiao_esp32s3_plus | Followed the audit. 11 flags added to the XIAO env. Flash 1,225,744 → 1,125,632 bytes (~98 KB saved, ~2× audit's conservative estimate). `firmware/xiao_esp32s3_plus/` artifacts regenerated. Parity envs unchanged. Commit `aeb47123`. |
| 5 | Design role-based UI split | See [`docs/ui-split-design.md`](ui-split-design.md). Auth model: reuse upstream's `settingsPIN` — already gates `/settings`, `/edit`, `/update`, `/json?cfg`, `/upload`; no new auth code. Default PIN: maintainer sets during AP-mode onboarding (welcome.htm gets a "Set admin PIN" step). UI impl: replace `index.htm` with slim user UI (color/effect/brightness/on-off/schedule); leave `/settings/*` as the admin surface. Implementation phased — actual code lands after Task #6 (effect curation defines what the picker shows). Commit `56df24d0`. |
| 6 | Trim effects to curated subset | See [`docs/fx-curation.md`](fx-curation.md). 22 keepers curated for sign / strip use (Solid, Breathe, Wipe, Colorloop, Rainbow, Fade, Theater, Running, Twinkle, Chase, Aurora, Lighthouse, Palette, Fire 2012, Colorwaves, Twinklefox, Starburst, Exploding Fireworks, Bouncing Balls, Pacifica, Sunrise, Blends). Implementation: single trim block at end of `setupEffectData()` gated by `-D WLED_LITE_FX_TRIM`, reverts non-keepers to `_data_RESERVED` while leaving upstream addEffect calls untouched — minimal merge surface. Effect IDs preserved. Function bodies still compile (linker can't strip them); flash impact +92 bytes only. Audio-reactive effects compiled in but excluded from picker (no mic on a sign). Particle 1D effects still compile (we kept the flag) but excluded from picker. Commit `970f0ed4`. |
| 7 | Support 8 strips on configurable pins | See [`docs/hardware-pinmap.md`](hardware-pinmap.md). S3 has 4 RMT + 8 I2S/LCD channels (`WLED_MAX_DIGITAL_CHANNELS` = 12) — 8 simultaneous LED outputs is well within capability. Pin map for XIAO ESP32-S3 Plus: LED buses on GPIO 4, 7, 8, 9, 10, 11, 12, 13 (4 front + 4 Plus sub-header); I2C on GPIO 5/6; buttons on GPIO 1/2; HW UART 43/44 kept free for serial-adapter debug; GPIO 38–42 reserved for expansion. Build-flag defaults applied via `DATA_PINS`, `LED_TYPES`, `PIXEL_COUNTS`, `BTNPIN`, `HW_PIN_SDA`, `HW_PIN_SCL`; admin UI (`/settings/leds`, `/settings/pin`) handles per-device override. Commit `7d7cbd17`. |
| 8 | Configurable button bindings | See [`docs/button-bindings.md`](button-bindings.md). Brightness-mode FSM lives in `wled00/wled_lite_brightness.{h,cpp}` with tunables (level table, timeout, button-index assignments) in `wled00/wled_lite_config.h`. `button.cpp` modified surgically with three hook points, all gated by `-D WLED_LITE_BUTTON_BRIGHTNESS_MODE`. Factory defaults for the sign use case: btn1 short = cycle effect, btn1 long = enter brightness mode (5 discrete levels: 12/25/50/75/100%); btn2 short = cycle palette, btn2 long = toggle power. 10 s auto-exit on idle. Admin-UI macros still override per device. Flash +188 bytes. Commit `29c22090`. |
| 9 | INA219 current limit | See [`docs/current-limit-ina219.md`](current-limit-ina219.md). New `usermods/INA219/` (modeled on upstream `INA226_v2`) wraps `wollewald/INA219_WE`. Safety-net mode over math ABL: polls every 200 ms; when measured current > limit × 110 %, directly reduces `bri` proportionally. Auto-detects on 0x40/0x41/0x44/0x45; coexists with other I2C devices via auto-detect fallback. Address, shunt resistance, max current, hysteresis all configurable via admin UI `/settings/um`. Internal data model is a sensor array (size 1 now) so future per-bus expansion is a `#define` change. Wired into `xiao_esp32s3_plus` via `custom_usermods = audioreactive INA219`. Flash +4.5 KB. Commit `f80e6477`. |
| 10 | Multi-controller sync (ESP-NOW + WiFi) | See [`docs/multi-controller-sync.md`](multi-controller-sync.md). Both transports kept and fully functional. UDP-on-port-21324 and ESP-NOW share the same packet protocol and 8-bit `syncGroups` bitmask routing. WLED-Lite-specific: a single one-bit default flipped via `-D WLED_LITE_SYNC_DEFAULTS` in `wled.h` — `sendNotifications` from `false → true` so two devices on the same WiFi with the (already-default) group 0x01 mirror automatically out of the box. ESP-NOW stays opt-in per device (security + power). `settings_sync.htm` left as-is; pairing-wizard UI work deferred to Task #5 Phase B. Flash unchanged. Commit `86a5f8be`. |
| 11 | Document upstream-sync workflow | See [`docs/upstream-sync.md`](upstream-sync.md). Playbook for monthly merges from `wled/WLED`: setup, branch-per-merge (`sync/upstream-YYYY-MM-DD`), per-file conflict guide (covers `platformio.ini`, `package.json`, `wled.h`, `button.cpp`, `FX.cpp`, etc.), what NOT to re-merge (re-introducing ESP8266 envs, dropped feature flags), post-merge audit + bench-test checklist, tagged-release strategy, OTA release-name caveats. Captures the current divergence: 20 ahead / 16 behind upstream `42f4bcb8` since fork point `8e94cf5b`. No code changes. |


---

## On-device verification still pending for Task #3

Compile-time build of `xiao_esp32s3_plus` was verified (config parses; full `pio run` requires PlatformIO ≥ 6 — the apt-shipped 4.3.4 is too old for the Tasmota platform-espressif32 2024.06 fork that upstream pins to). Hardware checks queued for first XIAO Plus bring-up:
- USB-CDC programming + serial debug over USB-C
- Default AP SSID = `WLED-Lite-AP`
- About page reflects new product / release name
- A/B the four S3 envs against the actual board to find which boots reliably (prior bring-up attempt: the obvious 16MB env did not work; a smaller ~4MB build did)

---

## Repo / dev setup notes

- Working directory: `~/Code/WLED-Lite/` (on work Linux machine)
- Dev tools: VSCode + PlatformIO and/or ESP-IDF extensions
- GitHub auth on this machine: HTTPS with PAT, `credential.helper=store`, file `~/.git-credentials` mode 600
- Per-repo commit identity: `Aaron Loar <1115581+aaronloar@users.noreply.github.com>` (set with `git config user.email/user.name`, not global)
- Working from home will use a different machine — clone fresh and configure the same PAT + commit identity, or generate a new PAT scoped to this repo.
- **PlatformIO version:** the Tasmota platform-espressif32 fork pinned by upstream WLED requires PlatformIO ≥ 6. Ubuntu's `apt`-shipped `platformio` (4.3.4 on 22.04) is too old and will fail with "Unknown development platform 'espressif32'". On this machine PIO 6.x is installed under `~/.local/bin/pio` via `pip install --user platformio`; VSCode's PlatformIO extension brings its own bundled PIO and works fine.
- **Pre-built firmware** is checked in under `firmware/<env>/` for each S3 env — `firmware.bin` for OTA and `merged-flash.bin` for first-time USB flash. See [`firmware/README.md`](../firmware/README.md). Re-generate after a meaningful rebuild by running all four envs and re-running `esptool merge_bin`.
- **Windows re-compile / flash:** see [`docs/dev-setup-windows.md`](dev-setup-windows.md).

---

## When picking this back up

1. Pull latest: `git fetch upstream && git fetch origin && git pull origin main`
2. Re-read this file
3. Resume at the next unfinished task above. The recommended order is sequential (each task informs the next), but tasks #4 (feature audit) and #5 (UI design) can be done in either order before #6 onward.
4. After each substantive task: commit with a focused message, push, and update the status table here.
