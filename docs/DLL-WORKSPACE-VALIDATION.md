# DLL palette workspace — implementation and validation

Branch: `codex/dll-palette-workspace`. Baseline: fork `origin/master` at `14df4b4`, the merge of the verified two-overlay branch (`5932b52`). The initial sandboxed fetch failed; a subsequent network-enabled fetch confirmed the merge has the same source tree.

## Review boundaries

- Phase 1: `4610664` reorganizes the DLL UI around a persistent target bar, searchable group list, and selected-group inspector. `f2b701c` retains the palette slider alongside the numeric field and Previous/Next buttons, as subsequently requested.
- Phase 2: `14886a9` adds the isolated sprite prototype, its local asset preparation script, and device/match lifecycle hooks. No live shader substitution is used by this pane.
- Follow-up renderer identification fixes address failures discovered while testing team and mirror matches. These are separate from the pane renderer and UI commits.

Palette-wide colors/effects remain in Advanced and assignments remain in Auto-load. Color edits and non-overlay shader changes retain immediate application. Overlay assignments are staged, with Apply and Pending/Applied/error reporting in the inspector. A target change offers Apply, Discard or Cancel for pending overlays. Unequal-size paste explicitly says **Paste matching swatches**; one Undo restores the pasted range, guarded against subsequent modifications. The .pal codec and network exclusions are unchanged.

## Combined local sprite preview

The Sprite preview includes a **Prepare combined sprite asset** button. Select
`characters-foits.gfs` (or its retail variant) in the installed game's `data01`
folder. The helper script is placed beside the DLL by the build/package scripts;
Python 3 must be available on PATH, or selected with **Choose Python executable**. It reads local `data01` archives only and
generates `sprite-preview.bin` plus a provenance/limitations file,
`sprite-preview.json`, in the game's working directory. Releases include the
helper, not the generated asset or extracted game artwork.

The generator currently supports the 18 playable character FOITs observed in
the local installation, version 9, art banks 4 and 6 from `characters-art-pt`
and bank 5 from `characters-art`, and normal body layers 0 and 1. Authored root
animation names, stable numeric move IDs, timeline-to-sprite references,
repeated timeline entries, durations, art-bank selection and per-layer offsets
are retained. Invalid references, unsupported banks/layers, malformed
timelines, and missing required local archives fail explicitly or are listed
in the JSON; no substitute sprite is selected for a rejected timeline.

On the inspected installation the generated bundle was 815,048,416 bytes,
with 9,960 supported animations and 53,067 selectable timeline frames.
Another 500 animations were excluded: 184 reference unsupported art bank 0
and 316 reference records without normal body layers 0/1. Ms. Fortune has
only 257 of 545 root animations supported in this increment. The UI exposes
the character sections, not a promise that every authored animation is
available. Authored names were recovered for all 10,460 root animation IDs.

The combined `SGMULT1` file has an indexed section per character. It is
streamed to disk, bounded to 4 GiB total and 128 MiB per character section;
the viewer reads only the selected character section. Each section retains
the original local RLE channels and decodes the chosen sprite lazily. No
artwork is checked in or downloaded. The JSON lists animations that have
additional populated art layers outside 0/1; those extra layers are not
composited in this increment and may affect visual completeness. The UI also
labels this limitation. Frame origins and layer offsets align frames to a shared pivot on a variable-size canvas. The continuation fixed mixed absolute/relative bounds and checked a crouch pose in-game; exact game-renderer equivalence is not claimed. In this installation, all 9,960 included animations
had additional populated layers listed in the JSON.

The pane snapshots the selected palette's colors into owned Original and
Edited vectors. **Original** is that captured snapshot, not a reload of the
palette's default game colors. Local swatches edit only the Edited vector.
Selected-group highlighting and local recoloring remain independent of
animation/frame selection. The viewer does not call live fighter animation,
native actor actions, palette writes, save APIs, or gameplay actions.
Textures are uploaded only when necessary and released on target changes,
device reset, and shutdown. Variable frame dimensions do not reset user zoom.
Overlays, native lighting, rainbow, redirects, transformations, super visuals,
and additional non-body art layers are explicitly **not previewed**.

Run manually from a developer checkout with:

`python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY`

In the game UI, use the button and select the installed `characters-foits.gfs`
instead. The helper reports progress/errors in the pane. Do not copy or replace
a loaded DLL; the user controls game restarts, matches, tags and display-mode
changes. The generator test used the local installation's 18 FOITs and matching
main/alternate art archives. It produced all 18 sections and authored-name
tables. This is asset/parser evidence only, not a claim of in-game visual
equivalence.

## Observed in the isolated Training runner

| Workflow | Result |
| --- | --- |
| Copy name/count | Inspector showed `hat skull (5 swatches)` after Copy. |
| Unequal paste and one Undo | Copied five swatches to eight-swatch `skin main`; the explicit matching paste changed live bands. One Undo restored all bands exactly. Read-only before/after snapshots agree. |
| Group search | Searching `dress` returned `dress main` and `dress middle`; selecting a result changed the inspector. |
| Pending target guard | Staged Water on hat main, pressed Next, observed Apply/Discard/Cancel, and Cancel retained the target and pending assignment. Apply/Discard transition branches still need direct UI verification. |
| Two-overlay inspector | Assigned Water to hat main and Fire to hair, then additionally dress main; Apply completed and the list showed applied effect names. Native records contained Water and Fire. Water was clearly visible; the secondary Fire appearance on the dark stage was not independently conclusive. |
| Autoload save and palette round trip | Saved `workspace-autoload-test.pal` with Save palette and auto-load; status reported both overlays loaded. Palette 1 → 3 → 1 restored the same native bands and Water/Fire definitions. The pane refreshed on target changes. |
| Pane-local edit and Original/Edited | Changed a local skull swatch and switched to Original. Both fighters' native palette bands and overlay definitions were unchanged. |
| Highlight | Hat skull and hat main landed on the correct regions. Hair and dress selection also highlighted their visible regions. |
| Match transition | User restarted with tagged teams; old preview snapshot was discarded. Initial two-overlay Autoload then exposed a renderer identification failure, described below. |
| Fullscreen/windowed and Alt-Tab | User completed these transitions with the preview visible and reported everything returned normally. Pre-reset texture release/recreation is implemented; no separate GPU leak instrument was used. |
| Mirror Autoload | After the palette-gradient fix, the user confirmed both Cerebellas on palettes 1 and 2 loaded; both Auto-load entries also visibly reported success. User subsequently completed tag-out/return and reported everything continued to work. |

Evidence is in ignored `build/workspace-*.json` files. `workspace-paste-check.json` reports `beforeEqualsUndo: true` and `pasteChangedBands: true`. `workspace-viewer-check.json` reports native bands and overlay definitions unchanged. `workspace-autoload-check.json` reports palette 3 away, palette 1 returned, matching bands and both texture names.

## Team/mirror renderer investigation

The first team restart failed with **Multiple fighter draws changed during two-texture preparation**. Global native cache refresh can change unrelated bindings during startup, so a single changed pointer is insufficient evidence. Reversible A/B/A/B confirmation excludes one-time changes and still rejects true ambiguity.

With two loaded Cerebellas, palette 1 loaded but palette 2 timed out. A read-only trace then showed distinct palette/feature textures for the two sides sharing the same native Chowder texture object. Therefore overlay-pointer changes alone cannot identify this case. The follow-up build compares all target color gradients against the managed native palette texture and explicitly binds its own requested primary texture alongside the secondary. It also rechecks final colors before using an existing effect, so a reused feature object after tagging is insufficient on its own. Identical color signatures remain ambiguous and are rejected. The user confirmed both mirror palettes loaded with this fix, and both success statuses were observed in the editor. Native palette signatures are refreshed after later live color edits so recoloring does not invalidate the mapping. The color-following update is built and installed; a focused recolor retest is pending. This path is not used by the isolated sprite pane.

## Initially benched fighter follow-up

The user subsequently identified a missing case: the second Cerebella began on the bench. Her Autoload preparation had timed out before she could produce a draw. This invalidates any broad claim that initial mirror success covered bench-to-point loading.

Autoload now watches recent, read-only palette texture snapshots before attaching a preparation marker. An offscreen target reports **Waiting for fighter to be visible (tag in)** and remains eligible. Visibility expires after two presentation frames. If a fighter disappears during preparation, timeout restores the prior native header and returns to waiting; that result is not cached as a permanent failed attempt. On later visible frames, Autoload retries automatically. Explicit inconsistent/multiple confirmed bindings remain errors. Visibility tracking never applies saved colors or effects itself. At the user's subsequent request, Autoload now validates the entire document and loads colors/line/shadows once while benched, preserving existing effects through the colors-only import path. Only two-overlay preparation is deferred. Later manual colors are retained when deferred overlays activate. The user confirmed the early-color stage worked. A read-only snapshot matched all 198 Cerebella base colors to the Autoload file; the user also confirmed automatic overlay activation after tag-in. Evidence: `build/workspace-early-colors-check.json`.

The new visibility-history tests cover absent targets, another palette, expiry, return and match clearing. The updated validation DLL builds and passes network exclusion inspection. The user confirmed the initially-benched -> tag-in workflow waits and then loads automatically.

## Build checks

`scripts/build-dll.ps1 -Validation` builds `build/dll/d3d9.dll` and copies the
local preparation helper beside it. Networking inspection reports palette
networking, lobby tools, Steam imports, and legacy protocol markers all false.
The final validation DLL SHA-256 is
`7E3C713D5680749C6A6C2449A8B075E5B1D8412FA67D7E6984F26A9D6C4E3FA9`.
`PaletteDocumentTests.exe` passed 32 existing format/legacy/validation/atomic
I/O checks. `OverlayBindingMatchTests.exe` passed. This is a validation build,
not a packaged release. The existing runner fixtures were not replaced or
changed in this increment.

## Combined animation preview evidence and remaining work

- Unit: all 11 `tests/SpriteTimelinePreparationTests.py` cases pass; they cover authored-name and
  numeric fallback behavior, timeline-to-sprite mapping, repeated frames,
  boundaries, invalid references/durations, art-bank selection, GFS lookup and
  bounds, origin/layer limits, and combined section indexing/bounds.
- Local assets: preparation read all 18 playable character FOITs from the
  available local version-9 archives, extracted authored names for every
  root move table, and generated an 18-section `SGMULT1` bundle with matching
  local art and palette data. The asset was regenerated in ignored build/dll and installed in the isolated
  runner while it was closed; no game data is committed. This verifies
  parsing and container generation, not rendered frame equivalence.
- UI/build: the DLL offers a local file-picker/button workflow, runs the
  helper asynchronously, reports completion/errors, indexes per-character
  sections, and reads only the selected section. The final Win32 validation
  build passed; compiler warnings remain in existing signed/unsigned and
  numeric-conversion code paths.
- Runtime evidence from the continuation is recorded below. The user controls
  all match/restart/tag and display-mode actions.
- Existing Autoload status remains separate: the prior user-confirmed
  initially-benched flow loaded base colors early and activated two overlays
  after tag-in. This task did not change live renderer/Autoload code.

## November 23 continuation from Copilot

Preserved Copilot's unfinished 18-character bundle/preparation workflow and completed the following fixes:

- Corrected layer offsets calculated against bounds in the wrong coordinate space. Negative offsets could previously wrap when converted to unsigned upload positions. Regression tests cover nonzero origins, multiple layers, positive offsets, and invalid bounds.
- Bounded native reads to the selected character section, including channel-length checks before allocation. A native boundary fixture confirms a read cannot cross into the next section.
- Kept numeric frame entry one-based, consistent with the frame dropdown. Removed the redundant zero-based frame slider.
- Separated metadata lookup from lazy pixel decoding so invalid channel errors are caught in the upload error path instead of escaping through frame labels.
- Added a Python executable picker for machines where the PATH entry is an unusable Windows app alias.
- Centered the initial pose on the shared canvas and added explicit Center/Fit controls. Asset preparation is collapsed when usable assets exist; frame controls are compact and the canvas uses remaining pane space.
- Restored Original-snapshot swatches when Original is selected; local color vectors and zoom remain intact across frame decoding.

Validation performed:

- All 11 Python preparation tests passed.
- Win32 native geometry and asset tests passed. The native loader read every character section, checked layer offsets for every stored sprite, decoded fallback/first/last animation samples, and retained Original/Edited vectors and zoom during selection. The tests also found native band 21, absent from Cerebella's fixed fallback, in Stand [20], frame 16, sprite record 16.
- Generated the 815,048,416-byte local bundle for all 18 characters. Native counts include 1,297 Cerebella sprites and 549 supported Cerebella animations.
- In Training, selected Crouch [21], stepped frames 1 and 2 (both sprite 17, durations 1 and 4 ticks), then selected frame 6 (sprite 20, 4 ticks). The pane rendered a crouch pose and highlighted the skull region.
- Edited a pane-local skull swatch and toggled Original/Edited. Read-only snapshots show all loaded fighters' palette bands and native overlay definitions unchanged; all existing runner .pal hashes were unchanged. Evidence: build/timeline-isolation-result.json.
- Verified the installed compact layout in Training: Fit resized the fallback pose; filtered by `cr`, selected Crouching [22], and stepped from frame 1 (sprite 2, 1 tick) to frame 2 (sprite 35, 2 ticks). Zoom stayed at 0.35x and the skull highlight remained on the skull. Scrolling the pane exposed the full canvas at this window size.
- The in-game preparation button/Python picker, exact pose equivalence, all-character GPU coverage, device reset and the benched Autoload scenario were not independently retested in this continuation. Extra artwork layers and shader effects remain explicitly unpreviewed. Existing live rendering/Autoload implementation was not modified.

Native tests: tests/SpritePreviewGeometryTests.vcxproj and tests/SpritePreviewAssetTests.vcxproj. Run the asset executable with the directory containing sprite-preview.bin as its argument. Generated images/assets remain ignored build/ artifacts.

## Classic color workflow follow-up

- Default layout now shows every matching group with directly editable swatches and per-group Copy/Paste; no group needs opening for these actions.
- Use workspace layout restores the inspector; direct list Copy/Paste is available there too. Layout and optional classic preview preferences persist in the existing configuration file. The target and clipboard are shared between layouts.
- Clipboard name/count and Undo paste are outside the scrolling panes. Existing PasteGroupColors and its single undo record are reused. Unequal-size paste remains explicitly labeled. Effects / wheel opens existing overlay controls with Apply/status; Advanced and Auto-load retain their roles.
- Win32 validation build and networking-exclusion inspection passed. Installed while the runner was closed, with a backup of the prior DLL. SHA-256: 2907A3C2BDC2C57D23B242C16101BB1A289692A8C76BBA69DCBBC1F857209092.
- In-game layout, switching, Copy/Paste/Undo and preference persistence checks are pending for this build. No new game-rendering or Autoload code was changed.

## Color Studio and stable group controls

- Group/Colors/Actions table columns and fixed-size Copy/Paste/Color Studio/Effects controls replace variable-width action rows. Clipboard summary space is reserved before the first copy. Paste keeps a stable ImGui ID when its label changes.
- Replaced the old immediate-write wheel with a staged group editor: linked hue/saturation wheel, whole-ramp Hue/Saturation/Brightness, individual RGB/hex and saturation/value picker, native-order Before/After strips, Reset, Apply, Cancel and guarded Undo apply.
- Group transforms preserve alpha; hue-only transforms preserve each swatch's HSV value. Saturation/brightness endpoint transforms are monotonic, avoiding reversal of shading order. No semantic highlight/shadow roles or game-equivalent effect previews are claimed.
- Applying to a changed live group is blocked; Reload explicitly adopts live colors. Drafts remain tied to their original slot/character/palette; a different target cannot receive Apply or Undo. Match teardown clears staged state. Saved palettes are not written by this panel.
- Native PaletteRampTests passed across 4,096 RGB samples plus hue-wrap, alpha, hue-spacing, shading-order and endpoint checks. Win32 DLL build and all four network-exclusion checks passed. In Training on Cerebella palette 5, observed linked wheel interaction and staged Before/After changes. Read-only snapshots confirmed staging left all live palette bands unchanged, Apply changed colors, Undo restored every loaded band, and all existing .pal hashes stayed unchanged (build/studio-isolation-result.json). Copying five swatches changed Paste labels while action rectangles and group rows stayed fixed. The initial picker was too tall; the final follow-up uses a larger opaque window, compact picker and explicit RGB/hex switch. That layout follow-up still needs a visual check after installation.

Final Color Studio follow-up built and installed while the runner was closed. DLL SHA-256: 2B65577BCA49C3916CD44FA7F7864710A32D3BFBDCDFC6E253406F61C1801045. Prior DLL backed up.
