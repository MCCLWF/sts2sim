#!/usr/bin/env python3
"""Read-only static Env inventory; these counts do not prove behavior support.

Official model IDs exclude Mock models. Headers mean a concrete C++ class is
present. IsUsable is the generated dispatch whitelist, which does not inspect
conditional Unported throws or establish oracle coverage.
"""

import argparse
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "gen"))
import gen_dispatch as gd

CARD_POOL_IDS = (
    "CARD_POOL.IRONCLAD_CARD_POOL",
    "CARD_POOL.SILENT_CARD_POOL",
    "CARD_POOL.DEFECT_CARD_POOL",
    "CARD_POOL.REGENT_CARD_POOL",
    "CARD_POOL.NECROBINDER_CARD_POOL",
    "CARD_POOL.EVENT_CARD_POOL",
)


def inventory():
    data = json.loads((ROOT / "data" / "content.json").read_text(encoding="utf-8"))
    models = [
        model for model in data["models"]
        if model["kind"] in gd.KINDS
        and not model.get("mock")
        and not model["id"].split(".", 1)[1].startswith("MOCK_")
    ]
    dispatch = (ROOT / "game" / "generated" / "Dispatch.cpp").read_text(encoding="utf-8")
    marker = "bool IsUsable(ModelKind kind, uint16_t id) {"
    if marker not in dispatch:
        raise ValueError("IsUsable definition not found; audit the changed dispatch format")
    dispatch = dispatch.split(marker, 1)[1]
    rows = []
    for kind, (model_kind, enum, folder, _) in gd.KINDS.items():
        selected = [model for model in models if model["kind"] == kind]
        headers = set()
        directory = ROOT / "game" / "Models" / folder
        for path in sorted(directory.rglob("*.h")) if directory.exists() else []:
            body = path.read_text(encoding="utf-8")
            classes = re.findall(r"^(?:struct|class) (\w+)\s*(?:final\s*)?:", body, re.M)
            for cls in classes:
                source = gd.SOURCE_NAMES.get((kind, cls), cls)
                headers.update(
                    model["id"] for model in selected
                    if model["type"].split(".")[-1] == source
                )
        block = re.search(
            r"case ModelKind::" + model_kind + r":(.*?)(?=\n    case ModelKind::|\n    default:)",
            dispatch, re.S,
        )
        if block is None:
            raise ValueError(f"IsUsable section missing for {kind}; audit the changed dispatch format")
        usable = set(re.findall(r"case " + enum + r"::(\w+):", block.group(1)))
        whitelisted = [model for model in selected if gd.id_enum_name(model["id"]) in usable]
        rows.append({
            "kind": kind,
            "official_ids": len(selected),
            "concrete_headers": len(headers),
            "is_usable_whitelist": len(whitelisted),
            "whitelisted_without_header": sorted(model["id"] for model in whitelisted if model["id"] not in headers),
        })
    return rows


def card_pool_inventory():
    """Count official CardModel IDs in canonical AllCardIds and their headers."""
    data = json.loads((ROOT / "data" / "content.json").read_text(encoding="utf-8"))
    models = [
        model for model in data["models"]
        if model["kind"] == "CardModel"
        and not model.get("mock")
        and not model["id"].split(".", 1)[1].startswith("MOCK_")
    ]
    cards_by_id = {model["id"]: model for model in models}
    cards_by_source = {}
    for model in models:
        source = model["type"].split(".")[-1]
        cards_by_source.setdefault(source, set()).add(model["id"])

    directory = ROOT / "game" / "Models" / gd.KINDS["CardModel"][2]
    header_classes = set()
    for path in sorted(directory.rglob("*.h")) if directory.exists() else []:
        body = path.read_text(encoding="utf-8")
        header_classes.update(re.findall(r"^(?:struct|class) (\w+)\s*(?:final\s*)?:", body, re.M))

    pool_models = {
        model["id"]: model for model in data["models"]
        if model["kind"] == "CardPoolModel"
    }
    rows = []
    for pool_id in CARD_POOL_IDS:
        pool = pool_models.get(pool_id)
        if pool is None:
            raise ValueError(f"Card pool {pool_id} missing from content.json")
        all_card_ids = pool.get("props", {}).get("AllCardIds")
        if not isinstance(all_card_ids, list) or not all(isinstance(card_id, str) for card_id in all_card_ids):
            raise ValueError(f"Card pool {pool_id} missing a valid AllCardIds list")
        official_ids = set(all_card_ids)
        missing = sorted(official_ids - cards_by_id.keys())
        if missing:
            raise ValueError(f"{pool_id} contains non-CardModel IDs: {missing}")

        concrete_ids = set()
        for cls in header_classes:
            source = gd.SOURCE_NAMES.get(("CardModel", cls), cls)
            concrete_ids.update(cards_by_source.get(source, set()) & official_ids)
        rows.append({
            "pool": pool_id,
            "official_ids": len(official_ids),
            "concrete_headers": len(concrete_ids),
        })
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="emit the static inventory as JSON")
    args = parser.parse_args()
    rows = inventory()
    card_pools = card_pool_inventory()
    note = "Static inventory only: headers and IsUsable are not full behavior or oracle coverage."
    if args.json:
        print(json.dumps({"note": note, "models": rows, "card_pools": card_pools}, ensure_ascii=False, indent=2))
    else:
        print(note)
        print("kind official_ids concrete_headers is_usable_whitelist")
        for row in rows:
            print(row["kind"], row["official_ids"], row["concrete_headers"], row["is_usable_whitelist"])
        print("card_pool official_ids concrete_headers")
        for row in card_pools:
            print(row["pool"], row["official_ids"], row["concrete_headers"])


if __name__ == "__main__":
    main()
