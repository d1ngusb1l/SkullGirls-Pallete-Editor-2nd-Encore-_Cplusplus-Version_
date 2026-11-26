# Shared move-phase context investigation

The all-attacks goal remains active. Current-animation membership alone cannot
distinguish different moves entering the same timed phase. The roster builder
records these edges; affected families require the synchronized DLL context
implemented below. Unknown origins leave their transformations inactive.
See [coverage](ROSTER-MOVE-COVERAGE.md).

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

`Data/MoveRouteTracker.h` is a pure per-fighter tracker connected through the
DLL appearance observer described below. A prebuilt table supplies full memberships and allowed edges.
Unique phases establish a route; a verified immediate incoming edge can seed a
shared phase. Repeated shared loops and their shared landing retain the established
route. Idle/hitstun clear it, and unknown origins or unproven skipped paths report
Unavailable. Root, actor, entity, palette, slot, table and match-generation changes
invalidate prior context. Observe/Reset perform no heap allocation.

The required build now runs `MoveRouteTrackerTests`, including the actual observed
Updo/Fenrir sequences, repeated shared loops, landing, interruption, mirrors, reused
addresses, unknown origins, contradictory incoming routes, bounds and allocation
checks. These tests establish the pure state machine, not render synchronization.

Live integration requires an update boundary before native palette predicate
evaluation. The existing Direct3D BeginScene wrapper is a candidate; its ordering
relative to palette updates must be verified. A possible adapter can update only
editor-owned constant predicate gates when the route changes, retaining native
animation bounds and the single redirect-table owner. It must never rewrite
gameplay state, rebuild tables every frame or free renderer-referenced buffers.
The implementation below supplies this path for the 15 shared-phase families;
its live gameplay checks are still pending. Further stance-entry/assist boundary audits also remain in scope.

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
trampoline copy would split the third instruction. The DLL now contains a guarded six-byte entry redirect and retained trampoline.
It installs only when a shared-phase rule is applied and restores the original
entry when the last such rule is removed. The candidate is now installed in the
isolated runner; gameplay validation remains pending.


## Implemented synchronized host (gameplay validation pending)

`NativePaletteObserver` preserves the palette object, all three stack arguments,
and the original call, including null-entity initialization. The observer and
original evaluation are serialized together. The six-byte signature, researched
PE build, partial writes, verification, rollback and foreign-entry ownership are
guarded. Retained delegate code and native conditions are never freed mid-match.
A private synthetic x86 function test exercises the same prologue, calling
convention and stack cleanup across 10,000 calls; it is not an in-game test.

`MoveContextHost.inl` validates the active root/mode, unique actor slot and root
entity, character name, palette identity/index, integral motion/history bounds,
and installed redirect header. It updates only editor-owned float constants in
native predicate chains before evaluation. It never writes actor state or motion
fields. Invalid snapshots, unknown origins and failed gate writes clear the
applicable gates. Child calls do not establish a root fighter route.

The 15 additional definitions expand through explicitly reviewed shared helper
loops and landing tails. Six generated route tables cover their owning fighters.
The stateless mappings retain native animation range bounds; the boundary audit
now exposes three additional Beowulf finishers independently (573 stateless
plus 15 contextual families).
A shared-phase predicate ANDs those bounds with its own route gate. Different
contextual routes can safely share affected shades because a fighter has only
one active route. Standalone hosts reject these rules before native allocation.
Configuration allocates once; stable observations allocate nothing and perform
no unchanged gate writes. Runtime pointers and gates are not serialized.

Tests cover the recorded Filia paths, mirrors, inactive palette gates, palette
changes, stale headers, epoch/owner changes, unknown origins, gate-write failures,
installation rollback, independent rules, idempotent Apply and final removal.
The required gameplay checks are Updo versus whiffed Fenrir with distinct colors
on the same group, their shared loop and landing, interruption, save/reload,
match changes, and mirror/tag/assist behavior. Other roster families are authored
data coverage, not individually verified attacks. Stance and substitution
boundaries still require audit before claiming complete all-attacks support.


## Untagged attack boundary correction

The source audit found that the old catch-all Grab Mode mapping included Bulldog
(319-329), Gringo Killa (330-337), and grab-mode snapback (285-289). Their native
scripts have no ATTACK/NORMAL/SPECIAL/SUPER/THROW tags, so counting all tagged
states had hidden this lack of individual selections. Explicit reviewed untagged
exceptions now expose their complete bodies, including EX and ground/air
variants, separately from holding/movement/release. The existing Grab Mode ID
is retained with corrected narrower membership; files using that ID must select
a finisher explicitly to affect that finisher. No new resource predicate is used.

Regression tests require exact ranges, full state counts, disjoint parent
membership, explicit categories, and rejection of missing or invalid exceptions.
Authored coverage is now 588 families. Attack tags and selection counts cannot
alone establish all-attacks completion.

## Installed candidate (2026-11-26)

The 588-family validation DLL was installed after confirming no SkullGirls process
was running. Its SHA-256 is
`33986A144E30314983C6542393D46DFF16597061B7997066C0D3458E2B070FCA`.
The previous DLL (`11105B9FDDB7280F25171A28C77C06A8393ECC745E167BCCF2DBB94A5086D51D`)
was backed up and the installed file was hash-verified. The standalone build,
2,520 compatibility checks, native owner/route/entry/service tests, 41 Python
checks and networking-disabled inspection passed. This is installation evidence,
not proof that every move or the synchronized observer works in gameplay.

## User validation and cleanup crash (2026-11-26)

Two live Filia palette-1 hair rules were applied through the editor: Trap Jaw
copies Samson eye colors (gold), and Fenrir copies Samson mouth colors (pink).
The editor reported both rules applied and the context status changed between
Idle and Tracking move. Gold and pink appearances were observed during the
user's actions, and the user confirmed the requested full-move behavior worked
perfectly, including the previously requested shared fall/landing comparison.
This verifies the Filia case, not every contextual family or match transition.

The user also reports that ground and air Scalpels transformations work and end
before ordinary falling. A source-only origin reachability audit flags the three
DioScalpels-named helper states (216-218): Alpha Blade and Flat Line have authored
routes to them, while Dio Scalpels has no such route in the extracted graph.
The generator now reports these unproven origin memberships instead of treating
their names as proof. No membership change has been made from this audit alone.

The transition back to character select reproduced a `0xc0000374` heap-corruption
crash with both the Filia setup and the supplied three-rule Valentine test palette.
The second dump identifies the freed pointer as the exact constant gate shared by
Alpha Blade's two disjoint redirect records. A read-only pre-exit ownership
snapshot contains both references. Native palette cleanup reaches the shared
allocation twice. This connects the failure to the installed graph ownership;
the earlier stack-only dump alone could not identify the corrupt allocation.

The correction gives every generated redirect record its own entire predicate
chain, including its own route constant. Conditions are no longer cached/shared
across affected bands or disjoint animation ranges. Native animation bounds and
single-table composition remain unchanged. Retired tables are still not freed
by the editor while rendering can reference them; the game's own teardown owns
the currently installed table's native graph lifetime.

Context suspension now clears CPU route history and marks gate values unknown
without writing through potentially retired pointers. Matching live headers force
fresh values when tracking resumes. Match cleanup abandons bindings without native
writes. Stale inactive-palette headers are skipped, and failed updates only retry
zero on bindings whose installed headers still validate. This preserves valid
live-owner recovery while avoiding writes into graphs already released by teardown.

Regression coverage destroys four complete graphs from two links and two ranges,
rejects any repeated node, and checks that cleanup performs no native writes after
release. Service tests cover stale headers, no-write suspension/abandonment, and
fresh live-owner recovery. The installed correction's repeated exit result is
recorded below. Raw dump addresses,
heap snapshots and the supplied test palette remain local ignored artifacts.

The native redirect-vector destructor also requires its large-buffer allocation
layout: at 4096 bytes and above, data is 32-byte aligned with the original native
allocation pointer at data-4 (the recovered offset must fit within 31 bytes).
Redirect tables now use that layout; scalar condition nodes retain their ordinary
native allocation. A larger multi-rule table regression checks recoverable
ownership and restoration across this threshold.

The read-only trace script now constructs UIntPtr from UInt64 explicitly, fixing
Windows PowerShell 5.1's rejection of a direct Int32-to-UIntPtr cast. The failed
Valentine trace collected no motion evidence and is not counted as validation.

## Installed cleanup correction (2026-11-26)

The corrected validation DLL was installed with no SkullGirls process running,
after backing up the previous candidate and verifying both hashes. Installed
SHA-256: `2B5F588E380CF9AB37D289496592AD0B0CF3DFCDA3EE22A93BFBB99643246F52`.
The required standalone build and 2,520 compatibility checks, native ownership/
teardown and large-vector regression, route/entry/service suites, 41 Python tests,
PowerShell syntax and UIntPtr compatibility checks, and DLL/network-disabled
inspection passed. The same Valentine test palette remains available locally.
The user reloaded the same three-rule Valentine test palette and confirmed that
returning from Training to character select no longer crashed. Before that exit,
a read-only audit found 71 appearance records with predicates and no shared
condition nodes; the previously shared Alpha Blade tail was no longer aliased.
The same runner process remained responsive afterward and no new crash dump
appeared. This verifies the reproduced transition after the ownership correction,
not every possible lifecycle or roster behavior. A motion trace attempted after
the exit rejected the inactive match and captured no motion samples.

## Eliza / Sekhmet validation (2026-11-26)

The supplied Eliza test palette contains eight automatic move rules affecting
nine Sekhmet bands. Its 99 generated redirect records cross the native 4096-byte
large-vector threshold before adding any original records. The native palette
decoder and round trip accepted the fixture. The user confirmed that Sekhmet's
body colors followed the moves, restored at idle, and leaving Training for
character select did not crash. No new DLL was needed for the alternate form.

A bounded read-only motion trace captured 138 observations on one stable actor /
registered entity identity, with no invalid current-animation samples. It includes
Eliza Stand and Summon, Sekhmet Stab/Kopesh, ground and air Axe, ground and air
Lunge, air Cartouche, and Sekhmet idle/movement/jump states. It stopped on match
change. This establishes that those alternate-form motions use the same verified
root animation provider; it does not prove unobserved supers or every spawned
entity behavior. The later ownership probe correctly rejected the inactive match.

The first fixture deliberately recolored blood/anchor-blood effect bands as
destination color banks, which also tinted the idle anchor blue/yellow. The user
reported that side effect while Sekhmet correctly remained default at idle. A
separate clean-anchor fixture restores those original effect colors and uses
Albus color banks instead. Its native decode/round trip passed and a byte-level
check confines base-color edits to those servant bands. That revised fixture's
appearance has not been separately confirmed. These test palettes and traces
remain local and are not bundled into source or the PR.

## Fukua shared throw validation (2026-11-26)

The two-rule test palette preserves every captured base color and borrows existing
ramps: normal command throw gives blue hair, armored command throw pink-red hair,
and idle retains the original light hair. The user confirmed the requested whiff /
successful-grab colors, shared finish, restoration, and return to character select
all remained correct with no crash.

The read-only trace contains 115 observations on one stable actor / registered
entity identity. It includes distinct normal-throw state 152 and armored-throw
state 155, followed by shared success 156 and recovery 157. It stopped on match
change. This supplies a second character's gameplay evidence for preserving move
origin through shared hit-confirm phases. No newer crash dump appeared. The
fixture, trace and ownership identifiers remain excluded from source.

## Big Band mirror validation and manual loading (2026-11-26)

The user confirmed Brass Knuckles, Horncrush and the level-five super changed
colors during their moves, with neither Big Band changing the other fighter.
The two manually loaded fixtures used different palette numbers and different
rules. A read-only identity snapshot showed that the actors shared a physical
palette object, so physical asset identity alone must not enroll another actor.

Changing player two to player one's palette number did not activate player one's
manually loaded transformation. Explicitly loading the same fixture on both
players made both work as expected. The user accepted and explicitly requested
this per-player opt-in behavior: these were manual loads after entering Training,
not autoload palettes. Matching palette numbers must not implicitly copy or
activate another player's setup. Existing autoload behavior is a separate path.

A service regression now checks that an unregistered actor using the same
physical palette and number produces no gate writes and remains unavailable;
an explicit second binding then activates that actor's own move context. The
Win32 Release service suite passes. This adds no product behavior or DLL change.
Big Band tag/assist and Training-exit behavior have not been separately confirmed.

## Black Dahlia and Beowulf gameplay confirmation

The user confirmed that Black Dahlia Counter and Teleport and Beowulf Grab
Mode Frog Splash and Three Wolf Moonsault work as expected. This confirmation
does not cover Beowulf Tackle or separately establish interruption, assist,
or Training-exit behavior for these cases.
