#!/usr/bin/env python3
"""Inventory v0.111.0 map content from exported IDs and original C# files.

This is a source-review queue, not a behavior-completion or reachability proof.
It intentionally does not infer completion from a C++ header or IsUsable.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import os
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
KINDS = ("ActModel", "EventModel", "AncientEventModel", "EncounterModel", "MonsterModel")
FOLDERS = {
    "ActModel": "Acts",
    "EventModel": "Events",
    "AncientEventModel": "Events",
    "EncounterModel": "Encounters",
    "MonsterModel": "Monsters",
}
SOURCE_SIGNALS = {
    "event_options": "EventOption(",
    "choice": "CardSelectCmd.",
    "rng": "Rng.",
    "combat": "Combat",
    "rewards": "RewardsCmd.",
    "cards": "CardCmd.",
    "relics": "RelicCmd.",
    "creatures": "CreatureCmd.",
    "player": "PlayerCmd.",
    "profile": "SaveManager.Progress",
    "async": "async Task",
}


def has_cpp_type_definition(header: Path, type_name: str) -> bool:
    """Return whether a header defines type_name, excluding forward declarations."""
    text = header.read_text(encoding="utf-8")
    declaration = re.compile(
        rf"\b(?:struct|class)\s+{re.escape(type_name)}\b[^;{{}}]*\{{",
        re.DOTALL,
    )
    return declaration.search(text) is not None


def find_shared_cpp_header(models_dir: Path, type_name: str) -> Path | None:
    """Find a unique header that contains a concrete definition of type_name."""
    matches = [
        header for header in sorted(models_dir.rglob("*.h"))
        if has_cpp_type_definition(header, type_name)
    ]
    return matches[0] if len(matches) == 1 else None


def build(source_root: Path) -> dict:
    data = json.loads((ROOT / "data/content.json").read_text(encoding="utf-8"))
    modeldb = data["modeldb"]
    models = {m["id"]: m for m in data["models"] if m["kind"] in KINDS and not m.get("mock")}
    acts = [m for m in models.values() if m["kind"] == "ActModel" and m["props"].get("Index", -1) >= 0]
    scopes = defaultdict(set)
    for act in acts:
        name = act["id"]
        scopes[name].add("act")
        for prop in ("AllEvents", "AllAncients", "AllEncounters", "AllMonsters"):
            for mid in act["props"].get(prop, []):
                scopes[mid].add(f"{name}:{prop}")
    for key in ("AllEvents", "AllAncients", "AllSharedEvents", "AllSharedAncients",
                "AllEncounters", "EventEncounters"):
        for mid in modeldb.get(key, []):
            scopes[mid].add(f"modeldb:{key}")
    for model in models.values():
        if model["kind"] in ("EventModel", "AncientEventModel"):
            encounter = model.get("props", {}).get("CanonicalEncounter")
            if encounter:
                scopes[encounter].add(f"{model['id']}:CanonicalEncounter")
        if model["kind"] == "EncounterModel":
            for monster in model.get("props", {}).get("AllPossibleMonsters", []):
                scopes[monster].add(f"{model['id']}:AllPossibleMonsters")

    rows = []
    missing_sources = []
    for mid, model in sorted(models.items(), key=lambda pair: (KINDS.index(pair[1]["kind"]), pair[0])):
        source_rel = model["source"]
        source = source_root / source_rel
        if not source.is_file():
            missing_sources.append(source_rel)
            continue
        source_text = source.read_text(encoding="utf-8")
        models_dir = ROOT / "game" / "Models" / FOLDERS[model["kind"]]
        cpp = models_dir / (Path(source_rel).stem + ".h")
        if not cpp.is_file():
            cpp = find_shared_cpp_header(models_dir, Path(source_rel).stem)
        cpp_text = cpp.read_text(encoding="utf-8") if cpp is not None and cpp.is_file() else ""
        rows.append({
            "id": mid,
            "kind": model["kind"],
            "source": source_rel,
            "source_sha256": hashlib.sha256(source_text.encode("utf-8")).hexdigest(),
            "overrides": model.get("overrides", []),
            "source_signals": sorted(key for key, token in SOURCE_SIGNALS.items() if token in source_text),
            "scope": sorted(scopes[mid]),
            "cpp_header": str(cpp.relative_to(ROOT)) if cpp is not None and cpp.is_file() else None,
            "cpp_has_unported_marker": "SIM_NOT_PORTED" in cpp_text or "throw Unported" in cpp_text,
            "review_state": "pending_manual_source_review",
        })
    if missing_sources:
        raise ValueError(f"Missing original C# source files: {missing_sources}")
    unknown_references = {mid: sorted(tags) for mid, tags in scopes.items() if mid not in models}
    if unknown_references:
        raise ValueError(f"Map inventory references absent model IDs: {unknown_references}")
    return {
        "game_version": data["meta"]["release_info"]["version"],
        "game_commit": data["meta"]["release_info"]["commit"],
        "semantics": "Source review queue; a header, dispatch entry, or IsUsable does not prove behavior.",
        "counts": dict(Counter(row["kind"] for row in rows)),
        "rows": rows,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path,
                        default=Path(os.environ.get(
                            "STS2_SRC", ROOT.parent / "Slay the Spire 2" / "src" / "Core")))
    parser.add_argument("--write", type=Path, help="write the generated JSON inventory")
    parser.add_argument("--check", type=Path, help="fail if an existing JSON inventory is stale")
    args = parser.parse_args()
    result = build(args.source_root)
    output = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.check:
        if not args.check.is_file():
            raise SystemExit(f"missing inventory: {args.check}")
        expected = args.check.read_text(encoding="utf-8")
        if expected != output:
            raise SystemExit(f"stale map source inventory: {args.check}; run with --write")
        return
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
