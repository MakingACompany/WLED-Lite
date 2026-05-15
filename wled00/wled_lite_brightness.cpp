// WLED-Lite — Brightness-mode FSM implementation.
//
// Tunables (level table, timeout, button-index assignments) live in
// wled_lite_config.h. Public API for button.cpp to call lives in
// wled_lite_brightness.h. Everything here is private state.

#include "wled.h"
#include "wled_lite_config.h"
#include "wled_lite_brightness.h"

#ifdef WLED_LITE_BUTTON_BRIGHTNESS_MODE

namespace {
  bool          s_active     = false;
  uint8_t       s_cursor     = 0;
  unsigned long s_lastInput  = 0;

  inline void applyLevel() {
    bri = WLED_LITE_BRI_LEVELS[s_cursor];
    stateUpdated(CALL_MODE_BUTTON);
  }
}

namespace WLEDLiteBrightness {

  bool isActive() {
    return s_active;
  }

  void toggle() {
    if (s_active) {
      s_active = false;
      return;
    }
    s_active = true;
    s_lastInput = millis();
    // Snap cursor to the smallest level >= current bri so the user sees
    // a meaningful step on the next short-press (always a step up, unless
    // already at the top -- then short-press wraps to the lowest level).
    s_cursor = 0;
    for (uint8_t i = 0; i < WLED_LITE_BRI_LEVEL_COUNT; i++) {
      s_cursor = i;
      if (WLED_LITE_BRI_LEVELS[i] >= bri) break;
    }
    applyLevel();
  }

  void advance() {
    s_lastInput = millis();
    s_cursor = (s_cursor + 1) % WLED_LITE_BRI_LEVEL_COUNT;
    applyLevel();
  }

  void tick() {
    if (s_active && (millis() - s_lastInput > WLED_LITE_BRI_MODE_TIMEOUT_MS)) {
      s_active = false;
    }
  }

} // namespace WLEDLiteBrightness

#endif // WLED_LITE_BUTTON_BRIGHTNESS_MODE
