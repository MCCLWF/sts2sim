#!/usr/bin/env python3
"""A value table for strategic decisions from the randomized comparisons of exploration_effects.py.

  tools/nosl/card_table.py --effects A.json[,B.json] --out TABLE.json [--kinds choice,rest,map,event,treasure]
                           [--shrink 300] [--min-context 600]

Within a context (nosl.contexts: one kind of decision offering one set of things) the comparison gives, for every
action label, the mean outcome of the runs where that was the uniformly random exploratory action. The table's value
of a label is that mean less the mean of the other exploratory actions in the same context; for a choice that names a
card it is the mean less that of the runs where the card was offered and another was picked. Several files are
pooled by their counts. A value is shrunk toward zero by n / (n + shrink): a label seen a few hundred times says
little. Contexts with fewer than --min-context exploratory decisions are left out.

The table is a context-free, low-variance estimate: it ignores everything about the run except what the context
names. nosl.strategy_q --table uses it over the value network's rule. It can only be as good as the policy that
collected the data let it see: the values are differences under that policy's play.
"""
from __future__ import annotations

import argparse
import collections
import json


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--effects", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--kinds", default="choice", help="context kinds to keep: choice, rest, map, event, treasure, rewards, shop")
    p.add_argument("--shrink", type=float, default=300.0)
    p.add_argument("--min-context", type=int, default=600)
    args = p.parse_args()
    kinds = tuple(args.kinds.split(","))
    files = [json.load(open(path, encoding="utf-8")) for path in args.effects.split(",")]
    # pooled sums per (context, label): n, sum of outcomes; and for cards the "offered, not taken" runs
    taken = collections.defaultdict(lambda: [0, 0.0])
    passed = collections.defaultdict(lambda: [0, 0.0])
    for data in files:
        for context, block in data["contexts"].items():
            if not context.startswith(kinds):
                continue
            for label, row in block["labels"].items():
                t = taken[(context, label)]
                t[0] += row["n"]
                t[1] += row["n"] * row["mean"]
                if "minus_offered_not_taken" in row:
                    o = passed[(context, label)]
                    o[0] += row["offered_not_taken_n"]
                    o[1] += row["offered_not_taken_n"] * (row["mean"] - row["minus_offered_not_taken"])
    totals = collections.defaultdict(lambda: [0, 0.0])
    for (context, label), (n, s) in taken.items():
        totals[context][0] += n
        totals[context][1] += s
    table = collections.defaultdict(dict)
    for (context, label), (n, s) in sorted(taken.items()):
        total_n, total_s = totals[context]
        if total_n < args.min_context or n == 0:
            continue
        if (context, label) in passed and passed[(context, label)][0] > 0:
            other = passed[(context, label)][1] / passed[(context, label)][0]
        elif total_n > n:
            other = (total_s - s) / (total_n - n)
        else:
            continue
        table[context][label] = round((s / n - other) * n / (n + args.shrink), 5)
    with open(args.out, "w", encoding="utf-8") as stream:
        json.dump({"shrink": args.shrink, "min_context": args.min_context, "kinds": list(kinds), "sources": args.effects.split(","),
                   "contexts": table}, stream, indent=1)
    by_kind = collections.Counter(context.split()[0] for context in table)
    print(f"{len(table)} contexts {dict(by_kind)}, {sum(len(v) for v in table.values())} labels")


if __name__ == "__main__":
    main()
