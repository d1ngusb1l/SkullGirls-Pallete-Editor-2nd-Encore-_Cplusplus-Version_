# Beowulf resource evidence — read-only first pass

Status: **Unsupported**. No Hype runtime predicate or polling field is enabled.
No resource value was written, no gameplay script was executed and no offset,
field type, resource range or comparison opcode was guessed.

## Reproducible archive observations (2026-11-24)

The local Steam manifest reports build **26588330**. The isolated runner executable
SHA-256 is `C18918F3592EA0BFBBEE3405E0777A5DDE04CBC190564762F1F88BEDB3428509`.
`scripts/investigate-foit-resources.py GAME_DIRECTORY OUTPUT_JSON` reads Beowulf's
version-9 FOIT from local `data01` archives through the existing bounded GFS reader.
It instruments expression decoding without changing sprite/timing extraction or
evaluating conditional branches. The full raw inventory stays in ignored build/.

| Observation | Result |
| --- | --- |
| Beowulf FOIT SHA-256 | `5302729ad0f055f1c89008c189f69ce2ce167f508ba77e619a889bbe59540411` |
| FOIT bytes | 2,457,242 |
| Move table offset in the file | 456,718 (file position, not actor memory) |
| Root animations / sprite records | 688 / 1,483 |
| Expression nodes across root and child tables | 72,713 |
| Authored names containing taunt/crowd/hype | `s.Taunt_As_Assist` [256], `s.Taunt` [257], `TagOut_Taunt` [585], `NotPoint_ExitScreen_Taunt` [588] |

For each decoded node the inventory records its expression-block file offset,
root animation index, table depth, raw operand kinds/values, opcode and comparison
flag. A chain shares its block's offset; that is not each node's individual offset.
The root `s.Taunt` inventory contains 121 nodes, with assignments to several script
variable identifiers (including `0x85`, `0x9e`, `0x9f`, `0xa8`, `0xa9`). None is
identified here as Hype. Many variables occur in unrelated animations/child actors;
co-occurrence with a taunt is insufficient semantic or ownership evidence.
A name search returned no explicit Hype-named root animation; absence of that name
is not proof that no Hype script/resource exists.

## What is not established

No sampled runtime field, native variable lookup mapping, units, valid range,
threshold, activation event, persistent boost/mode flag, expiry or owner identity
has been validated. Authored variable IDs are not actor offsets. Palette renderer
draw identities are not resource owners. Existing Annie/Filia legacy conditions
and native animation tags establish none of these Beowulf semantics.

The next reverse-engineering step is to trace relevant authored operands through
the game's variable lookup/comparison dispatch and its character-specific storage.
Only then can a read-only sampler be constructed with actor, character, team,
match generation and build checks. A fixed side, shared UI meter or last-drawn
actor is not an acceptable owner. A reliable native comparison would be preferred;
polling would require demonstrated identity, update timing and transactional
appearance changes without per-sample allocations. Neither path is ready.

## Controlled observation protocol for the next pass

After a candidate access path is verified, ask the user to produce baseline,
resource gain, threshold crossing, activation/spending, expiration/cancel,
tag-out/return, fresh match and mirrors on different teams. Capture repeated
traces and counterexamples. Distinguish resource amount and availability from
an activation event and a persistent active boost: spending may lower the meter
while the intended appearance remains active. Ask which observed condition the
user wants only after those meanings are established.

A future minimal typed provider would return Available/Unsupported/Unavailable,
with a verified character/build capability and actor identity. Missing/stale
ownership must fail closed as Unavailable rather than reporting inactive. Resource
comparisons and bounded all/any composition remain design candidates, not schema
or runtime support. Sampling phase, boundaries, reset behavior and any hysteresis
must follow evidence. Isolated manual simulation remains separate from live reads.
Other characters' resources are research candidates requiring the same validation.
