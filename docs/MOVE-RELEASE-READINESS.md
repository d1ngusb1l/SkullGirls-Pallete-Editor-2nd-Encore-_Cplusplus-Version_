# Move transformations: release readiness

This is a review checklist, not a claim that every authored attack path has been
gameplay-tested. The catalog contains 588 selections across 18 characters: 573
stateless families and 15 families requiring synchronized move-origin tracking.
See ROSTER-MOVE-COVERAGE.md for the mapping scope and limitations, and
MOVE-CONTEXT-INVESTIGATION.md for runtime evidence.

## Behavior to preserve

- Transform for the duration of the selected move, including its owned shared
  phases; restore after the move ends or is interrupted.
- Keep each fighter's move history and manually loaded configuration distinct.
  Matching palette numbers alone do not enroll another player. Explicit loading
  on each player permits both to use the same configuration.
- Preserve old palette decoding, existing autoload, independent transformations,
  static links, isolated viewer/export, and Beowulf Hype levels 0 through 3.
- Introduce no further resource providers and keep palette networking disabled.
- Never replace a DLL while the runner is running.

## Runtime evidence and remaining checks

| Area | Evidence | Still needed |
| --- | --- | --- |
| Filia shared Updo/Fenrir | User confirmed move colors and shared fall/landing | Broader interruptions/assist paths |
| Valentine | Ground/air Scalpels confirmed; ordinary fall excluded; corrected cleanup passed | Separate confirmation of every Alpha Blade/Flat Line branch |
| Fukua | Both command throws, shared finish, restoration and Training exit confirmed | Broader interruption/assist paths |
| Big Band | Three shared families and independent mirrors confirmed; explicit same-configuration loading works | Tag/assist and Training exit |
| Beowulf | Chair Toss, Gigantic Arm, Grab Mode Frog Splash, Three Wolf Moonsault and Hype confirmed | Tackle shared path |
| Black Dahlia | User confirmed Counter and Teleport work as expected | Unobserved conditional branches and lifecycle paths |
| Eliza/Sekhmet | Alternate-form moves, restoration and Training exit confirmed | Unobserved super/spawned-entity paths |
| Remaining roster | Authored catalog and automated validation | Representative gameplay and boundary checks; no blanket individual verification claim |

The Win32 tracker suite now also checks authored Beowulf landing/recovery and
Black Dahlia Counter/Teleport tails, unknown-origin mirrors and idle restoration.
These tests verify tracker decisions for supplied states, not actual game paths.
The service suite checks that a second fighter sharing a physical palette remains
unregistered until explicitly bound, then receives its own move context.

A fresh non-mutating build of the local catalog's route review on 2026-11-26
found authored paths from unique origins to every shared member in 14 of the 15
contextual families. The sole exception remains Dio Scalpels states 216–218:
the authored incoming paths belong to Alpha Blade and Flat Line, while airborne
Scalpels returns to generic fall. Those memberships do not establish Scalpels
ownership and must not seed a transformation. The user's confirmed exclusion of
ordinary post-Scalpels falling is consistent with that boundary. No inferred path
or additional resource probe was introduced. The local audit report is excluded
from source because it contains extracted game metadata.

## Release preparation gates

- Finish the remaining runtime checks above, documenting failures and limits.
- Review the full diff and reconcile documentation with the final implementation.
- Run the required standalone/native and catalog suites against final source.
- Build the final non-validation DLL, verify networking is disabled, and package
  with the tested shader and matching data. Validation DLLs are not release assets.
- Verify package contents, checksums, version and final-source revision; exclude
  game assets, captured palettes, memory traces and other local research outputs.
- Draft release notes covering independent rules, move selection, shared-phase
  tracking, Hype, compatibility and the native predicate cleanup correction.
- Publish source/PR updates and release assets only after explicit authorization.
  An earlier automatic approval review rejected publication; local preparation
  does not remove that requirement.

The installed tested DLL remains SHA-256
`2B5F588E380CF9AB37D289496592AD0B0CF3DFCDA3EE22A93BFBB99643246F52`.
Rebuild final source before packaging; a build-directory DLL alone is not evidence
that it matches the installed candidate or reviewed source.
