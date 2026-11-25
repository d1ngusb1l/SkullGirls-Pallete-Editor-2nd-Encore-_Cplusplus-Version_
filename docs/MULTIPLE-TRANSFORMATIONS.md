# Independent transformation rules — implementation and validation

Base: refreshed origin/master `9c029bf` on 2026-11-24. The working tree was clean;
`codex/multiple-transformations` was created from that reviewed merge, preserving
the documentation handoff branch and existing detached worktree.

## Data flow and conflict policy

`PaletteData::Document` owns portable rule values. `.pal` decoding migrates old
single transformations into its collection. Runtime validation checks character,
palette layout and predicates before mutation. `ColorAssignment` owns all static
and conditional assignments for one native table header. `BuildRedirectPlan`
composes the original native records, static links and enabled rules; only the
owner commits the resulting header. Condition graphs are allocated once per rule
and shared among its links. The game evaluates the existing allowlisted animation
predicates; the editor does not poll or set gameplay resources.

Rules have stable IDs, names, enabled flags, links, shade masks and individual
predicates. Source/shade overlaps are rejected even for disabled rules. This
reserves ownership and makes enabling independent of ordering. There are no
priority ties. Disjoint masks on one source are supported. Static links override
only their selected native shades; conditional rules override those baselines
when active. A disabled/normal rule contributes no redirect records. With no editor
assignments, the exact original header is restored.

Native evidence was re-inspected in the existing local Ghidra output
`research/palette-effects/decode.log` (outside the Git repository): the palette
updater evaluates 44-byte records forward at `00487b8f`, caches condition results,
and later advances through them by `0x2c`, overwriting the source band's selected
shade entries. Later active records therefore win on those shades; list order
was not assumed from the old editor implementation. The local shader source's
`GradientColor` samples redirect data once, changes the destination palette row,
then samples colors; it explicitly does not reread redirect data at the destination.
Chains are one-hop original destination ramps, including cycles. These are code
observations for build 26588330, not new visual confirmation in a match.

## Transactions, identity and lifetime

The existing actor/palette/match guards and effect update boundary remain in use.
Header verification or cache-refresh failure restores the previous header and
assignment registries. Reset uses the same rollback path. The portable fake-memory
suite compiles the actual table owner and injects partial header-write, refresh and
reset failures; it does not claim to emulate the game renderer. A changed actor is
rejected before allocation. Match cleanup invalidates staged rule edits, including
when addresses are reused. UI writes are disabled while overlay preparation is busy.

Identical Apply with the same installed header returns without allocation. Edits
allocate replacement tables; old tables and predicates remain retained until
process exit, as before. No safe renderer release epoch has been established.
This change deliberately does not free buffers the renderer might still reference.
Repeated edits can therefore retain additional memory; no polling allocation loop
or resource-trigger provider was added.

## Editor and isolated simulation

Effects and Advanced share a staged collection editor. Select/name a rule, choose
its own trigger and enabled state, add native links and select shades, then Apply
rules. Removing or editing A preserves B. Discard abandons unapplied rule edits.
Group controls constrain source-band entry to that display group; all links in a
rule still share its predicate and mask. A rule spanning other groups says so.
Display groups splitting native bands keep the explicit Advanced-controls link.
Direct Copy/Paste, separate Color Studio/Effects, palette selector and line/shadow
controls retain their existing layout. Rule undo is not introduced.

The isolated viewer snapshots rule definitions at target selection or explicit
Refresh rule snapshot. Each full-mask enabled rule has an independent manual
checkbox. Pose selection never changes those checkboxes or evaluates a live tag.
Simulation borrows destination ramps using the viewer's existing CPU sampler; it
is not game-renderer equivalence. Partial shader masks, native/static redirects,
lighting, overlays and other shader effects remain explicitly unpreviewed.
Original ignores simulation. Simulation never reaches save or native APIs.
PNG captures pixels immediately; GIF/APNG captures copy the local redirect map,
colors, layout and source frames. Changes and match cleanup cannot alter a pending
export. No local game artwork or generated assets are included in Git.

## Verification matrix

| A (ATTACK) | B (JUMPING) | A source | B source |
| --- | --- | --- | --- |
| inactive | inactive | baseline | baseline |
| active | inactive | A destination | baseline |
| inactive | active | baseline | B destination |
| active | active | A destination | B destination |

Offline checks cover this truth table, overlaps, masks, old single-rule modes,
V1/V2 plus two-overlay migration, V3 round trips, invalid versions/triggers/IDs,
editing/disabling/removing A while B remains unchanged, explicit color-only export,
header rollback/restoration and stale identities. Existing legacy-settings, overlay
binding, palette paste and color-ramp suites also run.

Sprite tests cover independent local simulation, Original/Edited isolation,
transparent PNG, GIF/APNG ranges and endpoint timing, cancellation, background
snapshot isolation and real local Cerebella animations. Independent Pillow decoding
checks exports, including a simulation capture after its pane state is cleared.
These offline Cerebella captures do not verify every character/effect in game.

Required remaining live checks: user-controlled neither/attack/jump/air-attack with
two disjoint rules on a fighter whose authored tags are confirmed; enable/remove,
Save/Load/Autoload, both overlays, tagging, mirrors, new matches and display/device
transitions. The user controls all gameplay, menus, tags, restarts and launching.
The DLL must only be installed with the isolated runner closed. See the final
validation record for the exact installed hash and actual live results.

## Validation build record

- `scripts/build.ps1`: standalone editor and 67 palette/schema/legacy/atomic-I/O
  checks passed; the required script also builds/runs NativeRuleRuntimeTests.
- Native suites passed: NativeRuleRuntimeTests, OverlayBindingMatchTests,
  PalettePasteHistoryTests, LegacyConfigMigrationTests, PaletteRampTests,
  SpritePreviewGeometryTests, SpritePreviewFocusTests, SpritePngTests and
  SpriteAnimationTests. The sprite export suites used the existing local bundle.
- All 16 Python timeline preparation tests passed; Python syntax checks passed.
- Independent Pillow decoding passed for GIF/APNG fixtures and Cerebella Crouch,
  Stand and s.HP captures, plus a pane-simulation snapshot after state clearing.
- `scripts/build-dll.ps1 -Validation` passed. Networking, Steam lobby tools,
  Steam imports and legacy palette protocol markers all reported False.
- Validation DLL SHA-256:
  `2FC9D278CBB0E266CDF010D3F72C8BFD2D69D16ABA290226CD85365265B24A5E`.
- Installed that DLL into the isolated `.work/lobby-spectator/runtime-game`
  only after verifying no SkullGirls process was running; installation hash
  matched the build. Previous DLL is backed up under ignored
  `build/multiple-transformations-backup/` by its SHA-256.
- Live confirmation is pending user-controlled launching/gameplay. No runtime
  truth-table, tagging/mirror/Autoload or device-transition success is claimed
  for this new build. Existing compiler warnings remain in inherited code.

## User-controlled Beowulf continuation

The installed validation DLL was not replaced while the runner was running.
A read-only native trace captured all four states on Beowulf palette 4: neither
(109 changed-state samples), ATTACK only (43), JUMPING only (24), and both (59).
The editor loaded A `band 2 <- band 1` (pink hair on ATTACK) and B
`band 25 <- band 2` (brown shirt on JUMPING). An air attack visibly showed pink
hair and a brown shirt together, consistent with one-hop destination colors.
All six fighters' base-color arrays were unchanged by loading these redirects.

Through the editor, A was disabled and then removed. B retained its name,
JUMPING trigger, enabled flag, band 25 <- 2 link and full mask. The disabled-A
trace contained B-only records in both inactive and active states; removing A
also retained B. Reload restored both rules. Save palette and auto-load produced
an exactly equivalent V3 document, preserved all previous Autoload rows, and
palette 4 -> 5 -> 4 restored the two rules. The existing palette-5 imported
Autoload entry loaded without acquiring either test rule.

The user confirmed tag-out/return still behaved correctly. A fresh match with
opposing Beowulfs on palettes 4 and 2 restored player 1's two native records
through Autoload; player 2's redirect table remained empty. These observations
cover the tested Beowulf configurations, not every character/effect.

The generated test palettes and traces remain local. The test Autoload assignment
for Beowulf palette 4 references `multiple-beowulf-saved.pal`; its predecessor
settings were backed up in ignored build/. A save attempt using an absolute path
inside the filename field failed before writing; a simple filename succeeded.
This is a file-dialog/path limitation observed here, not a new codec failure.
Device/display transitions and two-overlay combinations have not been retested
in this continuation.


## Four-level Hype validation build

The amount-provider extension is separated from the already tested independent
animation rules. V4 carries resource predicates; only distinct Hype levels may
share a source/shade. Tags remain independent and all other overlaps reject.
The isolated viewer offers a single local Hype level, without reading live values
or saving an accidental override. Other character resources and persistent boosts
remain unsupported.

Build verification: `scripts/build.ps1` passed **108** palette/schema/legacy/atomic
checks and native-owner tests including actual encoded Hype chains, invalid
samples, fake mirror owners and idempotent Apply. The nine relevant native suites,
16 Python tests, independent Pillow GIF/APNG and local Hype snapshot decoding,
Python/PowerShell syntax checks and diff whitespace checks passed. The validation
DLL network inspection again reported all four exclusion checks False.

New validation DLL SHA-256:
`74B97F796E34F0571F9F92F732D597D3E03D9F77F2C7E1AA7E92AEB822D66450`.
Installed with no SkullGirls process running; the previous `2FC9D278...` validation
DLL was backed up by its complete hash. Native appearance tests passed with
user-controlled gameplay: all four colors, V4 save/reload and palette-switch
Autoload, independent player-2 resource appearance, and player-1 tag/assist return. The reversible local test palette uses four mutually exclusive hair
redirects (tan/pink/white/gold at levels 0/1/2/3) and retains the independent
JUMPING shirt rule. No palette colors, resource samples or runtime pointers are
included in Git.

The four-level native trace contained 311 samples, each with exactly one active
Hype redirect matching the completed resource level. Table headers remained
unchanged during the cycle; all loaded fighters retained their base color arrays.
Player 2 independently changed blond -> white at 3 -> blond below 3 while player 1
was unchanged. The user also confirmed gold hair persisted through a tag to Annie,
tag back and assist call at Hype 3. Fresh-match V4 Autoload and device transitions
remain separate checks; the earlier animation-rule fresh-match test passed.
