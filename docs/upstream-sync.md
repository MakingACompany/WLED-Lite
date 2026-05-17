# WLED-Lite — Upstream Sync Workflow

Task #11 of the [project plan](WLED-LITE-PLAN.md). Playbook for periodically merging changes from upstream `wled/WLED` into WLED-Lite, with a per-file map of where to expect conflicts and how to resolve them.

## TL;DR

```bash
# From a clean working tree on main:
git fetch upstream
git fetch origin
git log --oneline HEAD..upstream/main   # what's coming in
git checkout -b sync/upstream-$(date +%Y-%m-%d)
git merge upstream/main                 # resolve conflicts per the guide below
pio run -e xiao_esp32s3_plus -e esp32s3dev_16MB_opi -e esp32s3dev_8MB_opi -e esp32s3_4M_qspi
# refresh firmware/xiao_esp32s3_plus/{firmware,merged-flash}.bin per docs/dev-setup-windows.md
git push -u origin HEAD                  # open PR for review (or merge to main directly)
```

The named branch (`sync/upstream-YYYY-MM-DD`) keeps the merge reviewable and reversible without rebasing main.

## Cadence

**Default: monthly.** First Monday of the month is a reasonable trigger — frequent enough that conflicts stay small, slow enough that upstream-only churn settles. Skip the cadence when:

- The maintainer is mid-feature on a WLED-Lite branch that overlaps an upstream-changed file (e.g. mid-Task #5 UI work + upstream just touched `index.htm`). Defer until the WLED-Lite branch lands so the merge surface is the smaller of the two.
- A commercial deployment cycle is in progress — see "Tagged releases" below.

**Out-of-cycle:** when upstream announces a security fix or a critical regression, merge immediately. Check the `wled/WLED` releases page and `CHANGELOG.md` once a week so this isn't a surprise.

## Remotes

The working repo has two remotes, configured at fork time:

```
$ git remote -v
origin    https://github.com/MakingACompany/WLED-Lite.git  (push)
upstream  https://github.com/wled/WLED.git                 (fetch, read-only)
```

If `upstream` is missing on a freshly cloned machine:

```bash
git remote add upstream https://github.com/wled/WLED.git
```

Never push to `upstream`; the remote is read-only by convention even though git wouldn't stop you (you wouldn't have permission anyway, but the discipline matters).

## What's in our divergence

As of writing, WLED-Lite is 20 commits ahead and 16 commits behind upstream/main (since the fork point at `8e94cf5b`). Our divergence breaks down as:

**New files (clean merges always — upstream doesn't touch these):**
- `usermods/INA219/INA219.cpp`, `library.json`, `README.md`
- `wled00/wled_lite_brightness.{cpp,h}`
- `wled00/wled_lite_config.h`
- `docs/*` (all WLED-Lite design docs)
- `firmware/<env>/*` (committed pre-built binaries)

**Localized #ifdef additions to upstream files (low conflict risk):**
- `wled00/wled.h` — one `#ifdef WLED_LITE_SYNC_DEFAULTS` block around `sendNotifications` defaults (Task #10).
- `wled00/button.cpp` — three small `#ifdef WLED_LITE_BUTTON_BRIGHTNESS_MODE` hook points and an include block at top (Task #8).
- `wled00/FX.cpp` — one `#ifdef WLED_LITE_FX_TRIM` block at the *end* of `setupEffectData()` (Task #6). The block is positional-stable: it runs after every upstream `addEffect()` call.

**Heavily-trimmed files (conflicts here are mostly delete/add — keep the delete):**
- `platformio.ini` — went from ~765 → ~265 lines. Most upstream changes here are to env blocks we deliberately removed; resolution is almost always "keep ours" (i.e., keep the deletion).
- `package.json` — small but pointed changes (name, version, license, repo URL).

If any of the above categories needs more detail during a merge, see [Per-file conflict guide](#per-file-conflict-guide).

## Step-by-step merge workflow

### 1. Start from clean state on main

```bash
git checkout main
git status         # MUST report clean working tree
git pull origin main
```

If `status` isn't clean, stash or commit before continuing. Don't merge into a dirty tree — it makes conflict markers ambiguous.

### 2. Fetch and survey

```bash
git fetch upstream
git fetch origin
git log --oneline HEAD..upstream/main
```

That last line shows every upstream commit you're about to absorb. Skim it. Two things to look for:

- **Anything touching files in our "localized additions" set** (especially `wled.h`, `button.cpp`, `FX.cpp`). Flag in your head — those will probably conflict.
- **Anything you actively want to take or reject.** E.g., upstream adds a new ESP32-S3 board variant you'd like to support? Note it for after the merge.

### 3. Branch the merge

```bash
git checkout -b sync/upstream-$(date +%Y-%m-%d)
```

Naming convention: `sync/upstream-YYYY-MM-DD`. Self-evident, sorts nicely, never collides.

### 4. Merge

```bash
git merge upstream/main
```

If clean: skip to step 6.

If conflicts: git lists them. Resolve per the [conflict guide](#per-file-conflict-guide). For each conflict file:

1. Open and look at the `<<<<<<<`/`=======`/`>>>>>>>` markers.
2. Decide per the guide.
3. `git add <file>`.
4. After all resolved: `git commit` (git pre-fills the merge message; expand it with a one-line summary of what conflicts you resolved).

### 5. Audit the merged result

A few sanity checks before building:

```bash
# Confirm our WLED-Lite build flags are still set
grep -nE "WLED_LITE_(FX_TRIM|BUTTON_BRIGHTNESS_MODE|SYNC_DEFAULTS)" platformio.ini

# Confirm our usermod is still in custom_usermods
grep -nE "custom_usermods.*INA219" platformio.ini

# Confirm the wled_lite_* files are still present and included where expected
grep -rnE "wled_lite_brightness|wled_lite_config" wled00/
```

Each should report what you expect. If the merge accidentally lost a hook point (e.g., button.cpp's `WLEDLiteBrightness::tick()` call vanished), restore it manually now — the build won't catch it as an error (it'll just silently disable the feature).

### 6. Build all four S3 envs

```bash
pio run -e xiao_esp32s3_plus -e esp32s3dev_16MB_opi -e esp32s3dev_8MB_opi -e esp32s3_4M_qspi
```

If any env fails: most likely cause is upstream changed an API the FX trim block, brightness FSM, or INA219 usermod depended on. Fix the dependency before continuing.

### 7. Refresh committed firmware artifacts

Only for `xiao_esp32s3_plus` (the others are smoke-test envs without committed binaries):

```bash
cp .pio/build/xiao_esp32s3_plus/firmware.bin firmware/xiao_esp32s3_plus/firmware.bin
BOOT_APP0=~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 merge-bin \
  -o firmware/xiao_esp32s3_plus/merged-flash.bin --flash-mode dio --flash-size 16MB \
  0x0     .pio/build/xiao_esp32s3_plus/bootloader.bin \
  0x8000  .pio/build/xiao_esp32s3_plus/partitions.bin \
  0xe000  $BOOT_APP0 \
  0x10000 firmware/xiao_esp32s3_plus/firmware.bin
```

(That snippet also lives in `docs/dev-setup-windows.md`.)

### 8. Bench-test on hardware

Before pushing, flash a XIAO Plus and verify:

- Device boots, joins WiFi, exposes `WLED-Lite-AP` (password `wled1234`) on first boot.
- Effect picker shows the expected ~22 curated effects (Task #6).
- Both buttons respond — short presses cycle effect / palette; long-press btn1 enters brightness mode (Task #8).
- If an INA219 is wired: `/json/info` shows live current/voltage/power (Task #9).
- Two flashed devices on the same WiFi mirror state without any settings page visited (Task #10).

A merge that breaks any of these means an upstream change collided with our hooks. Investigate before merging to `main`.

### 9. Push & merge

```bash
git push -u origin HEAD
```

Open a PR if you want a final review pass (recommended for commercial-deployment-class merges). Otherwise merge to `main` directly:

```bash
git checkout main
git merge --ff-only sync/upstream-YYYY-MM-DD
git push origin main
```

Keep the sync branch around for a release cycle in case you need to bisect.

### 10. Update the status table

Add an entry to `docs/WLED-LITE-PLAN.md` Status table noting the upstream commit you merged to. Example: "Synced to upstream `42f4bcb8` (16 commits absorbed). Commit `<merge-sha>`."

## Per-file conflict guide

### `platformio.ini`

By far the most-modified file in WLED-Lite. The trim from Task #3 removed nearly half the file. Upstream changes here usually fall into three categories:

| Upstream change | Resolution |
|---|---|
| Modify an env block we removed (e.g. `[env:esp32dev]`) | **Keep ours.** Take the delete. |
| Add a new env block for a board we don't target | **Keep ours.** Take the delete (don't re-introduce the block). |
| Add a new env for an ESP32-S3 variant we *do* want | Cherry-pick the env block in; verify it doesn't define `default_envs` overlap. |
| Modify a shared section we kept (`[esp32_idf_V4]`, `[esp32s3]`, `[common]`, etc.) | **Take theirs**, then re-confirm our env blocks still inherit cleanly. |
| Modify `lib_deps` for libraries we use | **Take theirs**, then rebuild — upstream may have moved a library version we depend on. |
| Change our `[env:xiao_esp32s3_plus]` block | Shouldn't happen — upstream doesn't have this env. If git claims a conflict here, double-check the file boundaries. |

After resolving: `git diff HEAD~1 -- platformio.ini` should still show a substantial deletion footprint. If it doesn't, you accidentally accepted too much from upstream.

### `package.json`

Small fields, frequent conflicts:

| Field | Upstream-side | WLED-Lite-side | Resolution |
|---|---|---|---|
| `name` | `wled` | `wled-lite` | Keep ours. |
| `version` | bumps regularly | `0.1.0-lite` (or current WLED-Lite version) | **Keep ours.** Bump WLED-Lite version separately. |
| `license` | `EUPL-1.2` | `EUPL-1.2` | Same, no conflict. |
| `repository.url` | upstream URL | our fork URL | Keep ours. |
| `dependencies` (UI build chain) | bumps regularly | inherits upstream | Take theirs. |

### `wled00/wled.h`

Our `#ifdef WLED_LITE_SYNC_DEFAULTS` block around `sendNotifications` is the only modification. Conflict risk: upstream changing the same `sendNotifications` initialization region.

- **If upstream renames or removes `sendNotifications`**: keep our block but update field name. The intent is "master sync on by default" — re-implement against whatever the new field is.
- **If upstream changes the default value to `true`**: our override becomes a no-op. Remove the `#ifdef` block; document the removal in the merge commit.
- **If upstream adds new sync-related globals near ours**: take theirs; place our `#ifdef` block immediately after their additions.

### `wled00/button.cpp`

We have three hook points and an include block at top. Upstream changes here usually fall into:

- **Refactoring `shortPressAction` / `longPressAction`** (function signature, body restructuring). Reapply our hook surgically; the WLED-Lite block has a clear comment marker — copy/paste it into the new structure.
- **Adding new button event types** (e.g., triple-press). Our hooks ignore unknown events; upstream additions usually slot in alongside cleanly.
- **`handleButton()` body restructuring**. Move the `WLEDLiteBrightness::tick()` call to wherever the new top-of-loop position is.

After resolving, grep for `WLED_LITE_BUTTON_BRIGHTNESS_MODE` in `button.cpp` — should still be present at three sites + the include block at top.

### `wled00/FX.cpp`

Our trim block lives at the very end of `setupEffectData()` (line ~11225). It iterates the `_mode` / `_modeData` vectors and resets non-keepers to `_data_RESERVED`.

- **Upstream adds new `addEffect()` calls**: clean — they run *before* our block, and our block automatically reverts them unless they're in the keepers list. No action needed. *Periodically* review the new entries to decide if any belong in our keepers array; that's a Task #6 follow-up, not a merge-time decision.
- **Upstream restructures `setupEffectData()`**: rare but possible. Re-apply our `#ifdef WLED_LITE_FX_TRIM` block at the new end of the function. Make sure it runs after every `addEffect()` call.
- **Upstream changes the `_mode` vector type or `_data_RESERVED` semantics**: rebuild the trim block against the new model. The keepers array stays the same.

### `wled00/data/*.htm` / `*.js`

We haven't touched these yet (Task #5 Phase B is deferred). Conflicts here will be substantial *when* the slim user UI lands. Until then: take upstream changes verbatim.

### Sub-trees we don't touch

`wled00/src/dependencies/*`, `wled00/data/cpal/`, `wled00/data/icons-ui/`, etc. — always take upstream verbatim.

## What NOT to merge back in

A merge can accidentally re-introduce things we deliberately deleted. Things to watch for:

- **ESP8266 / classic ESP32 build envs** in `platformio.ini`. If git's three-way merge thinks an upstream-modified `[env:nodemcuv2]` block (for example) should be kept because upstream changed it — manually remove it. The intent is "we don't build for ESP8266"; one block of new lines is easier to spot than a 200-line revert later.
- **HUB75 envs, WROOM-2 envs, etc.** Same shape.
- **Feature flags we explicitly chose to disable** (`-D WLED_DISABLE_ALEXA`, etc.). If upstream removes a `-D` line we *added*, that's expected (we control that). If upstream removes a `-D` line we *keep*, manually re-add it.
- **2D-related effects re-enabling in `FX.cpp`**. Our trim block already silently neutralizes new effects, but a future user who reads the picker thinking "I can keep Lightning back" can re-add Lightning's ID to the keepers array as a small follow-up — they shouldn't have to re-do an upstream merge.

If unsure: the audit docs (`docs/feature-flag-audit.md`, `docs/fx-curation.md`, `docs/hardware-pinmap.md`) capture the original intent. Diff suspicious merge results against those.

## Tagged releases

WLED-Lite's `version` in `package.json` is independent of upstream's. Increment it when:

- A merge from upstream lands cleanly and you want a marker for "this is the build that absorbed upstream X.Y.Z".
- A WLED-Lite-specific feature ships (e.g., the slim user UI from Task #5 Phase B).
- A commercial deployment cycle is closing — tag a release before fielding devices so OTA paths have a known anchor.

Version scheme suggestion: `<wled-tracked-major.minor>-lite.<wled-lite-patch>`. E.g. if upstream is on `0.16.x`, our versions might run `0.16.0-lite.1`, `0.16.0-lite.2`, … Lets a reader know roughly which upstream we're tracking from the version string alone.

**OTA caveat reminder**: `WLED_RELEASE_NAME` (currently `"XIAO_ESP32S3_LITE"`) gates OTA-update acceptance. Don't change it across releases for already-deployed devices; bump version freely, but keep release name stable. If a commercial product line gets its own release name (`MAKINGACO_SIGN_V1`), that's a one-way door per device — see [`project_commercial_intent`](../.claude/projects/-home-aaron-loar-Code-WLED-Lite/memory/project_commercial_intent.md) (maintainer's memory) for context.

## When the merge fights back

Some warning signs that say "stop, think, don't just `--continue`":

- Conflicts in `wled00/wled_lite_*.{h,cpp}` or `usermods/INA219/*`. These are pure-WLED-Lite files. Upstream shouldn't have touched them. Conflict here means *we* edited them on `main` in a way that we forgot was different from the sync branch. Investigate.
- The merge resolves "cleanly" but the build fails. Almost always means upstream renamed a symbol we hook into. `git log -p upstream/main -- <conflicting-file>` to see the renamings.
- The merge resolves cleanly, the build passes, but a sanity check (effect picker, button behavior, sync) reports surprising behavior. Likely an upstream behavioral change that didn't trip the compiler. Walk through the symptoms; bisect upstream within the merge range if needed.

When stuck: `git merge --abort` returns to clean main state. Try again with a smaller merge window (`git merge upstream/main~N` to take the first N upstream commits, then iterate).

## Tools that help

- **`gh pr view --repo wled/WLED <number>`** — read an upstream PR description before deciding whether you want its changes. Saves a browser trip.
- **`git log -p upstream/main -- <file>`** — see exactly how upstream changed a specific file across the merge window.
- **`pio project config`** — quick sanity check after `platformio.ini` conflicts; confirms the resolved file still parses.
- **`grep -rE "WLED_LITE_" wled00/`** — confirms all our hook points / build flags are intact post-merge.

## Open follow-ups for this workflow

- **Automate the build-and-binary-refresh.** A GitHub Action that, on every push to `main`, builds the four S3 envs and uploads the artifacts would replace step 7 of the manual workflow. Worth setting up before commercial deployments begin (so the firmware on `main` always matches what `firmware/xiao_esp32s3_plus/*` claims).
- **Track upstream releases, not just `main`.** Once upstream cuts a tagged release (e.g. `v0.16.0`), prefer merging against the tag rather than `main`. Less surface, less risk.
- **Periodic re-audit of the keepers / flag lists.** Three months from now, the keepers in `fx-curation.md` and the disabled flags in `feature-flag-audit.md` should get a quick pass to see if upstream's churn introduced anything worth flipping. Add to a quarterly project-maintenance calendar.
