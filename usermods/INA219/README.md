# INA219 Current-Monitor Usermod

WLED-Lite usermod that reads an INA219 over I2C and acts as a safety net over the math-based ABL. See `docs/current-limit-ina219.md` for the design rationale.

## Wiring

| Carrier signal | XIAO ESP32-S3 Plus pin |
|---|---|
| SDA | GPIO 5 (D4) |
| SCL | GPIO 6 (D5) |
| INA219 V+ / Vin− | wired across a shunt resistor in series with the LED supply rail |
| INA219 Vbus | LED supply (e.g. 5 V) |
| INA219 VCC | 3.3 V |
| INA219 GND | shared with XIAO GND |

The four I2C address-select straps (`A0`, `A1`) give addresses 0x40, 0x41, 0x44, 0x45. The usermod auto-detects on the four standard addresses unless `AutoDetect` is unchecked.

## Compile-time defaults

All overridable from `-D` in `platformio.ini` or the admin UI:

| Macro | Default | Meaning |
|---|---|---|
| `INA219_DEFAULT_ADDRESS` | `0x40` | I2C address probed first (auto-detect overrides) |
| `INA219_DEFAULT_SHUNT_MICRO_OHMS` | `100000` | shunt resistance in μΩ (100 mΩ typical) |
| `INA219_DEFAULT_MAX_CURRENT_MA` | `3200` | safety-net cap in mA |
| `INA219_DEFAULT_POLL_INTERVAL_MS` | `200` | how often to read |
| `INA219_DEFAULT_HYSTERESIS_PCT` | `110` | trigger at 110 % of max (avoid oscillation) |
| `INA219_ENABLED_DEFAULT` | `false` | usermod off by default; flip in admin UI per device |

## Admin UI

Under **Config → Usermods**:

- `Enabled` — turn the module on/off
- `AutoDetect` — probe addresses 0x40/0x41/0x44/0x45 at boot; otherwise honor the configured `I2CAddress`
- `EnforceCap` — true = reduce `bri` when measured current > limit × hysteresis ; false = telemetry only
- `I2CAddress` — manual address override (hex)
- `ShuntMilliOhms` — shunt resistance (m Ω)
- `MaxCurrentMa` — configured safety-net cap (mA)
- `PollIntervalMs` — measurement period
- `HysteresisPct` — % of `MaxCurrentMa` at which the safety net engages (`110` = 10 % grace before action)

## Telemetry

`/json/info`'s `"u"` (usermod) section gains three entries:
- `INA219 Current` (A)
- `INA219 Voltage` (V)
- `INA219 Power` (W)

These show "no device" if the chip didn't ACK on the bus.
