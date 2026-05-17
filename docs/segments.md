# WLED-Lite — Segments (per-letter / per-region control)

Lets the maintainer carve a single LED strip into named regions and lets the end user pick any region from the slim UI to control its color, effect, and brightness independently. The intended use case: a four-letter sign (e.g. **L · A · B · B**) on one continuous strip — the user picks "L" and changes just that letter to red.

## How it works

Two pieces, one per audience:

| Audience | Page | What you do there |
|---|---|---|
| **Admin** (you) | `/wled-lite/segments` (also linked from the settings hub as **Segments**) | Add / edit / delete named regions. Each region has a name, a start pixel index, and a stop pixel index. PIN-gated. One-time setup per device. |
| **End user** (Dad) | The slim UI at `/` | A row of friendly pills appears above the controls — "All", plus one per segment. Tapping a pill makes the color picker, effect picker, and brightness slider target just that segment. "All" applies to every segment at once. |

Below the hood, segment definitions are part of WLED's normal state JSON (`/json/state` -> `seg: [...]`), which means they persist in WLED's cfg.json the same way segments created via upstream's main UI did.

## Admin: defining segments

Open `/wled-lite/segments` (or tap **Segments** in the settings hub). You'll see a table:

| Name | Start | Stop | |
|---|---|---|---|
| `L` | 0 | 50 | × |
| `A` | 50 | 100 | × |
| `B1` | 100 | 150 | × |
| `B2` | 150 | 200 | × |

- **Name** — what the end user sees on the pill. Keep it short (one letter, two letters, "Top", "Left", etc.). Names must be unique.
- **Start / Stop** — pixel indices on the strip, zero-based, half-open. A 50-pixel region starts at 0 and stops at 50 (covering pixels 0..49).
- **×** — remove this segment.
- **+ Add segment** — adds a new row. Defaults to the next available ID and a 30-pixel range starting where the last segment ended.

Click **Save** to commit. Segments take effect immediately, persist across reboot, and appear in the slim UI on next page load.

## User: picking and controlling a segment

In the slim UI:

- A pill row appears above the power toggle, **only when 2+ segments are defined**.
- The first pill is **All** (selected by default). Behind it, one pill per segment with its name.
- Tap any pill to make subsequent color / effect / "turn off in" / schedule changes target just that segment.
- Brightness still applies device-wide — it's a global value in WLED.

The displayed color and effect snap to the active segment's current state when you switch pills (the slim UI refetches state on selection change).

## What's deliberately *not* exposed in the user UI

- Pixel ranges (start / stop) — confusing for non-technical users; admin-only concern.
- Segment IDs — internal; we use the admin-set name instead.
- Grouping / spacing / offset — advanced segment features for matrix-mode use cases.
- Per-segment palette and effect-parameter sliders — could be added later as a polish pass, but most sign use cases want one effect + one color per region.

## Open follow-ups

- **Confirm boot-state persistence under aggressive scenarios.** WLED's cfg.json persists the bus and segment configuration, and state changes that touch `seg` array geometry trigger a config save. If a future bench-test shows segments disappearing across reboot, the fix is to also save the new state as a "boot preset" (preset 248) with `bootps` set, so it auto-loads on every boot.
- **Single-segment devices**: the pill row is hidden when there's only one segment. If admin later adds a second, the row appears on the next page reload. Could add a WebSocket-driven dynamic update so the user doesn't have to refresh, but low priority.
- **Per-segment brightness**: WLED supports it (segment-local `bri` field 0-255). Not exposed in the slim UI yet because brightness as "device brightness" matches user mental model; per-segment dimming would clutter the picker. Worth revisiting if a customer asks.
- **Per-segment schedule**: the daily schedule (Task #x) currently captures whole-device state. If you want "letter L pulses red 6-7pm, then all on white 7pm-midnight", that's a real scheduling-language extension — captured here as a v2 wishlist item.
