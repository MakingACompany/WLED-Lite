# WLED-Lite — Button Bindings

Task #8 of the [project plan](WLED-LITE-PLAN.md). Defines the end-user-facing button UX and the firmware state machine that backs it.

## End-user UX

Two buttons on the carrier board: **Button 1** (GPIO 1) and **Button 2** (GPIO 2). See [`hardware-pinmap.md`](hardware-pinmap.md) for the wiring.

### Default state (lights on, normal use)

| Button | Short press | Long press (≥ 600 ms) |
|---|---|---|
| **Button 1** | Cycle to next effect | Enter brightness mode |
| **Button 2** | Cycle to next palette | Toggle power (off ↔ on) |

### Brightness mode

Triggered by long-press of Button 1. While in this mode:

| Button | Short press | Long press |
|---|---|---|
| **Button 1** | Step to next brightness level | Exit brightness mode |
| **Button 2** | (still) cycle palette | (still) toggle power |

The five discrete brightness levels are approximately **12 % / 25 % / 50 % / 75 % / 100 %** of full output (`{32, 64, 128, 192, 255}`). The first short-press after entering the mode always advances *upward* from the current brightness — so the user sees a step *up* on the first tap, regardless of where the slider was. Past 100 % the cycle wraps back to 12 %.

**The mode auto-exits after 10 seconds of no Button-1 activity.** If the user gets distracted mid-cycle, the device quietly returns to default state and Button 1 short-presses go back to cycling effects.

### Why this layout works for the target user

- **Two buttons, two roles.** Button 1 controls "what's happening" (animation, brightness); Button 2 controls "the colors" + "is it on?". The end user doesn't have to think about which button does which mode — they have a job, and you tap it.
- **No double-press.** Upstream WLED uses single + long + double, but double-press timing is fiddly for older fingers. WLED-Lite uses only single + long.
- **No accidental factory reset.** Upstream's button 0 has a "hold 10 s → factory reset" path. On WLED-Lite the buttons start at index 0 by way of `BTNPIN=1,2`, but they map to non-power-button GPIOs, so the AP / factory-reset triggers don't apply (those check `b == 0 && dur > WLED_LONG_AP/FACTORY_RESET` in `handleButton()`, but our button 0 mapping changes the action well before that point).
- **Brightness mode is sticky but forgiving.** Long-press to enter is a deliberate two-second gesture; the 10 s timeout means the user can't get trapped in it.

## Firmware architecture

Three files, gated by `-D WLED_LITE_BUTTON_BRIGHTNESS_MODE`:

| File | Role |
|---|---|
| `wled00/wled_lite_config.h` | Constants only. Brightness level table, timeout in ms, and which button index drives which feature. No code. New WLED-Lite features add their own config blocks here. |
| `wled00/wled_lite_brightness.h` + `.cpp` | Self-contained state machine. Private state (`isActive`, level cursor, last-input timestamp); public API: `isActive()`, `toggle()`, `advance()`, `tick()`. |
| `wled00/button.cpp` | **Lightly modified upstream file.** Includes the two headers above and calls into the state machine at three hook points: `shortPressAction()`, `longPressAction()`, top of `handleButton()`. Every hook is inside `#ifdef WLED_LITE_BUTTON_BRIGHTNESS_MODE`; the upstream defaults remain in the `#else` branches untouched. |

### Why three files instead of one

Putting the state machine in its own translation unit means it can't accidentally reach into `button.cpp`'s static globals (`buttonBriDirection`, etc.) — the dependency is one-way. Putting tunables in a separate header means the state machine and the integration hooks read from a single source of truth (no risk of `button.cpp` and `wled_lite_brightness.cpp` disagreeing about which button index drives the FSM). When future WLED-Lite features need to know "which button is the brightness one" they include `wled_lite_config.h` rather than duplicating constants.

### Button index ↔ GPIO ↔ user-facing label

The hookup goes through three layers:

| Layer | "Button 1" | "Button 2" |
|---|---|---|
| **User-facing label** | "Button 1" on the carrier silk | "Button 2" on the carrier silk |
| **GPIO** | 1 (XIAO D0) | 2 (XIAO D1) |
| **`buttons[]` index** in `cfg.cpp` | 0 (first entry in `BTNPIN=1,2`) | 1 (second entry) |
| **`wled_lite_config.h` constant** | `WLED_LITE_BRI_BTN_INDEX = 0` | `WLED_LITE_OFF_BTN_INDEX = 1` |

If the carrier layout ever swaps the buttons, only `BTNPIN` in `platformio.ini` needs to change. The state-machine code is button-index-agnostic.

### Why we don't use upstream's macro/preset system

WLED has a built-in mechanism for binding button events to preset IDs (`macroButton`, `macroLongPress`, `macroDoublePress` per button). It's powerful but has two costs:
1. **Presets must exist.** A "Button 2 long = off" macro needs a preset that turns the device off. The maintainer would have to pre-load those presets during onboarding, and they'd live as JSON config rather than in source — invisible from a code-review standpoint.
2. **No state.** The macro system fires once per event; it has no notion of "we're in a sticky mode now". Implementing brightness-mode via macros would require either an external state daemon or polluting the preset config with mode flags.

The direct C++ hook approach avoids both: defaults live in source, are reviewable, and can hold state. Per-device customization (a maintainer who wants different bindings) is still available — setting `macroButton` etc. via the admin UI takes priority over the WLED-Lite defaults (`if (!buttons[b].macroButton)` gates the whole custom block).

## Per-device override

Everything described above is the *factory default*. The admin UI at `/settings/sync` (the "Button setup" section) lets a maintainer assign preset IDs to any button event. If `macroButton` for button 0 is set to a non-zero preset ID, the WLED-Lite default code does not run for that button's short-press — the preset fires instead. Same for long-press and double-press.

This means a maintainer can override individual button behaviors per device without recompiling. The brightness-mode FSM only activates when `!buttons[b].macroButton` and `!buttons[b].macroLongPress` for the brightness-button index.

## What's NOT in this task

- **Double-press handling.** WLED-Lite intentionally does not assign a default double-press action; the system still fires `doublePressAction(b)` if upstream's double-press timer detects one (350 ms between releases), but with no macro set, the function is a no-op for the relevant button indices. A maintainer can still wire a double-press preset per device.
- **MQTT button-event publishing.** Upstream publishes `<topic>/button/<n>` with the press type. That code is untouched by WLED-Lite — if MQTT is configured at the admin level (Task #4 keeps it compiled in, Task #5 hides the UI from end users), button events still propagate.
- **Capacitive-touch buttons.** Upstream supports `BTN_TYPE_TOUCH` on ESP32-S3. WLED-Lite's defaults assume push buttons (`BTN_TYPE_PUSH`); switching a device to touch input is a per-device admin choice.
- **PIR / motion sensors on a button pin.** Same as touch — upstream handles it, WLED-Lite doesn't pre-configure it.

## Open follow-ups

- **Bench test on hardware** when the maintainer has a XIAO Plus + buttons to verify timing on a real device (the firmware was developed against the unit-less `millis()` clock).
- **Visual feedback when entering brightness mode**: today the user sees "brightness changed to the closest level" as confirmation. A dedicated cue — e.g. a quick all-segments flash, or a status LED on the carrier — would be friendlier. Carrier-board concern (drives the GPIO 38–42 reserved expansion pins), not firmware concern yet.
- **Bench-press scenario**: what should the device do if a user holds Button 1 *while in brightness mode*? Today the long-press exits the mode. An alternative — long-press while in mode "snaps to max brightness" — could be considered, but introduces a third long-press semantic. Recommend leaving the toggle behavior unless the maintainer specifically wants the snap.
- **Sticky brightness across power cycles**: the brightness picked in mode persists via the normal `bri` save mechanism (upstream's `stateUpdated()` triggers a write). Confirm on hardware that the saved level survives a reboot — should "just work" but worth verifying once.
