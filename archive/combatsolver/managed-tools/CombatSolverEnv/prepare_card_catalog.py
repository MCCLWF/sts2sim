"""Freeze the pinned Env card IDs and variable baselines for the live mapper."""
import json
import re
import sys
from pathlib import Path


def generate(repo, output):
    content = json.loads((repo / "data/content.json").read_text())
    ids = (repo / "content/generated/ids.h").read_text()
    body = re.search(r"enum class CardId[^\{]*\{(.*?)\};", ids, re.S).group(1)
    entries = re.findall(r"^\s*(\w+)\s*(?:=\s*\d+)?\s*,?\s*$", body, re.M)
    entries.remove("Count")
    models = {m["id"].split(".", 1)[1]: m for m in content["models"] if m["kind"] == "CardModel"}
    entries = ["NULL" if entry == "NULL_" else entry for entry in entries]
    expected = {entry for entry, model in models.items() if not model["props"].get("IsMock", False)}
    if entries[0] != "NONE" or set(entries[1:]) != expected:
        raise ValueError("generated CardId/content mismatch")
    cards = {}
    for index, entry in enumerate(entries[1:], 1):
        model = models[entry]
        variables = {v["name"]: v["base"] for v in model.get("vars", [])}
        levels = [list(variables.values())]
        for upgrade in model.get("upgrades", []):
            if "error" in upgrade:
                raise ValueError(f"{entry}: failed upgrade export")
            for var in upgrade.get("vars", []):
                if var["name"] not in variables:
                    raise ValueError(f"{entry}: upgrade adds variable")
                variables[var["name"]] = var["base"]
            levels.append(list(variables.values()))
        cards[entry] = {
            "id": index, "type": "MegaCrit.Sts2.Core.Models." + model["type"], "variables": list(variables), "levels": levels,
            "variable_metadata": model.get("vars", []),
            "tags": model["props"].get("Tags", []),
            # Instance subclass state requires an explicit mapper, even when zero.
            "subclass_fields": [name for name in model["fields"] if not name.startswith("CardModel.")],
        }
    monster_body = re.search(r"enum class MonsterId[^\{]*\{(.*?)\};", ids, re.S).group(1)
    monster_entries = re.findall(r"^\s*(\w+)\s*(?:=\s*\d+)?\s*,?\s*$", monster_body, re.M)
    monster_entries.remove("Count")
    monster_models = {m["id"].split(".", 1)[1]: m for m in content["models"] if m["kind"] == "MonsterModel"}
    monster_expected = {entry for entry, model in monster_models.items() if not model["props"].get("IsMock", False)}
    if monster_entries[0] != "NONE" or set(monster_entries[1:]) != monster_expected:
        raise ValueError("generated MonsterId/content mismatch")
    monsters = {entry: {"id": index, "type": "MegaCrit.Sts2.Core.Models." + monster_models[entry]["type"],
                        "fields": [name for name in monster_models[entry]["fields"] if not name.startswith("MonsterModel.")],
                        "variables": monster_models[entry].get("vars", [])}
                for index, entry in enumerate(monster_entries[1:], 1)}
    document = {"model_id_hash": content["meta"]["model_id_hash"],
                "main_assembly_hash": content["meta"]["release_info"]["main_assembly_hash"], "cards": cards, "monsters": monsters}
    for category, enum, kinds in (
        ("characters", "CharacterId", ("CharacterModel",)),
        ("acts", "ActId", ("ActModel",)),
        ("encounters", "EncounterId", ("EncounterModel",)),
        ("events", "EventId", ("EventModel", "AncientEventModel")),
        ("relics", "RelicId", ("RelicModel",)),
        ("powers", "PowerId", ("PowerModel",)),
        ("potions", "PotionId", ("PotionModel",)),
        ("orbs", "OrbId", ("OrbModel",)),
    ):
        enum_body = re.search(r"enum class " + enum + r"[^\{]*\{(.*?)\};", ids, re.S).group(1)
        enum_entries = re.findall(r"^\s*(\w+)\s*(?:=\s*\d+)?\s*,?\s*$", enum_body, re.M)
        enum_entries.remove("Count")
        catalog_models = {m["id"].split(".", 1)[1]: m for m in content["models"] if m["kind"] in kinds}
        # CharacterId retains all character entries, including Deprived; other
        # generated model categories omit mocks. Preserve the reserved ordinal.
        catalog_expected = {entry for entry, model in catalog_models.items()
                            if category == "characters" or not model["props"].get("IsMock", False)}
        if enum_entries[0] != "NONE" or set(enum_entries[1:]) != catalog_expected:
            raise ValueError(f"generated {enum}/content mismatch")
        document[category] = {entry: {"id": index, "type": "MegaCrit.Sts2.Core.Models." + catalog_models[entry]["type"],
                                     "mock": catalog_models[entry]["props"].get("IsMock", False)}
                              for index, entry in enumerate(enum_entries[1:], 1)}
        if category in ("powers", "relics", "potions", "orbs"):
            base_type = {"powers": "PowerModel", "relics": "RelicModel", "potions": "PotionModel", "orbs": "OrbModel"}[category]
            for entry, metadata in document[category].items():
                metadata["fields"] = [name for name in catalog_models[entry]["fields"] if not name.startswith(base_type + ".")]
                metadata["variables"] = catalog_models[entry].get("vars", [])
        if category == "encounters":
            for entry, metadata in document[category].items():
                metadata["slots"] = catalog_models[entry]["props"].get("Slots", [])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    generate(Path(sys.argv[1]), Path(sys.argv[2]))
