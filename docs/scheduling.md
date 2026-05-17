# WLED-Lite — Daily on/off schedule

A small finite-state-machine on top of WLED's existing timer system that gives the **end user** (not the admin) the ability to set a daily on/off schedule from the slim user UI, **without** needing the admin PIN. Designed for the "Dad's sign turns on at 6 pm, off at midnight, every day" use case.

## What the user sees

Two cards on the main page:

- **Turn off in** — one-shot countdown timer (Never / 1 hr / 4 hr / 12 hr). Hidden underneath is WLED's nightlight; setting "1 hr" tells WLED to fade off in 60 minutes.
- **Daily schedule** — two time pickers (On at, Off at) and a Save button. Sets two timer entries that fire every day.

The daily schedule activates whatever state the sign was in at the moment "Save" was tapped. If the user changed the color/effect/brightness and wants the schedule to use the new look, they tap Save again — the current state becomes the new "Daily ON" preset.

## Architecture

| Piece | File / location |
|---|---|
| Reserved preset slots (250 = Daily ON, 249 = Daily OFF) | Hardcoded constants in `wled00/wled_lite_schedule.cpp` |
| Timer entries in `timers` vector | Set via WLED's `addTimer()` (in `fcn_declare.h`) |
| HTTP endpoint `POST /wled-lite/schedule` | `wled00/wled_lite_schedule.{h,cpp}` |
| UI (Daily schedule card + JS) | `wled00/data/index.htm`, `index.css`, `index.js` |
| Build flag | `-D WLED_LITE_SCHEDULE` |
| Persistence | Timers in `cfg.json`, presets in `presets.json` — both survive reboot, both held in the device's LittleFS partition (NVM equivalent on ESP32-S3) |

## Why a separate endpoint (and not `/json?cfg`)

`/json?cfg` is PIN-gated — only the admin can change the full configuration. The whole point of the daily-schedule feature is that the **end user** (who doesn't know the admin PIN) can set it. So we expose a narrow, scope-limited endpoint that can ONLY:

1. Save the current device state as preset 250 ("Daily ON")
2. Save an off-state as preset 249 ("Daily OFF")
3. Add or replace two entries in the `timers` vector at the requested times, with all weekdays enabled
4. Trigger a write of `cfg.json`

Nothing else is reachable through this endpoint. Anyone on the local network can hit it — the same threat model that already applies to the slim UI's color, effect, brightness, etc.

## API

`POST /wled-lite/schedule`, content-type `application/json`.

**Set the schedule:**
```json
{ "on": "18:00", "off": "00:00" }
```
Times are local time on the device (so the device's timezone must be set in `Settings → Time` first — see "Prerequisites" below). 24-hour format. The same schedule applies every day.

**Clear the schedule:**
```json
{ "clear": true }
```
Removes the timer entries. The reserved presets (249 / 250) are left in place since they're harmless and re-saving the schedule will overwrite them.

**Responses:**
- `200 OK` body `{"ok": true}` on success
- `400` body `{"error": "..."}` on bad input

## Prerequisites for accurate scheduling

The scheduler fires when local wall-clock time matches the timer's `hour:minute`. So three things have to be right:

1. **WiFi connected** — needed for NTP sync. Without it, the device's clock is whatever it was when it last had WiFi (or zero, on a fresh boot).
2. **NTP enabled, pointing at a real time server** — WLED-Lite ships with `WLED_NTP_ENABLED=true` and `WLED_LITE_NTP_SERVER=time.nist.gov` (a .gov-operated stratum-1 server). Sync interval is `NTP_SYNC_INTERVAL=300` (5 minutes), set as a build flag. These are all overridable in `Settings → Time`.
3. **Timezone set** — the maintainer sets this in `Settings → Time` during onboarding. Otherwise "18:00" means 18:00 UTC, which is wrong for any non-UTC time zone.

If the device boots without WiFi (e.g., the home router is down) the schedule won't fire correctly until NTP can re-sync. There is no battery-backed RTC on the XIAO Plus.

## Open follow-ups

- **Manual time entry** for offline / no-WiFi installations. A small UI section that lets the end user enter the current time, plus internal logic to keep the offset accurate during the session. Drifts whenever the device reboots without WiFi, but better than nothing. Captured as a deferred task.
- **Weekday selection** — currently the schedule fires every day. A "Weekdays only" / "Weekends only" / per-day toggle would be a natural extension. The `weekdays` bitmask on each timer entry already supports it.
- **Sunrise/sunset offsets** — WLED supports timers like "30 minutes after sunset". Useful for outdoor signs. Same underlying timer struct, different `hour` value (`TH_SUNRISE` / `TH_SUNSET` constants). The slim UI's time pickers could grow a "sunrise + 30min" mode.
- **Show current schedule when re-opening the UI** — the time pickers currently default to 18:00 / 00:00 and don't reflect the saved schedule. Reading the schedule out would require either a new GET endpoint or surfacing it through `/json/info`. Low priority since the user knows what they set.
