# Skullgirls Palette Editor — DLL release candidate

This version builds on the `dll_version` editor at commit `0b09e4c`, retaining its in-game interface, color wheel, group Copy/Paste, and eyedropper. It shares the advanced effect backend and compatible `.pal` format with the standalone V1 editor. The native effect layouts currently target game build **26588330**.

## Install and use

Close the game before replacing the DLL. Extract **d3d9.dll**, **groups.json**, **overlay-textures.json**, **prepare-dll-preview.py**, and the **overlay-shader** folder beside the isolated runner's `SkullGirls.exe`. Keep a backup of any existing `d3d9.dll`. Restart the game after a DLL update. Press **F1** or **Insert** to show or hide the editor.

- The persistent target bar shows team, character, palette number and the active Autoload file. Use the numeric field, Previous/Next, or the retained slider. Palette and Advanced share this target; pending overlay changes offer Apply, Discard or Cancel before switching.
- **Palette** has a searchable group list with swatches/applied effects and a selected-group inspector for color editing, Copy/Paste, the color wheel and shader assignments. A different-size paste explicitly offers **Paste matching swatches**. **Undo paste** restores the entire paste in one action, provided those colors have not subsequently changed.
- Assign up to two distinct overlay textures to disjoint groups and click **Apply overlays** in the same inspector. The nearby status reports Pending, Applied, or an error. Display groups that split a native shader band link to Advanced.
- **Advanced** keeps palette-wide line/shadow colors, effect settings and native band assignment controls. **Auto-load** manages file assignments.
- **Sprite preview** shows isolated frames for the 18 playable characters. Click **Prepare combined sprite asset**, select `characters-foits.gfs` from the local game's `data01` folder, and let the bundled Python helper generate `sprite-preview.bin` beside the runner. Python 3 must be on PATH, or selected with **Choose Python executable**. The combined asset is generated from local game archives and is not included in releases; extracted artwork is never sent or bundled. Search authored animation names (stable move IDs remain visible), then choose an animation and a timeline frame. The selector preserves repeated frames and shows each underlying sprite record and authored duration. Local swatch edits, zoom, highlighting and Original/Edited affect only this pane. Normal body layers 0/1 and art banks 4, 5 and 6 are supported; unsupported animations are reported rather than replaced. Overlays, native lighting, rainbow, redirects, transformations and super visuals are not previewed. See [workspace validation](../docs/DLL-WORKSPACE-VALIDATION.md) for preparation details, limits and verification status.
- **File → Save Palette** saves colors and applied advanced assignments together. **Save palette and auto-load** also assigns the file to the current slot. **Export legacy colors only** produces an exact legacy file.
- **Auto-load** supports enabled assignments, a character/palette number, a file picker, and status messages. It loads once per palette, again after a file update, and again in a new match. For two-overlay assignments, validated base colors load while benched; shader preparation waits for a matching visible draw and retries automatically on tag-in. An offscreen timeout is not a permanent failure, and later manual color edits are retained. Later manual edits are retained. Disabling an assignment stops future loading; it does not undo the current palette.

Settings are stored in **PaletteEditorDLLConfig.json** in the game working directory, independently of standalone editor settings. Keep the DLL and standalone editor from editing the same match simultaneously. In 1.0.1, online palette sending/receiving and editor Steam lobby/chat tools are excluded from the build, including their callbacks and Steam API linkage. They cannot be re-enabled in the UI. Game networking is unchanged; assignments and auto-load operate locally on matching loaded characters.

## Build and validation

Run `scripts/build-dll.ps1` from the repository root with Visual Studio 2022's C++ tools installed. Output is `build/dll/d3d9.dll` plus its required JSON files. Before packaging, generate `build/overlay-shader/prototype.bin` using `scripts/build-overlay-preview.ps1` and the tested game shader source. `-Package` includes the generated shader and its hash in the DLL archive in `dist/`.

`-Validation` enables an opt-in render-thread probe. In the isolated Training runner, create `dll-smoke.request` in the game working directory. The probe writes `dll-smoke-result.json`, verifies combined effect save/load and legacy compatibility, and restores the baseline. Validation builds cannot be packaged. See [remaining work](../docs/DLL-NEXT-STEPS.md) for the test inventory.

### Color editing layouts

Palette defaults to a classic scrolling group list: all swatches are directly editable, with Copy and Paste beside each group. Unequal sizes explicitly offer **Paste matching swatches**. The copied group/count and one-step Undo paste remain above the list. Effects opens the existing overlay controls; palette-wide settings remain in Advanced.

Check **Use workspace layout** to use the group inspector layout instead. Its list also offers direct Copy/Paste. **Sprite preview window** opens a separate movable/resizable ImGui window in either layout. The editor supports a narrow side-panel width: classic groups stack their controls, and workspace stacks its list above the inspector when needed. Layout and preview visibility are saved in PaletteEditorDLLConfig.json. Switching layout retains the target, clipboard, selected group, and pending assignments.

### Color Studio

Group rows use fixed columns and fixed-size actions, including unequal-size Paste. The clipboard summary reserves its own line before copying, so the table stays in place.

Open **Color Studio** from a group. Before/After strips show the native swatch order; select a swatch in After or on the hue/saturation wheel. **Link whole group** rotates all hues and adjusts saturation together when dragging the wheel. Each color retains its own brightness and alpha. Whole-group Hue, Saturation, and Brightness sliders also work on the staged ramp. Brightness and saturation approach their endpoints proportionally rather than clipping additive offsets; hue-only adjustments preserve the existing shading values. The ramp is not sorted or automatically assigned highlight/shadow roles.

The selected-swatch panel always shows RGBA values and an editable hex field immediately underneath and a saturation/value picker. Wheel linkage applies to wheel dragging; numeric/picker editing intentionally changes only the selected swatch. Reset draft restores Before. Apply group writes the whole ramp to the match; Undo apply restores that application while the live group still matches it. Saving a .pal remains a separate action. Until Apply, edits change only the Before/After swatches and wheel, not the match or the sprite pane. Cancel / close discards the staged draft.

If live colors change elsewhere, Apply is blocked until Reload match. A staged group remains tied to its original target when changing targets; return to it or cancel. Finish/cancel the current draft before opening another group. Match teardown clears the studio. Undo apply is available for the current open group session and is blocked while a new draft is pending.

The detached sprite preview follows the selected target/group across editor tabs and keeps its animation/frame and local-color state independent of the editor layout. Highlighting preserves selected sprite pixels exactly and dims other sprite regions, leaving the neutral background unchanged. No highlight tint is added.
