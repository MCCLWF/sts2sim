#!/usr/bin/env python3
"""Guard the v0.111.0 non-hero relic pool coverage and generated usability."""
import json
import os
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = pathlib.Path(os.environ.get("STS2_SRC", ROOT.parent / "Slay the Spire 2" / "src" / "Core"))
if not SRC.exists():
    SRC = ROOT.parents[1] / "Slay the Spire 2" / "src" / "Core"
SCOPE_POOLS = ("Shared", "Event", "Fallback", "Deprecated")
HERO_POOLS = ("Ironclad", "Silent", "Defect", "Regent", "Necrobinder")


def pool_classes(name):
    path = SRC / "Models" / "RelicPools" / f"{name}RelicPool.cs"
    source = path.read_text(encoding="utf-8")
    return re.findall(r"ModelDb\.Relic<([A-Za-z0-9_]+)>\(\)", source)


def nonhero_relic_inventory():
    content = json.loads((ROOT / "data" / "content.json").read_text(encoding="utf-8"))
    relics = [m for m in content["models"] if m["kind"] == "RelicModel"]
    by_source = {pathlib.Path(m["source"]).stem: m for m in relics}
    if len(by_source) != len(relics):
        raise SystemExit("duplicate relic source class in data/content.json")

    def resolve(pools):
        classes = []
        for pool in pools:
            members = pool_classes(pool)
            if len(members) != len(set(members)):
                raise SystemExit(f"duplicate model in {pool}RelicPool")
            classes.extend(members)
        missing = sorted(set(classes) - by_source.keys())
        if missing:
            raise SystemExit(f"pool models absent from data/content.json: {missing}")
        return {by_source[name]["id"].removeprefix("RELIC.") for name in classes}

    in_scope = resolve(SCOPE_POOLS)
    hero_only = resolve(HERO_POOLS)
    if len(in_scope) != 259:
        raise SystemExit(f"expected 259 non-hero relics, found {len(in_scope)}")
    if len(hero_only) != 40:
        raise SystemExit(f"expected 40 hero-pool relics, found {len(hero_only)}")
    if in_scope & hero_only:
        raise SystemExit(f"relics appear in both scope and hero pools: {sorted(in_scope & hero_only)}")
    if len(relics) != len(in_scope | hero_only):
        raise SystemExit("RelicModel content does not equal the source pool inventory")

    dispatch = (ROOT / "game" / "generated" / "Dispatch.cpp").read_text(encoding="utf-8")
    usable_start = dispatch.index("bool IsUsable(ModelKind kind, uint16_t id) {")
    relic_start = dispatch.index("case ModelKind::Relic:", usable_start)
    potion_start = dispatch.index("case ModelKind::Potion:", relic_start)
    usable = set(re.findall(r"case RelicId::([A-Z0-9_]+):", dispatch[relic_start:potion_start]))
    missing_usable = sorted(in_scope - usable)
    if missing_usable:
        raise SystemExit(f"non-hero relics rejected by IsUsable: {missing_usable}")
    unported = {m["id"].removeprefix("RELIC.") for m in relics} - usable
    if unported - hero_only:
        raise SystemExit(f"unusable relics outside hero-only pools: {sorted(unported - hero_only)}")

    override_models = sum(bool(m.get("overrides")) for m in relics if m["id"].removeprefix("RELIC.") in in_scope)
    override_count = sum(len(m.get("overrides", [])) for m in relics if m["id"].removeprefix("RELIC.") in in_scope)
    return {
        "in_scope": sorted(in_scope),
        "hero_only": sorted(hero_only),
        "unported": sorted(unported),
        "override_models": override_models,
        "override_count": override_count,
    }


def main():
    inventory = nonhero_relic_inventory()
    print(f"non-hero relic catalog: {len(inventory['in_scope'])}/{len(inventory['in_scope'])} usable; "
          f"{inventory['override_models']} models with {inventory['override_count']} source overrides; "
          f"{len(inventory['hero_only'])} hero-only excluded ({len(inventory['unported'])} remain unported)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as exc:
        raise SystemExit(str(exc))
