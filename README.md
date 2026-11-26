# Skullgirls Palette Editor

Palette editing and Autoload run locally. The DLL builds with palette sharing and the editor's Steam lobby and chat tools disabled.

V1 release assets are prepared with `scripts/prepare-release.ps1` from a clean committed tree. It rebuilds the DLL and standalone EXE, runs offline checks, and creates `dist/v1.0.1/` containing both complete packages, raw assets matching earlier release conventions, source ZIP/tar.gz archives, release notes, and SHA-256 checksums. It does not create a tag or publish a release. See [release notes and validation status](release-docs/RELEASE-NOTES.md) and [DLL installation](release-docs/INSTALL-DLL.md).

This fork brings the current advanced palette features into one editor, with compatible `.pal` saving and full-palette auto-loading. The standalone editor and DLL target Skullgirls game build **26588330**.

## Standalone editor

Keep `PaletteEditor-V1.exe`, `groups.json`, and `overlay-textures.json` together in a writable folder outside the game directory. Start the game and editor, then open a match.

- **Palette** contains the normal color editor. Its collapsed **Advanced** section contains overlay textures, rainbow, opacity, always-on color links, and conditional transformations. Existing named triggers and manual previews remain available.
- Add independent transformation rules using **General**, **Character moves**, and **Resources** triggers. Search or filter 588 mapped move families across all 18 fighters, or set Beowulf Hype rules for levels 0–3. Character-move rules use V5 palette saves.
- Apply advanced changes before saving. **File → Save palette** stores live colors, opacity, line/shadow colors, and applied advanced assignments in one `.pal` file. Unapplied controls are not saved.
- **File → Load palette** accepts plain legacy `.pal` files and supported extended formats (V1–V5). Invalid or mismatched files are rejected before game data changes.
- **File → Save palette and auto-load** saves and assigns the file to the selected character/palette number. The **Auto-load** tab also lets you add, browse, enable/disable, and retry assignments.
- Auto-load waits for the assigned character and palette number, applies once per match/palette, and reapplies after a new match or file update. It does not continuously overwrite subsequent manual edits. Disabling an assignment stops future loads; it does not undo the current palette.

Keep the editor running for auto-loading. Advanced overrides are restored on normal editor exit; color edits retain the original editor's in-match behavior. Settings live in `PaletteEditorConfig.json` beside the executable. On first use, existing `Documents/Skullgirls/PalEditConfig.json` settings are copied when available; the legacy settings file is left untouched.

## In-game DLL

- The DLL tracks shared move phases per fighter so a transformation can continue through its associated loops, landings, and recovery. Load palettes manually for each player, even when both players use the same palette number.
- Save palettes with two overlay textures. The standalone editor previews the primary texture and preserves both assignments. The DLL renders both supported overlays with the bundled shader.
- Search color groups and switch targets from the shared Palette and Advanced controls. Paste matching swatches between different-sized groups, undo a paste in one step, or stage linked ramp edits in Color Studio.
- Browse animation frames for all 18 fighters in the detachable sprite viewer using locally prepared assets. Export transparent PNG frames or GIF and APNG animations with frame ranges, loop controls, and background cancellation. Regenerate older preview bundles for corrected frame indexing and timing.
- Autoload imports assignments from the original DLL mod and gives current DLL settings precedence. For benched fighters with two-overlay palettes, it applies base colors early and finishes overlay setup on tag-in.
- The native predicate cleanup fix addresses a crash when leaving Training for character select. See the recorded Training checks and the gameplay paths still to verify. [Move release readiness](docs/MOVE-RELEASE-READINESS.md)

See the [DLL guide](DllEditor/README.md) for setup and controls.

## Legacy `.pal` compatibility

The `.pal` format retains the original binary color prefix and uses a versioned effects block for advanced settings. Older editors can read base colors and may strip newer settings when saving. **Export legacy colors only** creates an exact legacy-length file for stricter tools.

The effects block stores editor-applied overrides. Unmodified built-in palette rules remain native to the destination palette slot; arbitrary native condition graphs and character animation assets are not exported. See [the format specification](docs/PAL-FORMAT.md) for the layout and validation rules.

## Build and test

Install Visual Studio 2022 with **Desktop development with C++**, the v143 toolset, and a Windows SDK. The project vendors its C++ dependencies.

```powershell
./scripts/build.ps1          # Win32 Release build and offline compatibility tests
./scripts/build.ps1 -Package # Also produce dist/PaletteEditor-1.0.1.zip
./scripts/build-dll.ps1      # Current in-game DLL editor
./scripts/prepare-release.ps1 # Complete V1 release assets, from a clean commit
```

The opt-in `/persistence-smoke` executable argument tests real save/load and auto-load against an open isolated Training runner. It temporarily changes a palette and restores it, and writes `persistence-smoke.json` plus local fixtures beside the executable. Do not run it alongside another editor. Other existing opt-in effect probes remain available for regression work.

The normal build requires the Microsoft Visual C++ x86 runtime. Automated tests cover legacy/extended files, packed color preservation, malformed input, Unicode paths, and atomic saves. Runtime validation covers colors plus all current editor effects, auto-load persistence, manual-edit retention, and session reapplication. See [contribution procedures](CONTRIBUTING.md) for the branch/review workflow.

## Project history

Based on [Skullgirls Community's Cut's palette editor](https://github.com/Skullgirls-Communitys-Cut/SkullGirls-Pallete-Editor-2nd-Encore-_Cplusplus-Version_). The [original README](docs/UPSTREAM-README.md) is retained for attribution and historical EXE/DLL instructions; those old releases are separate from this fork's V1 standalone build.
