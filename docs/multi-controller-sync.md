# WLED-Lite — Multi-Controller Sync

Task #10 of the [project plan](WLED-LITE-PLAN.md). Two devices on the same shop floor should mirror each other — same color, same effect, same brightness — without the installer touching a settings page. This doc covers what's already in upstream WLED, the one default we flip, and the recommended onboarding flow.

## TL;DR for the installer

Default state on a fresh WLED-Lite device:

- **WiFi sync is on**. Devices that share a WiFi LAN and a sync group (1 by default) will mirror state changes — color, effect, brightness, palette — automatically.
- **ESP-NOW is off**. Admin opts in per device when WiFi isn't available or reliable.
- **No "pairing" step exists or is needed for WiFi sync** — devices in the same group on the same LAN find each other automatically.

If you want two devices to sync: connect both to the same WiFi (or even the same WLED-Lite-AP), leave the defaults alone, change color on one. The other follows within a few hundred ms.

If you want **groups** of devices that don't mirror each other (e.g., two storefronts in the same WiFi LAN): give each group a different bit in the sync mask (Settings → Sync → Send/Receive group 1 / 2 / …).

## Architecture, briefly

There are two transports for the same packet shape:

| Transport | Port | Range | Auth | When it makes sense |
|---|---|---|---|---|
| **UDP notify** | 21324 / 65506 | One WiFi LAN | None (LAN-trusted) | Default. Multiple signs in one shop on one router. |
| **ESP-NOW** | n/a (radio) | Direct radio, ~50 m line of sight | None — broadcast-only | Outdoor signs, intermittent WiFi, store with no router. |

Both transports send the same `notify()` payload — a ~37-byte global state + 36 bytes per segment. They're not exclusive; a device can have either or both enabled. ESP-NOW broadcasts to FF:FF:FF:FF:FF:FF (any device in radio range); UDP broadcasts to the LAN broadcast address.

**Group routing** is identical for both transports: an 8-bit `syncGroups` bitmask on send side, `receiveGroups` on receive side. Receiver drops packets whose source-group bits don't overlap with its receive bits. This is how multiple groups of devices coexist on the same WiFi or in the same RF radius.

**Per-aspect receive filters** let an installer say "I want to receive color changes from peers but ignore their segment geometry" — useful for mixed installations (one tall storefront sign + one shelf strip; they should share color but not bus length). The fields are:

- `receiveNotificationBrightness` (default true)
- `receiveNotificationColor` (default true)
- `receiveNotificationEffects` (default true)
- `receiveNotificationPalette` (default true)
- `receiveSegmentOptions` (default false — apply incoming effect-mode parameters like speed/intensity sliders)
- `receiveSegmentBounds` (default false — apply incoming segment geometry)

## The one default WLED-Lite changes

Upstream's `wled00/wled.h` ships:

```cpp
WLED_GLOBAL bool sendNotifications   _INIT(false);  // master OFF
WLED_GLOBAL bool sendNotificationsRT _INIT(false);
```

Out of the box, no sync packets are sent even though every other default (`notifyDirect=true`, `notifyButton=true`, `syncGroups=0x01`, all receive flags on) is correct. The installer has to enable the master switch in `Settings → Sync` to get sync to work.

WLED-Lite flips this one bit:

```cpp
#ifdef WLED_LITE_SYNC_DEFAULTS
WLED_GLOBAL bool sendNotifications   _INIT(true);   // master ON
WLED_GLOBAL bool sendNotificationsRT _INIT(true);
#else
... upstream defaults ...
#endif
```

Build flag `-D WLED_LITE_SYNC_DEFAULTS` lives in `[env:xiao_esp32s3_plus]`. The upstream merge surface is one localized block — easy to review when upstream changes those defaults.

**ESP-NOW stays opt-in.** Enabling it by default would broadcast every state change to anything in radio range with zero authentication, and would keep the ESP-NOW radio always-on (~80 mA). Two reasons to leave it for admin opt-in:

- **Security.** A shop on a busy street can be within ESP-NOW radio range of a neighbor's WLED device. Default-off means a customer's device doesn't broadcast to anyone in radio range.
- **Power.** For battery-backed installations the radio idle current matters.

To enable ESP-NOW per device, the admin sets it in `Settings → WiFi → Enable ESP-NOW` + `Settings → Sync → Use ESP-NOW sync`. Both flags need to be on.

## Recommended onboarding flow

When the AP-mode welcome wizard (Task #5 work) lands, add a "Sync group" step. The maintainer enters a single digit 1–8 (or leaves the default 1). For installations where all devices should sync: leave the default. For installations with multiple separate groups: assign each group a number.

Until that wizard lands, the documented flow is:

1. (Phase A — already shipped) Connect device to WiFi via `welcome.htm`.
2. (Phase A — already shipped) Set admin PIN.
3. (Phase B — Task #5) On the same welcome page, optionally enter a sync group number (default 1).
4. (Done) Power on the second device. Provided it's on the same WiFi and the same sync group, it mirrors immediately.

For ESP-NOW (outdoor / no-WiFi installations):

1. After WiFi setup (or skipping WiFi entirely), the admin enters `Settings → WiFi` and ticks **Enable ESP-NOW**.
2. In `Settings → Sync`, ticks **Use ESP-NOW sync**.
3. Reboots. Both devices repeat the same.
4. (Done) Devices within radio range mirror each other.

## Per-aspect tuning notes for the installer

A few non-obvious choices the admin might want to know about.

**Segment options / Segment bounds**. These default OFF and should usually stay off in mixed installations. They control whether incoming packets reshape *the geometry* of the receiver's segments (e.g., change segment start/stop pixels). Two signs of different sizes shouldn't share geometry; only the colors / effects / brightness.

**Sync group bits are independent on send and receive.** A device can listen to group 1 but broadcast to group 3 — useful as a "relay" or for one-way slave devices. Not common; mention only if asked.

**UDP retransmissions** (`udpNumRetries`, default 0). Each retransmission adds 250 ms latency between change and visible peer update. Increase only if a flaky WiFi is dropping packets; the cost is sync latency, not throughput.

**Receive realtime** (`receiveDirect`) is unrelated to peer sync — it's the toggle for accepting `E1.31` / `Art-Net` / `DDP` realtime pixel streams from a sequencer. Leave it as upstream default (on) unless the device is dedicated to one role.

## What's *not* in this commit (deferred to Task #5 Phase B)

The `settings_sync.htm` page is a long form with eight independent group checkboxes per direction, six per-aspect receive toggles, four send-trigger toggles, port numbers, retransmit counts, and node-list controls. It's appropriate for an admin / power user — it just isn't compact. WLED-Lite leaves it as-is in this commit.

When Task #5's "slim user UI" work lands, the welcome wizard gets a "Sync group" step, and a future polish pass could collapse `settings_sync.htm` into:

- A prominent "Sync enabled (master switch)" toggle
- A single "Sync group" number 1–8 (replaces the 16 bit-flags with one digit, defaults to OR'd `syncGroups | receiveGroups`)
- An "Advanced…" disclosure with the rest

That's a follow-up; the current page works, just isn't friendly. The end user never sees it — `Settings/*` is admin-only by Task #5's auth model.

## Other follow-ups

- **ESP-NOW security.** Broadcast-only with no auth is fine for a single deployment but is a real concern for commercial multi-tenant scenarios. A "pair MAC" mode (only accept ESP-NOW packets from a pre-registered list) is on the carrier-v2 wishlist; upstream supports this for WiZ Mote remotes already (`linked_remotes` vector) — extending it to peer state sync is a usermod-scale addition.
- **Bench-test on hardware.** This commit changes a default; it doesn't change any logic. Still worth confirming on real hardware that two flashed devices on the same WiFi mirror without any settings page visited.
- **mDNS device naming.** The carrier's device name appears as `wled-<chipid>.local`. Useful for admins; not user-facing. Setting a human-readable name (`MainStorefront-Sign-A`) during onboarding is part of the Task #5 wizard.
