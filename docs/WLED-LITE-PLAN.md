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

### Next up
| # | Task | Notes |
|---|------|-------|
| 5 | Design role-based UI split | Sketch user vs admin surfaces. Decide auth model (single admin password? per-user accounts? admin-only token-gated route?). Plan before touching `wled00/data/` HTML/JS. |
| 6 | Trim effects to curated subset | Review FX.cpp / FX.h. Keep ~15–25 well-curated effects; preserve effect IDs of survivors so saved presets don't shift. |
| 7 | Support 8 strips on configurable pins | Verify XIAO ESP32-S3 has enough RMT/I2S channels for 8 simultaneous outputs. Admin UI for per-pin assignment. Document the carrier-board pin map. |
| 8 | Configurable button bindings | Audit `wled00/button.cpp`. Extend to support per-button short/long actions plus a "brightness mode" state machine (long-press enters mode, short-presses cycle brightness). Defaults TBD. |
| 9 | INA219 current limit | New I2C driver. Auto-detect on bus; when present, enforce cap against measured current instead of or alongside math estimate. Address configurable in admin UI. Must coexist with other I2C devices. |
| 10 | Multi-controller sync (ESP-NOW + WiFi) | Audit existing sync code. Keep both transports. Simplify pairing UI. |
| 11 | Document upstream-sync workflow | Write `docs/upstream-sync.md`. Cadence (e.g. monthly), conflict-resolution approach. |


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
