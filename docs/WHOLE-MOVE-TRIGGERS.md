# Whole-move triggers — first implementation stage

The desired selection is a move, not an animation phase. A transformation should
cover that move's startup, execution, loops and recovery, then stop on completion
or interruption. The eventual selector will separate General tags from Character
moves, with a searchable list restricted to the selected fighter. Hype remains the
only implemented resource provider; this work introduces no resource investigation.

## Implemented

`scripts/prepare-move-catalog.py` builds a metadata-only catalogue from the user's
local archives, using the existing bounded FOIT reader. The reader now optionally
retains root animation IDs, raw tag hashes, successor links, conditional transition
targets and source-frame counts. Existing preview output and timing are unchanged.
Child tables are parsed but never mixed into the fighter's root-state namespace.
Dispatchers and other states with no previewable sprite remain in the catalogue.

The local build produced **10,460 root states across all 18 playable characters**.
The catalogue also resolves known general tags for filtering. These counts describe
authored states, not a claim that 10,460 distinct gameplay moves are implemented.
Generated catalogue output stays in ignored build/, outside Git and packages.

`PalleteEditor/data01/move-families.json` introduces explicit, build-bound whole-move
definitions. The first mappings are Beowulf Chair Toss (all strengths, eight states)
and Gigantic Arm (three states). Their startup, loop/dispatcher and recovery states
are one selection each. The maps bind to the full character FOIT SHA-256 and resolve
exact authored names; missing names, ambiguous names, mismatched builds, duplicate
identities and shared state ownership reject. These are authored-data mappings,
**not yet gameplay-validated or selectable live triggers**.

Each resolved move contains explicit membership IDs, an exact union of consecutive
ID ranges, and outgoing boundaries for review. Gaps are never filled with nearby
animation IDs. Boundary targets are reported rather than recursively absorbed.
The tool does not execute conditions, interpret script branches, read game memory,
write palette data or install a DLL. Palette schema and runtime behavior are unchanged.

Run from the repository root:

```powershell
python scripts/prepare-move-catalog.py GAME_DIRECTORY build/move-catalog.json PalleteEditor/data01/move-families.json
python -m unittest discover -s tests -p MoveCatalogTests.py
```

## Why a generic reachability walk is insufficient

The authored transition lists include cancels into other moves, hit reactions,
snaps and other interruptions. Following every edge would include unrelated moves.
Even shared recovery matters: Black Dahlia `Reload_Catch` has a successor named
`s.GunShot_ReversalWindow`, which is also reached from gunshot states. The name or
current state alone cannot distinguish which action entered a shared recovery.
The initial resolver deliberately rejects assigning one state to two move entries;
it does not pretend this resolves the underlying shared-state problem.

For the Beowulf examples, Chair Toss includes its named reversal windows, which
lack the SPECIAL tag. Gigantic Arm's loop exits into its own end state; its end
returns to Stand. Hit/cancel/DHC destinations are outside the selected membership.
These are file observations, not newly observed in-game duration checks.

## Next integration steps

1. Extend the explicit mappings across the roster, checking entry, loop, recovery,
   cancellation and shared-state boundaries. Keep unresolved families unavailable.
2. Add a typed whole-move predicate to palette persistence, preserving V1–V4 and
   rejecting unsupported move IDs/builds. Persist stable move identity, not runtime
   addresses or samples. Do not treat a move ID as a generic animation tag.
3. Compile exact state membership through the existing native animation variable
   0x27 and verified comparisons. Multiple ranges need a bounded union, composed
   through the existing single table owner. Retain rollback and conflict checks.
4. Resolve shared recovery using verified actor-specific move context or an
   explicitly validated entry/exit tracker; never silently claim exact whole-move
   duration using shared current-state membership alone.
5. Add General / Character moves selection to the staged rule editor. Keep phase
   details behind Advanced, and keep isolated-viewer simulation manual and local.
6. Validate representative multi-phase moves, interruption, mirrors, tag/assist,
   save/Autoload and match reset in the isolated runner before enabling mappings.

No additional per-character resource probing is required for this catalogue.
Runtime integration can reuse the current-animation mechanism; the remaining
investigation concerns move boundaries and shared states, rather than resource
amounts or persistent modes.
