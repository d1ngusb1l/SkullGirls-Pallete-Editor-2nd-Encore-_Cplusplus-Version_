# Versioned `.pal` format (V1/V2/V3/V4)

V1 retains the existing `.pal` extension and binary prefix. The new reader accepts plain legacy files and files with the optional V1 effect block. Existing readers can read the colors from an extended file, because the added block follows the fields they already read. Saving through an older editor strips the extra block and loses its advanced assignments.

## Legacy prefix

All integers are little-endian. Packed colors are unsigned 32-bit ARGB values; the bytes are unchanged from the original editor.

| Field | Bytes |
| --- | ---: |
| Character identifier, NUL-terminated and padded | 16 |
| Color count, including transparent index zero | 4 |
| Legacy hue increment | 1 |
| Legacy hue interval | 1 |
| Colors 1 through `count - 1` | `4 * (count - 1)` |
| Line color | 4 |
| Super-shadow color 1 | 4 |
| Super-shadow color 2 | 4 |

Color zero is implicitly transparent black. The prefix length is `22 + 4 * (count - 1) + 12`. Legacy import verifies the character, count and complete file contents before writing to the game. A nonzero hue increment with a valid interval is imported as a rainbow assignment across the visible groups. Plain legacy imports preserve existing editor effect assignments that the file cannot represent.

## Optional V1 effect block

Immediately after the legacy prefix:

| Field | Bytes |
| --- | ---: |
| Marker `SGPEFX1` followed by NUL | 8 |
| UTF-8 JSON payload length | 4 |
| UTF-8 JSON payload | Payload length |

The JSON object has `format: "skullgirls-palette"`, `version: 1`, `character`, `groupSizes`, `colors`, `lineColor`, `superShadowColors`, and `effects`. Its colors and legacy hue fields must agree with the prefix. Unsupported versions, unrecognized suffixes, inconsistent prefixes, truncated files and oversized payloads are rejected. Files are capped at 4 MiB and 4,096 colors.

`groupSizes` contains the native color count of every palette band, including transparent band zero. It is checked against the running character before effects are applied; display labels in `groups.json` are not the identifiers.

`effects` contains these four keys, each either `null` or an object:

| Key | Object fields |
| --- | --- |
| `overlay` | `texture`, `bands`, `lighting` (0–2), `tintBand` |
| `rainbow` | `bands`, `speed` (1–255), `interval` (1–255) |
| `colorLinks` | `links` (source/destination pairs), `shadeMask` (1–255) |
| `transformation` | `links`, `shadeMask`, `previewMode` (0 automatic, 1 normal, 2 transformed), `activationTag`, `legacyTrigger` |

A named transformation has an `activationTag` such as `SUPER` and an empty `legacyTrigger`. The legacy Annie/Filia profiles use an empty tag and their named legacy trigger. Native addresses, allocated buffers, condition pointers, texture handles, cache flags and animation timers are never serialized.

Opacity is saved in the actual color alpha values. Saving captures applied effect assignments, not unapplied controls. `null` restores the target palette's native behavior for that feature by removing an editor override. Built-in game rules that the editor has not overridden remain native to the selected palette slot; V1 does not export arbitrary game-owned condition graphs or transfer character animation assets.

## Loading and saving

Manual loading and auto-loading use the same decoder and runtime validator. Character, group layout, effect ranges, triggers and texture availability are checked before mutation. If applying a validated file fails partway through, the editor attempts to restore the previous live colors and effect headers.

Saves use a temporary file beside the destination, flush its contents, and replace the destination with a single filesystem move. A failed write/replacement leaves the previous destination intact. **Export legacy colors only** deliberately omits the V1 block for tools that require an exact legacy file length.

## Compatibility changes

Do not change the legacy prefix. Any incompatible extension change needs a new marker or schema version and explicit migration tests. Run `tests/PaletteDocumentTests.vcxproj` and the opt-in runner `/persistence-smoke` test for changes to this format or its runtime application.


## V3 transformation rule collection

New saves use JSON schema version **3**, retaining the exact legacy prefix and
`SGPEFX1` envelope. The envelope identifies the extension family; the JSON version
identifies its schema. Readers accept V1, V2 and V3; unknown versions fail explicitly.
V2 requires two overlays. V3 supports either zero, one or two overlays, using the
existing `overlay` and optional `secondOverlay` objects and disjoint overlay bands.

V3 replaces `effects.transformation` with `effects.transformations`, an array of
at most 32 rules. A V3 file containing the old single-rule key is rejected; V1/V2
files containing the collection key are rejected to prevent a lossy downgrade.
Every rule has:

| Field | Meaning |
| --- | --- |
| `id` | Unique stable ASCII identifier, 1–64 bytes (`A-Z`, `a-z`, digits, `_`, `-`). |
| `name` | User name, 1–96 bytes, no control characters. |
| `enabled` | Boolean. Disabled rules remain configured and reserve their shades. |
| `links` | Nonempty native source/destination band pairs. |
| `shadeMask` | Nonzero 8-bit mask shared by this rule's links. |
| `activationTag`, `legacyTrigger` | Same allowlisted predicate as V1; unknown or ambiguous predicates are rejected. |
| `previewMode` | Compatibility **live** mode: 0 automatic, 1 forced normal, 2 forced transformed. This is not isolated-viewer simulation. |

A V1/V2 transformation migrates to exactly one enabled rule with ID `legacy`,
name `Imported transformation`, and identical links, mask, predicate and live mode.
An absent transformation becomes an empty collection. Existing forced modes remain
visible in the UI with an explicit action to switch to automatic activation.
New UI rules start automatic. Save/Load and Autoload use the same codec and validator.
Original-mod Autoload settings retain their existing JSON assignment/import format;
the referenced palette file uses this versioned codec.

Two rules may share a source only when their shader shade masks are disjoint.
Conflicts are rejected before native allocation or mutation, including disabled
rules. There are no priorities or ties. Native records are retained first; static
links replace their selected source shades only; active rules override the static
or native baseline for their selected shades. Destinations borrow their original
palette ramps in one hop: `A -> B`, `B -> C` does not redirect A recursively to C.
The native precedence evidence and limits are recorded in MULTIPLE-TRANSFORMATIONS.md.

No runtime pointer, condition graph, actor sample, allocation or viewer override is
serialized. `Export legacy colors only` explicitly discards all effect configuration.
Opening a V3 palette in an older editor is unsupported and may strip its extension;
there is no editor-provided silent downgrade.


## V4 Beowulf Hype predicates

A document containing Hype rules is saved as schema **4**. Documents using only
legacy/animation predicates continue to save as V3. V4 preserves the same prefix,
envelope, overlays and collection metadata. It adds this typed object to a rule:

```json
"predicate": {"provider": "beowulf-hype", "level": 2}
```

The provider is allowlisted to Beowulf; `level` is an integer 0–3. It cannot combine
with `activationTag`, `legacyTrigger` or non-automatic `previewMode`. Unknown
providers, additional provider fields/arbitrary offsets, unsupported characters,
out-of-range levels and V1/V2/V3 files carrying predicates are rejected. Resource
rules cannot be exported through legacy transformation presets; use a V4 .pal save.

Completed-level intervals are [0,1), [1,2), [2,3), and exactly 3. Fractional resource
progress stays in the lower level. Negative, nonfinite and above-3 values activate
no level. This follows resource amount, not a persistent effect after spending.

Different automatic Hype levels may overlap the same source/shades because their
native predicates are mutually exclusive. Duplicate levels, animation/resource
mixtures and all other overlapping rules remain rejected, including disabled
rules. Static/native baseline behavior and one-hop destinations are unchanged.

The native provider is restricted to researched build 26588330, with actor-bound
availability validation before Apply. Sampled amounts, entity pointers and local
viewer Hype simulation are never serialized. Viewer simulation uses one local
level selector so two mutually exclusive levels cannot be forced on together.

## V5 character-move predicates

A document containing a mapped move saves as schema **5**; tag-only and Hype-only
documents retain their V3/V4 versions. V1–V4 remain readable. The binary prefix,
effects envelope, overlays, rule metadata and original Autoload settings are unchanged.

```json
"predicate": {"provider": "character-move", "move": "beowulf/chair-toss"}
```

The allowlist now contains 570 authored-data mappings across 18 characters, with
each move restricted to its character and researched game build 26588330. The
original `beowulf/chair-toss` (all strengths, states 191–198) and
`beowulf/organic-arm` (Gigantic Arm, states 234–236) IDs are preserved and passed
user gameplay checks. Roster additions await runtime validation; 15 shared-phase
families remain excluded. Runtime IDs/ranges and per-character table counts come from the compiled catalogue, not the
palette file. Unknown IDs, wrong characters, arbitrary state/offset fields, mixed
predicates, forced modes and downgrade to V1–V4 reject. Legacy transformation
presets cannot carry move predicates; use a .pal save or explicit colors-only export.

Mapped moves may share source/shades only when their compiled state sets are
disjoint. Duplicate/overlapping moves, move/Hype and move/general-tag overlaps
reject, including disabled rules. Every range gets a native animation predicate;
range records compose through the same single table owner. No resource writes,
move forcing or per-frame redirect-table rebuilds are added.

The isolated viewer uses one manual local move selector. It never follows the
displayed pose or reads the live animation, and its choice is never serialized.
