# WLED-Lite — flash helper for Windows.
#
# Usage (from repo root):
#   .\tools\flash.ps1 -Port COM3
#   .\tools\flash.ps1 -Port COM3 -Env xiao_esp32s3_plus -Method piecemeal
#
# Defaults:
#   -Env    xiao_esp32s3_plus
#   -Method merged           (single merged-flash.bin at 0x0)
#   Method "piecemeal"       flashes bootloader+partitions+boot_app0+firmware at separate addresses
#                            (matches what install.wled.me does internally)
#
# Always does an `erase_flash` first.
#
# Requires Python + esptool: `python -m pip install --user esptool`

param(
  [Parameter(Mandatory=$true)]
  [string]$Port,
  [string]$Env = 'xiao_esp32s3_plus',
  [ValidateSet('piecemeal','merged')]
  [string]$Method = 'piecemeal',
  [int]$Baud = 460800
)
# Default is piecemeal. merged-flash.bin has been observed to boot-loop on
# real XIAO Plus hardware for this build configuration even though piecemeal
# of the SAME bins works fine. Root cause not yet isolated -- likely
# esptool merge_bin's flash-params header rewriting interacting badly with
# this specific bootloader/PSRAM combo. Piecemeal sidesteps it entirely
# and matches what install.wled.me's web installer does internally.

$ErrorActionPreference = 'Stop'
$dir = Join-Path $PSScriptRoot "..\firmware\$Env"
if (-not (Test-Path $dir)) {
  Write-Host "ERROR: firmware\$Env directory not found." -ForegroundColor Red
  Write-Host "Available envs:"
  Get-ChildItem (Join-Path $PSScriptRoot "..\firmware") -Directory | ForEach-Object { Write-Host "  $($_.Name)" }
  exit 1
}

Write-Host "==> Erasing flash on $Port..." -ForegroundColor Cyan
python -m esptool --chip esp32s3 --port $Port erase_flash
if ($LASTEXITCODE -ne 0) { Write-Host "erase_flash failed" -ForegroundColor Red; exit 1 }

if ($Method -eq 'merged') {
  $merged = Join-Path $dir 'merged-flash.bin'
  Write-Host "`n==> Flashing merged image: $merged" -ForegroundColor Cyan
  python -m esptool --chip esp32s3 --port $Port --baud $Baud write_flash 0x0 $merged
} else {
  $boot = Join-Path $dir 'bootloader.bin'
  $part = Join-Path $dir 'partitions.bin'
  $app0 = Join-Path $dir 'boot_app0.bin'
  $fw   = Join-Path $dir 'firmware.bin'
  foreach ($f in @($boot, $part, $app0, $fw)) {
    if (-not (Test-Path $f)) {
      Write-Host "ERROR: $f not found. This env may not have piecemeal artifacts." -ForegroundColor Red
      exit 1
    }
  }
  Write-Host "`n==> Flashing piecemeal: bootloader + partitions + boot_app0 + firmware" -ForegroundColor Cyan
  python -m esptool --chip esp32s3 --port $Port --baud $Baud write_flash `
    0x0 $boot `
    0x8000 $part `
    0xe000 $app0 `
    0x10000 $fw
}

if ($LASTEXITCODE -ne 0) {
  Write-Host "`nFlash FAILED." -ForegroundColor Red
  exit 1
}

Write-Host "`n==> Done. Unplug + replug USB-C, then look for the WiFi AP on your phone." -ForegroundColor Green
