"""Resolve roster move families from an extracted local catalogue.

Usage: python scripts/build-roster-moves.py CATALOG OUTPUT_REPORT [--write]
--write writes the explicit portable map and compiled definitions after validation.
No runtime probing, gameplay emulation, sprite extraction or resource provider.
"""
import collections
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("roster_catalog", Path(__file__).with_name("prepare-move-catalog.py"))
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
ATTACK_TAGS = {"ATTACK", "NORMAL", "SPECIAL", "SUPER", "THROW"}
DISPATCHERS = {"Normals", "FlyingNormals"}
BUTTONS = dict(WP="LP", WK="LK", MP="MP", MK="MK", HP="HP", HK="HK", LP="LP", LK="LK",
               Jab="LP", Short="LK", Strong="MP", Fwd="MK", Fierce="HP", RH="HK")
NORMAL = re.compile(r"^([scjbf]j?\.)([A-Z]\+)?(WP|WK|MP|MK|HP|HK|LP|LK|Jab|Short|Strong|Fwd|Fierce|RH)(?:_|$)")
PHASE = re.compile(r"Recovery|Reversal|Loop|Fall(?=[A-Z_]|$)|Land(?=[A-Z_]|$)|Success|ForReal|Catch|Grabbed|Bounce|StandUp|Retract")


def slug(value):
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", value)
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def bare(name):
    name = re.sub(r"^[a-z]{1,2}\.", "", name)
    return re.sub(r"^Kara_?", "", name)


def title(value):
    return re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", value).replace("_", " ")


def classify(state, families):
    name = state["name"]
    if name in DISPATCHERS or {"HITSTUN", "BLOCKSTUN", "DEAD"}.intersection(state["generalTags"]):
        return None
    # Specific aliases win over parent stances and generic throw prefixes.
    body = bare(name)
    matches = [(len(alias), roots[0]) for roots in families for alias in roots
               if body == alias or body.startswith(alias + "_")]
    if matches:
        longest = max(length for length, _ in matches)
        winners = {root for length, root in matches if length == longest}
        if len(winners) != 1:
            raise ValueError("Ambiguous authored family: " + name)
        root = winners.pop()
        return slug(root), title(root), "family"
    if (body.startswith("Throw") or body.startswith("TagInThrow")) and not re.match(r"Throw(?:Tech|_Stun)", body):
        stance = "air" if name.startswith("j.") else "ground"
        return stance + "-throw", stance.title() + " throw (all directions)", "throw"
    normal = NORMAL.match(re.sub(r"^(?:Reverse_|NewChain_)", "", name))
    if normal:
        posture, direction, button = normal.groups()
        base = posture + (direction or "") + button
        label = posture + (direction or "") + BUTTONS[button]
        return "normal-" + slug(base), label, "normal"
    if body.startswith("SnapOut"):
        return "snap-out", "Snap out", "attack"
    return None


def resolve_character(character, families, untagged=None):
    untagged = untagged or {}
    if any(category not in {"Attack", "Special", "Super", "Throw"} for category in untagged.values()):
        raise ValueError("Invalid reviewed untagged move category")
    states = character["states"]
    if not re.fullmatch(r"[0-9a-f]{64}", character["foitSha256"]):
        raise ValueError("Missing character archive digest")
    states = catalog.state_catalog([state["name"] for state in states], states)
    owners, groups = {}, {}
    for state in states:
        family = classify(state, families)
        if family is None:
            continue
        key, label, kind = family
        # Preserve the two already shipped stable IDs and labels.
        if character["character"] == "Beowulf":
            key, label = {"chair-toss": ("chair-toss", "Chair Toss (all strengths)"),
                          "organic-arm": ("organic-arm", "Gigantic Arm")}.get(key, (key, label))
        group = groups.setdefault(key, dict(id=key, label=label, kind=kind, stateIds=[]))
        group["stateIds"].append(state["id"])
        owners[state["id"]] = key
    if set(untagged) - set(groups):
        raise ValueError("Reviewed untagged family did not resolve")
    # A prefix alone does not turn a non-attack helper into a selectable attack.
    # Reviewed exceptions are explicit: some grab finishers have no attack tags.
    for key in list(groups):
        group = groups[key]
        tags = set().union(*(set(states[i]["generalTags"]) for i in group["stateIds"]))
        if not tags.intersection(ATTACK_TAGS) and group["kind"] != "attack" and key not in untagged:
            for i in group["stateIds"]:
                owners.pop(i)
            del groups[key]
            continue
        group["category"] = ("Normal" if group["kind"] == "normal" else "Throw" if group["kind"] == "throw"
                             else "Super" if "SUPER" in tags else "Special" if "SPECIAL" in tags else "Attack")
        if key in untagged:
            group["category"] = untagged[key]
        group["sharedPhases"] = []
    seen = set()
    for state in states:
        source_owner = owners.get(state["id"])
        if source_owner is None:
            continue
        # Entering a different move's entry is a cancel. Entering its timed helper
        # phase is ambiguous even when an input condition is present (throw hit
        # confirms also have condition steps). Never infer semantics from flags.
        edges = [(state["successor"], "successor")]
        edges += [(edge["target"], "conditional") for edge in state.get("conditionalEdges", [])]
        for target, kind in edges:
            target_owner = owners.get(target)
            if target_owner is None or target_owner == source_owner:
                continue
            phase = states[target]
            if not phase.get("timedTicks", phase["sourceFrames"]) or not PHASE.search(phase["name"]):
                continue
            key = source_owner, target_owner, target
            if key in seen:
                continue
            seen.add(key)
            issue = {"source": state["id"], "sourceName": state["name"], "target": target,
                     "targetName": phase["name"], "kind": kind,
                     "sourceMove": source_owner, "targetMove": target_owner}
            groups[source_owner]["sharedPhases"].append(issue)
            groups[target_owner]["sharedPhases"].append(issue)
    attack_states = [state for state in states if ATTACK_TAGS.intersection(state["generalTags"])]
    for group in groups.values():
        group["stateIds"].sort()
        group["membershipRanges"] = catalog.membership_ranges(group["stateIds"])
        group["states"] = [states[i]["name"] for i in group["stateIds"]]
        group["available"] = not group["sharedPhases"]
    coverage = []
    for state in attack_states:
        owner = owners.get(state["id"])
        status = ("dispatcher" if state["name"] in DISPATCHERS else "unmapped" if owner is None
                  else "shared-phase-review" if not groups[owner]["available"] else "mapped")
        coverage.append({"id": state["id"], "name": state["name"], "status": status, "move": owner})
    return {"character": character["character"], "foitSha256": character["foitSha256"], "animationCount": len(states),
            "moves": sorted(groups.values(), key=lambda move: (move["category"], move["label"])),
            "attackCoverage": coverage, "counts": dict(collections.Counter(state["status"] for state in coverage))}


def build(source, rules):
    if source.get("format") != "skullgirls-move-catalog" or source.get("version") != 1 or rules.get("version") != 1:
        raise ValueError("Unsupported roster catalogue/rules")
    characters = source["characters"]
    names = [c["character"] for c in characters]
    expected = {label for label, _ in catalog.preview.PLAYABLE_CHARACTERS}
    if len(names) != len(set(names)) or set(names) != expected or set(names) != set(rules["families"]):
        raise ValueError("Roster and family rules differ")
    reviewed_untagged = rules.get("reviewedUntaggedFamilies", {})
    if set(reviewed_untagged) - expected:
        raise ValueError("Unknown reviewed untagged character")
    result = [resolve_character(c, rules["families"][c["character"]], reviewed_untagged.get(c["character"])) for c in characters]
    return {"format": "skullgirls-roster-move-review", "version": 1, "characters": result}


def context_routes(report, source):
    """Expand only proven shared helper tails, never the general cancel graph."""
    characters = []
    for reviewed, raw in zip(report["characters"], source["characters"]):
        if not any(m["sharedPhases"] for m in reviewed["moves"]):
            continue
        states = raw["states"]
        moves = {m["id"]: {**m, "stateIds": set(m["stateIds"])} for m in reviewed["moves"]}
        owners = {state: m["id"] for m in reviewed["moves"] for state in m["stateIds"]}
        shared_edges = {(issue["source"],issue["target"]) for move in reviewed["moves"] for issue in move["sharedPhases"]}
        for move in moves.values():
            for issue in move["sharedPhases"]:
                if issue["sourceMove"] != move["id"]:
                    continue
                pending = [issue["target"]];visited = set()
                while pending:
                    target = pending.pop()
                    if target in visited:
                        continue
                    visited.add(target);move["stateIds"].add(target)
                    state = states[target]
                    for following in [state["successor"], *state["conditionalTargets"]]:
                        if owners.get(following) is not None and PHASE.search(states[following]["name"]) and (owners[following]==owners[target] or (target,following) in shared_edges):
                            pending.append(following)
        candidates = [[] for _ in states]
        resolved = []
        for move in moves.values():
            move["stateIds"] = sorted(move["stateIds"])
            move["membershipRanges"] = catalog.membership_ranges(move["stateIds"])
            move["states"] = [states[i]["name"] for i in move["stateIds"]]
            move["contextual"] = bool(move["sharedPhases"])
            for state in move["stateIds"]:
                candidates[state].append(move["id"])
            resolved.append(move)
        for choices in candidates:
            if len(choices)>1 and any(not moves[key]["contextual"] for key in choices):
                raise ValueError("Shared membership escaped context guards")
        edges = set()
        for state in states:
            for target in [state["successor"], *state["conditionalTargets"]]:
                if len(candidates[target])>1:
                    edges.add((state["id"],target))
        characters.append({**reviewed, "moves": resolved, "routeEdges": sorted(edges)})
    return {"format":"skullgirls-context-route-review", "version":1,
            "runtimeSupport":"Requires synchronized DLL observer; gameplay validation pending", "characters":characters}


def write_context_definitions(report):
    definitions = ["// Generated shared-phase definitions; require synchronized DLL context."]
    routes = ["// Generated route tables and authored incoming edges. No gameplay emulation."]
    for c in report["characters"]:
        routes.append("if(character=="+json.dumps(c["character"])+")return MoveRouteTable("+str(c["animationCount"])+",{")
        for move in c["moves"]:
            identifier = c["character"].lower()+"/"+move["id"]
            ranges = "{"+",".join("{%d,%d}" % tuple(r) for r in move["membershipRanges"])+"}"
            routes.append("{"+json.dumps(identifier)+","+ranges+"},")
            if move["contextual"]:
                fields = [identifier,c["character"],move["label"],c["foitSha256"]]
                definitions.append("{"+",".join(json.dumps(value) for value in fields)+","+ranges+","+str(c["animationCount"])+","+json.dumps(move["category"])+",true},")
        routes.append("},{"+",".join("{%d,%d}" % tuple(edge) for edge in c["routeEdges"])+"});")
    routes.append('throw std::runtime_error("No contextual route table for this character.");')
    (ROOT / "PalleteEditor/Data/ContextMoveDefinitions.inl").write_text("\n".join(definitions)+"\n",encoding="utf-8")
    (ROOT / "PalleteEditor/Data/CharacterMoveRoutes.inl").write_text("\n".join(routes)+"\n",encoding="utf-8")


def write_definitions(report, source):
    if any(c["counts"].get("unmapped", 0) for c in report["characters"]):
        raise ValueError("Unaccounted attack states remain; inspect the report before exporting")
    # Portable explicit maps remain a reviewable source alongside the generator.
    portable = {"version": 1, "characters": []}
    lines = ["// Generated by scripts/build-roster-moves.py from local authored metadata.",
             "// No sprite/art payloads. Shared-phase ambiguities are excluded."]
    for c in report["characters"]:
        moves = [m for m in c["moves"] if m["available"]]
        portable["characters"].append({"character": c["character"], "foitSha256": c["foitSha256"],
                                       "moves": [{k: m[k] for k in ("id", "label", "states")} for m in moves]})
        for m in moves:
            identifier = c["character"].lower() + "/" + m["id"]
            text = [identifier, c["character"], m["label"], c["foitSha256"]]
            ranges = "{" + ",".join("{%d,%d}" % tuple(r) for r in m["membershipRanges"]) + "}"
            lines.append("{" + ",".join(json.dumps(value) for value in text) + "," + ranges
                         + "," + str(c["animationCount"]) + "," + json.dumps(m["category"]) + "},")
    # Resolve every exported name again through the strict build-bound map reader.
    for mapping, c in zip(portable["characters"], source["characters"]):
        catalog.resolve_moves(c, mapping)
    (ROOT / "PalleteEditor/data01/move-families.json").write_text(json.dumps(portable, indent=2) + "\n", encoding="utf-8")
    (ROOT / "PalleteEditor/Data/CharacterMoveDefinitions.inl").write_text("\n".join(lines) + "\n", encoding="utf-8")
    available = sum(sum(m["available"] for m in c["moves"]) for c in report["characters"])
    pending = sum(sum(not m["available"] for m in c["moves"]) for c in report["characters"])
    summary = ["# Roster move coverage", "", "Generated from the researched local FOIT catalogue by `scripts/build-roster-moves.py`.",
               "These are authored-data mappings, not blanket gameplay verification. The original",
               "Beowulf Chair Toss and Gigantic Arm passed the user's gameplay checks.", "",
               f"The catalogue contains **{available} stateless families across all 18 characters**.",
               f"A further **{pending} shared-phase families** require synchronized per-fighter route tracking",
               f"in the DLL host, for {available + pending} selectable families. They remain unavailable in the standalone",
               "host. Unknown origins keep transformations inactive. Gameplay validation and broader",
               "attack-path coverage remain part of the active goal.", "",
               "| Character | Mapped families | Shared-phase cases | Attack-tagged states mapped / pending |",
               "| --- | ---: | ---: | ---: |"]
    for c in report["characters"]:
        n = sum(m["available"] for m in c["moves"])
        summary.append(f"| {c['character']} | {n} | {len(c['moves'])-n} | {c['counts'].get('mapped',0)} / {c['counts'].get('shared-phase-review',0)} |")
    summary += ["", "Generic Normals/FlyingNormals dispatchers are accounted for separately, not exposed",
                "as individual attacks. Input-cancel destinations are not recursively absorbed into",
                "the current move. Untimed dispatchers do not establish visible shared-phase ownership.", "",
                "Attack-tag coverage is not a completeness proof: Beowulf's Bulldog, Gringo Killa",
                "and grab-mode snapback have no attack tags. Explicit reviewed exceptions expose them",
                "as separate complete-body selections. The existing Grab Mode ID now covers holding,",
                "movement and release; its former accidental inclusion of these finishers is corrected.", "",
                "## Shared-phase families requiring the DLL host", ""]
    for c in report["characters"]:
        names = [m["label"] for m in c["moves"] if not m["available"]]
        if names:
            summary.append("- **" + c["character"] + "**: " + ", ".join(names) + ".")
    summary += ["", "The detailed edge evidence remains in ignored build/roster-move-review.json.",
                "Authored membership does not establish every conditional gameplay path, assist",
                "substitution, shared stance entry, spawned-entity attack or interruption boundary.",
                "Those cases still require audit and representative user-controlled runtime tests.",
                "No additional character resource provider is introduced.", ""]
    (ROOT / "docs/ROSTER-MOVE-COVERAGE.md").write_text("\n".join(summary), encoding="utf-8")


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4) or (len(sys.argv) == 4 and sys.argv[3] not in ("--write","--write-context")):
        raise SystemExit(__doc__)
    source = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    rules = json.loads((ROOT / "PalleteEditor/data01/roster-move-rules.json").read_text(encoding="utf-8"))
    report = build(source, rules)
    contextual = len(sys.argv)==4 and sys.argv[3]=="--write-context"
    if contextual:
        report = context_routes(report,source)
    Path(sys.argv[2]).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if contextual:
        write_context_definitions(report)
    elif len(sys.argv) == 4:
        write_definitions(report, source)
    for c in report["characters"]:
        print(c["character"], "moves", sum(m["available"] for m in c["moves"]), "/", len(c["moves"]), c["counts"])
