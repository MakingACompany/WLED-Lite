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

The maintainer is standardizing on this module for multiple projects (lighted sign, ventilation controller, etc.), so WLED-Lite is being narrowed to just this module and close ESP32-S3 variants. Most other boards (ESP8266, classic ESP32, S2, C3, etc.) will be removed from `platformio.ini`.

A custom carrier board is being designed around the module with:
- Up to **8 LED strip outputs** on independent GPIO pins
- **I2C bus** shared by: INA219 current sensor, OLED display, I2C PWM driver (for computer fans in ventilation projects), other sensors
- **Configurable buttons** (short / long press), e.g. for the first deployment — a lighted sign:
  - btn1 short = effect cycle; btn1 long = enter brightness mode (subsequent short presses cycle brightness)
  - btn2 short = mode switch; btn2 long = off
- **USB-C** programming and debug must keep working — flashing and serial debug over USB are mandatory for bench bring-up.

---

## Scope

### Keep
- Color picker, effect picker (curated subset), brightness, on/off schedule
- WiFi onboarding (simple flow for non-technical users)
- DMX (admin only)
- Multi-controller sync — both ESP-NOW and WiFi
- Current capping — math-based (existing ABL) and INA219 I2C measured feedback
- I2C device support (multiple devices on shared bus)
- Configurable buttons with short/long press behaviors
- Up to 8 strips per controller
- USB programming/debug
- Server settings, user administration, feature enable/disable (admin only)

### Drop / simplify / make admin-only
- Most MCU board targets (keep only XIAO ESP32-S3 Plus + close variants)
- ~80% of effects — aim for 15–25 curated effects, not the full ~180
- NTP / mDNS visibility — keep functionality, hide config from user view
- Hue sync / Art-Net / MQTT — admin only (or remove if not used)
- Complex network tuning surface
- Audio-reactive (likely dropped — adds complexity)

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

### Next up
| # | Task | Notes |
|---|------|-------|
| 3 | Trim platformio.ini + WLED-Lite build flags | See **Task #3 detail** below. |
| 4 | Audit feature flags, decide keep/drop/admin-only | Inventory `WLED_DISABLE_*` / `WLED_ENABLE_*` / `USERMOD_*`. Produce a decision matrix file. No code changes — just reading + a docs file. |
| 5 | Design role-based UI split | Sketch user vs admin surfaces. Decide auth model (single admin password? per-user accounts? admin-only token-gated route?). Plan before touching `wled00/data/` HTML/JS. |
| 6 | Trim effects to curated subset | Review FX.cpp / FX.h. Keep ~15–25 well-curated effects; preserve effect IDs of survivors so saved presets don't shift. |
| 7 | Support 8 strips on configurable pins | Verify XIAO ESP32-S3 has enough RMT/I2S channels for 8 simultaneous outputs. Admin UI for per-pin assignment. Document the carrier-board pin map. |
| 8 | Configurable button bindings | Audit `wled00/button.cpp`. Extend to support per-button short/long actions plus a "brightness mode" state machine (long-press enters mode, short-presses cycle brightness). Defaults TBD. |
| 9 | INA219 current limit | New I2C driver. Auto-detect on bus; when present, enforce cap against measured current instead of or alongside math estimate. Address configurable in admin UI. Must coexist with other I2C devices. |
| 10 | Multi-controller sync (ESP-NOW + WiFi) | Audit existing sync code. Keep both transports. Simplify pairing UI. |
| 11 | Document upstream-sync workflow | Write `docs/upstream-sync.md`. Cadence (e.g. monthly), conflict-resolution approach. |

---

## Task #3 detail — Trim platformio.ini

Add a dedicated env block:

```ini
[env:xiao_esp32s3_plus]
extends = env:esp32s3dev_16MB_opi  ; or the closest existing S3 env — verify
board = seeed_xiao_esp32s3
build_flags =
  ${env:esp32s3dev_16MB_opi.build_flags}
  -D WLED_BRAND="\"WLED-Lite\""
  -D WLED_PRODUCT_NAME="\"WLED-Lite\""
  -D WLED_RELEASE_NAME="\"XIAO_ESP32S3_LITE\""
  -D WLED_VERSION=0.1.0-lite
  -D WLED_REPO="\"MakingACompany/WLED-Lite\""
  ; USB-CDC must stay enabled for programming + debug over USB-C
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=1
```

Reduce `default_envs` to just `xiao_esp32s3_plus` (and maybe one bare ESP32-S3 dev board for upstream-parity smoke tests). Remove ESP8266 envs entirely.

Verify before merging:
- `pio run -e xiao_esp32s3_plus` succeeds
- USB-CDC programming works
- Default AP SSID becomes `WLED-Lite-AP`
- About page reflects new product name

---

## Repo / dev setup notes

- Working directory: `~/Code/WLED/` (on work Linux machine)
- Dev tools: VSCode + PlatformIO and/or ESP-IDF extensions
- GitHub auth on this machine: HTTPS with PAT, `credential.helper=store`, file `~/.git-credentials` mode 600
- Per-repo commit identity: `Aaron Loar <1115581+aaronloar@users.noreply.github.com>` (set with `git config user.email/user.name`, not global)
- Working from home will use a different machine — clone fresh and configure the same PAT + commit identity, or generate a new PAT scoped to this repo.

---

## When picking this back up

1. Pull latest: `git fetch upstream && git fetch origin && git pull origin main`
2. Re-read this file
3. Resume at the next unfinished task above. The recommended order is sequential (each task informs the next), but tasks #4 (feature audit) and #5 (UI design) can be done in either order before #6 onward.
4. After each substantive task: commit with a focused message, push, and update the status table here.
