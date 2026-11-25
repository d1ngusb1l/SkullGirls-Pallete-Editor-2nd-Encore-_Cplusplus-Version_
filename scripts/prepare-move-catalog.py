"""Build local whole-move authoring metadata without reading a running process.

Usage: python scripts/prepare-move-catalog.py GAME_DIRECTORY OUTPUT_JSON [MOVE_MAP]
MOVE_MAP is an explicit, build-bound grouping of authored state names. Output is
metadata only, not sprites, resource providers, or an enabled runtime predicate.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile

spec = importlib.util.spec_from_file_location("move_preview_reader", Path(__file__).with_name("prepare-dll-preview.py"))
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)

GENERAL_TAGS = ("ATTACK", "NORMAL", "SPECIAL", "SUPER", "THROW", "TAUNT", "ASSIST",
                "IDLE", "HITSTUN", "BLOCKSTUN", "JUMPING")


def tag_hash(name):
    value = 5381
    for byte in name.encode("ascii"):
        value = ((value * 33) ^ byte) & 0xffffffff
    return value


def state_catalog(names, metadata):
    if not names or len(names) != len(metadata) or len(names) > 65535:
        raise ValueError("Animation metadata count does not match names")
    count = len(names)
    states = []
    for index, (name, record) in enumerate(zip(names, metadata)):
        if record["id"] != index or not isinstance(name, str) or not name:
            raise ValueError("Invalid authored animation identity")
        targets = [record["successor"], *record["conditionalTargets"]]
        if any(type(target) is not int or not 0 <= target < count for target in targets):
            raise ValueError("Invalid authored animation target")
        states.append({**record, "name": name,
                       "generalTags": [tag for tag in GENERAL_TAGS if tag_hash(tag) in record["tags"]]})
    # Retain every state, including dispatchers with no previewable sprite.
    return states


def membership_ranges(state_ids):
    """Exact union plan: never fill gaps with unrelated animation states."""
    if not state_ids or any(type(value) is not int or not 0 <= value <= 65534 for value in state_ids):
        raise ValueError("Invalid animation membership")
    result = []
    for value in sorted(set(state_ids)):
        if result and value == result[-1][1] + 1:
            result[-1][1] = value
        else:
            result.append([value, value])
    return result


def resolve_moves(character, mapping):
    """Resolve explicit membership; never infer it from graph reachability."""
    if not isinstance(mapping, dict) or set(mapping) != {"character", "foitSha256", "moves"}:
        raise ValueError("Move map must contain character, foitSha256 and moves")
    if mapping["character"] != character["character"] or mapping["foitSha256"] != character["foitSha256"]:
        raise ValueError("Move map character or FOIT digest does not match")
    if not isinstance(mapping["moves"], list) or len(mapping["moves"]) > 1024:
        raise ValueError("Invalid move list")
    by_name = {}
    for state in character["states"]:
        by_name.setdefault(state["name"], []).append(state["id"])
    moves, identifiers, owners = [], set(), {}
    for move in mapping["moves"]:
        if not isinstance(move, dict) or set(move) != {"id", "label", "states"}:
            raise ValueError("Each move needs id, label and explicit states")
        identifier, label, names = move["id"], move["label"], move["states"]
        if not isinstance(identifier, str) or not identifier or len(identifier) > 96 or identifier in identifiers:
            raise ValueError("Invalid or duplicate move ID")
        if not isinstance(label, str) or not label or len(label) > 128:
            raise ValueError("Invalid move label")
        if not isinstance(names, list) or not 1 <= len(names) <= 256 or any(not isinstance(name, str) for name in names) or len(set(names)) != len(names):
            raise ValueError("Invalid, duplicate or oversized move membership")
        ids = []
        for name in names:
            candidates = by_name.get(name, [])
            if len(candidates) != 1:
                raise ValueError("Missing or ambiguous authored state: " + name)
            state_id = candidates[0]
            if state_id in owners:
                raise ValueError("Shared state needs explicit move-family review: " + name)
            owners[state_id] = identifier
            ids.append(state_id)
        identifiers.add(identifier)
        members = set(ids)
        boundaries = []
        for state_id in sorted(ids):
            state = character["states"][state_id]
            for kind, targets in (("successor", [state["successor"]]), ("conditional", state["conditionalTargets"])):
                for target in targets:
                    if target not in members:
                        boundaries.append({"source": state_id, "target": target, "kind": kind,
                                           "targetName": character["states"][target]["name"]})
        moves.append({"id": identifier, "label": label, "stateIds": sorted(ids),
                      "membershipRanges": membership_ranges(ids),
                      "states": names, "outgoingBoundaries": boundaries,
                      "validation": "Mapped; gameplay duration and interruption not verified"})
    return moves


def prepare(game, move_map=None):
    folder = Path(game) / "data01"
    characters = []
    for label, asset_name in preview.PLAYABLE_CHARACTERS:
        foit = preview.first_asset(folder, ["characters-foits.gfs", "characters-foits-retail.gfs"], "characters/" + asset_name + ".foit")
        art = preview.first_asset(folder, ["characters-art.gfs", "characters-art-retail.gfs"], "characters/" + asset_name + ".art")
        sprites, offset = preview.frame_table(foit, len(art))
        del art
        metadata = []
        animations, names = preview.read_move_table(preview.Cursor(foit, offset), len(foit), len(sprites), True, move_metadata=metadata)
        del animations
        character = {"character": label, "foitSha256": hashlib.sha256(foit).hexdigest(),
                     "states": state_catalog(names, metadata), "moves": []}
        characters.append(character)
    if move_map is not None:
        if not isinstance(move_map, dict) or set(move_map) != {"version", "characters"} or type(move_map["version"]) is not int or move_map["version"] != 1 or not isinstance(move_map["characters"], list):
            raise ValueError("Unsupported whole-move map")
        seen = set()
        for mapping in move_map["characters"]:
            if not isinstance(mapping, dict):
                raise ValueError("Invalid character mapping")
            label = mapping.get("character")
            if label in seen:
                raise ValueError("Duplicate character mapping")
            seen.add(label)
            matches = [character for character in characters if character["character"] == label]
            if len(matches) != 1:
                raise ValueError("Unsupported character mapping")
            matches[0]["moves"] = resolve_moves(matches[0], mapping)
    return {"format": "skullgirls-move-catalog", "version": 1,
            "runtimeSupport": "Not enabled; authoring metadata only",
            "limitations": ["Authored edges are not an executed gameplay trace",
                            "Conditional and expression-driven branches are not emulated",
                            "Child entity moves are outside fighter-root membership",
                            "Names, tags and graph reachability do not prove whole-move duration"],
            "characters": characters}


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    mapping = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8")) if len(sys.argv) == 4 else None
    result = prepare(sys.argv[1], mapping)
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    # Finish extraction/validation before replacing an existing catalogue.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=output.parent, prefix=output.name + ".", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(json.dumps(result, indent=2) + "\n")
        temporary.replace(output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    print(json.dumps({"characters": len(result["characters"]),
                      "states": sum(len(c["states"]) for c in result["characters"]),
                      "mappedMoves": sum(len(c["moves"]) for c in result["characters"])}))
