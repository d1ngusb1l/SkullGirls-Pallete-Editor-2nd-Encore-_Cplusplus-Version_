# Shader and texture inventory

Build 26588330; inventory taken from the isolated main-game runner on November 22, 2026. Executable SHA-256: `c18918f3592ea0bfbbee3405e0777a5dde04cbc190564762f1f88bedb3428509`.

The complete asset list (archive, path, format, byte count, SHA-256, declarations and entry points) is generated with:

```powershell
node scripts/inventory-shaders.cjs <game-root> build/shader-inventory
```

The JSON also includes every named packaged variant, all 74 palette-effect sequence names, texture dimensions, and the archive scan results. CSV provides one row per shader asset. Neither output proves that a program is currently bound by the renderer.

## Coverage

All 34 GFS archives in data01 and data02 were parsed and their payload lengths verified. Each set contains the same 97 shader paths: 10 FX source files, 48 GLSL stage files and 39 platform shader binaries. Thus there are 194 packaged entries and 97 unique paths, with 78 distinct byte contents. All shader entries are in core.gfs. Separate Salmon/SGBeta installations are outside this main-game build inventory.

There are 29 packaged family/variant labels, including aliases and platform differences. Windows compiles FX sources at runtime; the packaged stage counts are not a count of Windows programs or interchangeable palette effects. No complete CTAB-bearing shader-model-2/3 bytecode streams were found by the executable scan; this is not proof that no other embedded shader representation exists.

## Shader families

| Family | Role | Packaged variants |
| --- | --- | --- |
| DebugDraw | Diagnostic colored geometry | Base |
| Otter | UI, superflash cards, death portraits/backgrounds | Default, JojosDeathBackground, JojosDeathPortrait, Palettized, SuperflashCard, Untextured |
| renderer | World meshes, skinning, skies, lines and simple sprites | Default, Default_Alpha_Greater_255, Default_Alpha_Less_255, Lines, PalettedSprites, Rigid, Skin, Skydome, Sprites, SpritesPalette |
| TiledSprite | Character palettes, overlays, lighting, outlines and shadow passes | Base, FloorShadow, FloorShadowNoCutoff, LevelFlatBG, PaletteGradient, PaletteGradient8Bit, PaletteGradientBigCharacters, PaletteGradientHandheld, PaletteGradientNoLight, SuperOutline, SuperShadow |
| ScreenEffect | Screen/copy source entry points; packaged source is not a ready-made blur/glow filter | Source only |

## FX source entry points and helpers

These declarations are source-level possibilities, not all separate renderable passes.

| Source | Entry points and helpers |
| --- | --- |
| `core/shaders/DebugDraw_fp.fx` | Base |
| `core/shaders/DebugDraw_vp.fx` | Base |
| `core/shaders/Otter_fp.fx` | Default, Untextured, Apply_SuperFlash, SuperflashCard, GradientColor, Palettized, JojosDeathPortrait, JojosDeathBackground |
| `core/shaders/Otter_vp.fx` | Default, Untextured, Palettized, GetFlashColorLow, GetFlashColorHigh, JojosDeathPortrait, JojosDeathBackground |
| `core/shaders/renderer.fx` | GetFlashColorLow, GetFlashColorHigh, Apply_SuperFlash, Apply_SuperShadowBG, VS_Rigid, VS_Skin, VS_Skydome, compute_light, compute_lights, PS, VS_Lines, PS_Lines, VS_Sprites, PS_Sprites, PS_SpritesPalette |
| `core/shaders/ScreenEffect_fp.fx` | Default, Background_ScreenEffects, CopyInterp, CopyNoInterp |
| `core/shaders/ScreenEffect_vp.fx` | Default, Background_ScreenEffects, CopyInterp, CopyNoInterp |
| `core/shaders/TiledSprite_fp_NOLIGHT.fx` | ComputeDirLight, ComputePtLight, GetFlashColorLow, GetFlashColorHigh, Apply_SuperFlash, Apply_SuperShadowBG, ChowderEffectSample, ChowderEffectMix, CalcChowderAnchor, BandRedirect, HueShift, GetUVsForTexel, ReconstituteNormal, GradientColor, Default, Base, AlwaysWrite, ScreenSpace, FloorPlane, ForMotionBlur, Debug, Apply_LevelSuperFlash, OpaquePixels, TransparentPixels, LevelFlatBG, PaletteGradient, PaletteGradientAlt, PaletteGradientBigCharacters, PaletteGradientNoLight, PaletteGradient8Bit, SuperOutline, PaletteDEBUG, FloorShadow, FloorShadowNoCutoff, SuperShadow |
| `core/shaders/TiledSprite_fp.fx` | ComputeDirLight, ComputePtLight, GetFlashColorLow, GetFlashColorHigh, Apply_SuperFlash, Apply_SuperShadowBG, ChowderEffectSample, ChowderEffectMix, CalcChowderAnchor, BandRedirect, HueShift, GetUVsForTexel, ReconstituteNormal, GradientColor, Default, Base, AlwaysWrite, ScreenSpace, FloorPlane, ForMotionBlur, Debug, Apply_LevelSuperFlash, OpaquePixels, TransparentPixels, LevelFlatBG, PaletteGradient, PaletteGradientAlt, PaletteGradientBigCharacters, PaletteGradientNoLight, PaletteGradient8Bit, SuperOutline, PaletteDEBUG, FloorShadow, FloorShadowNoCutoff, SuperShadow |
| `core/shaders/TiledSprite_vp.fx` | VPAtlasBlock, Default, Base, OpaquePixels, TransparentPixels, AlwaysWrite, LevelFlatBG, ScreenSpace, PaletteGradient, PaletteGradientAlt, SuperOutline, PaletteGradientBigCharacters, PaletteGradientNoLight, PaletteGradient8Bit, SuperShadow, FloorShadow, FloorShadowNoCutoff, FloorPlane, ForMotionBlur, Debug |

## Built-in overlay textures

All 12 resource names below are compiled into both editors. Native filename case and case-insensitive resource keys are preserved. Pixels stay in the installed sprites.gfs archive and are requested through the game resource loader before an assignment is attached; no donor fighter or loose DDS file is required. The optional overlay-textures.json can still supply additional preset metadata.

| Resource | Dimensions |
| --- | --- |
| `sprites/chowder_BlueSpace.dds` | 2048 × 2048 |
| `sprites/chowder_bunny_fishnet.dds` | 512 × 512 |
| `sprites/chowder_Fire_large.dds` | 1686 × 1686 |
| `sprites/chowder_Fire.dds` | 512 × 512 |
| `sprites/chowder_krainbow.dds` | 1200 × 1200 |
| `sprites/chowder_Space_large.dds` | 2048 × 2048 |
| `sprites/chowder_Space.dds` | 512 × 512 |
| `sprites/chowder_umbrella_crack.dds` | 256 × 256 |
| `sprites/chowder_umbrella_veins.dds` | 256 × 256 |
| `sprites/chowder_umbrellawater.dds` | 1024 × 1024 |
| `sprites/chowder_Water_large.dds` | 2048 × 2048 |
| `sprites/chowder_Water.dds` | 512 × 512 |

## Palette editing limits

Palette/group-compatible controls are overlay texture, hue/rainbow, palette alpha, line/shadow colors and conditional band redirects. The character source also contains lighting, tint/flash and shade-mask behavior. UI shaders, world shaders, floor shadows, additional outline passes and afterimage draw scheduling require their corresponding renderer inputs and passes; inventory presence alone does not make them safe group assignments. Simultaneous two-texture rendering remains in its investigation branch.

## Loading contract

Manual load and Autoload share ApplyPalette validation. Built-in templates are available even without the optional JSON catalog. The resource check resolves sprites/chowder_<name>.dds before changing colors or effect headers and names the requested texture on failure. Legacy .pal prefixes and their existing import semantics are unchanged: plain legacy files preserve effect assignments they cannot represent; extended files without an overlay remove the editor override.
