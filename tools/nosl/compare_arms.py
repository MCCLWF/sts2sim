#!/usr/bin/env python3
"""Paired comparison of whole-run evaluations on the same seeds.

  tools/nosl/compare_arms.py BASE.jsonl ARM.jsonl [ARM.jsonl ...]

Each file is the row output of nosl.strategy_q evaluate. For every arm: the outcome target, how far the runs got
(act 2, act 3, floors), how the two bosses went (how many runs reached the boss floor, how many of those got past),
every kind of end, and the paired difference to BASE with a 95% interval. Runs are matched by seed; a file is
written in the order runs finish, so nothing here depends on row order.
"""
import json
import math
import sys
from pathlib import Path

BOSS_FLOOR = (17, 33)


def target(r):
    return r["floor"] / 50 + 0.5 * r["act"] + (2 if r["end"] == "victory" else 0)


def load(path):
    return {r["seed"]: r for r in map(json.loads, Path(path).read_text().splitlines())}


def paired(a, b, f):
    d = [f(a[s]) - f(b[s]) for s in a if s in b]
    n = len(d)
    m = sum(d) / n
    se = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1) / n)
    return m, m - 1.96 * se, m + 1.96 * se


def describe(rows):
    n = len(rows)
    ends = {}
    for r in rows.values():
        ends[r["end"]] = ends.get(r["end"], 0) + 1
    out = {"runs": n, "ends": ends, "target": sum(map(target, rows.values())) / n,
           "floor": sum(r["floor"] for r in rows.values()) / n,
           "act2": sum(r["act"] >= 1 for r in rows.values()), "act3": sum(r["act"] >= 2 for r in rows.values())}
    for act, floor in enumerate(BOSS_FLOOR):
        reached = sum(r["act"] > act or (r["act"] == act and r["floor"] >= floor) for r in rows.values())
        passed = sum(r["act"] > act for r in rows.values())
        out[f"boss{act + 1}"] = (reached, passed)
    return out


def main():
    base = load(sys.argv[1])
    for path in sys.argv[1:]:
        rows = load(path)
        d = describe(rows)
        line = (f"{Path(path).stem:22s} runs {d['runs']}  target {d['target']:.4f}  floor {d['floor']:.2f}  act2 {d['act2']} "
                f"({d['act2'] / d['runs']:.1%})  act3 {d['act3']}  boss1 {d['boss1'][1]}/{d['boss1'][0]} "
                f"({d['boss1'][1] / max(1, d['boss1'][0]):.1%})  boss2 {d['boss2'][1]}/{d['boss2'][0]}  ends {d['ends']}")
        print(line)
        if path != sys.argv[1]:
            for name, f in (("target", target), ("act2", lambda r: float(r["act"] >= 1)), ("act3", lambda r: float(r["act"] >= 2)),
                            ("floor", lambda r: float(r["floor"]))):
                m, lo, hi = paired(rows, base, f)
                print(f"      {name:7s} {m:+.4f} [{lo:+.4f}, {hi:+.4f}]", end="")
            print()


if __name__ == "__main__":
    main()
