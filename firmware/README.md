# WLED-Lite — Pre-built Firmware

Pre-built ESP32-S3 binaries are checked in here so a freshly cloned repo can flash a board without re-running PlatformIO. If you need a build for a different config or a newer commit, recompile — see [`../docs/dev-setup-windows.md`](../docs/dev-setup-windows.md).

## Which directory should I use?

| Directory | Board / chip | Flash | PSRAM | Notes |
|---|---|---|---|---|
| `xiao_esp32s3_plus/` | **Seeed XIAO ESP32-S3 Plus** | 16 MB | 8 MB OPI | **Primary target.** WLED_RELEASE_NAME = `XIAO_ESP32S3_LITE`. |
| `esp32s3dev_16MB_opi/` | Generic ESP32-S3 dev board | 16 MB | 8 MB OPI | Same hardware spec as XIAO Plus, vanilla board pinout. Useful as a fallback / parity check. |
| `esp32s3dev_8MB_opi/` | Generic ESP32-S3 dev board | 8 MB | 8 MB OPI | Try this if 16MB doesn't boot on your XIAO. |
| `esp32s3_4M_qspi/` | LOLIN S3 Mini / 4MB-class S3 | 4 MB | 2 MB QSPI | Smallest variant. The build that flashed successfully on a prior XIAO bring-up attempt; useful as an A/B reference. |

Each directory contains:
- **`merged-flash.bin`** — single-image flash for first-time programming (bootloader + partitions + OTA-data + app, all in one file, addressed at `0x0`).
- **`firmware.bin`** — app-only image. Use this for OTA updates via the WLED web UI once the device is already running WLED-Lite.

## First-time flash (Windows, esptool)

Install `esptool` once:

```
python -m pip install --user esptool
```

Put the XIAO into download mode (hold the BOOT button while plugging in USB-C, or press BOOT then briefly press RESET while powered). Find the COM port in Device Manager. Then:

```
python -m esptool --chip esp32s3 --port COM3 --baud 460800 write_flash 0x0 firmware\xiao_esp32s3_plus\merged-flash.bin
```

Replace `COM3` with the actual port and the path with the env directory you want.

After flashing, unplug/replug. The device boots WLED-Lite and exposes an AP named `WLED-Lite-AP` on first run (no saved WiFi).

## First-time flash (PlatformIO Upload)

If you've already done a local `pio run -e xiao_esp32s3_plus`, the easiest path is:

```
pio run -e xiao_esp32s3_plus -t upload --upload-port COM3
```

PlatformIO handles the bootloader/partitions/app addresses itself.

## OTA update (after first flash)

Open the device's web UI → **Config → Security & Updates → Manual OTA Update** → upload `<env>/firmware.bin`.

OTA only works between firmwares with the **same `WLED_RELEASE_NAME`**. The XIAO Plus build's release name is `XIAO_ESP32S3_LITE`; the generic dev envs each use their own name (e.g. `ESP32-S3_16MB_opi`). Cross-release OTA is refused by design — re-flash via USB to switch.

## What's baked into these binaries

- **Version:** `0.1.0-lite` (from `package.json`).
- **Brand:** `WLED-Lite` (overrides upstream `WLED`). Default AP SSID becomes `WLED-Lite-AP`.
- **Build commit:** see `git log` of this commit — the binaries match this tree exactly. Re-build to capture later commits.

## Re-generating these files

```
pio run -e xiao_esp32s3_plus -e esp32s3dev_16MB_opi -e esp32s3dev_8MB_opi -e esp32s3_4M_qspi
# Then re-run the merge_bin step documented in docs/dev-setup-windows.md
```
