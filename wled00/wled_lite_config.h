#pragma once

// WLED-Lite — Central configuration shared by WLED-Lite specific modules.
//
// Constants and per-feature tunables live here so the integration points (e.g.
// button.cpp's hooks into wled_lite_brightness, future hooks into the INA219
// current-cap or status-OLED usermods, etc.) can stay logic-only and pull
// their config from a single header.
//
// Every block is gated by the same build flag that gates its consumer, so a
// build that drops the flag compiles unchanged upstream behavior.
//
// Sections in this file:
//   1. Brightness-mode FSM tunables  (paired with wled_lite_brightness.{h,cpp})
//
// As new WLED-Lite features land they add their own section here.

#include <stdint.h>

// =========================================================================
// 1. Brightness-mode FSM tunables
//
//    Consumed by  : wled_lite_brightness.cpp (state machine)
//                   button.cpp                (integration hooks)
//    Build flag   : WLED_LITE_BUTTON_BRIGHTNESS_MODE
//    See          : docs/button-bindings.md
// =========================================================================
#ifdef WLED_LITE_BUTTON_BRIGHTNESS_MODE

  // Which entry in BTNPIN drives the brightness FSM.
  // 0 means the first button declared in BTNPIN (GPIO 1 on the XIAO Plus carrier).
  // Short-press toggles between "cycle effect" (default) and "advance level" (in mode);
  // long-press toggles in/out of brightness mode.
  static constexpr uint8_t WLED_LITE_BRI_BTN_INDEX = 0;

  // Which entry in BTNPIN drives the simple power-off long-press behavior.
  // 1 = the second button declared in BTNPIN (GPIO 2 on the XIAO Plus carrier).
  // Short-press cycles palette; long-press turns LEDs off.
  static constexpr uint8_t WLED_LITE_OFF_BTN_INDEX = 1;

  // Auto-exit brightness mode after this many milliseconds of no btn input
  // (input = short or long press on WLED_LITE_BRI_BTN_INDEX). Keeps the device
  // from getting stuck in the alternate behavior if the user walks away mid-cycle.
  static constexpr unsigned long WLED_LITE_BRI_MODE_TIMEOUT_MS = 10000;

  // Discrete brightness levels the FSM cycles through, low to high.
  // Picked to approximate logarithmic perception: ~12%, 25%, 50%, 75%, 100%.
  // First short-press in mode advances to the smallest level >= current bri,
  // so the visual feedback is always a step-up unless already at max (then wraps).
  static constexpr uint8_t WLED_LITE_BRI_LEVELS[] = {32, 64, 128, 192, 255};
  static constexpr uint8_t WLED_LITE_BRI_LEVEL_COUNT =
      sizeof(WLED_LITE_BRI_LEVELS) / sizeof(WLED_LITE_BRI_LEVELS[0]);

#endif // WLED_LITE_BUTTON_BRIGHTNESS_MODE
