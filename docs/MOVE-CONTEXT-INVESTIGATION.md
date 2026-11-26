# Shared move-phase context investigation

The all-attacks goal remains active. Current-animation membership alone cannot
distinguish different moves entering the same timed phase. The roster builder
records these edges and excludes affected families rather than activating one
move's transformation during another move. See [coverage](ROSTER-MOVE-COVERAGE.md).

## Native observations for build 26588330

The executable's ordinary entity operand lookup already establishes script 0x27
as the float at entity+0x9c. Disassembly of the motion-assignment function beginning
at **0x0049cdf0** shows it copying entity+0x9c to **entity+0xa0**, then loading the
new motion descriptor's index into entity+0x9c. The same operand lookup exposes
entity+0xa0 through script **0x28**. This establishes a previous-assignment value;
it does not establish an original whole-move identity or preservation across loops.

The function also copies the old index to **entity+0xa4** (script **0x29**) when
entity+0x367 has bit 0x40 set, and then clears that bit. The updater beginning at
**0x00499980** sets that bit on a nonzero entity+0xe8 value (script 0x3a). This is
related to the existing same-tick advance investigation. The lifetime and useful
semantics of 0x29 are **not proven**; it is not enabled as a trigger.

The initial search of the existing bounded Ghidra instruction listing yielded no
field-write evidence. A complete local executable disassembly subsequently found
the assignments above. Raw disassembly and search output stay in ignored build/.
No new resource fields, gameplay patches or forced animation transitions were used.

## Read-only diagnostic

`scripts/read-move-context.ps1` samples only the isolated runner. It checks the
researched PE signatures, active root, requested fighter name and unique entity
whose actor owner matches that fighter. It reads floats 0x27/0x28/0x29 and the
transition byte above, rechecking ownership and match identity. Output explicitly
labels these history values as candidates. No palette/resource/gameplay memory
write API is imported. It stops after at most 60 seconds or a detected match/owner
change; observations and runtime addresses are local evidence only.

```powershell
./scripts/read-move-context.ps1 -Character Filia -Seconds 60 -OutputPath build/filia-move-context.json
```

Next controlled observations should compare Filia's Updo route with the Fenrir
route that enters the same TrapJaw fall-loop state, including idle before/after,
repeated loops and interruption. If neither history value preserves an unambiguous
route, a properly synchronized per-actor entry/exit mechanism will be necessary.
Do not infer an original move from a plausible value or polling trace alone.
Any implementation must preserve mirror ownership, tag/reset handling, one native
table owner and allocation-free steady-state operation.

## Controlled comparison and counterexample

The user loaded Filia in Training and performed Updo, then two whiffed Fenrir
Drive attempts. The read-only trace confirmed these paths with stable actor/entity
ownership. Updo entered states 161 -> 164 -> 165 -> 166 -> 167 -> 168. Fenrir
entered 182 -> 183 -> 184 -> 185 -> **167 -> 167 -> 168**. At first entry to 167,
both history values were 185 (Fenrir_Fall). After the loop repeated, current,
previous and conditional-history values were all **167**. Landing then had current
168 and both histories 167, indistinguishable from Updo's landing in isolation.

This is a direct counterexample to using either history operand as a persistent
original-move ID. Neither candidate is enabled as a standalone route provider.
The initial capture contained only idle; the repeated capture contains the actual
Updo observations. Raw evidence remains in ignored `build/filia-context-*.json` and
logs. No resource field was read and no gameplay value was changed.

## Tracking foundation

`Data/MoveRouteTracker.h` is a pure per-fighter tracker, not yet connected to live
palette writes. A prebuilt table supplies full memberships and allowed edges.
Unique phases establish a route; a verified immediate incoming edge can seed a
shared phase. Repeated shared loops and their shared landing retain the established
route. Idle/hitstun clear it, and unknown origins or unproven skipped paths report
Unavailable. Root, actor, entity, palette, slot, table and match-generation changes
invalidate prior context. Observe/Reset perform no heap allocation.

The required build now runs `MoveRouteTrackerTests`, including the actual observed
Updo/Fenrir sequences, repeated shared loops, landing, interruption, mirrors, reused
addresses, unknown origins, contradictory incoming routes, bounds and allocation
checks. These tests establish the pure state machine, not render synchronization.

Live integration still requires an update boundary before native palette predicate
evaluation. The existing Direct3D BeginScene wrapper is a candidate; its ordering
relative to palette updates must be verified. A possible adapter can update only
editor-owned constant predicate gates when the route changes, retaining native
animation bounds and the single redirect-table owner. It must never rewrite
gameplay state, rebuild tables every frame or free renderer-referenced buffers.
The 15 shared-phase families remain unavailable until that path is implemented
and tested. Further stance-entry/assist boundary audits also remain in scope.

## Synchronization evidence for the next integration

Complete local disassembly located the native palette updater at **0x00487a60**.
Its per-entity call at **0x0058fcb6** supplies the palette object in ECX and three
stack arguments: palette index, a root-owned palette cache, and the entity context.
The call follows an entity-update call at 0x0058fc5a. Other callers initialize
palette caches and can supply a null context. This does not prove that a Direct3D
BeginScene observer runs before the native predicate evaluation; do not assume it.

A synchronized observer at this **appearance updater**, preserving its original
call and arguments, is another candidate. It must not hook the motion-assignment
or gameplay functions. Any such adapter must verify the exact build/signature,
actor/entity/palette ownership, preserve unrelated native records, and update only
editor-owned condition gates before the original palette evaluator. Partial patch
or configuration failure must restore the previous header, registries and code.

The palette updater starts with `53 8b dc 83 ec 08`: push EBX, move ESP into EBX,
and subtract eight from ESP. These are **six** complete bytes, not five; a five-byte
trampoline copy would split the third instruction. No trampoline or live entry
patch has been installed by this investigation. Signature, lifecycle, cache order,
failure rollback and preserved-call tests are required before enabling one.
