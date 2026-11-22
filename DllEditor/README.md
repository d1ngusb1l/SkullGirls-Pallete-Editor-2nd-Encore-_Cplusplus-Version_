# Skullgirls Palette Editor — DLL release candidate

This version builds on the `dll_version` editor at commit `0b09e4c`, retaining its in-game interface, color wheel, group Copy/Paste, and eyedropper. It shares the advanced effect backend and compatible `.pal` format with the standalone V1 editor. The native effect layouts currently target game build **26588330**.

## Install and use

Close the game before replacing the DLL. Extract **d3d9.dll**, **groups.json**, **overlay-textures.json**, and the **overlay-shader** folder beside the isolated runner's `SkullGirls.exe`. Keep a backup of any existing `d3d9.dll`. Restart the game after a DLL update. Press **F1** or **Insert** to show or hide the editor.

- **Palette** contains ordinary colors, line/shadow settings, color wheels, and group Copy/Paste.
- **Advanced** is a separate tab containing overlay textures, rainbow, opacity, color links, and conditional transformations. It has its own character and palette selectors, shared with Palette.
- **Advanced → Overlay textures** starts with one texture. Use **Add second texture** to assign another texture to separate groups, then click **Apply overlays**. Both textures share one lighting setting.
- **Shader settings → Assign overlay** in an expanded color group stages that group's overlay choice. Use **Apply overlays** in Advanced to commit it. Other group shader changes apply immediately. Use Advanced for display groups that split a native shader band.
- **File → Save Palette** saves colors and applied advanced assignments together. **Save palette and auto-load** also assigns the file to the current slot. **Export legacy colors only** produces an exact legacy file.
- **Auto-load** supports enabled assignments, a character/palette number, a file picker, and status messages. It loads once per palette, again after a file update, and again in a new match. Later manual edits are retained. Disabling an assignment stops future loading; it does not undo the current palette.

Settings are stored in **PaletteEditorDLLConfig.json** in the game working directory, independently of standalone editor settings. Keep the DLL and standalone editor from editing the same match simultaneously. In 1.0.1, online palette sending/receiving and editor Steam lobby/chat tools are excluded from the build, including their callbacks and Steam API linkage. They cannot be re-enabled in the UI. Game networking is unchanged; assignments and auto-load operate locally on matching loaded characters.

## Build and validation

Run `scripts/build-dll.ps1` from the repository root with Visual Studio 2022's C++ tools installed. Output is `build/dll/d3d9.dll` plus its required JSON files. Before packaging, generate `build/overlay-shader/prototype.bin` using `scripts/build-overlay-preview.ps1` and the tested game shader source. `-Package` includes the generated shader and its hash in the DLL archive in `dist/`.

`-Validation` enables an opt-in render-thread probe. In the isolated Training runner, create `dll-smoke.request` in the game working directory. The probe writes `dll-smoke-result.json`, verifies combined effect save/load and legacy compatibility, and restores the baseline. Validation builds cannot be packaged. See [remaining work](../docs/DLL-NEXT-STEPS.md) for the test inventory.
