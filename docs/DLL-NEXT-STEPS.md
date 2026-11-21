# DLL editor next steps

## Completed

- Imported the newer DLL editor baseline without replacing its group Copy/Paste, color wheel, or eyedropper implementation.
- Ported the shared overlay, rainbow, appearance, conditional transformation, and compatible `.pal` backend to the render callback. In-process updates avoid suspending the current process and call the game's allocator directly.
- Added full-effect save/load, legacy export, enabled auto-load assignments, palette-switch detection, and native allocation tracking across matches.
- Moved full Advanced controls to their own tab beside Palette. Kept character/palette selectors in both tabs and added group-row Shader settings.
- Built the DLL and observed its Advanced controls in the isolated running game.
- The DLL render-thread probe passed combined effect save/load, legacy compatibility, invalid-file rejection, and baseline restoration on November 21, 2026. All 28 shared offline format checks also passed after the backend refactor.
- Built and packaged the production DLL with validation probes disabled; checked archive contents and a clean-source manifest. Changes are committed on the feature branch.

## Release gates, in order

1. Verify the final separate-tab layout in-game, including shared character/palette selection and ordinary color editing.
2. Exercise group Copy/Paste and the new group Shader settings. Check Apply/removal, native band mapping, shared-setting restrictions, and saving/reloading the same assignments from both entry points.
3. Repeat 7 → assigned 8 → 1 switching in the DLL. Check ordinary palettes, hit lighting, and changes between characters. Confirm no assignment leakage or black rendering.
4. Test DLL auto-load after an actual new match and full game restart; verify Unicode paths, missing/invalid-file status, disabled entries, Retry, and retention of manual edits. Keep QA fixtures out of normal settings.
5. Test the packaged production DLL after installation, push the feature branch, and open a draft PR against the fork. Do not merge or publish a release before review.

## Later feature work

- Multiple independent transformations/triggers per palette, with explicit priority and overlap behavior. V1 currently has one transformation assignment and a shared trigger.
- More complete named movement triggers, including distinctions between jumping and air-dash states, with cross-character visual validation.
- Multiple overlay textures on different groups of the same palette. V1 currently has one overlay definition and shared texture/lighting settings.
- Additional native shader presets and palette-specific visuals, including which super flash/sparkle assets can be reused independently of character-authored animations.
- Redesign and validate network compatibility before restoring any online palette sharing. Version 1.0.1 excludes legacy palette networking and editor Steam lobby/chat code from the build entirely.

Passing the backend probe does not imply the remaining UI and gameplay scenarios have been visually verified.
