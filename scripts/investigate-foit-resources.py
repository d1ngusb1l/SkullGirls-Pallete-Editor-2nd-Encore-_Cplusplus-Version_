"""Read-only expression inventory; script variables are NOT runtime actor offsets.
Usage: python investigate-foit-resources.py GAME_DIRECTORY OUTPUT_JSON [character]
Uses the bounded preview parser without changing or evaluating gameplay branches.
"""
import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import sys


def inventory(game, character="beowulf"):
    if character not in {"beowulf"}:
        raise ValueError("This evidence collector currently allowlists Beowulf only")
    script = Path(__file__).with_name("prepare-dll-preview.py")
    spec = importlib.util.spec_from_file_location("preview_resource_reader", script)
    preview = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(preview)
    folder = Path(game) / "data01"
    foit = preview.first_asset(folder, ["characters-foits.gfs", "characters-foits-retail.gfs"], "characters/" + character + ".foit")
    art = preview.first_asset(folder, ["characters-art.gfs", "characters-art-retail.gfs"], "characters/" + character + ".art")
    sprites, move_offset = preview.frame_table(foit, len(art))
    del art
    observations = []
    reader = preview.read_expression

    def observe(cursor):
        offset = cursor.pos
        frames = []
        frame = sys._getframe(1)
        while frame:
            if frame.f_code.co_name == "read_move_table":
                frames.append(frame.f_locals.get("i"))
            frame = frame.f_back
        expressions = reader(cursor)
        if len(observations) + len(expressions) > 100000:
            raise ValueError("Expression inventory exceeds evidence bounds")
        for expression in expressions:
            observations.append({"expressionBlockOffset": offset, "rootAnimation": frames[-1] if frames else None,
                                 "tableDepth": len(frames), **expression})
        return expressions

    preview.read_expression = observe
    animations, names = preview.read_move_table(preview.Cursor(foit, move_offset), len(foit), len(sprites), True)
    destinations = collections.Counter()
    for observation in observations:
        kind, bits = observation["destination"]
        if kind == 1:
            destinations[f"0x{bits:02x}"] += 1
    return {"character": character, "foitSha256": hashlib.sha256(foit).hexdigest(), "foitBytes": len(foit),
            "parser": "version-9 bounded preview reader; conditional branches not emulated", "moveTableOffset": move_offset,
            "rootAnimations": len(animations), "spriteRecords": len(sprites), "expressionCount": len(observations),
            "namedCandidates": [{"id": i, "name": name} for i, name in enumerate(names)
                                if any(word in name.lower() for word in ("hype", "crowd", "taunt"))],
            "scriptVariableDestinations": dict(destinations.most_common()), "observations": observations,
            "runtimeResourceSupport": "Unsupported: actor ownership, field type, units and persistent mode semantics unverified"}


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    report = inventory(sys.argv[1], sys.argv[3] if len(sys.argv) == 4 else "beowulf")
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "observations"}, indent=2))
