#!/usr/bin/env python3
"""What the uniformly random strategic actions of a collection were followed by.

  tools/nosl/exploration_effects.py --data DIR[,DIR...] --out FILE.json [--workers 6] [--limit-runs N]

nosl.strategy_q collect replaces the strategic policy's action by a uniformly
random legal one with a fixed probability. Those actions are a randomized
trial: within one kind of decision, which action was taken does not depend on
the state. This tool replays the recorded runs, keeps only the exploratory
decisions, labels each by what the action was (rest or upgrade, the type of
the map node entered, the card taken or skipped, ...) and reports the mean
outcome target of the runs for every label, with the difference from the
other exploratory actions in the same context. A context is one kind of
decision offering one set of things, so that a difference is not a
difference in where the run already was.

The outcome target is the training signal of nosl.strategy_q
(floor / 50 + 0.5 * acts completed + 2 * victory). The comparison is over the
states the behaviour policy visited and is followed by that same exploring
policy; it says which actions that policy undervalues, not what an optimal
player should do. No model is involved.
"""
from __future__ import annotations

import argparse
import collections
import json
import math
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from nosl import native  # noqa: E402
from nosl.replay import replay  # noqa: E402
from nosl.strategy_q import outcome_target, read_runs  # noqa: E402

G_DECISION, G_ACT, G_HP, G_MAX_HP, G_PROMPT, G_SOURCE = 1, 4, 7, 8, 32, 33
T_TYPE, T_CARD, T_MISC_A, T_N0 = 0, 1, 6, 8
ACTION_NAMES = ("StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption", "RestOption",
                "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume")
DECISIONS = ("map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary")
REST = ("Heal", "Smith", "Mend", "Lift", "Dig", "Cook", "Clone", "Hatch", "Kindle")
MAP_POINT = ("Unassigned", "Unknown", "Shop", "Treasure", "RestSite", "Monster", "Elite", "Boss", "Ancient")
REWARD = ("None", "Card", "Gold", "Potion", "Relic", "RemoveCard", "SpecialCard")
SHOP = ("CharacterCard", "ColorlessCard", "Relic", "Potion", "CardRemoval")
CHOICE_OPTION = ("Card", "Bundle", "Reward", "Alternative", "Proceed")
MISC_MAP, MISC_REWARD, MISC_SHOP, MISC_REST, MISC_CHOICE, MISC_EVENT = 1, 17, 33, 41, 57, 128
KNOWN_TEXT = ("CardSelectCmd", "CardReward", "rewards", "take a reward, or proceed", "FromSimpleGridForRewards", "index",
              "count", "FromDeckGeneric", "FromDeckForEnchantment", "FromDeckForUpgrade", "FromDeckForTransformation",
              "FromDeckForRemoval", "FromChooseACardScreen", "FromChooseABundleScreen", "bundle", "crystal_sphere",
              "CRYSTAL_SPHERE.minigame")


def bucket(text):
    """nosl/observation.cpp Bucket(): the misc id of a text key."""
    h = 2166136261
    for c in text.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return 256 + h % 1024


def enum_names(enum):
    text = (ROOT / "content/generated/ids.h").read_text()
    body = re.search(r"enum class %s[^{]*\{([^}]*)\}" % enum, text, re.S).group(1)
    return [x.split("=")[0].strip() for x in body.split(",") if x.strip()]


class Mean:
    __slots__ = ("n", "s", "ss")

    def __init__(self):
        self.n, self.s, self.ss = 0, 0.0, 0.0

    def add(self, y):
        self.n += 1
        self.s += y
        self.ss += y * y

    def stats(self):
        mean = self.s / self.n
        var = max(0.0, self.ss / self.n - mean * mean)
        return mean, math.sqrt(var / self.n)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--workers", type=int, default=6)
    p.add_argument("--limit-runs", type=int, default=0)
    p.add_argument("--min-count", type=int, default=200, help="labels with fewer exploratory actions are not printed")
    p.add_argument("--min-context", type=int, default=1500, help="contexts with fewer exploratory actions are not printed")
    args = p.parse_args()
    runs, skipped = read_runs(args.data.split(","))
    if args.limit_runs:
        runs = runs[:args.limit_runs]
    for r in runs:
        r["explored_set"] = set(r.get("explored", []))
        r["target"] = outcome_target(r["floor"], r["act"], r["status"])
    cards, events = enum_names("CardId"), enum_names("EventId")
    text_of = {bucket(t): t for t in KNOWN_TEXT}
    table = collections.defaultdict(Mean)       # (context, label) -> outcomes of exploratory actions
    offered = collections.defaultdict(Mean)     # (context, card) -> outcomes when offered and something else was taken

    def card_label(token):
        return cards[token[T_CARD]] + "+" * int(token[T_N0]) if 0 < token[T_CARD] < len(cards) else "?"

    samples = 0
    for chunk in replay(runs, workers=args.workers, order_seed=1):
        for i in np.flatnonzero(chunk["explored"]):
            g, tokens, actions = chunk["global"][i], chunk["tokens"][i], chunk["actions"][i]
            y = float(chunk["target"][i])
            kind = DECISIONS[g[G_DECISION]]
            action = actions[chunk["choice"][i]]
            token = tokens[action[1]] if action[1] >= 0 else None
            name = ACTION_NAMES[action[0]]
            act = f"act{g[G_ACT] + 1}"
            samples += 1
            if kind in ("rest", "map", "rewards", "shop", "treasure"):
                # Only decisions that offered the same set of things are compared with one another: what is on
                # offer depends on where the run is (a rest site is deeper than the first fights; a relic is only
                # for sale to a run that has the gold), and the run's outcome depends on that too.
                def simple(row):
                    tok = tokens[row[1]] if row[1] >= 0 else None
                    what = ACTION_NAMES[row[0]]
                    if tok is None:
                        return "leave" if kind == "shop" and what == "Proceed" else what
                    if kind == "rest" and what == "RestOption":
                        return REST[tok[T_MISC_A] - MISC_REST]
                    if kind == "map" and what == "MapMove":
                        return MAP_POINT[tok[T_MISC_A] - MISC_MAP]
                    if kind == "rewards" and what == "TakeReward":
                        return "take " + REWARD[tok[T_MISC_A] - MISC_REWARD]
                    if kind == "shop" and what == "ShopBuy":
                        return "buy " + SHOP[tok[T_MISC_A] - MISC_SHOP]
                    return what
                label = simple(action)
                menu = "|".join(sorted({simple(row) for row in actions[:int(chunk["counts"][i, 1])]}))
                where = kind
                if kind == "rest":
                    hp = g[G_HP] / max(1, g[G_MAX_HP])
                    where = "rest " + ("hp<50%" if hp < 0.5 else "hp 50-80%" if hp < 0.8 else "hp>=80%")
                elif kind == "map":
                    where = "map " + act
                table[(f"{where} offering {menu}", label)].add(y)
            elif kind == "event":
                if name == "EventOption" and token is not None and MISC_EVENT <= token[T_MISC_A] < MISC_EVENT + len(events):
                    table[("event " + events[token[T_MISC_A] - MISC_EVENT], f"option {int(token[T_N0])}")].add(y)
                else:
                    table[("event ?", name)].add(y)
            elif kind == "choice":
                source = text_of.get(int(g[G_SOURCE]), f"#{int(g[G_SOURCE])}")
                prompt = text_of.get(int(g[G_PROMPT]), f"#{int(g[G_PROMPT])}")
                context = f"choice {source}/{prompt}"
                if name != "Answer" or token is None:
                    table[(context, name + f" {int(action[3])}")].add(y)
                    continue
                option = CHOICE_OPTION[token[T_MISC_A] - MISC_CHOICE] if MISC_CHOICE <= token[T_MISC_A] < MISC_CHOICE + 5 else "?"
                label = card_label(token) if option == "Card" else option
                table[(context, label)].add(y)
                if prompt == "CardReward":
                    table[("card reward: card or not", "take a card" if option == "Card" else "skip")].add(y)
                # Every other card on offer: this run is one where it was offered and not taken.
                for row in actions[:int(chunk["counts"][i, 1])]:
                    if row[1] >= 0 and row[1] != action[1]:
                        other = tokens[row[1]]
                        if MISC_CHOICE <= other[T_MISC_A] < MISC_CHOICE + 5 and CHOICE_OPTION[other[T_MISC_A] - MISC_CHOICE] == "Card":
                            offered[(context, card_label(other))].add(y)
    contexts = collections.defaultdict(dict)
    for (context, label), m in table.items():
        contexts[context][label] = m
    result = {"data": args.data, "runs": len(runs), "skipped_runs": dict(skipped), "exploratory_decisions": samples,
              "target": "floor/50 + 0.5*act + 2*victory", "contexts": {}}
    for context in sorted(contexts):
        labels = contexts[context]
        total_n = sum(m.n for m in labels.values())
        total_s = sum(m.s for m in labels.values())
        total_ss = sum(m.ss for m in labels.values())
        rows = {}
        for label, m in labels.items():
            mean, se = m.stats()
            row = {"n": m.n, "mean": mean, "se": se}
            rest_n = total_n - m.n
            if rest_n > 0:
                rest_mean = (total_s - m.s) / rest_n
                rest_var = max(0.0, (total_ss - m.ss) / rest_n - rest_mean * rest_mean)
                row["minus_other_actions"] = mean - rest_mean
                row["minus_other_actions_se"] = math.sqrt(se * se + rest_var / rest_n)
            if (context, label) in offered and offered[(context, label)].n > 0:
                o_mean, o_se = offered[(context, label)].stats()
                row["minus_offered_not_taken"] = mean - o_mean
                row["minus_offered_not_taken_se"] = math.sqrt(se * se + o_se * o_se)
                row["offered_not_taken_n"] = offered[(context, label)].n
            rows[label] = row
        result["contexts"][context] = {"n": total_n, "labels": rows}
    Path(args.out).write_text(json.dumps(result, indent=1) + "\n")
    print(f"runs {len(runs)}  exploratory decisions {samples}")
    for context, entry in result["contexts"].items():
        shown = {k: v for k, v in entry["labels"].items() if v["n"] >= args.min_count}
        if len(shown) < 2 or context.startswith("event ") or entry["n"] < args.min_context:
            continue
        key = "minus_offered_not_taken" if any("minus_offered_not_taken" in v for v in shown.values()) else "minus_other_actions"
        print(f"\n{context}  (n={entry['n']}; difference: {key.replace('_', ' ')})")
        for label, v in sorted(shown.items(), key=lambda kv: -kv[1].get(key, -9))[:40]:
            if key in v:
                print(f"  {label:28s} n={v['n']:6d}  mean {v['mean']:.4f}  diff {v[key]:+.4f} ± {1.96 * v[key + '_se']:.4f}")
            else:
                print(f"  {label:28s} n={v['n']:6d}  mean {v['mean']:.4f}")


if __name__ == "__main__":
    main()
