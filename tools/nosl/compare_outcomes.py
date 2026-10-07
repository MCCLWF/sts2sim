#!/usr/bin/env python3
"""Paired comparison of two whole-run evaluations on the same seeds.

  tools/nosl/compare_outcomes.py BASE.jsonl CANDIDATE.jsonl [--out FILE.json] [--samples 10000] [--rng 20261006]

Both files hold one line per attempt (nosl.strategy_q evaluate, or any
evaluation with seed/status/act/floor). Every attempt of both files must be an
original victory or death on the same seed set; anything else is an error
here, because a fault is not a death. Reports each policy's real outcomes and
the paired differences with a seed-level bootstrap 95% interval.
"""
from __future__ import annotations

import argparse
import json
import math

import numpy as np

VICTORY, DEATH = 1, 2


def wilson(successes, n, z=1.96):
    p = successes / n
    centre = (p + z * z / (2 * n)) / (1 + z * z / n)
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / (1 + z * z / n)
    return [max(0.0, centre - half), min(1.0, centre + half)]


def read(path):
    rows = {}
    for line in open(path, encoding="utf-8"):
        if not line.strip():
            continue
        r = json.loads(line)
        if "seed" not in r or r.get("summary"):
            continue
        if r["seed"] in rows:
            raise SystemExit(f"{path}: seed {r['seed']} appears twice")
        rows[r["seed"]] = r
    return rows


def facts(r):
    if r["status"] not in (VICTORY, DEATH):
        raise SystemExit(f"{r['seed']} ended as {r.get('end')}: not a real terminal")
    win = r["status"] == VICTORY
    return {"win": float(win), "act2": float(win or r["act"] >= 1), "act3": float(win or r["act"] >= 2),
            "floor": float(r["floor"]), "target": r["floor"] / 50.0 + 0.5 * r["act"] + 2.0 * win}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("base")
    p.add_argument("candidate")
    p.add_argument("--out", default="")
    p.add_argument("--samples", type=int, default=10000)
    p.add_argument("--rng", type=int, default=20261006)
    args = p.parse_args()
    base, cand = read(args.base), read(args.candidate)
    if set(base) != set(cand):
        raise SystemExit("the two evaluations are not on the same seeds")
    seeds = sorted(base)
    a = [facts(base[s]) for s in seeds]
    b = [facts(cand[s]) for s in seeds]
    n = len(seeds)
    rng = np.random.default_rng(args.rng)
    draws = rng.integers(0, n, size=(args.samples, n))
    result = {"seeds": n, "base": args.base, "candidate": args.candidate, "policies": {}, "paired": {}}
    for name, rows in (("base", a), ("candidate", b)):
        wins = int(sum(r["win"] for r in rows))
        result["policies"][name] = {"attempts": n, "victories": wins, "win_rate": wins / n, "win_rate_ci95": wilson(wins, n),
                                    "act2_rate": float(np.mean([r["act2"] for r in rows])),
                                    "act3_rate": float(np.mean([r["act3"] for r in rows])),
                                    "floor_mean": float(np.mean([r["floor"] for r in rows])),
                                    "target_mean": float(np.mean([r["target"] for r in rows]))}
    for key in ("win", "act2", "act3", "floor", "target"):
        diff = np.array([y[key] - x[key] for x, y in zip(a, b)])
        means = diff[draws].mean(axis=1)
        result["paired"][key] = {"difference": float(diff.mean()),
                                 "ci95": [float(np.percentile(means, 2.5)), float(np.percentile(means, 97.5))]}
    text = json.dumps(result, indent=1)
    if args.out:
        open(args.out, "w", encoding="utf-8").write(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
