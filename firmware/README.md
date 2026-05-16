# WLED-Lite — Pre-built Firmware

Three pre-built WLED-Lite binaries are checked in for the Seeed Studio XIAO ESP32-S3 Plus. They all run the **same WLED-Lite firmware** (slim UI, curated effects, brightness-mode buttons, INA219 safety net, security gates, etc.) — only the chip-side **memory configuration** differs.

If you just want to flash and go, see [`../docs/flash-from-windows.md`](../docs/flash-from-windows.md).

## Which variant should I flash?

| Directory | Flash | PSRAM mode | WLED_RELEASE_NAME | When to use |
|---|---|---|---|---|
| `xiao_esp32s3_plus/` | 16 MB | OPI (qio_opi) | `XIAO_ESP32S3_LITE` | **Try this first.** Standard XIAO ESP32-S3 Plus config — 16 MB flash, octal PSRAM. |
| `xiao_esp32s3_plus_8MB/` | 8 MB | OPI (qio_opi) | `XIAO_LITE_8MB` | **Try second** if 16 MB doesn't boot. Same PSRAM mode but smaller flash partition — narrows the issue to flash sizing. |
| `xiao_esp32s3_plus_4M/` | 4 MB | QSPI (qio_qspi) | `XIAO_LITE_4M` | **Try third** if 8 MB also fails. Switches PSRAM mode to QSPI — the prior bring-up attempt that booted successfully was a ~4 MB build, so this matches that working configuration. |

> All three target the **same physical board** (`seeed_xiao_esp32s3`). They differ in how the firmware *talks to* the chip's flash + PSRAM. The XIAO Plus hardware itself is fixed (16 MB + 8 MB OPI), but a smaller-config build will still boot on bigger hardware (it just doesn't use all the available flash/PSRAM).

> **OTA caveat:** each variant has a distinct `WLED_RELEASE_NAME`, which means **OTA updates won't cross between variants.** If you flash `XIAO_LITE_4M` first and decide to OTA up to `XIAO_ESP32S3_LITE`, the device refuses. Re-flash over USB to switch variants.

## What's in each directory

- `firmware.bin` — app image only. For **OTA updates** via the WLED web UI after the device is already running WLED-Lite.
- `merged-flash.bin` — single-file image (bootloader + partitions + OTA-data + app, addressed at `0x0`). For **first-time USB flash** with `esptool`.

## Quick flash (Windows)

Full instructions in [`../docs/flash-from-windows.md`](../docs/flash-from-windows.md). The short version:

```
python -m pip install --user esptool
python -m esptool --chip esp32s3 --port COM3 --baud 460800 write-flash 0x0 firmware\xiao_esp32s3_plus\merged-flash.bin
```

Replace `COM3` with the actual COM port from Device Manager.

## What's baked into these binaries

- **Version:** `0.1.0-lite` (from `package.json`)
- **Brand:** `WLED-Lite` (overrides upstream `WLED`). Default AP SSID becomes `WLED-Lite-AP`.
- **Build commit:** matches the commit history of this checkout — re-build to capture later commits.

After flashing and connecting WiFi, the welcome wizard walks you through the three-step setup: WiFi → Set admin PIN → Use your sign. Until the admin PIN is set, `/` redirects to `/welcome` (so a fresh device can't be misused on a shared network).

## Re-generating these files

```
pio run -e xiao_esp32s3_plus -e xiao_esp32s3_plus_8MB -e xiao_esp32s3_plus_4M
# Then re-run the merge-bin step per docs/dev-setup-windows.md, with the correct
# --flash-size per variant (16MB / 8MB / 4MB).
```

The CI workflow (`.github/workflows/wled-lite-build.yml`) also produces these as downloadable artifacts on every push to `main`.
