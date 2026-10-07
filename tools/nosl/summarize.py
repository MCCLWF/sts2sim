#!/usr/bin/env python3
"""Summarize nosl_run outputs: every attempt counted, grouped by end type.

  python3 tools/nosl/summarize.py artifacts/nosl-q1/a-random.jsonl [more.jsonl ...]
"""
import collections
import json
import sys


def quantile(values, p):
    if not values:
        return None
    values = sorted(values)
    return values[min(len(values) - 1, int(p * len(values)))]


def main(paths):
    for path in paths:
        rows = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
        if not rows:
            print(path, "empty")
            continue
        ends = collections.Counter(r["end"] for r in rows)
        acts = collections.Counter(r["act"] + 1 for r in rows)
        floors = [r["floor"] for r in rows]
        steps = sum(r["steps"] for r in rows)
        print(f"{path}: policy={rows[0]['policy']} class={rows[0]['policy_class']} A{rows[0]['ascension']} attempts={len(rows)}")
        print(f"  ends={dict(ends)} ended_in_act={dict(sorted(acts.items()))}")
        print(f"  floor min/median/p90/max = {min(floors)}/{quantile(floors, .5)}/{quantile(floors, .9)}/{max(floors)}")
        ms = {k: sum(r[k] for r in rows) for k in ("ms_total", "ms_legal", "ms_step", "ms_policy")}
        print(f"  decisions={steps} thread-ms total={ms['ms_total']:.0f} legal={ms['ms_legal']:.0f} "
              f"step={ms['ms_step']:.0f} policy={ms['ms_policy']:.0f} "
              f"other(reset etc.)={ms['ms_total'] - ms['ms_legal'] - ms['ms_step'] - ms['ms_policy']:.0f}")
        kinds = collections.Counter()
        for r in rows:
            kinds.update(r["steps_by_kind"])
        print(f"  decisions by kind={dict(kinds.most_common())}")
        for (end, message), n in collections.Counter(
                (r["end"], r.get("message", "")[:100]) for r in rows if r["end"] not in ("death", "victory")).most_common(8):
            print(f"  non-terminal: {n} x {end}: {message}")
        deaths = collections.Counter(r["last_encounter"] for r in rows if r["end"] == "death")
        print(f"  death encounters: {deaths.most_common(8)}")
        solves = [s for r in rows for s in r.get("teacher", {}).get("solves", [])]
        if solves:
            t = [s["ms"] for s in solves]
            print(f"  teacher: solves={len(solves)} won={sum(s['won'] for s in solves)} "
                  f"ms median/p90/max={quantile(t, .5):.0f}/{quantile(t, .9):.0f}/{max(t):.0f} "
                  f"stops={dict(collections.Counter(s['stop'] for s in solves))} "
                  f"route_illegal={sum(r['teacher']['route_illegal'] for r in rows if 'teacher' in r)} "
                  f"fallback_decisions={sum(r['teacher']['fallback_decisions'] for r in rows if 'teacher' in r)} "
                  f"errors={dict(collections.Counter(k for r in rows for k in r.get('teacher', {}).get('errors', {})))}")
            combats = [c for r in rows for c in r.get("combat_log", [])]
            lost = [c["hp_before"] - c["hp_after"] for c in combats if c["survived"]]
            if lost:
                print(f"  combats={len(combats)} survived={len(lost)} hp lost per survived combat mean={sum(lost)/len(lost):.1f}")


if __name__ == "__main__":
    main(sys.argv[1:])
