# WLED-Lite — reference guides

Two static HTML pages that visually mirror the actual firmware UI and double as documentation. Both are self-contained and can be hosted directly (e.g. dropped onto an installer's website) without a build step.

| File | Audience | Purpose |
|---|---|---|
| `user.html` | End customer | A scrollable walkthrough of the slim user UI: how to power, brightness, color, effects, timer, schedule, multi-letter segments. Live color wheel demo via `iro.js`. |
| `admin.html` | Maintainer (future-self) | Comprehensive settings reference. Every settings sub-page, every WLED-Lite custom flag, the hardware pinout, flashing procedures. Written for "5 years from now I've forgotten everything." |

## Files

- `user.html`, `admin.html` — the guides themselves
- `styles.css` — shared chrome (typography, phone-frame mockups, callouts, admin tables) plus `@import` of `_slim-ui.css`
- `_slim-ui.css` — **copy** of `wled00/data/index.css`. Refreshed manually so the guides stay visually true to the firmware
- `iro.js` — copy of `wled00/data/iro.js` for the user guide's live color-wheel demo

## Hosting

Drop the whole `docs/guide/` folder somewhere served by a static-file host:

```
your-website/
  sign-help/
    user.html
    admin.html
    styles.css
    _slim-ui.css
    iro.js
```

No server-side code. All assets are local; no CDN dependency.

## Keeping it in sync with the firmware

When the slim UI gets a visual change (CSS or HTML), refresh the copied assets:

```sh
cp wled00/data/index.css docs/guide/_slim-ui.css
cp wled00/data/iro.js     docs/guide/iro.js
# Then visually review user.html and update any mockups whose markup diverged.
```

When firmware features change (new settings, new build flags, new admin pages), update `admin.html` to match. Cross-references in `admin.html` § "Further reading" link back into `docs/` so when one doc updates, the guide should too.
