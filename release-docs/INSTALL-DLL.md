# Skullgirls Palette Editor V1 — DLL

Use this package for the in-game editor. All binaries are Windows x86 (32-bit), including on 64-bit Windows. Advanced effects currently target Skullgirls game build **26588330**.

1. Close Skullgirls and any standalone palette editor.
2. Back up any existing `d3d9.dll` in the game folder.
3. Extract **all** files from this ZIP beside `SkullGirls.exe`, including `groups.json` and `overlay-textures.json`.
4. Start the game and enter a match. Press **F1** or **Insert** to show/hide the editor.

**Palette** contains normal colors, group Copy/Paste, color wheels, and the eyedropper. **Advanced** is a separate tab for textures, rainbow, opacity, color links, and conditional transformations. Expanded color groups also offer **Shader settings**. The selectors in both tabs refer to the same character and palette number.

Apply settings before saving. **Save Palette** saves colors and applied shader assignments in a compatible `.pal` file. **Save palette and auto-load** also assigns it to the current character/palette number. **Auto-load** provides assignment controls and loading status. Keep the editor installed for auto-loading in later matches.

`PaletteEditorDLLConfig.json` stores DLL settings in the game working directory. This package does not replace personal palettes or include preset assignments. Disabling auto-load stops future loading; it does not undo an already loaded palette. Settings from the standalone editor are separate.

There is one overlay definition, one rainbow timing configuration, and one conditional transformation trigger per palette. Group controls preserve assignments on other groups; use Advanced for shared settings. Display groups that split a native shader band must use Advanced's native groups.

Online palette sending, receiving, and the editor's Steam lobby/chat tools are disabled at build time in 1.0.1. The DLL does not register those callbacks or link to Steam's API library. There is no setting to turn sharing back on. The game's own online play is unchanged. Palette assignments and auto-load remain local, and the Steam-dependent "only my side" option is unavailable.

Legacy `.pal` files remain supported. Older editors can read the color prefix of V1 files, but re-saving in an older editor removes the new effects block. Use **Export legacy colors only** for stricter legacy tools. See `PAL-FORMAT.md`.

To uninstall, close the game and remove this package's `d3d9.dll`, or restore your previous DLL backup. Do not use the DLL and standalone editor on the same running match.

The Microsoft Visual C++ x86 runtime is required. Dependency installers are not bundled. Read `RELEASE-NOTES.md` for validation status and remaining checks before treating these files as a stable release.

Based on [Skullgirls Community's Cut's palette editor](https://github.com/Skullgirls-Communitys-Cut/SkullGirls-Pallete-Editor-2nd-Encore-_Cplusplus-Version_). Original documentation is included as `UPSTREAM-README.md`.
