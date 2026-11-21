# Skullgirls Palette Editor V1 — 1.0.0

Release files are prepared for review. Final in-game verification of the separate DLL tab, group controls, and packaged binaries is still pending. Keep this release in draft or mark it as a pre-release until the outstanding checks in `docs/DLL-NEXT-STEPS.md` are signed off. No tag or GitHub release is created by the packaging script.

## Downloads

| File | Purpose |
| --- | --- |
| `PaletteEditor-DLL-1.0.0-win32.zip` | In-game DLL plus required JSON files and instructions; recommended for the newer interface |
| `PaletteEditor-EXE-1.0.0-win32.zip` | Fresh standalone EXE build plus required JSON files and instructions |
| `PaletteEditor-1.0.0-source.zip` / `.tar.gz` | Complete tracked source at the manifest's commit, including build scripts and vendored dependencies |
| `d3d9.dll` | Raw DLL, following the earlier release convention; also download both required JSON files |
| `SkullGirls.Pallete.Editor.2nd.Encore.exe` | Raw standalone EXE; keep outside the game directory with both required JSON files |
| `groups.json`, `overlay-textures.json` | Required data files for both variants |
| `3.7.14.json` | Character labels under the historical release filename; same contents as `groups.json` |
| `SHA256SUMS.txt`, `release-manifest.json` | Asset integrity checks, exact source commit, architecture and build status |

The JSON filename `3.7.14.json` reflects the historical label dataset, not a claim of support for every game version. The native shader implementation currently targets game build **26588330**. Both binaries are Windows x86 Release builds and require the Microsoft Visual C++ x86 runtime.

## Changes

- Shared overlay textures, per-group rainbow masks, opacity, always-on color links, and conditional color transformations.
- Compatible `.pal` saving of colors and applied shader settings. Legacy import and exact legacy export remain available; no `.pal2` extension is needed.
- Full-palette auto-load with enabled assignments, file selection, status, retry, file-change detection and reapplication in new matches. Later manual edits are retained.
- The newer DLL editor retains group Copy/Paste, color wheels, and its eyedropper. Its full Advanced controls are in a separate tab; group-row Shader settings provide another entry point to the same assignments.
- The standalone EXE is rebuilt from current source with the shared backend. Its normal interface retains a collapsible Advanced section; DLL UI features have not been ported to the standalone interface.

## Installation and compatibility

Use the ZIP packages and read their README. Close the game before replacing a DLL. Back up an existing `d3d9.dll`, then extract the DLL package beside `SkullGirls.exe`. Extract the EXE package outside the game directory. Do not run both variants against the same match.

Personal `.pal` files and settings are excluded from every asset. Each variant stores its own settings. Older editors can read V1's legacy color prefix but strip the extended effect block when re-saving. Advanced assignments are editor overrides; arbitrary built-in condition graphs, character animation assets, and native super flash/sparkles are not exported.

V1 supports one overlay definition, shared rainbow timing, and one conditional transformation assignment per palette. Multiple independent textures or triggers and network transfer of shader settings remain future work.

## Validation

- Both variants are rebuilt from the exact source commit recorded in the manifest; the source tree must be clean when packaging.
- The offline suite contains 28 palette-format, legacy-compatibility, validation and atomic-file checks.
- Earlier standalone runtime probes passed persistence, overlay, rainbow, appearance, transformation and native-trigger checks and reported restoration. The exact Annie 7 → assigned 8 → 1 sequence and real new-match standalone auto-load were verified.
- The DLL render-thread probe passed combined effect save/load, legacy compatibility, invalid-file rejection and baseline restoration. Its Advanced controls were observed in-game.
- Still pending: final DLL tab/group-control visual checks, DLL Copy/Paste regression, full DLL palette-switch/hit-lighting checks, and DLL auto-load across real new matches and full restarts. These results must not be inferred from successful builds or the backend probe.

The release-preparation script checks package membership, binary architecture/version, clean manifests, checksums and source archive exclusions. It does not claim to automate the pending gameplay/UI checks.
