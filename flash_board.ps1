# flash_board.ps1
# See flash_board.sh for the full description. Updates the FlashLight
# submodule, then hands off to FlashLight's own provisioning tool with this
# repo as the target project.
#
# Usage:  .\flash_board.ps1

$RepoRoot = $PSScriptRoot
$FlDir = Join-Path $RepoRoot "FlashLight"

if (-not (Test-Path (Join-Path $FlDir "tools\requirements.txt"))) {
    Write-Error "FlashLight submodule not initialized. Run: git submodule update --init"
    exit 1
}

Write-Host "Updating FlashLight submodule to its latest version..." -ForegroundColor Yellow
git -C $RepoRoot submodule update --remote --init FlashLight

$VenvDir = Join-Path $FlDir "tools\.venv"
if (-not (Test-Path $VenvDir)) {
    Write-Host "Setting up a local virtual environment (first run only)..." -ForegroundColor Yellow
    python -m venv $VenvDir
    if ($LASTEXITCODE -ne 0) { Write-Error "venv creation failed"; exit 1 }
    & "$VenvDir\Scripts\pip.exe" install --quiet -r (Join-Path $FlDir "tools\requirements.txt")
}

Write-Host "Building FlashLight Core..." -ForegroundColor Yellow
pio run --project-dir $FlDir
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$PioRoot = $RepoRoot
if (-not (Test-Path (Join-Path $RepoRoot "platformio.ini"))) { $PioRoot = Join-Path $RepoRoot "firmware" }
Write-Host "Building WLED-Lite..." -ForegroundColor Yellow
pio run --project-dir $PioRoot
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$env:PYTHONPATH = Join-Path $FlDir "tools"
& "$VenvDir\Scripts\python.exe" -m flashlight_provision.cli --project-root $RepoRoot --project-name "WLED-Lite" @args
