# Whole-move triggers

The desired selection is a move, not an animation phase. A transformation should
cover that move's startup, execution, loops and recovery, then stop on completion
or interruption. The rule selector separates General tags, Character moves and
Resources, with move search restricted to the selected fighter. Hype remains the
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
**selectable in the validation DLL, but not yet gameplay-validated**.

Each resolved move contains explicit membership IDs, an exact union of consecutive
ID ranges, and outgoing boundaries for review. Gaps are never filled with nearby
animation IDs. Boundary targets are reported rather than recursively absorbed.
The tool does not execute conditions, interpret script branches, read game memory,
write palette data or install a DLL. Its generated output is an authoring tool;
the compiled runtime allowlist is separate and currently contains only these two moves.

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

## Runtime integration

V5 .pal predicates persist a stable allowlisted move ID. The native backend checks
character, researched PE signatures, current match, unique actor-bound entity,
integer animation within the researched table, and owner/palette identity again
before Apply. It does not sample or change Hype to enable a move predicate.
The native game evaluator reads current animation variable 0x27 in its fighter
context; each range uses verified >= / <= comparisons. Multiple ranges expand into
separate conditional records sharing the same table owner, so gaps stay inactive.
No polling appearance writer or new allocation loop is introduced.

Two move rules may overlap a source/shade only when their compiled memberships
are disjoint. Existing rollback, stale-match handling and retained-allocation
lifetime are reused. The viewer has one manual local move selector, cleared on
match/snapshot changes; exported pixels/redirect snapshots remain independent.
Viewer pose selection does not evaluate move predicates.

To test: choose Beowulf, open a group's Effects or Advanced transformation rules,
create a rule, choose **Trigger type → Character moves**, select Chair Toss or
Gigantic Arm, set a color link, then **Apply rules**. Save palette/Autoload supports
the V5 predicate. Existing overlapping test rules must be removed, moved to a
different source/shade, or replaced; disabled rules still reserve their shades.

## Remaining validation and roster coverage

1. Extend the explicit mappings across the roster, checking entry, loop, recovery,
   cancellation and shared-state boundaries. Keep unresolved families unavailable.
2. Resolve shared recovery using verified actor-specific move context or an
   explicitly validated entry/exit tracker; never silently claim exact whole-move
   duration using shared current-state membership alone.
3. Validate representative multi-phase moves, interruption, mirrors, tag/assist,
   save/Autoload and match reset in the isolated runner before enabling mappings.

No additional per-character resource probing is required for this catalogue.
Runtime integration can reuse the current-animation mechanism; the remaining
investigation concerns move boundaries and shared states, rather than resource
amounts or persistent modes.

## Installed validation build

DLL SHA-256:
`92EB64A9A039292369CFABAE7EA3D85E6E1B4FDDD67AB0B7ACB571626770ACA5`.
Installed into the isolated runner only after checking no SkullGirls process was
running. The previous DLL was backed up by full hash. The compiled move IDs,
labels, state ranges and FOIT digest were checked against both explicit maps and
the installed Beowulf archive before installation. Runtime guards check the
researched PE signatures and actor/entity identity; the DLL does not rehash
character archives during each Apply.

Offline validation passed: required standalone build and **120** palette/schema/
legacy/atomic checks, actual native-owner tests (all 688 animation IDs, both move
boundaries, invalid actors/builds/animations, overlap, idempotency, rollback and
restoration), all nine relevant native suites, all 26 Python catalogue/timeline
tests, and diff whitespace checks. Independent Pillow decoding confirmed export
fixtures and a local move-color snapshot after the simulation state changed.
The DLL inspection reported all four networking/lobby/Steam/protocol checks False.

Gameplay testing of this DLL is delegated to the user. No new in-game duration,
interruption, mirror, tag/assist, save/Autoload or device-transition result is
claimed for the move predicates. Other characters have metadata coverage but no
enabled whole-move mappings yet. Existing resource support remains Beowulf Hype.
