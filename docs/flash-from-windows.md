# Flash WLED-Lite from Windows

Quick-start for flashing a pre-built WLED-Lite binary from a Windows machine onto a Seeed Studio XIAO ESP32-S3 Plus. Everything you need is checked into this repo — no compilation required.

If you also want to **re-compile** locally on Windows (modify the firmware, then build), see [`dev-setup-windows.md`](dev-setup-windows.md) instead.

---

## TL;DR (5 minutes)

```powershell
git clone https://github.com/MakingACompany/WLED-Lite.git
cd WLED-Lite
python -m pip install --user esptool
# Find the COM port of your XIAO in Device Manager. Then:
.\tools\flash.ps1 -Port COM3
```

The XIAO boots WLED-Lite within a few seconds. It exposes an access point named **WLED-Lite-AP** (open, no password) for first-run setup.

> **PowerShell execution policy**: if PowerShell refuses to run the script, run this once and try again:
> `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`

> **Background**: an earlier version of `merge-bin` invocation used `--flash-mode qio` which corrupted image-header byte 2 (Puya flash chips can't access flash in QIO before the second-stage bootloader initializes it). Fixed by switching to `--flash-mode dio` (matching the prebuilt bootloader's boot-time mode). Both merged and piecemeal flash methods now work; merged is simpler and is the default.

---

## What you need

| Item | Notes |
|---|---|
| **Windows 10 or 11** | Built-in USB-CDC drivers handle the XIAO out of the box. If Device Manager shows the board as "USB-Serial CDC" without a yellow exclamation mark, you're set. |
| **Python 3.10+** | For `esptool`. <https://www.python.org/downloads/windows/> — check "Add python.exe to PATH" during install. |
| **Git for Windows** | To clone this repo. <https://git-scm.com/download/win> |
| **USB-C cable** | A working cable that supports data (not "charge only"). |
| **The XIAO ESP32-S3 Plus board** | Connected via USB-C. |

You do **not** need PlatformIO or VSCode for flashing-only. (You only need those if you want to recompile.)

---

## Step 1 — clone the repo

```powershell
git clone https://github.com/MakingACompany/WLED-Lite.git
cd WLED-Lite
```

You're now in the repo. The pre-built binaries live under `firmware\`.

If you already have the repo, just `git pull` to get the latest binaries.

---

## Step 2 — install esptool

In a PowerShell or Command Prompt window:

```powershell
python -m pip install --user esptool
```

Verify:

```powershell
python -m esptool version
```

Should print something like `esptool.py v4.x`. If `python` isn't found, you didn't add Python to PATH during install — easiest fix is to re-run the Python installer and check that box, then restart the terminal.

---

## Step 3 — find the COM port

1. Plug the XIAO into USB-C.
2. Open **Device Manager** (Win+X → Device Manager).
3. Expand **Ports (COM & LPT)**.
4. Note the COM number on the line that appears when you plug the board in (e.g. `USB Serial Device (COM3)`).

That number — `COM3` in the example — is what you pass to `esptool`.

If nothing appears: try a different USB-C cable (some cables are charge-only and won't enumerate as a serial device). If still nothing: hold the **BOOT** button on the XIAO while plugging in — it forces bootloader mode and should always enumerate.

---

## Step 4 — pick a variant

WLED-Lite ships three binaries. **Start with `xiao_esp32s3_plus`** (the 16 MB build):

| Binary path | When to use |
|---|---|
| `firmware\xiao_esp32s3_plus\merged-flash.bin` | **Try this first.** 16 MB flash + OPI PSRAM — the standard XIAO ESP32-S3 Plus configuration. |
| `firmware\xiao_esp32s3_plus_8MB\merged-flash.bin` | Fallback if 16 MB doesn't boot. Same chip, smaller flash partition. |
| `firmware\xiao_esp32s3_plus_4M\merged-flash.bin` | Last fallback. 4 MB + QSPI PSRAM (not OPI). The prior bring-up attempt that booted successfully used a 4 MB build, so this matches that working config. |

See [`../firmware/README.md`](../firmware/README.md) for the full comparison.

---

## Step 5 — flash

Single command, replace `COM3` with your actual port:

```powershell
python -m esptool --chip esp32s3 --port COM3 --baud 460800 write-flash 0x0 firmware\xiao_esp32s3_plus\merged-flash.bin
```

esptool will:
1. Connect to the chip
2. Read its flash ID
3. Erase the regions it's about to overwrite
4. Write the merged image starting at address `0x0`
5. Verify (briefly)

The whole thing takes 30–90 seconds depending on flash size. When it finishes you'll see:

```
Leaving...
Hard resetting via RTS pin...
```

If you instead see `Failed to connect to ESP32-S3` — the board isn't in download mode. Hold **BOOT**, briefly tap **RESET**, release BOOT, then re-run the flash command.

---

## Step 6 — first-run setup

After flashing, the XIAO reboots and starts WLED-Lite.

1. On your phone or laptop, look for a WiFi network named **WLED-Lite-AP**. Connect to it (no password).
2. Your device should auto-open a captive portal at `4.3.2.1`. If not, open a browser and go to `http://4.3.2.1`.
3. You'll see the **welcome wizard** with three numbered steps:
   - **Connect to WiFi** — pick your home/shop network and enter the password.
   - **Set an admin PIN** — pick a 4-digit code. Keep it somewhere safe; you need it to change advanced settings later.
   - **Use your sign** — drops you into the main user UI (color, effects, brightness, on/off timer).

After WiFi is configured, the XIAO joins your network and the AP goes away. You can reach it at `http://wled-lite.local` (or whatever name you gave it during setup), or at the IP address shown in your router.

---

## Troubleshooting

### "It flashed, but the LEDs didn't light up / the device didn't boot"

Try the next-smaller variant. The XIAO Plus has fixed hardware (16 MB flash + 8 MB OPI PSRAM), but a smaller-config build still boots fine on bigger hardware — and it isolates whether the issue is flash partitioning or PSRAM mode.

Order to try:

1. `firmware\xiao_esp32s3_plus\merged-flash.bin` (16 MB OPI — try this first)
2. `firmware\xiao_esp32s3_plus_8MB\merged-flash.bin` (8 MB OPI — same PSRAM mode, smaller flash)
3. `firmware\xiao_esp32s3_plus_4M\merged-flash.bin` (4 MB QSPI — different PSRAM mode entirely)

If even the 4 MB variant doesn't produce a working USB-Serial port in Device Manager after a reset, the chip itself may need a hardware reset or the bootloader may be in an odd state — see "Recover a bricked XIAO" below.

### "Failed to connect to ESP32-S3"

Board isn't in bootloader mode:

1. Hold the **BOOT** button on the XIAO.
2. While holding BOOT, briefly tap **RESET** (or unplug + re-plug USB).
3. Release BOOT.
4. Re-run the flash command.

### "A fatal error occurred: Could not open COM3, the port doesn't exist"

The COM port changed when the board was reset, or another program is holding it open. Re-check Device Manager for the current port. Close PuTTY, Arduino IDE Serial Monitor, or any other program that might have the port open.

### "Permission denied" on the COM port

Another program has it open. Same fix as above.

### Recover a bricked XIAO

If a previous bad flash left the chip unresponsive:

1. Hold **BOOT** while plugging in USB-C.
2. Run `python -m esptool --chip esp32s3 erase-flash` (will wipe everything to factory-empty state).
3. Then re-flash per Step 5.

### Reset to factory state (after a successful WLED-Lite install)

If you want to wipe the device's saved config (WiFi credentials, PIN, presets) but keep the firmware:

- Hold the BOOT button for **10 seconds** after the device is fully booted. The firmware will format the filesystem and reboot — you'll see `WLED-Lite-AP` come back.

> Note: this only works on board *index 0* in the BTNPIN list, which for the standard XIAO Plus carrier maps to physical Button 1.

---

## Updating later

Two paths once a device is in service:

### Over the network (OTA)

Easiest. From a browser on the same network:

1. Go to `http://<device>/` → tap the gear icon top-right → enter admin PIN → **Security & Updates**.
2. Click **Manual OTA Update**.
3. Upload `firmware\xiao_esp32s3_plus\firmware.bin` (the **app-only** file, not `merged-flash.bin`).
4. Device reboots into the new firmware.

OTA only works between firmwares with the **same `WLED_RELEASE_NAME`**. The three WLED-Lite variants have *different* release names — see `../firmware/README.md`. To switch *between* variants, you must re-flash over USB.

### Over USB

Same as Step 5. The device will pick up the new firmware on next boot. Saved config (WiFi credentials, PIN, presets, etc.) is preserved as long as you don't pick a variant with a different filesystem partition layout. The three WLED-Lite variants in this repo all use different layouts (4 MB / 8 MB / 16 MB), so switching variants over USB **does** reset the saved config.

---

## Open questions you may hit

- **"My XIAO Plus is on COM3 but Device Manager shows two entries"** — the XIAO exposes two serial-like devices when USB-CDC is enabled (one is the data port, one is a JTAG endpoint). Use whichever shows up under "Ports (COM & LPT)"; the other one is hidden under "Universal Serial Bus controllers".
- **"Does this work on macOS or Linux?"** — Yes, the same `esptool` command works. Change the port from `COM3` to `/dev/cu.usbmodem...` (macOS) or `/dev/ttyACM0` (Linux). Re-compile path is in [`dev-setup-windows.md`](dev-setup-windows.md) (most of which applies to mac/linux too — install PIO via `pip install --user platformio` instead of via the Windows binary installer).
- **"I want to flash multiple devices at once"** — esptool flashes one board at a time. For a small batch, a USB hub + a quick shell script that iterates over `COM3, COM4, COM5...` is fine. For larger production-line scale, see the open follow-ups in the maintainer's project plan around per-device PIN provisioning.
