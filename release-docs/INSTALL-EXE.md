# Skullgirls Palette Editor V1 — standalone EXE

Use this package for the external editor. This is a fresh Win32 Release build of the current standalone source, with the shared V1 shader and persistence backend. It is not the older executable from the v0.6.1 release.

1. Extract **all** files into a writable folder outside the game directory.
2. Keep `SkullGirls.Pallete.Editor.2nd.Encore.exe`, `groups.json`, and `overlay-textures.json` together.
3. Run the game and editor, then enter a match and select a character.

The executable is Windows x86 (32-bit), including on 64-bit Windows, and requires the Microsoft Visual C++ x86 runtime. Advanced effects currently target Skullgirls game build **26588330**. Dependency installers are not bundled.

**Palette** contains colors, color wheels, and a collapsible **Advanced** section. Use **Overlay textures → Add second texture for DLL** to configure two assignments. A color group's **Shader settings → Assign overlay** stages its choice for the same panel.

The EXE runs on its own and previews the primary texture. **Preview primary and keep both** retains the secondary assignment, which standard Save, Load, and Auto-load preserve in the same .pal file. The in-game DLL renders both textures together.

Apply effects before saving. **Save palette** stores live colors and applied overlay, rainbow, color-link, and conditional transformation assignments in a compatible `.pal` file. **Save palette and auto-load** also assigns it to the current character/palette number. **Auto-load** provides enabled assignments and status messages.

Keep the editor running for auto-loading. It applies once per palette, again on a file update or retry, and again in a new match. Subsequent manual edits are retained. Settings are stored in `PaletteEditorConfig.json` beside the executable; existing legacy Documents settings are copied on first use when available. No personal settings or palettes are included in this package.

Normal editor exit restores editor-owned advanced overrides. Ordinary color edits remain in the current match. Do not run this editor while the DLL editor is also editing that match.

Legacy `.pal` files remain supported. Older editors can read the color prefix of V1 files, but re-saving in an older editor removes the new effects block. Use **Export legacy colors only** for stricter legacy tools. See `PAL-FORMAT.md`.

Read `RELEASE-NOTES.md` for validation status and remaining checks before treating these files as a stable release.

Based on [Skullgirls Community's Cut's palette editor](https://github.com/Skullgirls-Communitys-Cut/SkullGirls-Pallete-Editor-2nd-Encore-_Cplusplus-Version_). Original documentation is included as `UPSTREAM-README.md`.
