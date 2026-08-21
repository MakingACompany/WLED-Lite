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
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"

function Test-VenvReady {
    if (-not (Test-Path $VenvPython)) { return $false }
    & $VenvPython -c "import questionary, rich, requests, requests_toolbelt, serial, esptool" 2>$null
    return ($LASTEXITCODE -eq 0)
}

if (-not (Test-VenvReady)) {
    if (-not (Test-Path $VenvDir)) {
        Write-Host "Setting up a local virtual environment (first run only)..." -ForegroundColor Yellow
        python -m venv $VenvDir
        if ($LASTEXITCODE -ne 0) { Write-Error "venv creation failed"; exit 1 }
    } else {
        Write-Host "Virtual environment exists but is missing packages -- reinstalling..." -ForegroundColor Yellow
    }
    & "$VenvDir\Scripts\pip.exe" install --quiet -r (Join-Path $FlDir "tools\requirements.txt")
    if (-not (Test-VenvReady)) {
        Write-Error "Package install did not complete successfully. On Windows this is often caused by antivirus or OneDrive locking a file mid-install -- try running this script again (it will retry automatically), or temporarily exclude this folder from antivirus/OneDrive sync if it keeps failing."
        exit 1
    }
}

Write-Host "Building FlashLight Core..." -ForegroundColor Yellow
pio run --project-dir $FlDir
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# WLED-Lite itself is built by the Python tool below, once it knows which
# of this project's environments (hardware variants) you actually want.

$env:PYTHONPATH = Join-Path $FlDir "tools"
& "$VenvDir\Scripts\python.exe" -m flashlight_provision.cli --project-root $RepoRoot --project-name "WLED-Lite" @args
