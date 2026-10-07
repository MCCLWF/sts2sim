#!/usr/bin/env python3
"""What a policy does outside card play, from nosl_decision_diag output.

  tools/nosl/decision_summary.py NAME=decisions.jsonl [NAME=decisions.jsonl ...]

One column per file. Every count is over the runs in that file (200 dev
runs in the reports). The file's lines of one run are read in file order.
"""
from __future__ import annotations

import collections
import json
import sys


def fight_kind(encounter: str) -> str:
    for suffix, name in (("_BOSS", "boss"), ("_ELITE", "elite"), ("_WEAK", "weak")):
        if encounter.endswith(suffix):
            return name
    return "normal"


def summarize(path: str) -> dict:
    runs = collections.defaultdict(list)
    for line in open(path, encoding="utf-8"):
        if line.strip():
            row = json.loads(line)
            runs[row["seed"]].append(row)
    c = collections.Counter()
    boss_hp, boss_deck, final_deck, boss_potions = [], [], [], []
    for rows in runs.values():
        deck = None
        last_fight = None
        for i, r in enumerate(rows):
            kind = r["kind"]
            if kind == "CombatSummary":
                where = fight_kind(r["encounter"])
                c["fights_" + where] += 1
                c["potions_used"] += r["potions"]
                c["turns"] += r["end_turns"]
                c["incoming"] += r["incoming"]
                c["unblocked"] += r["unblocked"]
                c["overblock"] += r["overblock"]
                if where == "boss":
                    boss_hp.append(r["hp_before"])
                    boss_potions.append(r["potions"])
                    if deck is not None:
                        boss_deck.append(deck)
                    c["boss_wins"] += r["hp_after"] > 0
                last_fight = (where, r["hp_after"])
                continue
            deck = r.get("deck", deck)
            options, chosen = r["options"], r["options"][r["chosen"]]
            nxt = next((x for x in rows[i + 1:] if x["kind"] != "CombatSummary"), None)
            if not r.get("in_combat") and nxt is not None and nxt["max_hp"] < r["max_hp"]:
                c["max_hp_given_up"] += r["max_hp"] - nxt["max_hp"]
            if kind == "RestSite":
                both = "RestOption:Heal" in options and "RestOption:Smith" in options
                name = chosen.split(":", 1)[-1]
                c["rest_" + ("heal" if name == "Heal" else "smith" if name == "Smith" else "other")] += 1
                if both and r["hp"] >= 0.8 * r["max_hp"]:
                    c["rest_high_hp"] += 1
                    c["rest_high_hp_smith"] += name == "Smith"
            elif kind == "Map":
                if "MapMove:Elite" in options and any(o != "MapMove:Elite" for o in options):
                    c["elite_offered"] += 1
                    c["elite_taken"] += chosen == "MapMove:Elite"
            elif kind == "Choice" and r.get("prompt") == "CardReward":
                c["card_skip" if chosen == "Answer:Alternative" else "card_take"] += 1
            elif kind == "Rewards" and chosen == "Proceed":
                for o in options:
                    if o.startswith("TakeReward:"):
                        c["left_" + o.split(":")[1].lower()] += 1
            elif kind == "Treasure":
                c["treasure_take" if chosen.startswith("TreasurePick") else "treasure_skip"] += 1
            elif kind == "Shop":
                c["shop_buy" if chosen.startswith("ShopBuy") else "shop_other"] += 1
            if chosen.startswith("DiscardPotion"):
                c["potions_discarded"] += 1
        if deck is not None:
            final_deck.append(deck)
        if last_fight and last_fight[1] <= 0:
            c["death_" + last_fight[0]] += 1

    def mean(values):
        return round(sum(values) / len(values), 1) if values else None

    turns = max(1, c["turns"])
    return {
        "runs": len(runs),
        "rest: heal / smith / other": f"{c['rest_heal']} / {c['rest_smith']} / {c['rest_other']}",
        "rest at >= 80% HP: smith / all": f"{c['rest_high_hp_smith']} / {c['rest_high_hp']}",
        "elite chosen / offered with another way": f"{c['elite_taken']} / {c['elite_offered']}",
        "card reward: take / skip": f"{c['card_take']} / {c['card_skip']}",
        "left behind: gold / card / relic / potion": f"{c['left_gold']} / {c['left_card']} / {c['left_relic']} / {c['left_potion']}",
        "treasure: take / skip": f"{c['treasure_take']} / {c['treasure_skip']}",
        "shop purchases": c["shop_buy"],
        "max HP given up by choice": c["max_hp_given_up"],
        "potions: used / discarded": f"{c['potions_used']} / {c['potions_discarded']}",
        "deck size at the last decision": mean(final_deck),
        "per turn: incoming / unblocked / overblock":
            f"{c['incoming'] / turns:.1f} / {c['unblocked'] / turns:.1f} / {c['overblock'] / turns:.1f}",
        "fights: weak / normal / elite / boss":
            f"{c['fights_weak']} / {c['fights_normal']} / {c['fights_elite']} / {c['fights_boss']}",
        "deaths: weak / normal / elite / boss":
            f"{c['death_weak']} / {c['death_normal']} / {c['death_elite']} / {c['death_boss']}",
        "boss fights won": f"{c['boss_wins']} / {c['fights_boss']}",
        "boss fight: HP at entry / deck size / potions used": f"{mean(boss_hp)} / {mean(boss_deck)} / {mean(boss_potions)}",
    }


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    names, tables = [], []
    for arg in sys.argv[1:]:
        name, _, path = arg.partition("=")
        names.append(name if path else arg)
        tables.append(summarize(path or arg))
    width = max(len(k) for k in tables[0])
    print(" " * width + "".join(f" | {n:>22}" for n in names))
    for key in tables[0]:
        print(f"{key:{width}}" + "".join(f" | {str(t[key]):>22}" for t in tables))


if __name__ == "__main__":
    main()
