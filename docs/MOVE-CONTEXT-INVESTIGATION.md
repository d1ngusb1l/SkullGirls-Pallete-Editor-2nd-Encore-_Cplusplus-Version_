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
