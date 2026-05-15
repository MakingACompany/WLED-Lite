# WLED-Lite — INA219 Current Monitor (Safety-Net ABL)

Task #9 of the [project plan](WLED-LITE-PLAN.md). A self-contained usermod (`usermods/INA219/`) that reads an INA219 over I2C and protects the device when *measured* current exceeds the configured limit, layered on top of WLED's existing math-based ABL.

## What's already in upstream (math ABL)

The Auto Brightness Limiter in `wled00/bus_manager.cpp` runs every frame:

1. `BusManager::show()` calls `applyABL()`.
2. For each bus, `BusDigital::estimateCurrent()` sums the requested per-pixel color channels and multiplies by the per-LED current constant (`_milliAmpsPerLed`).
3. If the estimated total exceeds the global cap (`_gMilliAmpsMax`), brightness is scaled down via `applyBriLimit()`.

This is fast and frame-accurate — but only as good as the configured mA/LED constant. Real-world draw can diverge: LED batch variation, voltage drop across long wire runs, ambient temperature, etc.

## What this usermod adds (measured safety net)

The INA219 sits in series with the LED supply rail and reports actual current. The usermod polls it on a slow timer (~200 ms by default) and, when measurement exceeds the configured cap by more than the hysteresis margin (110 % default), directly reduces `bri` to bring the device back into safe range.

The two layers cooperate:

| Layer | When it runs | Source of truth | Reaction time |
|---|---|---|---|
| Math ABL (upstream) | Every frame (60+ Hz) | Per-pixel color × `mA/LED` | Sub-frame |
| INA219 (this usermod) | Every 200 ms (configurable) | Shunt measurement | ~Poll interval |

Math ABL handles 99 % of cases. The INA219 catches the edge cases where the estimate misses — and only acts when there's a real safety problem.

## Topology

**Phase 1 (current):** one INA219 monitoring the carrier's main 5 V supply. Simplest BOM, simplest firmware.

**Phase 2 (future):** per-LED-bus INA219s for fine-grained protection. The usermod's internal data model already uses an array (`_sensors[INA219_MAX_SENSORS]`) sized to 1 for now; expanding to 8 sensors is a `#define` change plus iterating in `setup()` and the readFromConfig parser to take per-sensor settings. No public-API change.

## Implementation

| File | Role |
|---|---|
| `usermods/INA219/INA219.cpp` | The usermod itself. Wraps `wollewald/INA219_WE` (same author as the INA226_v2 library upstream uses). Single class `UsermodINA219` with standard hooks. |
| `usermods/INA219/library.json` | Declares dependency on `wollewald/INA219_WE ~1.3.8`. |
| `usermods/INA219/README.md` | Wiring + admin-UI reference. |

The class hooks into:

- `setup()` — instantiate the driver. With `AutoDetect` on (default), probe addresses 0x40, 0x41, 0x44, 0x45 and bind to the first that ACKs. Otherwise honor the configured `I2CAddress`.
- `loop()` — poll on the configured interval, skipping while `strip.isUpdating()` to avoid interfering with LED timing. After every poll, call `enforceCap()` which compares the sum of all sensors' last readings against the sum of their `MaxCurrentMa` limits, applies hysteresis, and reduces `bri` proportionally if over.
- `addToJsonInfo()` — publish current / voltage / power into `/json/info`'s `"u"` block so the admin UI shows live readings.
- `addToConfig()` / `readFromConfig()` / `appendConfigData()` — persist and serve the per-device settings form.

Build flag: `-D USERMOD_INA219` is **not** needed — usermod selection is via `custom_usermods = audioreactive INA219` in `[env:xiao_esp32s3_plus]`. The `load_usermods.py` PIO pre-script picks up the entry and includes the directory.

## Hysteresis math

When all sensors' measurements sum to `measured_mA` and their limits sum to `limit_mA`:

```
trigger_mA = limit_mA × hysteresisPct / 100        # default 110 %
if measured_mA > trigger_mA:
    newBri = bri × limit_mA / measured_mA          # scale proportionally
    if newBri < bri:
        bri = newBri
        stateUpdated(CALL_MODE_DIRECT_CHANGE)
```

The 10 % hysteresis margin matters because of the speed mismatch with math ABL: if the safety net fires at exactly the limit, the next frame's math ABL might dim the strip below the limit, the INA219 reads a lower value next poll, the safety net releases bri, etc. — oscillation. The 10 % buffer means the safety net only fires when measurement genuinely exceeds the math estimate's expectation.

## Default config

Compile-time, overridable via `-D` or per device via admin UI:

| Setting | Default | Notes |
|---|---|---|
| `Enabled` | `false` | Admin opts in per device. Devices without an INA219 wired up should leave this off. |
| `AutoDetect` | `true` | Probes 0x40, 0x41, 0x44, 0x45. |
| `I2CAddress` | `0x40` | Used when AutoDetect is off. |
| `ShuntMilliOhms` | `100` | 0.1 Ω shunt (common breakout-board value). |
| `MaxCurrentMa` | `3200` | Conservative cap for a small sign; adjust per device. |
| `PollIntervalMs` | `200` | 5 Hz polling. Fast enough to catch overshoot before LED damage; slow enough to leave I2C bus open for other devices. |
| `HysteresisPct` | `110` | Safety net engages at 110 % of `MaxCurrentMa`. |
| `EnforceCap` | `true` | False = telemetry only (don't touch `bri`). Useful for calibration. |

## Coexistence with other I2C devices

The XIAO Plus carrier shares one I2C bus across:
- INA219 (this usermod, 0x40–0x45)
- OLED display (typically 0x3C / 0x3D, deferred to a future task)
- I2C PWM driver for fans / accessories (typically 0x40–0x47 — **collision risk** with the INA219)
- Other sensors (BME280 at 0x76/0x77, etc.)

The PWM driver and INA219 both default to 0x40. Conflict resolution at carrier-board design time: strap the PWM driver to 0x42 or higher, leaving 0x40/0x41 free for INA219(s).

Auto-detect makes it easy to recover from address-strap mistakes — the usermod simply iterates until something ACKs.

## What this does NOT do (open follow-ups)

- **Per-bus current monitoring** — Phase 2 hardware design choice; deferred until the carrier-board v1 ships and a customer asks for it.
- **Surge protection** — a fast spike (e.g. all-white flash) can over-current the supply faster than the 200 ms poll catches. Math ABL is the right defense for that. INA219 is a safety net for *sustained* over-current, not transients. If transient protection becomes important, drop the poll interval to ~20 ms or use the INA219's hardware-alert pin.
- **MQTT publishing** — INA226_v2 has full Home-Assistant-discovery support. Skipped here for v1 to keep the usermod compact; can copy the pattern verbatim if the maintainer wires MQTT for a deployment.
- **Power-rail monitoring beyond LED supply** — if the carrier ever adds a separate logic-rail INA219, that's another sensor in the array.
- **Calibration helpers** — the user has to enter the shunt value manually. A "measure my LEDs" wizard could compute shunt-implied current and offer to update the mA/LED constant. Worth doing if customer signs vary widely.
- **Bench-test on hardware** — math is correct on paper; verify against a real INA219 + known load before shipping.

## Footprint

| Resource | Cost |
|---|---|
| Flash | +4.5 KB (the `wollewald/INA219_WE` library is the bulk of it) |
| RAM | +64 bytes (single sensor; per-sensor struct is ~32 bytes plus the library's driver instance) |
| I2C traffic | ~6 register reads per poll = ~200 µs per 200 ms tick. Negligible. |
| Build flag | None — usermod selection is via `custom_usermods` in `platformio.ini`. |
