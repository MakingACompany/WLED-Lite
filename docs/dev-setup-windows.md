# WLED-Lite — Dev setup on Windows

This is the bench setup for re-compiling WLED-Lite on the home Windows machine. If all you want is to flash an existing build, skip this and use [`../firmware/README.md`](../firmware/README.md).

> **TL;DR:** install PlatformIO ≥ 6 (the version that ships with the VSCode extension is fine), `git clone`, then `pio run -e xiao_esp32s3_plus`. The Tasmota platform fork pinned by upstream WLED needs PIO 6+; older PIOs (e.g. apt's 4.x on Linux) fail with "Unknown development platform 'espressif32'".

## 1. Install prerequisites

| Tool | Why | How |
|---|---|---|
| **Python 3.10+** | PlatformIO needs it. | <https://www.python.org/downloads/windows/> — check "Add python.exe to PATH" during install. |
| **Git** | Clone the repo. | <https://git-scm.com/download/win> |
| **PlatformIO Core 6+** | Build system. | Either install VSCode + the PlatformIO IDE extension (recommended; brings its own isolated PIO), **or** install standalone: `python -m pip install --user --upgrade platformio`. |
| **CP210x / CH340 USB-serial driver** | Some ESP32 boards need it. The XIAO ESP32-S3 uses native USB-CDC and works without an extra driver on Windows 10/11. | Driver site varies by chip — only install if Device Manager shows the board as an unknown device. |

Verify the install (PowerShell):

```powershell
python --version          # 3.10+
pio --version             # 6.x or newer
```

If `pio` isn't on PATH after a pip --user install, it's under `%APPDATA%\Python\Python3xx\Scripts\` — either add that to PATH or run `python -m platformio` instead.

## 2. Clone the repo

```powershell
git clone https://github.com/MakingACompany/WLED-Lite.git
cd WLED-Lite
```

## 3. Build

The default-build environments are `xiao_esp32s3_plus` and `esp32s3dev_16MB_opi`:

```powershell
pio run                              # builds both default envs
pio run -e xiao_esp32s3_plus         # build just the primary target
```

Other available envs (for A/B-testing against the XIAO Plus hardware): `esp32s3dev_8MB_opi`, `esp32s3_4M_qspi`.

The first build downloads the Tasmota platform fork, the ESP32-S3 toolchain, and the lib_deps from GitHub — expect 5–15 minutes. Subsequent builds are ~30 seconds with the build cache.

Output:
- App image (for OTA): `build_output/release/WLED_0.1.0-lite_<release-name>.bin`
- Build tree (for upload): `.pio/build/<env>/firmware.bin`

## 4. Flash a fresh device

The simplest path is to let PlatformIO handle bootloader + partitions + app for you:

```powershell
pio run -e xiao_esp32s3_plus -t upload --upload-port COM3
```

Find the COM port in **Device Manager → Ports (COM & LPT)**. If the XIAO doesn't enter download mode automatically, hold BOOT while plugging in USB-C (or press BOOT + briefly press RESET while powered).

To produce a single merged image (matches what's in `firmware/<env>/merged-flash.bin`), use `esptool merge_bin`:

```powershell
python -m esptool --chip esp32s3 merge_bin -o merged-flash.bin --flash_mode qio --flash_size 16MB `
  0x0     .pio\build\xiao_esp32s3_plus\bootloader.bin `
  0x8000  .pio\build\xiao_esp32s3_plus\partitions.bin `
  0xe000  $env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin `
  0x10000 .pio\build\xiao_esp32s3_plus\firmware.bin
```

Then flash:

```powershell
python -m esptool --chip esp32s3 --port COM3 --baud 460800 write_flash 0x0 merged-flash.bin
```

Adjust `--flash_size` per env: `16MB` for `xiao_esp32s3_plus` and `esp32s3dev_16MB_opi`, `8MB` for `esp32s3dev_8MB_opi`, `4MB` for `esp32s3_4M_qspi`.

## 5. Serial console

```powershell
pio device monitor -b 115200
```

Or any serial terminal at 115200 baud. The XIAO's USB-CDC port disappears momentarily during reset — `pio device monitor` auto-reconnects.

## 6. Re-generating the checked-in firmware/

After building, copy the new artifacts in:

```powershell
foreach ($env in 'xiao_esp32s3_plus','esp32s3dev_16MB_opi','esp32s3dev_8MB_opi','esp32s3_4M_qspi') {
  Copy-Item .pio\build\$env\firmware.bin firmware\$env\firmware.bin
  # then run merge_bin per step 4 to refresh merged-flash.bin
}
```

Commit the updated `firmware/` directory.

## Troubleshooting

- **"Unknown development platform 'espressif32'"** — PlatformIO is too old. Upgrade to 6+ (`pip install --user --upgrade platformio`).
- **Board doesn't boot after flash** — try a different env binary; the XIAO Plus PSRAM/flash combo doesn't always match the `qio_opi` default. Walk through `esp32s3dev_8MB_opi` and `esp32s3_4M_qspi` to find one that boots, then we know which config matches the hardware.
- **"Failed to connect to ESP32-S3"** — board isn't in download mode. Hold BOOT while plugging in, or hold BOOT and tap RESET.
- **OTA update rejected** — the running firmware and the OTA `.bin` must share the same `WLED_RELEASE_NAME`. To switch release names (e.g. from `ESP32-S3_16MB_opi` to `XIAO_ESP32S3_LITE`), re-flash via USB.
