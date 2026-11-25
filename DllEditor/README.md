# Skullgirls Palette Editor — DLL release candidate

This version builds on the `dll_version` editor at commit `0b09e4c`, retaining its in-game interface, color wheel, group Copy/Paste, and eyedropper. It shares the advanced effect backend and compatible `.pal` format with the standalone V1 editor. The native effect layouts currently target game build **26588330**.

## Install and use

Close the game before replacing the DLL. Extract **d3d9.dll**, **groups.json**, **overlay-textures.json**, **prepare-dll-preview.py**, and the **overlay-shader** folder beside the isolated runner's `SkullGirls.exe`. Keep a backup of any existing `d3d9.dll`. Restart the game after a DLL update. Press **F1** or **Insert** to show or hide the editor.

- The persistent target bar shows team, character, palette number and the active Autoload file. Use the numeric field, Previous/Next, or the retained slider. Palette and Advanced share this target; pending overlay changes offer Apply, Discard or Cancel before switching.
- **Palette** shows searchable named groups with swatches, Color Studio, Effects, Copy and Paste in aligned columns. A different-size paste explicitly offers **Paste matching**. **Undo paste** restores the entire paste in one action, provided those colors have not subsequently changed.
- Assign up to two distinct overlay textures to disjoint groups and click **Apply overlays** in its Effects controls. The nearby status reports Pending, Applied, or an error. Display groups that split a native shader band link to Advanced.
- **Palette** keeps line/shadow colors visible. **Advanced** keeps palette-wide effect settings and native band assignment controls. **Auto-load** manages file assignments.
- **Sprite preview** shows isolated frames for the 18 playable characters. Click **Prepare combined sprite asset**, select `characters-foits.gfs` from the local game's `data01` folder, and let the bundled Python helper generate `sprite-preview.bin` beside the runner. Python 3 must be on PATH, or selected with **Choose Python executable**. The combined asset is generated from local game archives and is not included in releases; extracted artwork is never sent or bundled. Search authored animation names (stable move IDs remain visible), then choose an animation and a timeline frame. The selector preserves repeated frames and shows each underlying sprite record and authored duration. Local swatch edits, zoom, highlighting and Original/Edited affect only this pane. Normal body layers 0/1 and art banks 4, 5 and 6 are supported; unsupported animations are reported rather than replaced. Overlays, native lighting, rainbow, redirects, transformations and super visuals are not previewed. See [workspace validation](../docs/DLL-WORKSPACE-VALIDATION.md) for preparation details, limits and verification status.
- **File → Save Palette** saves colors and applied advanced assignments together. **Save palette and auto-load** also assigns the file to the current slot. **Export legacy colors only** produces an exact legacy file.
- **Auto-load** supports enabled assignments, a character/palette number, a file picker, and status messages. It loads once per palette, again after a file update, and again in a new match. For two-overlay assignments, validated base colors load while benched; shader preparation waits for a matching visible draw and retries automatically on tag-in. An offscreen timeout is not a permanent failure, and later manual color edits are retained. Disabling an assignment stops future loading; it does not undo the current palette.

Settings are stored in **PaletteEditorDLLConfig.json** in the game working directory, independently of standalone editor settings. Keep the DLL and standalone editor from editing the same match simultaneously. In 1.0.1, online palette sending/receiving and editor Steam lobby/chat tools are excluded from the build, including their callbacks and Steam API linkage. They cannot be re-enabled in the UI. Game networking is unchanged; assignments and auto-load operate locally on matching loaded characters.

## Build and validation

Run `scripts/build-dll.ps1` from the repository root with Visual Studio 2022's C++ tools installed. Output is `build/dll/d3d9.dll` plus its required JSON files. Before packaging, generate `build/overlay-shader/prototype.bin` using `scripts/build-overlay-preview.ps1` and the tested game shader source. `-Package` includes the generated shader and its hash in the DLL archive in `dist/`.

`-Validation` enables an opt-in render-thread probe. In the isolated Training runner, create `dll-smoke.request` in the game working directory. The probe writes `dll-smoke-result.json`, verifies combined effect save/load and legacy compatibility, and restores the baseline. Validation builds cannot be packaged. See [remaining work](../docs/DLL-NEXT-STEPS.md) for the test inventory.

### Grouped color editing

Palette uses one grouped-color layout. Bundled `groups.json` loads automatically from beside the DLL; an optional saved custom JSON is used when available. There is no Group Character Parts activation step or workspace toggle. If labels are unavailable, Palette offers a file picker and ungrouped swatches as a fallback.

Each group has its name above one aligned row: swatches, Color Studio, Effects, Copy, Paste. The action columns keep the same horizontal positions for every group. Larger swatch sets wrap inside their own column without moving the buttons. Button widths are measured from their full labels at the current font size; Paste keeps the same width when its label changes. Unequal sizes explicitly offer **Paste matching** (matching swatches). The clipboard summary and one-step Undo paste remain above the list. Line and both shadow colors are on Palette. The slider label sits above a full-width slider.

**Sprite preview window** opens the independent movable/resizable frame viewer. It follows the selected group and keeps its local colors and animation/frame state separate from the editor. The editor supports narrow side panels, with a minimum width of 420 pixels. Palette-wide shader settings remain in Advanced; assignment management remains in Auto-load.

### Color Studio

Group rows use fixed columns and fixed-size actions, including unequal-size Paste. The clipboard summary reserves its own line before copying, so the table stays in place.

Open **Color Studio** from a group. Before/After strips show the native swatch order; select a swatch in After or on the hue/saturation wheel. **Link whole group** rotates all hues and adjusts saturation together when dragging the wheel. Each color retains its own brightness and alpha. Whole-group Hue, Saturation, and Brightness sliders also work on the staged ramp. Brightness and saturation approach their endpoints proportionally rather than clipping additive offsets; hue-only adjustments preserve the existing shading values. The ramp is not sorted or automatically assigned highlight/shadow roles.

The selected-swatch panel always shows RGBA values and an editable hex field immediately underneath and a saturation/value picker. Wheel linkage applies to wheel dragging; numeric/picker editing intentionally changes only the selected swatch. Reset draft restores Before. Apply group writes the whole ramp to the match; Undo apply restores that application while the live group still matches it. Saving a .pal remains a separate action. Until Apply, edits change only the Before/After swatches and wheel, not the match or the sprite pane. Cancel / close or the title-bar X discards the staged draft.

If live colors change elsewhere, Apply is blocked until Reload match. A staged group remains tied to its original target when changing targets; return to it or cancel. Finish/cancel the current draft before opening another group. Match teardown clears the studio. Undo apply is available for the current open group session and is blocked while a new draft is pending.

The detached sprite preview follows the selected target/group across editor tabs and keeps its animation/frame and local-color state independent of the editor layout. Highlighting preserves selected sprite pixels exactly and dims other sprite regions, leaving the neutral background unchanged. No highlight tint is added.

### Original-mod Autoload compatibility

The original DLL stored `AutoLoadPals` in `Documents/Skullgirls/PalEditConfig.json`. The DLL now imports those assignments automatically, including on an existing installation of this fork. It also recognizes a local `PaletteEditorConfig.json`. Legacy `CharName`, zero-based `PalNumber`, and `PathToPal` are preserved; absent `Enabled` means enabled. Current DLL assignments, including disabled entries, win character/palette conflicts. Existing relative palette files beside a legacy JSON are resolved there.

Legacy sources are never rewritten. The existing DLL settings are backed up as `PaletteEditorDLLConfig.json.before-legacy-import.bak` before a migration save. Successful source imports are marked in the DLL settings so deleting an imported assignment does not resurrect it later. Malformed legacy rows are skipped and remain eligible after repair. An unreadable current DLL settings file is left untouched and saves are disabled for that session. Import status appears in Auto-load.

### Export a transparent sprite frame

In the separate Sprite preview window, select an animation/frame and choose Original or Edited colors. **Export PNG** opens a save dialog with a character/move/frame filename. The export captures the selected frame at native pixel dimensions with straight alpha transparency. It excludes the neutral canvas, zoom, selected-group highlighting and effects the viewer does not render. Partial transparency and supported layer overlap are preserved. Changing the match or frame after opening the dialog does not alter the captured pixels. Export does not apply live colors or save a .pal file. Windows' built-in PNG codec is used; no extra image library is required.

The local setup flow needs Python 3 on PATH (or Choose Python executable), the helper beside the DLL, and the local game's data01 archives. A fresh missing-asset screen opens the preparation controls. Select characters-foits.gfs with Prepare combined sprite asset and wait for completion; all assets stay local. Existing generated bundles may be backed up before a setup test; no game archives need removal.

### Export a transparent animation

Choose an animation in Sprite preview, then **Export animation**. Pick APNG or GIF and set **Plays** (0 loops forever; 1 plays once). The export options offer inclusive Start/End frame numbers and a two-handle range slider. Drag either endpoint or enter exact numbers; Whole animation restores the full range. The selected range keeps repeated frames and their original durations, using Original/Edited colors. A single-frame range is allowed. Frames share an origin-aligned canvas based on that animation's poses, so changing frame bounds does not make the fighter jump. Native pixel resolution is used; zoom/highlighting/background and unsupported effects are excluded.

APNG keeps full RGBA and 60 Hz timeline delays. Preparation resolves one-based sprite references and omits unconditional same-tick advance records. Conditional gameplay script branches are not emulated, so move playback still needs visual comparison with the game. Older SGPOSE5 bundles must be regenerated through the setup UI; SGPOSE6 prevents reusing their incorrect indices/timing. GIF uses a shared palette of up to 255 visible colors plus a transparent entry, no dithering, and a 128/255 alpha cutoff. GIF delays are rounded cumulatively to 10 ms units to avoid long-term drift; some viewers impose their own minimum delays. These format tradeoffs follow the [PNG/APNG specification](https://www.w3.org/TR/png-3/) and [GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt).

After choosing a destination, export runs in the background with progress and Cancel. The worker owns a snapshot and does not access the live fighter, D3D device or editable preview globals. Match/frame/color changes cannot alter an export already captured. Output replaces the destination atomically only after encoding succeeds. Limits: 2,000 timeline entries, 2,048 pixels per canvas side, 64 MiB captured source/decoded-frame limits and 128 MiB encoded output. Oversized animations report an error rather than blocking the editor or producing a partial file.
