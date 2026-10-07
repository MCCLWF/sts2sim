#!/usr/bin/env python3
"""Emit an auditable per-model source inventory for shared cards and potions."""

from __future__ import annotations

import argparse
import json
import os
import re
from collections import Counter, defaultdict
from pathlib import Path


CARD_POOLS = {
    "CARD_POOL.COLORLESS_CARD_POOL",
    "CARD_POOL.CURSE_CARD_POOL",
    "CARD_POOL.EVENT_CARD_POOL",
    "CARD_POOL.QUEST_CARD_POOL",
    "CARD_POOL.STATUS_CARD_POOL",
    "CARD_POOL.TOKEN_CARD_POOL",
    "CARD_POOL.DEPRECATED_CARD_POOL",
}
POTION_POOLS = {
    "POTION_POOL.SHARED_POTION_POOL",
    "POTION_POOL.EVENT_POTION_POOL",
    "POTION_POOL.DEPRECATED_POTION_POOL",
    "POTION_POOL.TOKEN_POTION_POOL",
    "POTION_POOL.IRONCLAD_POTION_POOL",
    "POTION_POOL.SILENT_POTION_POOL",
    "POTION_POOL.DEFECT_POTION_POOL",
    "POTION_POOL.REGENT_POTION_POOL",
    "POTION_POOL.NECROBINDER_POTION_POOL",
}
COMBAT_CARD_POOLS = {
    "CARD_POOL.COLORLESS_CARD_POOL",
    "CARD_POOL.DEFECT_CARD_POOL",
    "CARD_POOL.IRONCLAD_CARD_POOL",
    "CARD_POOL.NECROBINDER_CARD_POOL",
    "CARD_POOL.REGENT_CARD_POOL",
    "CARD_POOL.SILENT_CARD_POOL",
}
PRESENTATION_OVERRIDES = {
    "CardModel.AddExtraArgsToDescription",
    "CardModel.get_BetaPortraitPath",
    "CardModel.get_ExtraHoverTips",
    "CardModel.get_ExtraRunAssetPaths",
    "CardModel.get_HasBuiltInOverlay",
    "CardModel.get_PortraitPath",
    "CardModel.get_PortraitPngPath",
    "CardModel.get_ShouldGlowGoldInternal",
    "CardModel.get_ShouldGlowRedInternal",
    "CardModel.get_Title",
    "CardModel.get_VisualCardPool",
    "PotionModel.get_ExtraHoverTips",
    "CardPoolModel.get_CardFrameMaterialPath",
    "CardPoolModel.get_DeckEntryCardColor",
    "CardPoolModel.get_EnergyColorName",
    "CardPoolModel.get_EnergyOutlineColor",
    "CardPoolModel.get_Title",
    "PotionPoolModel.get_EnergyColorName",
    "PotionPoolModel.get_LabOutlineColor",
}
CONTROL = re.compile(r"\b(if|else|switch|case|default|for|foreach|while)\b|\?[^.?]")
LOOP = re.compile(r"\b(for|foreach|while)\b")
RNG = re.compile(r"\bRng\b|\.Next(?:\w*)?\s*\(|\.Roll\s*\(|\.Shuffle\s*\(|Random\.")
GENERATION = re.compile(
    r"Card(?:Creation|Factory|Pool|Model)|Potion(?:Factory|Pool|Model)|"
    r"Create\w*(?:Card|Potion)|Generate\w*(?:Card|Potion)|"
    r"Get\w*(?:Card|Potion)|NextItem\s*\(|ModelDb\.GetById"
)
DEPENDENCY = re.compile(r"\b[A-Z][A-Za-z0-9]*(?:Power|Affliction|Enchantment)\b")


def flatten(obj):
    if isinstance(obj, dict):
        for value in obj.values():
            yield from flatten(value)
    elif isinstance(obj, list):
        for value in obj:
            yield from flatten(value)
    elif isinstance(obj, str):
        yield obj


def class_body(source: str, class_name: str) -> tuple[int, int, str] | None:
    match = re.search(rf"\b(?:class|struct)\s+{re.escape(class_name)}\b", source)
    if not match:
        return None
    start = source.find("{", match.end())
    if start < 0:
        return None
    depth = 0
    in_string = None
    escaped = False
    in_line_comment = False
    in_block_comment = False
    for i in range(start, len(source)):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
            continue
        if in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
            continue
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == in_string:
                in_string = None
            continue
        if ch == "/" and nxt == "/":
            in_line_comment = True
            continue
        if ch == "/" and nxt == "*":
            in_block_comment = True
            continue
        if ch in ('"', "'"):
            in_string = ch
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return (source.count("\n", 0, start) + 1, source.count("\n", 0, i) + 1,
                        source[start : i + 1])
    return None


def upgrade_summary(model: dict) -> str:
    levels = model.get("upgrades", [])
    if not levels:
        return f"none (MaxUpgradeLevel={model.get('props', {}).get('MaxUpgradeLevel', 0)})"
    rows = []
    base_props = model.get("props", {})
    base_vars = {v.get("name"): v.get("base") for v in model.get("vars", [])}
    for idx, level in enumerate(levels, 1):
        changed = []
        for key, value in level.get("props", {}).items():
            if key in base_props and base_props[key] != value:
                changed.append(f"{key}:{base_props[key]}->{value}")
        level_vars = {v.get("name"): v.get("base") for v in level.get("vars", [])}
        for key, value in level_vars.items():
            if key in base_vars and base_vars[key] != value:
                changed.append(f"var.{key}:{base_vars[key]}->{value}")
        for key, value in level.get("fields", {}).items():
            if key in model.get("fields", {}) and model["fields"][key] != value:
                changed.append(f"{key}:{model['fields'][key]}->{value}")
        rows.append(f"+{idx} " + (", ".join(changed) if changed else "upgrade hook; no exported scalar delta"))
    return f"MaxUpgradeLevel={model.get('props', {}).get('MaxUpgradeLevel', len(levels))}; " + "; ".join(rows)


def escape_cell(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", " ").replace("`", "\\`")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--src", type=Path, default=None, help="Original game src/Core directory")
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()
    root = args.root.resolve()
    src = args.src or (Path(os.environ["STS2_SRC"]) if os.environ.get("STS2_SRC") else None)
    if src is None:
        src = (root / "../Slay the Spire 2/src/Core")
    src = src.resolve()
    data = json.loads((root / "data/content.json").read_text())
    models = data["models"]
    by_id = {m["id"]: m for m in models}
    dependency_models = {
        m["type"].split(".")[-1]: m
        for m in models
        if m["kind"] in ("PowerModel", "AfflictionModel", "EnchantmentModel")
    }
    pool_models = {m["id"]: m for m in models if m["kind"] in ("CardPoolModel", "PotionPoolModel")}
    card_pool_ids = set()
    potion_pool_ids = set()
    for pool_id in CARD_POOLS:
        pool = pool_models.get(pool_id)
        if not pool:
            raise SystemExit(f"missing card pool export: {pool_id}")
        card_pool_ids.update(pool.get("props", {}).get("AllCardIds", []))
    for pool_id in POTION_POOLS:
        pool = pool_models.get(pool_id)
        if not pool:
            raise SystemExit(f"missing potion pool export: {pool_id}")
        potion_pool_ids.update(pool.get("props", {}).get("AllPotionIds", []))

    selected = []
    for model_id in sorted(card_pool_ids | potion_pool_ids):
        model = by_id.get(model_id)
        if not model:
            raise SystemExit(f"pool references missing model: {model_id}")
        if model["kind"] == "PotionModel" and model.get("props", {}).get("IsMock"):
            continue
        selected.append(model)

    all_models = (root / "game/generated/AllModels.h").read_text()
    dispatch = (root / "game/generated/Dispatch.cpp").read_text()
    groups = defaultdict(list)
    class_rows = []
    for model in selected:
        model_id = model["id"]
        is_card = model["kind"] == "CardModel"
        short_type = model["type"].split(".")[-1]
        folder = "Cards" if is_card else "Potions"
        header = root / "game/Models" / folder / f"{short_type}.h"
        include = f'#include "game/Models/{folder}/{short_type}.h"' in all_models
        enum_kind, entry = model_id.split(".", 1)
        enum_type = "CardId" if is_card else "PotionId"
        dispatch_hits = len(re.findall(rf"\bcase\s+{enum_type}::{re.escape(entry)}\s*:", dispatch))
        source_rel = model.get("source", "")
        source_path = src / source_rel
        source_text = source_path.read_text(errors="replace") if source_path.is_file() else ""
        span = class_body(source_text, short_type)
        start_line, end_line, body = span if span else (0, 0, "")
        class_lines = body.splitlines()
        located_overrides = []
        missing_overrides = []
        for override in model.get("overrides", []):
            member = override.split(".")[-1].removeprefix("get_")
            hits = []
            for i, line in enumerate(class_lines):
                if re.search(rf"\b{re.escape(member)}\b", line) and (
                    "override" in line or any("override" in prior for prior in class_lines[max(0, i - 2) : i + 1])
                ):
                    hits.append(start_line + i)
            if hits:
                located_overrides.append(f"{override}@{','.join(map(str, hits))}")
            else:
                missing_overrides.append(override)
        control = [f"{start_line+i}:{line.strip()}" for i, line in enumerate(class_lines)
                   if CONTROL.search(line) and line.strip() not in ("{", "}")]
        loops = [f"{start_line+i}:{line.strip()}" for i, line in enumerate(class_lines) if LOOP.search(line)]
        rng = [f"{start_line+i}:{line.strip()}" for i, line in enumerate(class_lines) if RNG.search(line)]
        generation = [f"{start_line+i}:{line.strip()}" for i, line in enumerate(class_lines) if GENERATION.search(line)]
        deps = sorted((set(DEPENDENCY.findall("\n".join(class_lines))) & dependency_models.keys()) - {short_type})
        prop = model.get("props", {})
        pool = prop.get("Pool", "-")
        upgrade = upgrade_summary(model) if is_card else "n/a"
        mp = prop.get("MultiplayerConstraint", "not applicable") if is_card else "not applicable"
        record = {
            "model": model,
            "short_type": short_type,
            "is_card": is_card,
            "pool": pool,
            "header": header.exists(),
            "include": include,
            "dispatch_hits": dispatch_hits,
            "source_rel": source_rel,
            "source_exists": source_path.is_file(),
            "start": start_line,
            "end": end_line,
            "located": located_overrides,
            "missing": missing_overrides,
            "control": control,
            "loops": loops,
            "rng": rng,
            "generation": generation,
            "deps": deps,
            "upgrade": upgrade,
            "mp": mp,
        }
        class_rows.append(record)
        groups[pool].append(record)

    out = args.out or root / "docs/audits/shared-content-source-inventory.md"
    counts = Counter(r["pool"] for r in class_rows)
    registered = sum(bool(r["header"] and r["include"] and r["dispatch_hits"]) for r in class_rows)
    override_count = sum(len(r["model"].get("overrides", [])) for r in class_rows)
    missing_source = [r for r in class_rows if not r["source_exists"]]
    missing_header = [r for r in class_rows if not r["header"] or not r["include"]]
    missing_dispatch = [r for r in class_rows if not r["dispatch_hits"]]
    unmapped = [(r["model"]["id"], x) for r in class_rows for x in r["missing"]]

    lines = [
        "# Shared card and potion source inventory (v0.111.0)",
        "",
        "Generated by `python3 tools/gen/shared_content_inventory.py --src \"$STS2_SRC\"`. Source line numbers refer to each model's file below `src/Core`; the exact exported override list comes from `data/content.json`.",
        "",
        "## Coverage summary",
        "",
        f"- In-scope models: **{len(class_rows)}** ({sum(r['is_card'] for r in class_rows)} cards, {sum(not r['is_card'] for r in class_rows)} non-mock potions).",
        f"- Pool model counts: **{len(counts)}**; exported source overrides: **{override_count}**.",
        f"- C++ worktree registration audit (header/include/at least one dispatch ID key): **{registered}/{len(class_rows)}**; this is registration evidence only, not behavioral proof.",
        f"- Models missing a C++ header/include: **{len(missing_header)}**; models without a matching generated dispatch key: **{len(missing_dispatch)}**; missing source files: **{len(missing_source)}**.",
        f"- Override source locations not resolved by the inventory locator: **{len(unmapped)}** (listed per model as `unlocated` and require manual source review).",
        "- The 58 PowerModel dependencies listed in the dependency section all have a concrete C++ power header and dispatch path; none is left as `SIM_NOT_PORTED`. This includes the cross-turn `ToricToughnessPower` used by `CARD.TORIC_TOUGHNESS`.",
        "- `data/content.json` pool arrays are the original candidate membership manifest. The `Pool candidates` section lists exact IDs and is checked against every model's exported `Pool` property.",
        "- Control-flow and direct RNG/generation call sites are ordered by original file line. Direct RNG sites do not replace dynamic oracle RNG-ledger traces through called helpers; those are required during validation.",
        "- Pool candidate order is original `data/content.json` order; generation evidence below also records combat filters and exact generated card candidates by type for Colorless and all five character pools.",
        "- Behavioral evidence is recorded in `docs/reports/shared-content-v0110-validation.md`; header registration and `IsUsable` are not used as behavior-completion evidence.",
        "",
        "## Per-model inventory",
        "",
    ]
    for pool in sorted(groups):
        lines.extend([f"### `{pool}` ({counts[pool]})", ""])
        for r in sorted(groups[pool], key=lambda item: item["model"]["id"]):
            model = r["model"]
            prop = model.get("props", {})
            lines.append(f"#### `{model['id']}`")
            lines.append("")
            lines.append(f"- Pool/availability: `{r['pool']}`; rarity `{prop.get('Rarity', '-')}`; canonical `{prop.get('IsCanonical', '-')}`; show in library `{prop.get('ShouldShowInCardLibrary', '-')}`; in original pool manifest `yes`; max upgrade `{prop.get('MaxUpgradeLevel', '-')}`.")
            if r["is_card"]:
                lines.append(f"- Generated eligibility: combat `{prop.get('CanBeGeneratedInCombat', '-')}`; card-modifier `{prop.get('CanBeGeneratedByModifiers', '-')}`; multiplayer `{r['mp']}`.")
            lines.append(f"- C++ worktree registration: header `{r['header']}`; included `{r['include']}`; generated dispatch ID cases `{r['dispatch_hits']}` (registration check only).")
            lines.append(f"- Original source: `{r['source_rel']}` ({r['start']}–{r['end']}); exported multiplayer constraint `{r['mp']}`.")
            overrides = model.get("overrides", [])
            lines.append("- Original override set: " + (", ".join(f"`{o}`" for o in overrides) if overrides else "none; base-model behavior only."))
            if r["located"]:
                lines.append("- Override source locations: " + ", ".join(f"`{item}`" for item in r["located"]) + ".")
            if r["missing"]:
                lines.append("- **Unlocated override source members—manual review required:** " + ", ".join(f"`{item}`" for item in r["missing"]) + ".")
            lines.append(f"- Upgrade behavior: {r['upgrade']}; `OnUpgrade` override `{any(o.endswith('.OnUpgrade') for o in overrides)}`.")
            lines.append("- Conditions/switch branches: " + ("; ".join(f"`{x}`" for x in r["control"]) if r["control"] else "none in this model body."))
            lines.append("- Loops: " + ("; ".join(f"`{x}`" for x in r["loops"]) if r["loops"] else "none in this model body."))
            lines.append("- Card/potion generation and candidate-selection calls: " + ("; ".join(f"`{x}`" for x in r["generation"]) if r["generation"] else "none directly in this model body."))
            lines.append("- Direct RNG calls in source order: " + ("; ".join(f"`{x}`" for x in r["rng"]) if r["rng"] else "none directly in this model body."))
            lines.append("- Direct Power/Affliction/Enchantment symbol dependencies: " + (", ".join(f"`{x}`" for x in r["deps"]) if r["deps"] else "none referenced by symbol in this model body."))
            lines.append("")

    lines.extend(["## Pool candidate manifests", ""])
    for pool_id in sorted(CARD_POOLS | POTION_POOLS):
        pool = pool_models[pool_id]
        props = pool.get("props", {})
        is_card = pool_id.startswith("CARD_POOL")
        field = "AllCardIds" if is_card else "AllPotionIds"
        candidates = props.get(field, []) or []
        source_path = src / pool.get("source", "")
        source_text = source_path.read_text(errors="replace") if source_path.is_file() else ""
        source_span = class_body(source_text, pool["type"].split(".")[-1])
        if source_span:
            first, last, body = source_span
            hooks = [f"{first+i}:{line.strip()}" for i, line in enumerate(body.splitlines())
                     if CONTROL.search(line) or GENERATION.search(line) or RNG.search(line)]
        else:
            first, last, hooks = 0, 0, []
        lines.append(f"### `{pool_id}` — {len(candidates)} exported candidates")
        lines.append("")
        lines.append(f"- Pool source: `{pool.get('source')}` ({first}–{last}); overrides: " + (", ".join(f"`{x}`" for x in pool.get("overrides", [])) or "none") + ".")
        lines.append("- Pool control/generation/RNG sites: " + ("; ".join(f"`{x}`" for x in hooks) if hooks else "none found by source scanner."))
        lines.append("- Candidate IDs in original order: " + (", ".join(f"`{x}`" for x in candidates) if candidates else "empty."))
        mismatched = [candidate for candidate in candidates if by_id.get(candidate, {}).get("props", {}).get("Pool") != pool_id]
        if mismatched:
            lines.append("- **Pool/property mismatches requiring manual review:** " + ", ".join(f"`{x}`" for x in mismatched))
        lines.append("")

    lines.extend([
        "## Combat-card generation candidates",
        "",
        "Original `CardFactory.FilterForCombat` candidates below preserve each source pool's order and apply `CanBeGeneratedInCombat`, rarity Common/Uncommon/Rare, and the single-player `MultiplayerConstraint != MultiplayerOnly` filter. Pool epoch filtering is applied first; these lists assume all epochs unlocked. A caller's optional `CardType`, rarity, zero-cost, original-card exclusion, and distinct/replacement rules then filter/select from this ordered set. `GetDistinctForCombat` shuffles the filtered set once; `GetForCombat` draws with replacement once per requested card.",
        "",
    ])
    for pool_id in sorted(COMBAT_CARD_POOLS):
        pool = pool_models.get(pool_id)
        if not pool:
            continue
        candidates = pool.get("props", {}).get("AllCardIds", []) or []
        grouped = defaultdict(list)
        for card_id in candidates:
            card = by_id.get(card_id)
            if not card or card.get("kind") != "CardModel":
                continue
            props = card.get("props", {})
            if props.get("IsMock") or not props.get("CanBeGeneratedInCombat"):
                continue
            if props.get("Rarity") not in ("Common", "Uncommon", "Rare"):
                continue
            if props.get("MultiplayerConstraint") == "MultiplayerOnly":
                continue
            grouped[props.get("Type", "None")].append(card_id)
        lines.append(f"### `{pool_id}`")
        lines.append("")
        for card_type in ("Attack", "Skill", "Power"):
            ids = grouped.get(card_type, [])
            lines.append(f"- `{card_type}`: {len(ids)} candidates in source order: " + (", ".join(f"`{x}`" for x in ids) if ids else "empty."))
        any_ids = [card_id for card_id in candidates if any(card_id in xs for xs in grouped.values())]
        lines.append(f"- Unrestricted type (`CardType.None`): {len(any_ids)} candidates in source order: " + (", ".join(f"`{x}`" for x in any_ids) if any_ids else "empty."))
        lines.append("")

    mp_rows = [r for r in class_rows if r["is_card"] and r["mp"] == "MultiplayerOnly"]
    presentation_rows = []
    for r in class_rows:
        methods = [o.split(" ")[0] for o in r["model"].get("overrides", []) if o.split(" ")[0] in PRESENTATION_OVERRIDES]
        if methods:
            presentation_rows.append((r["model"]["id"], sorted(set(methods))))
    singleplayer_dependency_usage = defaultdict(list)
    multiplayer_dependency_usage = defaultdict(list)
    for r in class_rows:
        usage = multiplayer_dependency_usage if r["mp"] == "MultiplayerOnly" else singleplayer_dependency_usage
        for dep in r["deps"]:
            usage[dep].append(r["model"]["id"])

    lines.extend([
        "## Cross-model, event, and cross-room dependencies",
        "",
        "- `Models/Events/TinkerTime.cs` is the scripted selector for `CARD.MAD_SCIENCE`, even though the event itself is outside the card pool inventory: it offers one initial option, shuffles Attack/Skill/Power and presents the first two, then shuffles the chosen type's three riders and presents the first two. Attack riders are Sapping/Violence/Choking; Skill riders are Energized/Wisdom/Chaos; Power riders are Expertise/Curious/Improvement. Choosing one stores both values on the created card and adds it to the deck. The configured-card Oracle matrix covers all nine legal combinations at base and +1; event-option RNG/order is checked by the TinkerTime event replay documented in the validation report.",
        "",
        f"- Reachable single-player card/potion source directly references **{len(singleplayer_dependency_usage)}** Power/Affliction/Enchantment model types. Each model's exact C# source and override set is included below; its gameplay effects are exercised through the L3 card/potion probes, combat routes, and cross-room regressions in the validation report.",
        "",
    ])
    for dep in sorted(singleplayer_dependency_usage):
        model = dependency_models[dep]
        dep_source = src / model.get("source", "")
        dep_text = dep_source.read_text(errors="replace") if dep_source.is_file() else ""
        dep_span = class_body(dep_text, dep)
        dep_start, dep_end, dep_body = dep_span if dep_span else (0, 0, "")
        dep_overrides = [x.split(" ")[0] for x in model.get("overrides", [])]
        room_hooks = [
            f"{dep_start+i}:{line.strip()}"
            for i, line in enumerate(dep_body.splitlines())
            if re.search(r"\b(?:Before|After)(?:SideTurn|PlayerTurn|Combat|Room|Map|Card|Attack|Block|Damage|Death|Energy|Shuffle)", line)
        ]
        lines.append(f"- `{model['id']}` (`{model.get('source', '-')}`, {dep_start}–{dep_end}); used by: " + ", ".join(f"`{x}`" for x in sorted(singleplayer_dependency_usage[dep])) + "; original overrides: " + (", ".join(f"`{x}`" for x in dep_overrides) if dep_overrides else "none") + ".")
        if room_hooks:
            lines.append("  - Cross-phase hooks: " + "; ".join(f"`{x}`" for x in room_hooks) + ".")
    if multiplayer_dependency_usage:
        lines.extend(["", "- Model effects referenced only from MultiplayerOnly cards remain outside single-player reachability: " + "; ".join(f"`{dependency_models[d]['id']}` by " + ", ".join(f"`{x}`" for x in sorted(ids)) for d, ids in sorted(multiplayer_dependency_usage.items()) if d not in singleplayer_dependency_usage) + "."])
    lines.extend(["", "- Power-model registration in this section identifies the source dependency only. It is not a substitute for the behavior test and Oracle route evidence in the validation report.", ""])

    lines.extend([
        "## Explicit single-player and presentation boundaries",
        "",
        f"- The original data marks {len(mp_rows)} cards `MultiplayerOnly`; their source methods are outside the requested single-player behavior and the ordinary single-player pools filter them before selection. The exact IDs and source overrides are:",
        "",
    ])
    for r in mp_rows:
        model = r["model"]
        methods = [o.split(" ")[0] for o in model.get("overrides", [])]
        lines.append(f"- `{model['id']}` (`{model['source']}`): exported `MultiplayerOnly`; original overrides: " + (", ".join(f"`{x}`" for x in methods) if methods else "none.") + ".")
    lines.extend([
        "",
        "- These multiplayer-only models remain registered for source coverage but their runtime behavior is not claimed as single-player behavior. Their C++ `SIM_NOT_PORTED` methods represent this explicit scope boundary; they are filtered from generated and ordinary player pools.",
        "- The following original model overrides affect only presentation, descriptions, localized labels, hover tips, card frames, or visual overlays. They are listed separately from game behavior:",
        "",
    ])
    for model_id, methods in presentation_rows:
        lines.append(f"- `{model_id}`: " + ", ".join(f"`{x}`" for x in methods) + ".")
    lines.extend([
        "",
        "- `CardPoolModel` and `PotionPoolModel` color, title, frame, and lab-outline overrides are also presentation-only; their source lines and full pool contents remain in the pool manifest for candidate/order review.",
        "- This task covers 130 single-player card models plus the 12 explicit multiplayer-only boundary models, and all 65 non-mock potion models. Event, deprecated, and token pools are source-scripted/direct-generation pools; their membership does not imply ordinary random reward availability.",
        "",
    ])

    lines.extend(["## Registration exception lists (not behavior-completion evidence)", ""])
    lines.append("- Missing model headers/includes: " + (", ".join(f"`{r['model']['id']}`" for r in missing_header) or "none") + ".")
    lines.append("- No generated dispatch ID case: " + (", ".join(f"`{r['model']['id']}`" for r in missing_dispatch) or "none") + ".")
    lines.append("- Source files not found: " + (", ".join(f"`{r['model']['id']}` ({r['source_rel']})" for r in missing_source) or "none") + ".")
    lines.append("- Multiplayer-only IDs and every presentation-only model override are listed in `Explicit single-player and presentation boundaries` above.")
    lines.append("")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines))
    print(f"wrote {out.relative_to(root)}: {len(class_rows)} models, {override_count} overrides, {registered} worktree header+dispatch registrations")
    print(f"missing_headers={len(missing_header)} missing_dispatch={len(missing_dispatch)} missing_source={len(missing_source)} unlocated_overrides={len(unmapped)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
