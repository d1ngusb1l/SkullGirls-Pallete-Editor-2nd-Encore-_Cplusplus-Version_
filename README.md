# Skullgirls Palette Editor — V1 release candidate

V1 release assets are prepared with `scripts/prepare-release.ps1` from a clean committed tree. It rebuilds the DLL and standalone EXE, runs offline checks, and creates `dist/v1.0.0/` containing both complete packages, raw assets matching earlier release conventions, source ZIP/tar.gz archives, release notes, and SHA-256 checksums. It does not create a tag or publish a release. See [release notes and validation status](release-docs/RELEASE-NOTES.md) and [DLL installation](release-docs/INSTALL-DLL.md).

This fork brings the current advanced palette features into one editor, with compatible `.pal` saving and full-palette auto-loading. The standalone build currently targets Skullgirls game build **26588330**.

## Use

Keep `PaletteEditor-V1.exe`, `groups.json`, and `overlay-textures.json` together in a writable folder outside the game directory. Start the game and editor, then open a match.

- **Palette** contains the normal color editor. Its collapsed **Advanced** section contains overlay textures, rainbow, opacity, always-on color links, and conditional transformations. Existing named triggers and manual previews remain available.
- Apply advanced changes before saving. **File → Save palette** stores live colors, opacity, line/shadow colors, and applied advanced assignments in one `.pal` file. Unapplied controls are not saved.
- **File → Load palette** accepts both plain legacy `.pal` files and extended V1 files. Invalid or mismatched files are rejected before game data changes.
- **File → Save palette and auto-load** saves and assigns the file to the selected character/palette number. The **Auto-load** tab also lets you add, browse, enable/disable, and retry assignments.
- Auto-load waits for the assigned character and palette number, applies once per match/palette, and reapplies after a new match or file update. It does not continuously overwrite subsequent manual edits. Disabling an assignment stops future loads; it does not undo the current palette.

Keep the editor running for auto-loading. Advanced overrides are restored on normal editor exit; color edits retain the original editor's in-match behavior. Settings live in `PaletteEditorConfig.json` beside the executable. On first use, existing `Documents/Skullgirls/PalEditConfig.json` settings are copied when available; the legacy settings file is left untouched.

## Legacy `.pal` compatibility

A new file extension is **not** required. V1 preserves the original binary color prefix and appends an optional versioned effects block. Older readers can read the colors; they cannot apply the new effect assignments. Saving through an older editor strips that block. **Export legacy colors only** creates an exact legacy-length file for stricter tools.

V1 stores editor-applied overrides. Unmodified built-in palette rules remain native to the destination palette slot; arbitrary native condition graphs and character animation assets are not exported. See [the format specification](docs/PAL-FORMAT.md) for the layout and validation rules.

## Build and test

Install Visual Studio 2022 with **Desktop development with C++**, the v143 toolset, and a Windows SDK. The project vendors its C++ dependencies.

```powershell
./scripts/build.ps1          # Win32 Release build and offline compatibility tests
./scripts/build.ps1 -Package # Also produce dist/PaletteEditor-1.0.0.zip
./scripts/build-dll.ps1      # Current in-game DLL editor
./scripts/prepare-release.ps1 # Complete V1 release assets, from a clean commit
```

The opt-in `/persistence-smoke` executable argument tests real save/load and auto-load against an open isolated Training runner. It temporarily changes a palette and restores it, and writes `persistence-smoke.json` plus local fixtures beside the executable. Do not run it alongside another editor. Other existing opt-in effect probes remain available for regression work.

The normal build requires the Microsoft Visual C++ x86 runtime. Automated tests cover legacy/extended files, packed color preservation, malformed input, Unicode paths, and atomic saves. Runtime validation covers colors plus all current editor effects, auto-load persistence, manual-edit retention, and session reapplication. See [contribution procedures](CONTRIBUTING.md) for the branch/review workflow.

## Project history

Based on [Skullgirls Community's Cut's palette editor](https://github.com/Skullgirls-Communitys-Cut/SkullGirls-Pallete-Editor-2nd-Encore-_Cplusplus-Version_). The [original README](docs/UPSTREAM-README.md) is retained for attribution and historical EXE/DLL instructions; those old releases are separate from this fork's V1 standalone build.
