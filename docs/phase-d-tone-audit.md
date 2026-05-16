# WLED-Lite — Phase D Settings-Page Tone Audit

A focused review of the **admin** `/settings/*` pages for language and visual choices left over from upstream that read as "tinkerer / power user" rather than "deployment admin / installer." Recorded here rather than applied because every HTML edit creates an upstream-merge conflict — better to tee these up as a polish batch landed before a commercial release.

## Why bother

The admin is the maintainer — fluent enough to set up a sign — but the goal is to feel like fielding firmware for a product, not configuring a hobby device. Two specific reasons to clean this up:

1. **Customer over-the-shoulder.** A customer watching the maintainer configure their sign sees the admin UI. "WiFi Power" + "ESP-NOW Wireless" + "IR Remote" sound like a HAM radio shack. Slightly friendlier labels lift the perceived polish without hiding capability.
2. **Reduced help-desk burden.** Field installers (eventually not the same person as the maintainer) will read these labels and ask less if they're unambiguous.

## Findings, by page

### `/settings` (hub)

- Lists every sub-page in a flat menu, including ones we've removed (`2D Configuration`) or marked admin-only (`DMX Output`, `Sync Interfaces`, `Pin Info`). Visual cleanup: hide the menu entries whose features are compile-time disabled. This is already partly handled (`#ifndef WLED_DISABLE_2D` around the 2D link) — the audit is a pass to make sure each feature flag wraps its hub entry.

### `/settings/wifi`

- `Configure Access Point` — fine for an installer. Consider sub-label "(only needed if no WiFi available)".
- `ESP-NOW Wireless` — fine but could read "Peer-to-peer sync (advanced)" to set expectations. The current label is technically accurate; tone change is optional.
- `WiFi Power` — extremely low-level. Most admins don't know what to do with this. Consider hiding behind an "Advanced" disclosure.

### `/settings/leds`

- `LED outputs:` — good
- `Color & White` — fine
- `Hardware setup` heading wraps buttons and IR remote — but IR is `WLED_DISABLE_INFRARED`'d in WLED-Lite, so the heading currently appears next to nothing once IR is hidden. Audit: remove the heading entirely or consolidate with the next section.
- The "1/2/3-shaped" buttons next to LED outputs (`+`/`-`) for adding/removing LED buses are correct but unlabeled. Suggest `aria-label` tweaks for accessibility.

### `/settings/sync`

- `WLED Broadcast` heading — could read "Sync over WiFi" with a one-line "Other WLED devices on this network will mirror state changes."
- `Sync Groups` (8 checkboxes per direction × 2 directions = 16 boxes) — overwhelming. The TL;DR in `docs/multi-controller-sync.md` is "leave on Group 1 unless you have multiple zones." Surfacing that text in the page would prevent confusion.
- `UDP packet retransmissions` — power-user knob; demote to advanced.

### `/settings/time`

- Reads OK. The main issue is that **the user UI doesn't have a schedule wrapper yet** (see `docs/ui-split-design.md` open follow-ups), so end users who want a "turn on at sunset" rule have to ask the admin to configure it here. That's a feature gap, not a tone issue — flagging here for visibility.

### `/settings/sec`

- This is the page our welcome wizard sends people to for PIN setup. Important:
  - **Label of the PIN field** is currently `Settings PIN`. Should be `Admin PIN` to match the welcome wizard's terminology. One label change, one HTML edit.
  - **Help text** alongside the PIN field should mention "Once set, advanced settings require this PIN" so the admin understands the consequence.
- OTA section uses jargon (`Aircoookie & contributors`, `Build version`). Fine; admins look there.

### `/settings/um` (usermods)

- The INA219 usermod's settings form (Task #9) is acceptable: each field has a unit hint via `appendConfigData()`'s `addInfo()` calls. The order is alphabetical by usermod name, which mixes "Audio Reactive" and "INA219" oddly when there are 2+ usermods present. Low priority.

### `/settings/pininfo`

- Pin map view. Useful diagnostic. No tone changes needed.

## What this audit does NOT change

- Functionality. Every form field and toggle stays.
- Layout. No CSS rewriting of `style.css` (used by all settings pages); the styling is workable.
- Routes. Same URLs, same form submissions.

## Implementation plan when this lands

Before a commercial release, apply each finding above. Order:

1. `/settings/sec`: PIN label + help text. **One small edit, blocks the rest of the deployment story.**
2. `/settings` hub: ensure every link is wrapped in the matching `#ifdef` for its feature.
3. `/settings/wifi`: hide `WiFi Power` behind an Advanced disclosure.
4. `/settings/sync`: collapse the 16-bit-group UI into a single "Sync group (1-8)" picker for simple installs, with the matrix view in an Advanced disclosure.
5. Everything else: skip unless customer feedback flags it.

Each item is 1-3 lines of HTML. Doing all in one commit keeps the upstream-merge diff localized to "WLED-Lite admin polish." Worth aligning with the commercial-release milestone so the commit lands close to first ship.

## Why this is deferred and not done now

- The current bench-test priority is verifying the core firmware works on a real XIAO Plus. Touching admin HTML at this stage adds risk to that bench-test.
- These changes accumulate upstream-merge surface area. Better to land them as a batch right before a release, after a known-good merge from upstream.
- No customer-facing impact yet — the maintainer-only audience tolerates the upstream-leftover language.
