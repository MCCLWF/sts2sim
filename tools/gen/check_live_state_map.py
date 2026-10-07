#!/usr/bin/env python3
"""Consistency of data/live_state_map.json (source: live_state_map_src.py).

  * the committed JSON is what the source generates;
  * every mapped field exists in data/live_field_inventory.json, the list of
    instance fields the game's model classes actually declare;
  * extra slots are inside the port's slot arrays and no two fields of one
    model write the same slot;
  * every field of every vanilla (non-mock) model is classified, except the
    models listed in UNPORTED below, whose capture is refused by name.

It does not check that a slot is the right one: that is what
tests/live/live_capture_diff.py does against real combats.

Usage: tools/gen/check_live_state_map.py [--coverage]
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import live_state_map_src  # noqa: E402

# Map category -> (inventory base class, content.json kind, slots in the port's `extra`).
CATEGORIES = {
    "relics": ("RelicModel", "RelicModel", 4),
    "monsters": ("MonsterModel", "MonsterModel", 4),
    "powers": ("PowerModel", "PowerModel", 4),
    "cards": ("CardModel", "CardModel", 5),
    "potions": ("PotionModel", "PotionModel", 2),
    "orbs": ("OrbModel", "OrbModel", 0),
    "encounters": ("EncounterModel", "EncounterModel", 0),
    "enchantments": ("EnchantmentModel", "EnchantmentModel", 2),
    "afflictions": ("AfflictionModel", "AfflictionModel", 0),
}

# Models with private state that single-player combat cannot contain. Their
# fields stay unclassified, so a capture that meets one names it and refuses.
UNPORTED = {
    "cards": {"THE_BALL": "MultiplayerOnly card; the port has no behavior for it"},
    "powers": {
        "BEACON_OF_HOPE_POWER": "applied by a MultiplayerOnly card; not ported",
        "GRAVITY_POWER": "applied by a MultiplayerOnly card; not ported",
        "INTERCEPT_POWER": "applied by a MultiplayerOnly card; not ported",
    },
}


def slots(encoding):
    if "extra" not in encoding:
        return []
    width = 4 if encoding.get("decimal_bits") else 1
    used = list(range(encoding["extra"], encoding["extra"] + width))
    if "has" in encoding:
        used.append(encoding["has"])
    return used


def main():
    errors = []
    path = os.path.join(ROOT, "data", "live_state_map.json")
    if open(path).read() != live_state_map_src.render():
        errors.append("data/live_state_map.json is stale: run tools/gen/live_state_map_src.py")
    state_map = json.load(open(path))
    inventory = json.load(open(os.path.join(ROOT, "data", "live_field_inventory.json")))["models"]
    content = json.load(open(os.path.join(ROOT, "data", "content.json")))
    total = classified = 0
    for category, (base, kind, capacity) in CATEGORIES.items():
        models = {m["type"]: m for m in content["models"] if m["kind"] == kind and not m["props"].get("IsMock", False)}
        entries = {m["id"].split(".", 1)[1]: m["type"] for m in models.values()}
        mapped = state_map.get(category, {})
        for entry in mapped:
            if entry not in entries:
                errors.append(f"{category}.{entry}: no such vanilla model")
        for entry, type_name in sorted(entries.items()):
            declared = inventory[base].get(type_name, {})
            item = mapped.get(entry, {})
            used = {}
            for section, prefix in (("fields", ""), ("data", "Data.")):
                names = [field["name"] for field in declared.get(section, [])]
                for name in item.get(section, {}):
                    if name not in names:
                        errors.append(f"{category}.{entry}: {prefix}{name} is not a field of {type_name}")
                for name in names:
                    total += 1
                    encoding = item.get(section, {}).get(name)
                    if encoding is None:
                        if entry not in UNPORTED.get(category, {}):
                            errors.append(f"{category}.{entry}: {prefix}{name} is not classified")
                        continue
                    classified += 1
                    for slot in slots(encoding):
                        if not 0 <= slot < capacity:
                            errors.append(f"{category}.{entry}: {prefix}{name} uses slot {slot} outside extra[{capacity}]")
                        elif slot in used:
                            errors.append(f"{category}.{entry}: {prefix}{name} and {used[slot]} both write slot {slot}")
                        used[slot] = prefix + name
            for name, encoding in item.get("vars", {}).items():
                if name not in {v["name"] for v in models[type_name].get("vars", [])}:
                    errors.append(f"{category}.{entry}: variable {name} is not declared by {type_name}")
                for slot in slots(encoding):
                    if not 0 <= slot < capacity or slot in used:
                        errors.append(f"{category}.{entry}: variable {name} slot {slot} is invalid or already written")
                    used[slot] = "var " + name
            for slot in item.get("derived", {}):
                if not 0 <= int(slot) < capacity or int(slot) in used:
                    errors.append(f"{category}.{entry}: derived slot {slot} is invalid or already written")
    for message in errors:
        print("live state map: " + message, file=sys.stderr)
    if "--coverage" in sys.argv or errors:
        print(f"{classified}/{total} declared fields of vanilla models classified; "
              f"{sum(len(v) for v in UNPORTED.values())} models left unclassified on purpose")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
