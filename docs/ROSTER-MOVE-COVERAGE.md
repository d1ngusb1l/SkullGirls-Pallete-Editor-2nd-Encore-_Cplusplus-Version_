# Roster move coverage

Generated from the researched local FOIT catalogue by `scripts/build-roster-moves.py`.
These are authored-data mappings, not blanket gameplay verification. The original
Beowulf Chair Toss and Gigantic Arm passed the user's gameplay checks.

The compiled catalogue contains **570 mapped families across all 18 characters**.
**15 families remain unavailable** because distinct moves enter the same timed
phase. Resolving their move context remains part of the active all-attacks goal.

| Character | Mapped families | Shared-phase cases | Attack-tagged states mapped / pending |
| --- | ---: | ---: | ---: |
| Filia | 25 | 2 | 63 / 17 |
| Cerebella | 38 | 0 | 120 / 0 |
| Peacock | 30 | 0 | 72 / 0 |
| Parasoul | 34 | 0 | 68 / 0 |
| MsFortune | 31 | 0 | 93 / 0 |
| Painwheel | 31 | 0 | 115 / 0 |
| Valentine | 27 | 3 | 93 / 26 |
| Double | 31 | 0 | 108 / 0 |
| Squigly | 36 | 0 | 161 / 0 |
| BigBand | 27 | 3 | 73 / 27 |
| Eliza | 34 | 0 | 121 / 0 |
| Fukua | 29 | 2 | 79 / 7 |
| Beowulf | 35 | 3 | 97 / 44 |
| RoboFortune | 29 | 0 | 85 / 0 |
| Annie | 36 | 0 | 114 / 0 |
| Umbrella | 34 | 0 | 111 / 0 |
| BlackDahlia | 33 | 2 | 108 / 17 |
| Marie | 30 | 0 | 89 / 0 |

Generic Normals/FlyingNormals dispatchers are accounted for separately, not exposed
as individual attacks. Input-cancel destinations are not recursively absorbed into
the current move. Untimed dispatchers do not establish visible shared-phase ownership.

## Remaining shared-phase families

- **Filia**: Trap Jaw, Fenrir.
- **Valentine**: Alpha Blade, Dio Scalpels, Flat Line.
- **BigBand**: Brass Knuckles, Horncrush, Lv5.
- **Fukua**: Armored Cmd Throw, Cmd Throw.
- **Beowulf**: Grab Mode Frog Splash, Tackle, Three Wolf Moonsault.
- **BlackDahlia**: Counter, Teleport.

The detailed edge evidence remains in ignored build/roster-move-review.json.
Authored membership does not establish every conditional gameplay path, assist
substitution, shared stance entry, spawned-entity attack or interruption boundary.
Those cases still require audit and representative user-controlled runtime tests.
No additional character resource provider is introduced.
