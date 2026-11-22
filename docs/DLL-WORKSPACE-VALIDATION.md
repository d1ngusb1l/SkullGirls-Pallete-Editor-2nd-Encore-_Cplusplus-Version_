# DLL palette workspace validation

Baseline: local `codex/group-overlay-updates`, commit `5932b52` (recorded two-overlay DLL validation in MULTIPLE-OVERLAYS.md). Remote fetch was unavailable on November 22, 2026.

Phase 1 separates the searchable group list and inspector, adds the persistent target/Autoload bar and numeric palette navigation, puts overlay staging and Apply together, and makes unequal-size paste explicit with a guarded whole-paste undo. Colors and non-overlay shader edits retain immediate live application; overlay assignments remain pending until Apply. Target changes offer Apply, Discard and Cancel for pending overlays. Palette-wide controls stay in Advanced; assignment editing stays in Auto-load. The native .pal codec and networking exclusions are unchanged.

Validation build: `scripts/build-dll.ps1 -Validation`. Networking exclusion inspection reports all four flags false. In-game workflow validation is pending for this branch; prior baseline observations are not new-workflow verification.
