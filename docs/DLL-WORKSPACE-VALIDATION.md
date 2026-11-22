# DLL palette workspace — implementation and validation

Branch: `codex/dll-palette-workspace`. Baseline: fork `origin/master` at `14df4b4`, the merge of the verified two-overlay branch (`5932b52`). The initial sandboxed fetch failed; a subsequent network-enabled fetch confirmed the merge has the same source tree.

## Review boundaries

- Phase 1: `4610664` reorganizes the DLL UI around a persistent target bar, searchable group list, and selected-group inspector. `f2b701c` retains the palette slider alongside the numeric field and Previous/Next buttons, as subsequently requested.
- Phase 2: `14886a9` adds the isolated sprite prototype, its local asset preparation script, and device/match lifecycle hooks. No live shader substitution is used by this pane.
- Follow-up renderer identification fixes address failures discovered while testing team and mirror matches. These are separate from the pane renderer and UI commits.

Palette-wide colors/effects remain in Advanced and assignments remain in Auto-load. Color edits and non-overlay shader changes retain immediate application. Overlay assignments are staged, with Apply and Pending/Applied/error reporting in the inspector. A target change offers Apply, Discard or Cancel for pending overlays. Unequal-size paste explicitly says **Paste matching swatches**; one Undo restores the pasted range, guarded against subsequent modifications. The .pal codec and network exclusions are unchanged.

## Fixed-pose prototype

Run `python scripts/prepare-dll-preview.py GAME_DIRECTORY OUTPUT_DIRECTORY` with a working Python 3 installation. This reads the user's local `data01` archives and creates `cerebella-pose.bin` and a provenance JSON beside the test DLL. The decoder intentionally supports only the tested version-9 Cerebella frame table, frame 2, main art bank, body layers 0 and 1. Game artwork is not committed or downloaded. The prepared pose is 448 by 384 pixels. The source FOIT SHA-256 is `ff9fd46bbb5c4d81a6f196ec311db9787fadd0ab9c3a4ad1ae10dd53b363492c`.

The pane snapshots the selected palette's colors into owned Original and Edited vectors. **Original** means that snapshot, not a reload of the palette's default game colors. Its local swatches edit only the Edited vector. It evaluates neutral color gradients and composites into a pane-owned dynamic D3D9 texture, displayed through ImGui. The selected display group's intersecting native bands determine highlighting. It has no palette writes, Save operation, native actor calls, or draw replay. CPU results are uploaded only when colors/highlight change or the texture must be recreated. Match/target changes drop snapshots; pre-reset and shutdown release GPU textures.

Overlays, native lighting, rainbow, redirects, transformations, and super effects are explicitly **not previewed**. Animation/frame selectors are feasible follow-up work, but are deferred until this milestone is reliable. The fixed pose cannot show every group. The preparation decoder's format investigation reference is [skullgirls-rough-cut](https://github.com/Anonnie04/skullgirls-rough-cut), especially its sprite channel and layer descriptions. Runtime code and preparation script are scoped implementations for this prototype, not an imported sprite library.

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

With two loaded Cerebellas, palette 1 loaded but palette 2 timed out. A read-only trace then showed distinct palette/feature textures for the two sides sharing the same native Chowder texture object. Therefore overlay-pointer changes alone cannot identify this case. The follow-up build compares all target color gradients against the managed native palette texture and explicitly binds its own requested primary texture alongside the secondary. It also rechecks final colors before using an existing effect, so a reused feature object after tagging is insufficient on its own. Identical color signatures remain ambiguous and are rejected. The user confirmed both mirror palettes loaded with this fix, and both success statuses were observed in the editor. Native palette signatures are refreshed after later live color edits so recoloring does not invalidate the mapping. The latter follow-up is built and installed; a focused recolor retest is pending. This path is not used by the isolated sprite pane.

## Build checks

`./scripts/build-dll.ps1 -Validation` builds `build/dll/d3d9.dll`. Networking inspection reports palette networking, lobby tools, Steam imports, and legacy protocol markers all false. `OverlayBindingMatchTests.cpp` covers unrelated startup changes, true shared bindings, missing/inconsistent bindings, gradient interpolation, tolerance, and invalid layouts. `PaletteDocumentTests.exe` passed 32 existing format/legacy/validation/atomic-I/O checks.

The isolated runner contains the test DLL, pose asset and new workspace Autoload fixture. The user's existing Cerebella palette-2 fixture and assignment are retained. This is a validation build, not a packaged release.
