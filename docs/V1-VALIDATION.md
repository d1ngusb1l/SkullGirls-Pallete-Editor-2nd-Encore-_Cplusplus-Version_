# V1 release candidate validation

Validated on November 21, 2026 against the isolated Training runner for game build 26588330.

- Win32 Release build succeeded; 28 offline format and atomic-file checks passed.
- Runtime persistence, overlay, rainbow, appearance, transformation, and native-trigger probes passed and reported restoration of their starting state. Probes ran separately from the GUI editor.
- Persistence covered all applied effects together, applied settings versus draft controls, legacy import/export, invalid-file rejection without live changes, assignment persistence, manual-edit retention, and session reapplication.
- GUI loading through the native file picker restored the saved baseline colors.
- The reported Annie 7 → assigned 8 → 1 sequence passed after removing a leftover QA assignment for Palette 1. Palette 1's colors and its overlay, redirect, and hue configuration matched the clean baseline after the cycle. The user's Palette 8 file and assignment were preserved.
- With the editor left open across a real Training restart, the new native palette allocation was detected. Selecting assigned Palette 8 loaded the same saved colors and its conditional group redirect again. The editor then returned to Palette 1 and exited normally.

The switch and fresh-match checks used read-only native snapshots to verify colors and effect records; they do not constitute visual confirmation of every possible shader combination. Earlier individual effect work received user visual confirmation. Additional characters, overlapping native rules, and future game builds remain areas for regression testing.

Personal palettes, settings, raw snapshots, and generated binaries are excluded from source control. Release archives contain only the executable, required data, user documentation, and a source/hash manifest.
