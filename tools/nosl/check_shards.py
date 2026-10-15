#!/usr/bin/env python3
"""Are a collection's shards what a fresh replay of its recorded runs gives?

  tools/nosl/check_shards.py --data DIR [--runs 400] [--workers 3]

Replays the first `--runs` recorded runs of DIR (nosl.replay) and compares
every decision outside combat with the same decision in DIR/shards: the
global row, the token and action rows, the action taken, the exploratory
flag and the run's recorded end. A training set that does not hold what was
observed makes every result trained on it meaningless; this is the check
that it does.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
from nosl.replay import replay  # noqa: E402
from nosl.shards import load_shard, pad, shard_paths  # noqa: E402
from nosl.strategy_q import outcome_target, read_runs  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", required=True)
    p.add_argument("--runs", type=int, default=400)
    p.add_argument("--workers", type=int, default=3)
    args = p.parse_args()
    runs, _ = read_runs([args.data])
    subset = runs[:args.runs]
    for r in subset:
        r["explored_set"] = set(r.get("explored", []))
        r["target"] = outcome_target(r["floor"], r["act"], r["status"])
    fresh = {}
    for chunk in replay(subset, workers=args.workers, order_seed=3):
        for i in range(len(chunk["choice"])):
            nt, na = (int(x) for x in chunk["counts"][i])
            fresh[(int(chunk["run"][i]), int(chunk["step"][i]))] = (
                chunk["global"][i].tobytes(), chunk["tokens"][i, :nt].tobytes(), chunk["actions"][i, :na].tobytes(),
                int(chunk["choice"][i]), bool(chunk["explored"][i]), int(chunk["floor"][i]), int(chunk["run_act"][i]),
                int(chunk["status"][i]))
    seen = same = 0
    different = []
    for _, path in shard_paths([args.data]):
        data = load_shard(path)
        rows = np.flatnonzero(data["run"] < len(subset))
        if not len(rows):
            continue
        tokens = pad(data["tok"], data["tok_at"], data["nt"], rows)
        actions = pad(data["actions"], data["act_at"], data["na"], rows)
        for j, row in enumerate(rows):
            nt, na = int(data["nt"][row]), int(data["na"][row])
            key = (int(data["run"][row]), int(data["step"][row]))
            stored = (data["g"][row].tobytes(), tokens[j, :nt].astype(np.int32).tobytes(),
                      actions[j, :na].astype(np.int32).tobytes(), int(data["choice"][row]), bool(data["explored"][row]),
                      int(data["floor"][row]), int(data["run_act"][row]), int(data["status"][row]))
            seen += 1
            if fresh.get(key) == stored:
                same += 1
            elif len(different) < 5:
                different.append({"run": key[0], "step": key[1], "missing": key not in fresh,
                                  "fields_equal": [a == b for a, b in zip(fresh.get(key, ()), stored)]})
    result = {"data": args.data, "runs": len(subset), "replayed_decisions": len(fresh), "shard_decisions": seen,
              "identical": same, "first_differences": different}
    print(json.dumps(result))
    return 0 if same == seen == len(fresh) else 1


if __name__ == "__main__":
    sys.exit(main())
