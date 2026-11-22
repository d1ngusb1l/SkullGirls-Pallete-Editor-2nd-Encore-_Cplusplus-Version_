# Multiple overlay textures on separate groups

The next requested update is independent texture choices for different native groups on one palette. Version 1.0.1 stores one overlay definition; its bands share the texture, lighting, and tint settings.

## Native feasibility check

Before changing the portable format or production controls, the validation DLL can attach two 120-byte native overlay records to the current palette: Fire on one group and Water on another. The masks are disjoint. The renderer must visibly show both textures simultaneously; merely observing two records in memory is not sufficient.

The opt-in request is `dll-overlay-probe.request.json` in the runner working directory, for example:

```json
{"slot":0,"firstBand":1,"secondBand":15,"firstTexture":"Water","secondTexture":"umbrella_veins","seconds":30}
```

The example targets Player 1 Peacock's hat main and dress main. `umbrella_veins` must be available in the live catalog. Confirm the live character and native bands before running. The probe validates the request, reads the original vector header, allocates isolated native records, and restores the original header after the bounded interval. It writes `dll-overlay-probe-result.json` with the texture names, group labels, duration, native record count, and restoration status. Match changes discard stale pointers. Saved palettes, assignments and configuration are untouched. This code is excluded from production DLLs.

## Observed results

On November 21, 2026, the Player 1 Peacock Color 10 probe attached Water to hat main (native band 1) and Ravenous Veins (`umbrella_veins`) to dress main (band 15). A read-only live snapshot confirmed the two different native texture names. The user observed the same pattern on both groups. Reversing the two texture assignments produced Ravenous Veins on both groups. The earlier Fire/Water observation was visually inconclusive.

This establishes that two disjoint native overlay records alone do not provide independent per-group textures in this tested draw. It suggests a shared texture binding; the exact resource-selection rule still needs tracing. Do not expose a multiple-texture production UI or change the saved format based on the vector-record count alone.

These two tests reused the installed hardcoded probe by temporarily aliasing its catalog entries. The original catalog bytes were restored immediately after each request was consumed. Future validation builds accept explicit texture names as shown above, avoiding aliases and ambiguous report labels.

Both timed probes reported successful restoration of the original overlay header. The restored catalog hash matched the source catalog. The configurable validation DLL rebuilt successfully with the online-networking exclusion checks still passing. Production packages were not changed by these experiments.

The native Fire/Water probe was repeated on November 21, 2026 with Fire on
Peacock hat main (band 1) and Water on dress main (band 15). The live result
showed the same texture on both groups, confirming that the character draw's
single Chowder texture binding is shared rather than independently selected per
overlay record.

## Shader and texture tracing

The validation D3D9 proxy can optically capture the pixel shader and texture bindings used during live draws. Create `renderer-trace.request` in the game runner working directory, then continue for two presented frames. It writes `renderer-trace/capture.json` and up to 32 unique pixel-shader binaries in `renderer-trace/`.

The capture is read-only: it does not modify shaders, textures, constants, or draw calls. It records draw kind, primitive count, shader identity and constants, bound textures and texture dimensions, and pixel constants used by named shader constants. The request, output and tracer code are validation-only and excluded from production builds.

## Implementation after visual confirmation

1. Represent overlay assignments as a bounded list with independent texture, bands, lighting, and tint settings. Define overlap behavior explicitly; initially reject overlapping groups.
2. Keep existing legacy `.pal` color prefixes unchanged and import existing single-overlay V1 documents. Version the extended schema if the list cannot be represented compatibly; older readers must reject unsupported effects instead of silently losing them.
3. Make Advanced and group-row controls edit the same applied list. Applying or removing one group must preserve the others.
4. Add offline tests for migration, multiple-definition round trips, overlap/range limits, and malformed input. Add render-thread round-trip and restoration checks.
5. Verify simultaneous rendering, ordinary palette switching, hit lighting, save/load, auto-load and new matches. Preserve the compile-time exclusion of online palette sharing.

If the native renderer only supports one active texture resource despite multiple records, document that result and investigate the shader/feature builder before exposing controls that imply unsupported behavior.

## Live bindings and shader experiment

The runner trace captured 308 draws, including four character draws using pixel shader 4f2d54bdda78e7fd. Its constant table declares diffuse, palette, feature, and Chowder textures at s0 through s3 respectively. The captured character draws had no overlay texture bound, so this trace establishes the sampler layout but does not identify the winning texture when both effects are active. The two players had distinct palette and feature texture objects. Those objects are candidates for identifying the target draw, subject to verification across palette swaps and device resets.

The packaged Windows shader samples one Chowder texture, then applies its color inside the palette lookup using the post-redirect group's feature data. The prototype adds a second overlay at s4 and a point-sampled band selector at s5. It chooses the overlay after the redirect so neighboring samples and borrowed colors follow the existing palette lookup. It shares the native anchor, tint and blend settings for this first experiment.

Compiled against the locally extracted shader with Windows SDK FXC 10.0.26100.0. The standard PaletteGradient build retains the observed c0–c34 and s0–s3 bindings and adds s4–s5. The compiler reports 581 instruction slots for the prototype versus 540 for a recompile of the stock source. The NoLight prototype compiles at 386 slots. Compilation alone does not establish device acceptance or equivalence to the captured native shader. The generated local artifacts are in build/overlay-shader.

The runner was closed during compilation. Live validation needs a targeted draw substitution that checks shader identity, matches the target palette resource, validates device support, and restores the original shader and added sampler state after the draw. Test the lit variant first, then verify hit lighting and other palette shader variants before exposing controls or changing the file format.

The validation DLL also accepts an overlay-shader-check.request file. It tries to create baseline.bin, prototype.bin and unlit.bin from the runner's overlay-shader directory and records device limits and creation results in overlay-shader-check-result.json. It releases each shader immediately after the check. No shader is bound during this test.

The live runner accepted the baseline, lit prototype and unlit prototype through CreatePixelShader. All returned S_OK. The active device reports 4096 ps_3_0 instruction slots, so the 581-slot lit prototype fits on this device. A draw-level visual test is still pending.

## Live Water and Ravenous Veins bindings

Three captures on Player 1 Peacock Color 10 used Water on both test bands, Ravenous Veins on both, then Water on the hat and Ravenous Veins on the dress. The four draws per capture retained the same shader identity. The two players had distinct feature texture objects, and Player 1's feature texture object stayed stable throughout the captures. Sampler 3 used distinct objects in the single-texture cases. The mixed case bound the Water object only, consistent with the first definition winning in this arrangement. All three probes verified restoration of the original headers.

Water exposed a Chowder palette index of zero. Ravenous Veins exposed 39/255 despite the probe's zero tint input. The initial shader prototype shares a single tint uniform, so a faithful substitution should preserve each resource's effective tint as well as selecting the texture by band. The captures are session-local and their texture pointers must never be reused across processes.

## Draw substitution validation

The validation DLL accepts overlay-draw.request.json. During the Water-only native probe, send {"action":"capture-first","feature":"<sampler 2 object from this session>"}. During the Ravenous Veins-only probe, send the same request with capture-second. The DLL retains references only from draws matching the selected feature texture and the known lit shader. Start the mixed native probe with Water first, then send {"action":"preview","secondBand":15}. The 15-second preview uses locally compiled overlay-shader/prototype.bin and records the number of replaced draws in overlay-draw-result.json.

The band lookup stores texture selection in red and native tint in green. The replacement keeps the original lit shader constant register layout, including c30–c34 after removing the old shared tint uniform. The current prototype uses shared anchor and blend settings. Verify the picture and hit lighting in Training before adding editor controls or changing palette persistence.

Live result on November 21, 2026: Player 1 Peacock Color 10 showed Water on hat main and Ravenous Veins on dress main simultaneously. The user confirmed both distinct patterns and the draw probe reported 1,182 successful substituted draws. The native probe verified restoration of the original overlay header. Hit lighting, palette switching, and reset coverage still need targeted tests. This result validates the lit character draw path on the tested runner.

The user also confirmed that neither group turned black under hit lighting during the live preview. Palette-switch and device-reset regression testing remains open. The validation DLL cancels its preview and releases captured textures on an editor palette change, a detected palette change for the selected character, a match change, or a D3D device reset.
