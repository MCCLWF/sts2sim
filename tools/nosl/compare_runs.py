#!/usr/bin/env python3
"""Whole-run results side by side, one row per result file.

  python3 tools/nosl/compare_runs.py artifacts/nosl-q1/a-rule0.jsonl artifacts/nosl-q2/s1/dev.jsonl ...

The unit is the run. Every attempt stays in the denominator; runs that did
not end in an original victory or death are counted separately. The interval
is a 95% bootstrap interval of the mean floor (2000 resamples, fixed seed).
"""
import collections
import json
import random
import sys


def bootstrap(values, resamples=2000, seed=1):
    rng = random.Random(seed)
    means = sorted(sum(rng.choice(values) for _ in values) / len(values) for _ in range(resamples))
    return means[int(0.025 * resamples)], means[int(0.975 * resamples)]


def main(paths):
    print("| 结果文件 | 策略 | 类别 | 尝试 | 胜利 | 死亡 | 其他结束 | 进入第二幕+ | 平均层数 | 95% 区间 | 中位/最大层 |")
    print("|---|---|---|---:|---:|---:|---:|---:|---:|---|---|")
    for path in paths:
        rows = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
        ends = collections.Counter(r["end"] for r in rows)
        floors = sorted(r["floor"] for r in rows)
        low, high = bootstrap(floors)
        other = len(rows) - ends["victory"] - ends["death"]
        seeds = {r["seed"] for r in rows}
        note = "" if len(seeds) == len(rows) else " (重复种子!)"
        print(f"| `{path}`{note} | {rows[0]['policy']} | {rows[0]['policy_class']} | {len(rows)} | {ends['victory']} | {ends['death']} "
              f"| {other} | {sum(r['act'] >= 1 for r in rows)} | {sum(floors) / len(floors):.2f} | {low:.2f}–{high:.2f} "
              f"| {floors[len(floors) // 2]}/{floors[-1]} |")
        if other:
            print(f"|  ↳ 其他结束 | {dict((k, v) for k, v in ends.items() if k not in ('victory', 'death'))} |||||||||")


if __name__ == "__main__":
    main(sys.argv[1:])
