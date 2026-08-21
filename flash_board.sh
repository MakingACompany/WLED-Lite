#!/usr/bin/env bash
set -e

# flash_board.sh
# Interactive provisioning for this board: updates the FlashLight submodule
# to its latest version, then hands off to FlashLight's own provisioning
# tool with this repo as the target project -- auto-detects the serial
# port, checks whether Core is already installed, and offers full install
# (wipe + Core + WLED-Lite) or WLED-Lite application firmware only.
#
# Usage:  ./flash_board.sh

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FL_DIR="${REPO_ROOT}/FlashLight"

if [ ! -f "${FL_DIR}/tools/requirements.txt" ]; then
    echo "FlashLight submodule not initialized. Run: git submodule update --init"
    exit 1
fi

echo "Updating FlashLight submodule to its latest version..."
BEFORE="$(git -C "$REPO_ROOT" rev-parse :FlashLight 2>/dev/null || true)"
git -C "$REPO_ROOT" submodule update --remote --init FlashLight
AFTER="$(git -C "$REPO_ROOT" rev-parse :FlashLight 2>/dev/null || true)"
if [ "$BEFORE" != "$AFTER" ]; then
    echo "  FlashLight moved ${BEFORE:0:7} -> ${AFTER:0:7} -- commit this bump when convenient."
fi

VENV_DIR="${FL_DIR}/tools/.venv"
venv_ready() {
    [ -x "${VENV_DIR}/bin/python" ] &&
        "${VENV_DIR}/bin/python" -c "import questionary, rich, requests, requests_toolbelt, serial, esptool" >/dev/null 2>&1
}
if ! venv_ready; then
    if [ ! -d "$VENV_DIR" ]; then
        echo "Setting up a local virtual environment (first run only)..."
        python3 -m venv "$VENV_DIR"
    else
        echo "Virtual environment exists but is missing packages -- reinstalling..."
    fi
    "${VENV_DIR}/bin/pip" install --quiet -r "${FL_DIR}/tools/requirements.txt"
    if ! venv_ready; then
        echo "Error: package install did not complete successfully." >&2
        echo "Run this script again to retry, or check for disk/permission issues in ${VENV_DIR}." >&2
        exit 1
    fi
fi

echo "Building FlashLight Core..."
pio run --project-dir "$FL_DIR"

# WLED-Lite itself is built by the Python tool below, once it knows which
# of this project's environments (hardware variants) you actually want.

PYTHONPATH="${FL_DIR}/tools${PYTHONPATH:+:$PYTHONPATH}" exec "${VENV_DIR}/bin/python" \
    -m flashlight_provision.cli --project-root "$REPO_ROOT" --project-name "WLED-Lite" "$@"
