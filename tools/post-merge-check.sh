#!/usr/bin/env bash
# WLED-Lite — post-merge invariant check.
#
# Run after merging from upstream wled/WLED. Confirms the WLED-Lite hook
# points and configuration didn't silently disappear in the merge. Exit
# code 0 = pass, non-zero = something to investigate.
#
# Also used by the CI workflow at .github/workflows/wled-lite-build.yml to
# fail PRs that drop a WLED-Lite invariant.
#
# Usage:
#   ./tools/post-merge-check.sh
#
# When upstream changes -- e.g. renames a symbol our hook uses -- this
# script catches it before a build catches it. Each check has a clear
# error message pointing at the file to inspect.

set -uo pipefail
cd "$(dirname "$0")/.." || exit 2

PASS=0
FAIL=0

ok()   { echo "  [ok]    $*"; PASS=$((PASS+1)); }
fail() { echo "  [FAIL]  $*"; FAIL=$((FAIL+1)); }

section() { echo; echo "=== $* ==="; }

# ---------- 1. Build flags present in xiao_esp32s3_plus env ----------
section "platformio.ini -- WLED-Lite build flags"
PIO=platformio.ini
for flag in \
    WLED_LITE_FX_TRIM \
    WLED_LITE_BUTTON_BRIGHTNESS_MODE \
    WLED_LITE_SYNC_DEFAULTS \
    WLED_LITE_SECURITY_GATES \
    WLED_DISABLE_2D \
    WLED_DISABLE_ALEXA \
    WLED_DISABLE_HUESYNC \
    WLED_DISABLE_INFRARED \
    WLED_DISABLE_LOXONE \
    WLED_DISABLE_PARTICLESYSTEM2D \
    WLED_DISABLE_PIXELFORGE \
    WLED_ENABLE_DMX \
    WLED_ENABLE_DMX_OUTPUT \
    WLED_ENABLE_USERMOD_PAGE \
    ; do
  if grep -qE "^[[:space:]]*-D[[:space:]]+$flag\b" "$PIO"; then
    ok "$flag set"
  else
    fail "$flag missing from $PIO -- WLED-Lite scope drift detected"
  fi
done

# ---------- 2. INA219 usermod still selected ----------
section "platformio.ini -- usermod selection"
if grep -qE "custom_usermods.*INA219" "$PIO"; then
  ok "INA219 in custom_usermods"
else
  fail "INA219 dropped from custom_usermods -- INA219 usermod won't be linked"
fi
if grep -qE "custom_usermods.*audioreactive" "$PIO"; then
  ok "audioreactive in custom_usermods"
else
  fail "audioreactive dropped from custom_usermods"
fi

# ---------- 3. WLED-Lite source files exist ----------
section "WLED-Lite source files present"
for f in \
    wled00/wled_lite_brightness.cpp \
    wled00/wled_lite_brightness.h \
    wled00/wled_lite_config.h \
    usermods/INA219/INA219.cpp \
    usermods/INA219/library.json \
    ; do
  if [ -f "$f" ]; then
    ok "$f exists"
  else
    fail "$f missing"
  fi
done

# ---------- 4. WLED-Lite hook points in modified upstream files ----------
section "WLED-Lite hooks in upstream-shared files"

if grep -q 'WLED_LITE_FX_TRIM' wled00/FX.cpp; then
  ok "FX.cpp -- WLED_LITE_FX_TRIM block present"
else
  fail "FX.cpp -- WLED_LITE_FX_TRIM block is gone (curated effects won't apply)"
fi

if grep -q 'wled_lite_brightness.h' wled00/button.cpp; then
  ok "button.cpp -- includes wled_lite_brightness.h"
else
  fail "button.cpp -- WLED-Lite brightness FSM hook is gone"
fi

if grep -q 'WLEDLiteBrightness::tick' wled00/button.cpp; then
  ok "button.cpp -- WLEDLiteBrightness::tick() called from handleButton"
else
  fail "button.cpp -- tick() call is gone (brightness mode won't auto-exit)"
fi

if grep -q 'WLED_LITE_SYNC_DEFAULTS' wled00/wled.h; then
  ok "wled.h -- WLED_LITE_SYNC_DEFAULTS override block present"
else
  fail "wled.h -- sync-defaults override is gone (sync won't be on by default)"
fi

if grep -q 'WLED_LITE_SECURITY_GATES' wled00/wled_server.cpp; then
  ok "wled_server.cpp -- WLED_LITE_SECURITY_GATES gate present"
else
  fail "wled_server.cpp -- security gates are gone (/, /reset, /settings/* unprotected)"
fi

# ---------- 5. Three default env variants present ----------
section "platformio.ini -- WLED-Lite env variants"
for env in xiao_esp32s3_plus xiao_esp32s3_plus_8MB xiao_esp32s3_plus_4M; do
  if grep -qE "^\[env:$env\]" "$PIO"; then
    ok "[env:$env] defined"
  else
    fail "[env:$env] missing"
  fi
done

# ---------- 6. firmware/ binary tree present ----------
section "firmware/ artifact tree"
for env in xiao_esp32s3_plus xiao_esp32s3_plus_8MB xiao_esp32s3_plus_4M; do
  for f in firmware/$env/firmware.bin firmware/$env/merged-flash.bin; do
    if [ -f "$f" ]; then
      ok "$f"
    else
      # Not a hard fail in CI -- CI regenerates these. Warn only.
      echo "  [warn]  $f missing (CI rebuilds; refresh locally before tagging a release)"
    fi
  done
done

# ---------- 7. package.json sanity ----------
section "package.json -- branding"
if grep -qE '"name":\s*"wled-lite"' package.json; then
  ok "name=wled-lite"
else
  fail "package.json name reverted to upstream value"
fi
if grep -qE '"license":\s*"EUPL-1.2"' package.json; then
  ok "license=EUPL-1.2"
else
  fail "package.json license is not EUPL-1.2 -- check rebrand commit"
fi

# ---------- Summary ----------
echo
echo "======================================"
echo "  passed: $PASS"
echo "  failed: $FAIL"
echo "======================================"

if [ $FAIL -gt 0 ]; then
  echo
  echo "One or more WLED-Lite invariants drifted. Most likely cause: a recent"
  echo "merge from upstream wled/WLED removed something we relied on."
  echo "See docs/upstream-sync.md for the per-file conflict resolution guide."
  exit 1
fi

echo "All WLED-Lite invariants intact."
exit 0
