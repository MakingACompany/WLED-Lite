# WLED-Lite — Effect Curation

Task #6 of the [project plan](WLED-LITE-PLAN.md). Trims the upstream WLED effect picker from ~180 entries to a curated set of 22 effects intended for sign / strip use.

## Approach

**Goal:** show end users a small, well-curated set of effects instead of overwhelming them with the upstream library. Preserve effect IDs of the survivors so saved presets that reference dropped effect IDs don't shift to a different effect.

**Mechanism:** a single block at the end of `WS2812FX::setupEffectData()` in `wled00/FX.cpp`, gated by `-D WLED_LITE_FX_TRIM`. The block iterates the populated `_mode` / `_modeData` vectors and resets non-keeper slots to `mode_static` + `_data_RESERVED`. The upstream `addEffect()` calls earlier in the function are untouched.

Trade-offs picked here vs. alternatives:

| Approach | Flash saved | Merge friendliness | Verdict |
|---|---|---|---|
| **Trim block at end (chosen)** | ~0 KB (function bodies still compiled) | Excellent — addEffect calls untouched | Best for user UX; revisit later if flash matters |
| `#ifndef WLED_LITE_FX_TRIM` around each non-keeper `addEffect` | Modest (~10–30 KB, depending on linker --gc-sections) | Poor — 150+ surgical edits, conflicts on every upstream merge | Reserved for a future flash-pressure pass |
| Delete non-keeper `addEffect` calls + function bodies outright | Maximum (~100+ KB) | Catastrophic merge surface | Off the table — flash isn't the bottleneck on a 16MB XIAO |

The plan asked for "trim to a curated subset… preserve effect IDs of survivors." That's a UX requirement, not a flash requirement, so the cheap UX-only path is fine.

## Effect ID preservation guarantee

`setupEffectData()` pre-allocates 220 slots with `_data_RESERVED` before any `addEffect()` runs. Then each `addEffect(FX_MODE_X, …)` populates slot X. The trim block reverses *only the non-keepers* back to RESERVED. Survivor IDs never move.

This means:
- A preset saved on stock WLED at effect ID 80 (Twinklefox) — which is on the keeper list — will still play Twinklefox on WLED-Lite.
- A preset saved at effect ID 23 (Strobe) — not on the keeper list — will fall through `_data_RESERVED` slot and render as STATIC (the fallback pointed to by `_mode[id] = &mode_static`). Annoying but not broken; users can pick a real effect via the picker.
- New effects upstream adds in future merges automatically get reverted to RESERVED by the trim block, since their IDs won't be in the keepers array. We don't need to chase upstream — but we *should* periodically review whether any newcomer deserves the keepers list.

## The 22 keepers

| ID | `FX_MODE_*` constant | Display name | Why |
|---:|---|---|---|
| 0 | `STATIC` | Solid | Essential — fixed color. Hardcoded at slot 0, can't be curated out. |
| 2 | `BREATH` | Breathe | Gentle pulsation. Popular ambient effect. |
| 3 | `COLOR_WIPE` | Wipe | Classic color wipe across the strip. |
| 8 | `RAINBOW` | Colorloop | Slow whole-strip color cycle. |
| 9 | `RAINBOW_CYCLE` | Rainbow | Classic rainbow gradient. |
| 12 | `FADE` | Fade | Two-color crossfade. |
| 13 | `THEATER_CHASE` | Theater | Marquee chase — sign-shop classic. |
| 15 | `RUNNING_LIGHTS` | Running | Flowing wave. |
| 17 | `TWINKLE` | Twinkle | Festive random twinkle. |
| 28 | `CHASE_COLOR` | Chase | Basic chase pattern. |
| 38 | `AURORA` | Aurora | Northern-lights wash. |
| 41 | `COMET` | Lighthouse | Trailing comet. |
| 65 | `PALETTE` | Palette | Cycle through any palette. |
| 66 | `FIRE_2012` | Fire 2012 | Realistic fire. |
| 67 | `COLORWAVES` | Colorwaves | Flowing palette waves — beautiful. |
| 80 | `TWINKLEFOX` | Twinklefox | Gentle, well-liked twinkle. |
| 89 | `STARBURST` | Starburst | Sparkle bursts. |
| 90 | `EXPLODING_FIREWORKS` | Exploding Fireworks | Event / celebration use. |
| 91 | `BOUNCINGBALLS` | Bouncing Balls | Playful, motion-rich. |
| 101 | `PACIFICA` | Pacifica | Ocean waves — relaxing. |
| 104 | `SUNRISE` | Sunrise | Gentle wake-up gradient. |
| 115 | `BLENDS` | Blends | Smooth gradient blending. |

## Notable exclusions and why

- **All 2D effects** (matrix layouts, scrolling text, game-of-life, etc.) — already excluded earlier by `WLED_DISABLE_2D` in Task #4. The trim block doesn't need to handle them.
- **All 2D particle effects** — already excluded by `WLED_DISABLE_PARTICLESYSTEM2D` in Task #4.
- **1D particle effects** (PSDRIP, PSPINBALL, …) — `WLED_DISABLE_PARTICLESYSTEM1D` is intentionally *not* set, so these still compile in. They're not on the keepers list, so the trim block reverts them. Easy to re-enable per-effect by adding their IDs to `wled_lite_fx_keepers[]`.
- **Audio-reactive effects** (PIXELS, FREQWAVE, GRAVCENTER, etc.) — kept compiled in (the audioreactive usermod is still selected), but excluded from the picker because there's no microphone on a sign. When the audio-reactive personal project comes online, copy the relevant IDs into the keepers array for that env.
- **Upstream's "candidate for removal" duplicates** (DUAL_SCAN, THEATER_CHASE_RAINBOW, SINELON_DUAL, RIPPLE_RAINBOW, etc.) — annotated in upstream's `FX.h` comments as redundant with their non-dual / non-rainbow base effects. Easy drops.
- **Heavy-handed effects** (STROBE, MULTI_STROBE, STROBE_RAINBOW, LIGHTNING) — fine for parties; not what we want on a customer sign by default.
- **Niche effects** (TRAFFIC_LIGHT, ICU, PACMAN, TV_SIMULATOR, WASHING_MACHINE) — clever upstream demos, not sign-appropriate.

## Bringing an effect back

Two ways:

1. **Per-env, recommended:** add the ID to `wled_lite_fx_keepers[]` in `wled00/FX.cpp`, rebuild. The effect appears in the picker; ID is preserved; presets that referenced it (if any) resume working.
2. **Whole-env opt-out:** remove `-D WLED_LITE_FX_TRIM` from the env's `build_flags`. The build reverts to upstream's full effect picker. Useful for matrix-mode builds where you want particle / 2D effects back wholesale.

## Future work

- **Flash-pressure pass.** If a future product (e.g. 4 MB ESP32-S3 variant) runs out of flash, do the surgical `#ifndef WLED_LITE_FX_TRIM` wrap per non-keeper `addEffect` call to let the linker garbage-collect function bodies. Expect ~50–100 KB savings.
- **Palette curation.** The plan's Task #6 line says "trim effects" but the broader scope section also says "~90% of effect palettes — aim for 10–20 color palettes, not the full selection." That's a separate effort — see `palette.cpp` / `palettes_data.h`. Not addressed in this commit.
- **Effect picker UI.** Task #5's "slim `index.htm`" will render whatever the firmware reports as available. Since dropped slots return `_data_RESERVED`, the picker UI needs to filter those out (most upstream pickers already do — verify when implementing Phase B of Task #5).
