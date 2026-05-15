# WLED-Lite — XIAO ESP32-S3 Plus Pin Map

Task #7 of the [project plan](WLED-LITE-PLAN.md). The firmware-side pin assignments that drive the custom carrier-board layout. Build flags in `[env:xiao_esp32s3_plus]` apply these as the factory defaults; the admin UI (`/settings/leds` and `/settings/pin`) can override any of them per-device at runtime.

## Module pinout (XIAO ESP32-S3 Plus)

| Header | XIAO label | GPIO | Notes |
|---|---|---|---|
| Front | D0 | 1 | ADC1, used here as **Button 1** |
| Front | D1 | 2 | ADC1, used here as **Button 2** |
| Front | D2 | 3 | **Strapping pin — do not use** (boot mode) |
| Front | D3 | 4 | **LED Bus 1** |
| Front | D4 | 5 | **I2C SDA** |
| Front | D5 | 6 | **I2C SCL** |
| Front | D6 | 43 | HW UART TX — kept free for serial-adapter debug |
| Front | D7 | 44 | HW UART RX — kept free for serial-adapter debug |
| Front | D8 | 7 | **LED Bus 2** |
| Front | D9 | 8 | **LED Bus 3** |
| Front | D10 | 9 | **LED Bus 4** |
| Plus sub-header | — | 10 | **LED Bus 5** |
| Plus sub-header | — | 11 | **LED Bus 6** |
| Plus sub-header | — | 12 | **LED Bus 7** |
| Plus sub-header | — | 13 | **LED Bus 8** |
| Plus sub-header | — | 38 | Future expansion (status LED, relay, etc.) |
| Plus sub-header | — | 39 | Future expansion |
| Plus sub-header | — | 40 | Future expansion |
| Plus sub-header | — | 41 | Future expansion |
| Plus sub-header | — | 42 | Future expansion |

20 GPIOs total: 11 on the front breakout + 9 on the Plus sub-header.

## What this gives us

- **8 simultaneous LED outputs.** ESP32-S3 has 4 RMT channels + 8 I2S/LCD parallel channels = 12 independent digital LED buses possible (`WLED_MAX_DIGITAL_CHANNELS` in `wled00/const.h:88`). 8 strips fits comfortably with channels to spare.
- **Shared I2C bus** on the XIAO default pins (GPIO 5/6) for INA219 current sensing, OLED display, I2C PWM driver, and any future sensors. Multiple devices share the bus.
- **Two buttons** (short-press / long-press behaviours wired up in Task #8) on GPIO 1/2.
- **USB-C programming + serial debug** stays on the module's USB-CDC peripheral (no GPIO cost).
- **5 unused Plus-only GPIOs (38–42)** kept as expansion headroom — typical uses on a follow-up carrier rev would be a status LED, relay output, the SK6812 RGBW indicator that's a popular sign-controller add-on, or extra button inputs.

## Pins NOT routed to the carrier (and why)

| GPIO | Reason |
|---|---|
| 0 | BOOT button on the module itself; ESP32-S3 strapping pin — not broken out |
| 3 | Strapping pin (boot mode selection on ESP32-S3); broken out at D2 but **deliberately unused** |
| 19, 20 | USB-JTAG D-/D+; locked by `ARDUINO_USB_CDC_ON_BOOT=1` for USB-C programming. Not on the XIAO breakout anyway. |
| 22–37 | OPI flash + 8MB octal PSRAM (we build with `memory_type=qio_opi`). Not on the XIAO breakout. |
| 45, 46 | Strapping pins. Not on the XIAO breakout. |
| 43, 44 | Hardware UART TX/RX. Available on the XIAO front breakout (D6/D7) but **kept unused** so a USB-serial adapter can be plugged in for bench debug without contention. |

WLED's pin manager (`wled00/pin_manager.cpp:206-260`) refuses to allocate any of the blocked-by-hardware pins, so even if a user tries to set one via the admin UI, it fails gracefully.

## Build-flag defaults

These live in `[env:xiao_esp32s3_plus]` in `platformio.ini` and become the factory default on first boot. Each is overridable from the admin UI per device.

```ini
;; 8 LED outputs: front pins D3,D8,D9,D10 + Plus sub-header pins
-D DATA_PINS=4,7,8,9,10,11,12,13
-D LED_TYPES=TYPE_WS2812_RGB
-D PIXEL_COUNTS=30,30,30,30,30,30,30,30
;; 2 buttons on D0,D1 (ADC1-capable, easy front access)
-D BTNPIN=1,2
;; I2C on XIAO defaults D4 (SDA), D5 (SCL)
-D HW_PIN_SDA=5
-D HW_PIN_SCL=6
```

`PIXEL_COUNTS` sets `30` per bus as a sensible bring-up default — every output lights, the user reconfigures count and color order in the admin UI to match their actual hardware. Total of 240 pixels at boot is well under any current-cap or PSU concern.

## Runtime reconfiguration

The carrier-board defaults are not contractual. From `/settings/leds`:

- Any of the 8 buses can be deleted, retyped (WS2811, SK6812 RGBW, APA102, …), or reassigned to a different GPIO.
- Pixel count, color order, white-channel calibration, current cap per bus are all per-bus runtime settings.
- A bus that's not present on a given customer's sign can be left empty (zero pixels) without affecting the others.

From `/settings/pin`:

- Button GPIOs can be moved.
- Per-button macro / press / long-press / double-press actions are configured here. Task #8 layers the "brightness-mode state machine" on top of this.

From `/settings/um` (usermod settings):

- INA219 I2C address, OLED address, fan-driver address, etc. — Task #9 work.

## What this does **not** decide

- **Per-bus current cap.** Task #9 (INA219 measured feedback). Until then, ABL applies a math-only cap across all outputs.
- **Button behaviour** (short / long / "brightness mode" state machine). Task #8.
- **Carrier-board PCB layout, connector choice, level-shifters, fuses, power distribution.** Out of scope for firmware; the maintainer designs the PCB to deliver these GPIOs through the appropriate level-shifters/connectors to the field-wireable LED strip outputs and I2C / button breakouts.

## Open follow-ups

- **Level shifting.** ESP32-S3 GPIOs are 3.3V. WS2812-family strips typically expect 5V data. Carrier board needs a level shifter (74AHCT245 or similar) on each of the 8 LED outputs. Firmware doesn't care.
- **Bus order on the silk.** Whatever label the carrier-board silk uses for "Output 1 … Output 8" should map to GPIO 4, 7, 8, 9, 10, 11, 12, 13 in that order, so admin-UI bus indices match the physical connectors a user sees.
- **Reverse-mount the Plus sub-header pads?** A choice for the carrier-board designer — these are surface pads on the back of the module, so the carrier has to either mount the module on a socket (loses the sub-header pads) or solder it directly to a footprint that exposes the sub-header pins. Worth deciding before fabricating the first carrier prototype.
- **Decoupling / power.** The XIAO module's onboard 3V3 regulator handles a handful of mA from logic; high-current LED strips need their own 5V supply with the grounds tied together. Carrier board responsibility.
