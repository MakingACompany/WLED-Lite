#pragma once

// WLED-Lite — Discrete brightness-mode state machine (public API).
//
// A finite-state machine that turns one button into a two-mode controller:
//   - Default state:   short-press = cycle effect (button.cpp's default action)
//   - Brightness mode: short-press = advance to next discrete brightness level
//   - Long-press in either state toggles the mode.
//   - No input for WLED_LITE_BRI_MODE_TIMEOUT_MS auto-exits the mode.
//
// All persistent state is private to wled_lite_brightness.cpp. Tunables (level
// table, timeout, which button index drives the FSM) live in wled_lite_config.h.
// button.cpp only calls into this header.
//
// The whole module is gated by WLED_LITE_BUTTON_BRIGHTNESS_MODE in both
// translation units. A build without that flag compiles upstream's button.cpp
// behavior untouched.
//
// See docs/button-bindings.md for the user-facing UX spec.

#ifdef WLED_LITE_BUTTON_BRIGHTNESS_MODE

namespace WLEDLiteBrightness {

  // Is the FSM currently in brightness mode?
  // button.cpp checks this on btn0 short-press to dispatch:
  //   true  -> call advance()
  //   false -> upstream default ("cycle effect")
  bool isActive();

  // Toggle mode on btn0 long-press.
  // - Inactive -> active: snap cursor to the smallest level >= current bri
  //                       and apply it (visual feedback that mode is engaged).
  // - Active   -> inactive: leave bri at whatever the user last picked.
  void toggle();

  // Advance to the next discrete brightness level and apply it.
  // Wraps from the top back to the lowest. Called on btn0 short-press when
  // isActive() is true.
  void advance();

  // Idle tick. Call from the top of handleButton() each loop.
  // Auto-exits the mode after WLED_LITE_BRI_MODE_TIMEOUT_MS of no btn0 input.
  void tick();

} // namespace WLEDLiteBrightness

#endif // WLED_LITE_BUTTON_BRIGHTNESS_MODE
