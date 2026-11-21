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

## Implementation after visual confirmation

1. Represent overlay assignments as a bounded list with independent texture, bands, lighting, and tint settings. Define overlap behavior explicitly; initially reject overlapping groups.
2. Keep existing legacy `.pal` color prefixes unchanged and import existing single-overlay V1 documents. Version the extended schema if the list cannot be represented compatibly; older readers must reject unsupported effects instead of silently losing them.
3. Make Advanced and group-row controls edit the same applied list. Applying or removing one group must preserve the others.
4. Add offline tests for migration, multiple-definition round trips, overlap/range limits, and malformed input. Add render-thread round-trip and restoration checks.
5. Verify simultaneous rendering, ordinary palette switching, hit lighting, save/load, auto-load and new matches. Preserve the compile-time exclusion of online palette sharing.

If the native renderer only supports one active texture resource despite multiple records, document that result and investigate the shader/feature builder before exposing controls that imply unsupported behavior.
