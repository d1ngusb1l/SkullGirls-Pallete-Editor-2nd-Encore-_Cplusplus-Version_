# Palette shader load validation

Branch: codex/palette-shader-load-fixes, based on fetched origin/master 2c8745d.

The saved assignment is decoded by PaletteData::ReadFile. Manual Load and Autoload both call OverlayEditor::ApplyPalette; its validator checks character/group layout and resolves the requested native sprites/chowder_<name>.dds asset before writes. Apply attaches the selected-band overlay record. Refresh rebuilds the GPU feature table; the native Chowder cache creates the render texture and the character draw samples it. An extended clean palette removes the override; other palette slots retain their own overlay headers.

The previous catalog check established only preset metadata availability. Missing overlay-textures.json left supported textures dependent on presets from loaded fighters. Built-in templates now contain every Chowder resource shipped in this build, without copied pointers/cache state. Native asset resolution supplies pixels on demand and fails with the requested name before palette mutation.

The initial File → Load Palette run with the existing runner DLL and optional catalog present loaded Fire successfully from an empty Chowder cache. The user confirmed Fire was visible. That run did not reproduce a missing-pixel failure. The missing-catalog case and fresh new-DLL paths must be assessed separately below.

Transformation links remain serialized and written as [receiving band, donor band]. Copy colors from selects the donor; Apply colors to selects the receiving band. Thus [Hair, Shoes] makes Hair borrow Shoes, including existing files. No map migration or .pal schema change is needed.

## Build checks

- DLL Win32 Release validation build passed; networking-disabled checks passed.
- Standalone EXE Win32 Release build passed; all 28 format/legacy/atomic-I/O checks passed.
- Archive scan: all 12 built-in names match sprites.gfs; native hashes match the three legacy catalog records.
- Detailed inventory: docs/SHADER-INVENTORY.md and generated build/shader-inventory/inventory.json / inventory.csv.

## Runtime checks

Verified November 22, 2026, in fresh offline Filia-versus-Filia Training matches. The test DLL was installed only in .work/lobby-spectator/runtime-game; its previous DLL and optional texture catalog were backed up. No installed-game DLL was changed.

- **File → Load Palette:** Fire assigned to Hair loaded with overlay-textures.json absent and the Chowder renderer cache initially empty. The resulting native cache entry had the Fire key 0x7C7E165D and a non-null GPU texture. The visual effect was observed.
- **Autoload:** applying an assignment in the active match passed. After a full process restart, the saved Filia palette-2 assignment again applied Fire and created its render texture without a Fire-using fighter or the optional catalog.
- **Clean file:** loading the extended clean .pal removed the Fire override and restored the normal hair appearance.
- **Palette slots:** using the editor's palette selector, 2 → 3 removed Fire and showed slot 3's native colors; 3 → 2 restored Fire. The effect did not leak to the clean slot.
- **Transformation direction:** an existing-format [1,22] link (Hair receiving Shoes colors) with the automatic IDLE tag produced pale Hair while idle and native blue Hair outside the trigger. The user confirmed Hair borrows Shoes, and the applied row displayed “Copy colors from shoes; apply colors to hair.” Reset restored native appearance.
- **Texture availability:** the validation DLL loaded all 12 built-in native DDS resources. A genuinely absent MissingTexture resource returned “Could not load overlay texture 'MissingTexture': The installed game could not load sprites/chowder_MissingTexture.dds.” This asset check precedes palette mutation.
- **DLL persistence regression:** the render-thread smoke check passed combined-effect round trip, legacy compatibility, truncated-file rejection and baseline restoration.

Evidence is retained in ignored build/ files: fresh-texture-cache.json, manual-fire-cache.json, autoload-fresh-cache.json, texture-resources-result.json and dll-smoke-result.json. Production DLL and standalone EXE builds passed; standalone runtime load paths were not separately exercised in this run. The installed validation DLL differs from the production artifact only by the opt-in validation build configuration.

The runner retains the Filia palette-2 Fire Autoload test assignment. The temporary Idle transformation was reset. Plain legacy imports retain their existing semantics (preserving assignments the format cannot represent); clean extended files and native palette-slot switches were the clearing tests.

These checks establish supported resources in an intact build 26588330 installation. They do not promise success with missing/damaged archives, unsupported game builds, failed process access or graphics-device failures. Native asset leases are retained until game exit, matching the backend's retained-buffer lifetime model.
