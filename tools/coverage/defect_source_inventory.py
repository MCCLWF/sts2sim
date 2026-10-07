#!/usr/bin/env python3
"""Emit a source-derived, row-per-model/override/branch Defect inventory."""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TABLES_CPP = (ROOT / "content/generated/tables.cpp").read_text(errors="replace")
DEFAULT_SOURCE_CANDIDATES = (
    ROOT.parent / "Slay the Spire 2/src/Core",
    ROOT.parent.parent / "Slay the Spire 2/src/Core",
)
KINDS = {
    "CardModel": "Cards",
    "PowerModel": "Powers",
    "RelicModel": "Relics",
    "PotionModel": "Potions",
    "OrbModel": "Orbs",
}
FIELDS = [
    "row_kind", "scope", "model_type", "model_id", "source_file",
    "source_line", "member_or_condition", "cxx_file", "status", "details",
]


def source_argument(value: str | None) -> Path:
    if value:
        return Path(value).resolve()
    if os.environ.get("STS2_SRC"):
        return Path(os.environ["STS2_SRC"]).resolve()
    for candidate in DEFAULT_SOURCE_CANDIDATES:
        if candidate.is_dir():
            return candidate
    raise SystemExit("Cannot find v0.111.0 src/Core; pass --source or set STS2_SRC")


def extract_pool_types(text: str, method: str, kind: str) -> list[str]:
    start = text.find(method)
    if start < 0:
        raise ValueError(f"missing source method {method}")
    body = text[start:]
    return re.findall(rf"ModelDb\.{kind}<([A-Za-z0-9_]+)>\(\)", body)


def condition_records(path: Path, text: str, scope: str, model_type: str,
                      model_id: str, cxx_file: str, line_offset: int = 0) -> list[dict[str, str]]:
    lines = text.splitlines()
    records: list[dict[str, str]] = []
    method = "(enclosing member inferred from source)"
    method_re = re.compile(
        r"^\s*(?:(?:public|protected|private|internal|static|async|override|virtual|sealed|new|partial|unsafe)\s+)+"
        r"[\w<>,?.\[\] ]+?\s+(\w+)\s*(?:<[^>]+>)?\s*\("
    )
    property_re = re.compile(
        r"^\s*(?:(?:public|protected|private|internal|static|override|virtual|sealed|new)\s+)+"
        r"[\w<>,?.\[\] ]+?\s+(\w+)\s*(?:=>.*)?$"
    )
    pending_property = ""

    def condition_after(line_no: int, opening: int) -> str:
        source = "\n".join(lines[line_no - 1:])
        depth = 0
        in_string = False
        escaped = False
        for index, char in enumerate(source[opening:], opening):
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
                continue
            if char == '"':
                in_string = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    return re.sub(r"\s+", " ", source[opening + 1:index]).strip()
        return re.sub(r"\s+", " ", source[opening + 1:opening + 240]).strip()

    for line_no, line in enumerate(lines, 1):
        found_method = method_re.match(line)
        if found_method:
            method = found_method.group(1)
            pending_property = ""
        elif found_property := property_re.match(line):
            pending_property = found_property.group(1)
            method = pending_property
        elif pending_property and re.match(r"^\s*get\s*$", line):
            method = f"{pending_property}.get"
        # Strip a trailing line comment for branch detection while keeping source text intact.
        clean = re.sub(r"//.*$", "", line)
        for match in re.finditer(r"\b(if|else\s+if|switch|for|foreach|while)\s*\(", clean):
            token = re.sub(r"\s+", " ", match.group(1))
            cond = condition_after(line_no, match.end() - 1)
            label = {"if": "if", "else if": "else-if", "switch": "switch",
                     "for": "loop-for", "foreach": "loop-foreach", "while": "loop-while"}[token]
            records.append({
                "row_kind": "branch" if not label.startswith("loop-") else "iteration",
                "scope": scope, "model_type": model_type, "model_id": model_id,
                "source_file": path.as_posix(), "source_line": str(line_no + line_offset),
                "member_or_condition": f"{method}: {label} ({cond})",
                "cxx_file": cxx_file, "status": "unverified",
                "details": line.strip(),
            })
        if re.search(r"\belse\b", clean) and not re.search(r"\belse\s+if\b", clean):
            records.append({
                "row_kind": "branch", "scope": scope, "model_type": model_type,
                "model_id": model_id, "source_file": path.as_posix(),
                "source_line": str(line_no + line_offset), "member_or_condition": f"{method}: else",
                "cxx_file": cxx_file, "status": "unverified", "details": line.strip(),
            })
        if re.search(r"\bcase\b|\bdefault\s*:", clean):
            records.append({
                "row_kind": "branch", "scope": scope, "model_type": model_type,
                "model_id": model_id, "source_file": path.as_posix(),
                "source_line": str(line_no + line_offset), "member_or_condition": f"{method}: {line.strip()}",
                "cxx_file": cxx_file, "status": "unverified", "details": line.strip(),
            })
        # Exclude null-conditional, null-coalescing and nullable type markers.
        ternary = re.search(r"(?<![?.])\?(?![?.])", clean)
        if ternary and ":" in clean[ternary.end():]:
            records.append({
                "row_kind": "branch", "scope": scope, "model_type": model_type,
                "model_id": model_id, "source_file": path.as_posix(),
                "source_line": str(line_no + line_offset),
                "member_or_condition": f"{method}: ternary {clean.strip()}",
                "cxx_file": cxx_file, "status": "unverified", "details": line.strip(),
            })
    return records


def extract_members(text: str, member_name: str) -> list[tuple[int, str]]:
    """Return source line and body for every concrete C# overload of a member."""
    declaration = re.compile(
        rf"(?m)^\s*(?:(?:public|protected|private|internal|static|async|override|virtual|sealed|new|partial|unsafe)\s+)+"
        rf"[^;{{}}=\n]+?\b{re.escape(member_name)}\s*(?:<[^>]+>)?\s*\([^;{{}}]*\)"
    )
    result: list[tuple[int, str]] = []
    for match in declaration.finditer(text):
        opening = text.find("{", match.end())
        arrow = text.find("=>", match.end())
        semicolon = text.find(";", match.end())
        if opening < 0 or (arrow >= 0 and arrow < opening) or (semicolon >= 0 and semicolon < opening):
            continue
        depth = 0
        in_string = False
        escaped = False
        closing = len(text)
        for index in range(opening, len(text)):
            char = text[index]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
                continue
            if char == '"':
                in_string = True
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    closing = index + 1
                    break
        start = text.rfind("\n", 0, match.start()) + 1
        start_line = text.count("\n", 0, start) + 1
        result.append((start_line, text[start:closing]))
    return result


def override_source_line(text: str, override: str) -> int | None:
    """Find the concrete C# declaration backing an assembly override name."""
    name = override.rsplit(".", 1)[-1]
    candidates = [name]
    if name.startswith("get_"):
        candidates.insert(0, name[4:])
    lines = text.splitlines()
    for line_no, line in enumerate(lines, 1):
        if "override" not in line:
            continue
        if any(re.search(rf"\b{re.escape(candidate)}\b", line) for candidate in candidates):
            return line_no
    return None


def override_status(override: str, record: dict, cxx_text: str) -> tuple[str, str]:
    """Classify override coverage by executable C++, extracted content, or UI scope."""
    member = override.rsplit(".", 1)[-1]
    member = re.sub(r"\s*\(via [^)]+\)$", "", member)
    if member.startswith("get_"):
        prop = member[4:]
        if prop in {
            "ExtraHoverTips", "ExtraRunAssetPaths", "ChannelSfx", "DarkenedColor",
            "EvokeSfx", "PassiveSfx", "Description", "SmartDescriptionLocKey",
            "Title", "Image", "Icon", "Animation", "Vfx", "ShouldGlowGoldInternal",
            "DisplayAmount", "ShowCounter", "IsPositive",
        }:
            return "presentation-only", f"source getter {prop}; presentation assets are outside simulation state"
        if prop == "CanonicalVars" and record.get("vars"):
            return "data-driven", "canonical dynamic variables stored in extracted content and generated tables"
        data_prop = {
            "CanonicalKeywords": "CanonicalKeywords",
            "CanonicalTags": "Tags",
            "EnergyCost": "EnergyCost",
        }.get(prop, prop)
        if data_prop in record.get("props", {}):
            return "data-driven", f"source getter {prop}; value is present in content.props.{data_prop}"
        if re.search(rf"\b{re.escape(prop)}\s*\(", cxx_text):
            return "implemented-code", f"source getter {prop}; C++ model exposes {prop}()"
        if prop in {"HasEnergyCostX", "PassiveVal", "EvokeVal", "OrbEvokeType"} and re.search(
            rf"\b{re.escape(prop)}\s*\(", cxx_text
        ):
            return "implemented-code", f"source getter {prop}; C++ model exposes {prop}()"
    if member == "OnUpgrade":
        card_name = record.get("id", "").split(".")[-1]
        if re.search(rf"kCard_{re.escape(card_name)}_+levels\b", TABLES_CPP):
            return "data-driven", "upgrade behavior is encoded in generated CardLevel/VarDef tables"
    if re.search(rf"\b{re.escape(member)}\b", cxx_text):
        return "implemented-code", f"C++ model header contains {member} implementation/state access"
    if member == "InitInternalData" and ("extra(" in cxx_text or "st().extra" in cxx_text):
        return "implemented-state", "source internal data is stored in snapshotted model extra slots"
    return "unverified", "no C++ method or extracted data mapping found automatically"


def source_model_override_status(name: str, override: str, record: dict) -> tuple[str, str]:
    """Classify character/pool/unlock source overrides used by Defect availability."""
    member = re.sub(r"\s*\(via [^)]+\)$", "", override.rsplit(".", 1)[-1])
    if name in {"Defect2Epoch", "Defect3Epoch", "Defect4Epoch", "Defect5Epoch", "Defect6Epoch", "Defect7Epoch"}:
        return "progression-only", "epoch story/unlock metadata; Env run content uses the fully unlocked pool"
    if name in {"DefectCardPool", "DefectRelicPool", "DefectPotionPool"}:
        if member in {"GenerateAllCards", "GenerateAllRelics", "GenerateAllPotions", "GetUnlockedCards", "GetUnlockedRelics", "GetUnlockedPotions"}:
            return "data-driven", "exact source pool membership is checked against content tables; Env exposes its full pool"
        if member == "FilterThroughEpochs":
            return "full-pool-profile", "epoch gates are omitted for the fully unlocked Env; all pool members remain represented"
        if member.startswith("get_") and member[4:] in {"EnergyColorName", "CardFrameMaterialPath", "DeckEntryCardColor", "EnergyOutlineColor", "IsColorless", "Title", "LabOutlineColor"}:
            return "presentation-only", "card/potion/relic pool display metadata"
    if name == "Defect":
        prop = member[4:] if member.startswith("get_") else member
        if prop in {"GetArchitectAttackVfx", "AnimationStates", "AttackAnimDelay", "CastAnimDelay",
                    "CharacterTransitionSfx", "DialogueColor", "EnergyLabelOutlineColor", "Gender",
                    "MapDrawingColor", "NameColor", "PowerUpAnimDelay", "RemoteTargetingLineColor",
                    "RemoteTargetingLineOutline", "SpeechBubbleColor"}:
            return "presentation-only", f"character presentation member {prop}"
        if prop == "UnlocksAfterRunAs":
            return "progression-only", "character progression metadata; Env starts directly from a configured run"
        if prop in record.get("props", {}):
            return "data-driven", f"character setup value is present in content.props.{prop}"
    return "unverified", "no pool/character/unlock representation found automatically"


def condition_status(name: str, row: dict[str, str], cxx_text: str,
                     record: dict | None = None) -> tuple[str, str]:
    """Link each source condition/loop to its C++ path or classify its scope."""
    scope = row["scope"]
    member = row["member_or_condition"].split(":", 1)[0]
    if scope == "DEFECT_POOL_AND_UNLOCK_SOURCE":
        if name.endswith("Epoch"):
            return "progression-only", "epoch unlock/story branch; the simulator uses the fully unlocked content tables"
        return "full-pool-profile", "all source pool members are represented in the generated full-profile content tables"
    if scope == "SOURCE_INFRASTRUCTURE" and name.startswith("Defect") and name.endswith("Epoch"):
        return "progression-only", "epoch unlock/story branch; the simulator uses the fully unlocked content tables"
    if scope == "SOURCE_INFRASTRUCTURE" and name in {"DefectCardPool", "DefectRelicPool", "DefectPotionPool"}:
        if member in {"FilterThroughEpochs", "GetUnlockedRelics", "GetUnlockedPotions"}:
            return "full-pool-profile", "epoch-gated members are present in Env's fully unlocked content tables"
    if scope == "SOURCE_INFRASTRUCTURE" and name == "OrbModel":
        if member in {"SmartDescription.get", "HoverTips.get", "PlayPassiveSfx", "PlayEvokeSfx", "PlayChannelSfx"}:
            return "presentation-only", "orb description, hover-tip, and sound branches only drive presentation"
        if member == "CanonicalInstance.get":
            return "model-lifecycle-only", "source singleton/canonical instance handling; Env stores only combat orb state and model handles"
        if member == "Owner.get":
            return "state-invariant", "Env assigns the orb owner once when channeling and does not expose an owner re-assignment path"
    if scope == "SOURCE_INFRASTRUCTURE" and name == "OrbQueue":
        if member == "TryEnqueue":
            return "implemented-code", "OrbCmd::Channel handles zero capacity, evoking on a full queue, and enqueueing in order"
        if member == "Insert":
            return "unreferenced-source-helper", "v0.111.0 source has no call sites for OrbQueue.Insert; Defect content cannot reach these bounds branches"
        if member == "SmallWait":
            return "presentation-only", "local-context wait branches schedule multiplayer animation timing only"
        if member == "RemoveCapacity":
            return "implemented-code", "OrbCmd::RemoveSlots trims excess slots from the queue tail while size exceeds capacity"
    visual = {
        "ShouldGlowGoldInternal", "ShouldGlowGoldInternal.get", "ShowCounter", "ShowCounter.get",
        "DisplayAmount", "DisplayAmount.get", "Vfx", "Vfx.get", "ExtraHoverTips", "get_ExtraHoverTips",
        "ChannelSfx", "EvokeSfx", "PassiveSfx", "DarkenedColor",
    }
    if member in visual or any(token in member for token in ("Vfx.get", "Sfx.get", "Color.get")):
        return "presentation-only", "source branch only changes UI glow/counter/effect assets"
    if member == "OnUpgrade" and record:
        status, detail = override_status("CardModel.OnUpgrade", record, cxx_text)
        if status == "data-driven":
            return status, detail
    if member == "CanonicalVars" and "CalculatedHits" in cxx_text:
        return "implemented-code", "source calculated-variable branch is implemented by HelixDrill.CalculatedHits"
    if member == "Filter" and "OnPlay" in cxx_text:
        return "implemented-code", "source LINQ filter condition is implemented by the corresponding C++ OnPlay filter"
    if member == "ApplyLightningDamage" and ("Passive(" in cxx_text or "Evoke(" in cxx_text):
        return "implemented-code", "source helper branches are implemented in C++ LightningOrb passive/evoke target selection"
    if member in {"Passive", "Evoke"} and name == "FrostOrb":
        return "single-player-specialized", "source iterates every player; Env has one player and applies the same orb effect to its owner"
    if member in {"BeforeTurnEnd", "AfterTurnStart"} and name == "OrbQueue":
        return "implemented-code", "source queue iteration is implemented by OrbCmd over snapshotted PlayerCombatData.orbs"
    if re.search(rf"\b{re.escape(member)}\b", cxx_text):
        return "implemented-code", f"matching C++ model/helper member {member} contains the source branch or iteration path"
    if member.startswith("get_") and member[4:] in cxx_text:
        return "implemented-code", f"C++ model/data exposes source property {member[4:]}"
    return "unverified", f"could not map source member {member} to a C++ behavior path"


def cxx_target_text(target: str) -> str:
    """Read C++ files named in an inventory target, including shared state targets."""
    paths: set[Path] = set()
    for candidate in re.findall(r"(?:content|game)/[A-Za-z0-9_./*]+", target):
        candidate = candidate.rstrip(".,")
        if candidate.endswith(".*"):
            stem = candidate[:-2]
            for suffix in (".cpp", ".h", ".def"):
                path = ROOT / f"{stem}{suffix}"
                if path.is_file():
                    paths.add(path)
        else:
            path = ROOT / candidate
            if path.is_file():
                paths.add(path)
    if "PlayerCombatData::orbs" in target:
        paths.add(ROOT / "engine/sim/state.h")
        paths.add(ROOT / "game/Commands/OrbCmd.cpp")
    return "\n".join(path.read_text(errors="replace") for path in sorted(paths))


def engine_member_status(member: str, target: str, text: str) -> tuple[str, str]:
    aliases = {
        "GetDistinctForCombat": ("GetForCombat",),
        "FilterForCombat": ("GetForCombat",),
        "OrbQueue.BeforeTurnEnd": ("BeforeTurnEnd",),
        "OrbQueue.AfterTurnStart": ("AfterTurnStart",),
        "GetRandomOrb": ("next_item_index",),
    }
    candidates = aliases.get(member, (member,))
    if any(re.search(rf"\b{re.escape(candidate)}\s*\(", text) for candidate in candidates):
        return "implemented-code", f"C++ target {target} contains {'/'.join(candidates)}"
    if "PlayerCombatData::orbs" in target and "orbs" in text:
        return "implemented-state", "source queue is represented by PlayerCombatData.orbs and OrbCmd"
    return "unverified", f"C++ target {target} has no matching helper"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", help="v0.111.0 decompiled src/Core directory")
    parser.add_argument("--output", default="docs/audits/defect-v0110-source-inventory.csv")
    args = parser.parse_args()
    source = source_argument(args.source)
    content = json.loads((ROOT / "data/content.json").read_text())
    all_models_text = (ROOT / "game/generated/AllModels.h").read_text(errors="replace")
    dispatch_text = (ROOT / "game/generated/Dispatch.cpp").read_text(errors="replace")
    by_type = {m["type"].split(".")[-1]: m for m in content["models"]
               if m.get("kind") in KINDS}
    all_by_type = {m["type"].split(".")[-1]: m for m in content["models"]}

    card_pool_source = (source / "Models/CardPools/DefectCardPool.cs").read_text()
    relic_pool_source = (source / "Models/RelicPools/DefectRelicPool.cs").read_text()
    potion_pool_source = (source / "Models/PotionPools/DefectPotionPool.cs").read_text()
    epoch4_source = (source / "Timeline/Epochs/Defect4Epoch.cs").read_text()
    card_types = extract_pool_types(card_pool_source, "GenerateAllCards", "Card")
    relic_types = extract_pool_types(relic_pool_source, "GenerateAllRelics", "Relic")
    potion_types = extract_pool_types(epoch4_source, "Potions", "Potion")
    expected = {
        "CARD_POOL.DEFECT_CARD_POOL": {
            m["type"].split(".")[-1] for m in content["models"]
            if m.get("props", {}).get("Pool") == "CARD_POOL.DEFECT_CARD_POOL"
        },
        "RELIC_POOL.DEFECT_RELIC_POOL": {
            m["type"].split(".")[-1] for m in content["models"]
            if m.get("props", {}).get("Pool") == "RELIC_POOL.DEFECT_RELIC_POOL"
        },
        "POTION_POOL.DEFECT_POTION_POOL": {
            m["type"].split(".")[-1] for m in content["models"]
            if m.get("props", {}).get("Pool") == "POTION_POOL.DEFECT_POTION_POOL"
        },
    }
    actual = {"CARD_POOL.DEFECT_CARD_POOL": set(card_types),
              "RELIC_POOL.DEFECT_RELIC_POOL": set(relic_types),
              "POTION_POOL.DEFECT_POTION_POOL": set(potion_types)}
    for pool, names in actual.items():
        if names != expected[pool]:
            raise SystemExit(f"{pool} source/data mismatch: source-only={sorted(names-expected[pool])}; data-only={sorted(expected[pool]-names)}")

    roles: dict[str, set[str]] = {}
    for name in card_types:
        roles.setdefault(name, set()).add("DEFECT_CARD_POOL")
    for name in relic_types:
        roles.setdefault(name, set()).add("DEFECT_RELIC_POOL")
    for name in potion_types:
        roles.setdefault(name, set()).add("DEFECT_POTION_POOL")
    orb_dir = source / "Models/Orbs"
    for path in sorted(orb_dir.glob("*.cs")):
        if path.stem in {"OrbModel"}:
            continue
        roles.setdefault(path.stem, set()).add("DEFECT_ORB")

    # Resolve concrete model dependencies named in the original Defect pool/model sources.
    seeds = set(roles)
    for name in tuple(seeds):
        record = by_type.get(name)
        if not record:
            continue
        path = source / record["source"]
        if not path.is_file():
            continue
        text = path.read_text(errors="replace")
        for dependency, dep_record in by_type.items():
            if dependency == name or not re.search(rf"\b{re.escape(dependency)}\b", text):
                continue
            dep_kind = dep_record.get("kind")
            if dep_kind == "CardModel":
                roles.setdefault(dependency, set()).add("GENERATED_CARD")
            elif dep_kind == "PowerModel":
                roles.setdefault(dependency, set()).add("POWER_DEPENDENCY")
            elif dep_kind == "OrbModel":
                roles.setdefault(dependency, set()).add("DEFECT_ORB")

    # Pool-driven random generation must account for every possible Defect card, not just
    # the card names that happen to appear as a generic type in the source.
    combat_card_candidates = [
        name for name in card_types
        if by_type.get(name, {}).get("props", {}).get("CanBeGeneratedInCombat", False)
        and by_type.get(name, {}).get("props", {}).get("Rarity") not in {"Basic", "Ancient", "Event"}
        and by_type.get(name, {}).get("props", {}).get("MultiplayerConstraint") != "MultiplayerOnly"
    ]
    power_candidates = [name for name in combat_card_candidates
                        if by_type.get(name, {}).get("props", {}).get("Type") == "Power"]
    orb_candidates = sorted(n for n, r in roles.items() if "DEFECT_ORB" in r)
    expected_orb_order = ["LIGHTNING_ORB", "FROST_ORB", "DARK_ORB", "PLASMA_ORB", "GLASS_ORB"]
    for relative in ("game/Models/Cards/Chaos.h", "game/Models/Powers/TrashToTreasurePower.h"):
        text = (ROOT / relative).read_text(errors="replace")
        match = re.search(r"valid\s*\[\]\s*=\s*\{([^}]+)\}", text)
        actual_orb_order = re.findall(r"OrbId::(\w+)", match.group(1)) if match else []
        if actual_orb_order != expected_orb_order:
            raise SystemExit(f"{relative}: random orb candidate order {actual_orb_order} != {expected_orb_order}")
    generated_edges = []
    for generator, candidates, detail in (
        ("Discovery", combat_card_candidates, "3 distinct; GetUnlockedCards -> FilterForPlayerCount -> FilterForCombat; choose 1/skip; generated to hand"),
        ("WhiteNoise", power_candidates, "1 distinct Defect Power after the same combat-generation filters; generated free to hand"),
        ("CreativeAiPower", power_candidates, "turn-start distinct random Defect Power after the same combat-generation filters"),
        ("Chaos", orb_candidates, "random orb from OrbModel.GetRandomOrb's five-model candidate set"),
        ("TrashToTreasurePower", orb_candidates, "one random orb per generated Status, from OrbModel.GetRandomOrb's five-model candidate set"),
    ):
        for candidate in candidates:
            generated_edges.append({
                "row_kind": "generation_edge", "scope": generator,
                "model_type": candidate,
                "model_id": by_type.get(candidate, {}).get("id", ""),
                "source_file": by_type.get(generator, {}).get("source", ""),
                "source_line": "", "member_or_condition": detail,
                "cxx_file": "", "status": "candidate-set-checked",
                "details": "candidate set derived from the v0.111.0 source pool/content metadata and filters; representative RNG/choice traces are in the validation report, not every candidate is asserted to have been drawn",
            })

    rows: list[dict[str, str]] = []
    relevant_names = set(roles)
    for name in sorted(relevant_names):
        record = by_type.get(name)
        if not record:
            raise SystemExit(f"no content/source record for {name}")
        kind = record["kind"]
        cxx_file = f"game/Models/{KINDS[kind]}/{name}.h"
        candidates = list((ROOT / f"game/Models/{KINDS[kind]}").rglob(f"{name}.h"))
        cxx_file = candidates[0].relative_to(ROOT).as_posix() if candidates else cxx_file
        model_path = source / record["source"]
        cxx_text = candidates[0].read_text(errors="replace") if candidates else ""
        registered = bool(candidates and
                          re.search(rf"\bSIM_MODEL\s*\(\s*{re.escape(name)}\b", cxx_text) and
                          candidates[0].relative_to(ROOT).as_posix() in all_models_text and
                          name in dispatch_text)
        status = "registered" if registered else "missing"
        multiplayer_constraint = record.get("props", {}).get("MultiplayerConstraint")
        details = f"header={bool(candidates)}; SIM_MODEL/AllModels/Dispatch registered={registered}; source overrides={len(record.get('overrides', []))}"
        if multiplayer_constraint == "MultiplayerOnly":
            details += "; source MultiplayerConstraint=MultiplayerOnly; C++ behavior is directly exercised by tests/test_potions.cpp, while single-player Oracle generation/play filters exclude it"
        rows.append({
            "row_kind": "model", "scope": ",".join(sorted(roles[name])),
            "model_type": name, "model_id": record.get("id", ""),
            "source_file": record.get("source", ""), "source_line": "",
            "member_or_condition": "", "cxx_file": cxx_file,
            "status": status,
            "details": details,
        })
        for override in record.get("overrides", []):
            source_line = override_source_line(model_path.read_text(errors="replace"), override) if model_path.is_file() else None
            override_state, evidence = override_status(override, record, cxx_text)
            rows.append({
                "row_kind": "override", "scope": ",".join(sorted(roles[name])),
                "model_type": name, "model_id": record.get("id", ""),
                "source_file": record.get("source", ""), "source_line": str(source_line or ""),
                "member_or_condition": override, "cxx_file": cxx_file,
                "status": override_state if registered else "missing",
                "details": evidence,
            })
        if model_path.is_file():
            conditions = condition_records(Path(record["source"]), model_path.read_text(errors="replace"),
                                           ",".join(sorted(roles[name])), name,
                                           record.get("id", ""), cxx_file)
            for condition in conditions:
                condition["status"], condition["details"] = condition_status(name, condition, cxx_text, record)
            rows.extend(conditions)

    # Pool, character and unlock/epoch source models govern availability and
    # selection of the 146 concrete combat models above.
    source_models = ["DefectCardPool", "DefectRelicPool", "DefectPotionPool", "Defect",
                     "Defect2Epoch", "Defect3Epoch", "Defect4Epoch", "Defect5Epoch",
                     "Defect6Epoch", "Defect7Epoch"]
    generic_targets = {
        "DefectCardPool": "content/generated/tables.cpp + game/Factories/CardFactory.*",
        "DefectRelicPool": "content/generated/tables.cpp + game/Factories/RelicFactory.*",
        "DefectPotionPool": "content/generated/tables.cpp + game/Factories/PotionFactory.*",
        "Defect": "content/generated/tables.* (Character data)",
    }
    for name in source_models:
        record = all_by_type.get(name)
        if not record:
            relative = f"Timeline/Epochs/{name}.cs"
            path = source / relative
            if not path.is_file():
                raise SystemExit(f"no source model record or C# file for {name}")
            text = path.read_text(errors="replace")
            overrides = re.findall(
                r"\boverride\s+[\w<>?,.\[\]]+\s+(\w+)\s*(?=\(|=>|\{)", text
            )
            id_match = re.search(r"\bId\s*=>\s*\"([^\"]+)\"", text)
            record = {"kind": "EpochModel", "source": relative,
                      "id": id_match.group(1) if id_match else f"EPOCH.{name.upper()}",
                      "overrides": [f"EpochModel.{member}" for member in overrides]}
        target = generic_targets.get(name, "game/Run/UnlockState.*")
        source_text = (source / record["source"]).read_text(errors="replace")
        if name in {"Defect2Epoch", "Defect3Epoch", "Defect4Epoch", "Defect5Epoch", "Defect6Epoch", "Defect7Epoch"}:
            source_model_status = "progression-only"
            source_model_detail = "epoch unlock/story source; Env runs against the fully unlocked content tables"
        elif name in {"DefectCardPool", "DefectRelicPool", "DefectPotionPool", "Defect"}:
            source_model_status = "data-driven"
            source_model_detail = "source pool/character membership and setup values are represented by generated content tables"
        else:
            source_model_status = "unverified"
            source_model_detail = f"original kind={record.get('kind')}"
        rows.append({
            "row_kind": "source_model", "scope": "DEFECT_POOL_AND_UNLOCK_SOURCE",
            "model_type": name, "model_id": record.get("id", ""),
            "source_file": record.get("source", ""), "source_line": "",
            "member_or_condition": "", "cxx_file": target, "status": source_model_status,
            "details": f"{source_model_detail}; overrides={len(record.get('overrides', []))}",
        })
        for override in record.get("overrides", []):
            source_line = override_source_line(source_text, override)
            override_state, evidence = source_model_override_status(name, override, record)
            rows.append({
                "row_kind": "override", "scope": "DEFECT_POOL_AND_UNLOCK_SOURCE",
                "model_type": name, "model_id": record.get("id", ""),
                "source_file": record.get("source", ""), "source_line": str(source_line or ""),
                "member_or_condition": override, "cxx_file": target,
                "status": override_state, "details": evidence,
            })
        for condition in condition_records(Path(record["source"]), source_text,
                                           "DEFECT_POOL_AND_UNLOCK_SOURCE", name,
                                           record.get("id", ""), target):
            if name.startswith("Defect") and name.endswith("Epoch"):
                condition["status"] = "progression-only"
                condition["details"] = "epoch unlock/story branch; all content is present in the fully unlocked Env"
            elif name in {"DefectCardPool", "DefectRelicPool", "DefectPotionPool"}:
                condition["status"] = "full-pool-profile"
                condition["details"] = "full unlocked pool path; exact source membership is checked against C++ content tables"
            rows.append(condition)

    infrastructure = [
        "Models/CardPools/DefectCardPool.cs", "Models/RelicPools/DefectRelicPool.cs",
        "Models/PotionPools/DefectPotionPool.cs", "Models/Characters/Defect.cs",
        "Timeline/Epochs/Defect2Epoch.cs", "Timeline/Epochs/Defect3Epoch.cs",
        "Timeline/Epochs/Defect4Epoch.cs", "Timeline/Epochs/Defect5Epoch.cs",
        "Timeline/Epochs/Defect6Epoch.cs", "Timeline/Epochs/Defect7Epoch.cs",
        "Entities/Orbs/OrbQueue.cs", "Models/OrbModel.cs", "Commands/OrbCmd.cs",
    ]
    for relative in infrastructure:
        path = source / relative
        if not path.is_file():
            continue
        stem = path.stem
        if relative.startswith("Models/CardPools/"):
            cxx = "content/generated/tables.cpp + game/Factories/CardFactory.*"
        elif relative.startswith("Models/RelicPools/"):
            cxx = "content/generated/tables.cpp + game/Factories/RelicFactory.*"
        elif relative.startswith("Models/PotionPools/"):
            cxx = "content/generated/tables.cpp + game/Factories/PotionFactory.*"
        elif relative.endswith("/OrbCmd.cs"):
            cxx = "game/Commands/OrbCmd.*"
        elif relative.endswith("/OrbModel.cs"):
            cxx = "game/Models/OrbModel.* + game/Commands/OrbCmd.*"
        elif relative.endswith("/OrbQueue.cs"):
            cxx = "game/Combat/PlayerCombatData::orbs + game/Commands/OrbCmd.*"
        elif relative.startswith("Timeline/Epochs/"):
            cxx = "fully-unlocked Env profile"
        else:
            cxx = f"game/{stem}.*"
        conditions = condition_records(Path(relative), path.read_text(errors="replace"),
                                       "SOURCE_INFRASTRUCTURE", stem, "", cxx)
        target_text = cxx_target_text(cxx)
        for condition in conditions:
            condition["status"], condition["details"] = condition_status(stem, condition, target_text)
        rows.extend(conditions)

    # Shared helpers directly used by Defect card generation, pending choices and orb
    # Hook delivery are also source-audited, without importing unrelated methods from
    # their much larger implementation files.
    engine_members = [
        ("Factories/CardFactory.cs", "GetDistinctForCombat", "game/Factories/CardFactory.*"),
        ("Factories/CardFactory.cs", "FilterForCombat", "game/Factories/CardFactory.*"),
        ("Models/CardPoolModel.cs", "GetUnlockedCards", "game/Factories/CardFactory.*"),
        ("Commands/CardSelectCmd.cs", "FromChooseACardScreen", "game/Commands/CardSelectCmd.*"),
        ("Commands/CardSelectCmd.cs", "FromHand", "game/Commands/CardSelectCmd.*"),
        ("Commands/CardPileCmd.cs", "AddGeneratedCardToCombat", "game/Commands/CardPileCmd.*"),
        ("Commands/CardPileCmd.cs", "Draw", "game/Commands/CardPileCmd.*"),
        ("Commands/CardPileCmd.cs", "Shuffle", "game/Commands/CardPileCmd.*"),
        ("Commands/PowerCmd.cs", "Apply", "game/Commands/PowerCmd.*"),
        ("Commands/PowerCmd.cs", "ModifyAmount", "game/Commands/PowerCmd.*"),
        ("Commands/PowerCmd.cs", "Remove", "game/Commands/PowerCmd.*"),
        ("Commands/PowerCmd.cs", "TickDownDuration", "game/Commands/PowerCmd.*"),
        ("Hooks/Hook.cs", "AfterOrbChanneled", "game/Hooks/Hook.*"),
        ("Hooks/Hook.cs", "AfterOrbEvoked", "game/Hooks/Hook.*"),
        ("Models/OrbModel.cs", "GetRandomOrb", "game/Models/Cards/Chaos.h + game/Models/Powers/TrashToTreasurePower.h"),
        ("Models/OrbModel.cs", "TriggerPassive", "game/Models/OrbModel.*"),
        ("Commands/OrbCmd.cs", "AddSlots", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "RemoveSlots", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "Channel", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "EvokeNext", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "EvokeLast", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "Passive", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "BeforeTurnEnd", "game/Commands/OrbCmd.*"),
        ("Commands/OrbCmd.cs", "AfterTurnStart", "game/Commands/OrbCmd.*"),
        ("Entities/Orbs/OrbQueue.cs", "BeforeTurnEnd", "game/Combat/PlayerCombatData::orbs"),
        ("Entities/Orbs/OrbQueue.cs", "AfterTurnStart", "game/Combat/PlayerCombatData::orbs"),
    ]
    for relative, member_name, cxx in engine_members:
        path = source / relative
        if not path.is_file():
            continue
        full_text = path.read_text(errors="replace")
        for line_offset, member_text in extract_members(full_text, member_name):
            member_status, member_evidence = engine_member_status(member_name, cxx, cxx_target_text(cxx))
            rows.append({
                "row_kind": "engine_member", "scope": "DEFECT_RUNTIME_DEPENDENCY",
                "model_type": path.stem, "model_id": "", "source_file": relative,
                "source_line": str(line_offset), "member_or_condition": member_name,
                "cxx_file": cxx, "status": member_status,
                "details": member_evidence,
            })
            conditions = condition_records(Path(relative), member_text,
                                           "DEFECT_RUNTIME_DEPENDENCY", path.stem, "", cxx,
                                           line_offset - 1)
            target_text = cxx_target_text(cxx)
            for condition in conditions:
                condition["status"], condition["details"] = condition_status(path.stem, condition, target_text)
            rows.extend(conditions)
    rows.extend(generated_edges)
    rows.sort(key=lambda row: (row["row_kind"], row["scope"], row["model_type"],
                               int(row["source_line"] or 0), row["member_or_condition"]))
    output = ROOT / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    counts: dict[str, int] = {}
    for row in rows:
        counts[row["row_kind"]] = counts.get(row["row_kind"], 0) + 1
    missing_models = sum(row["row_kind"] == "model" and row["status"] == "missing" for row in rows)
    print(f"pools: cards={len(card_types)} relics={len(relic_types)} potions={len(potion_types)}")
    print(f"models={counts.get('model', 0)} overrides={counts.get('override', 0)} branches={counts.get('branch', 0)} iterations={counts.get('iteration', 0)} generation_edges={counts.get('generation_edge', 0)}")
    print(f"C++ models missing={missing_models}; Discovery/WhiteNoise/CreativeAI eligible Defect cards={len(combat_card_candidates)}/{len(power_candidates)}; random Defect orb candidates={len(orb_candidates)}; inventory={output.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
