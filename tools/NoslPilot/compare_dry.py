#!/usr/bin/env python3
"""A live pilot log against the pilot's run without a game (tools/nosl/nosl_pilot --out).

  python3 tools/NoslPilot/compare_dry.py LIVE/pilot.jsonl DRY.jsonl

Both must come from the same seed, weights and policy seed. Prints one JSON
line: how many actions each has, whether every action and every digest is
the same, and the first difference if there is one. Exit code 1 on a difference.
"""
import json
import sys


def actions(path):
    rows = []
    for line in open(path, encoding="utf-8"):
        row = json.loads(line)
        if "decision" in row:
            rows.append({k: row.get(k) for k in ("step", "decision", "forced", "action", "digest", "victory", "floor")})
    return rows


def main(live_path, dry_path):
    live, dry = actions(live_path), actions(dry_path)
    end = next((json.loads(l) for l in open(live_path, encoding="utf-8") if '"event":"end"' in l), {})
    first = next((i for i, (a, b) in enumerate(zip(live, dry)) if a != b), None)
    same = first is None and len(live) == len(dry)
    out = {"live_actions": len(live), "dry_actions": len(dry), "same": same, "outcome": end.get("outcome"), "reason": end.get("reason", "")[:200],
           "floor": end.get("floor"), "digest_checks": end.get("digest_checks"), "wall_s": end.get("wall_s"),
           "next_ms_median": end.get("next_ms_median"), "next_ms_p95": end.get("next_ms_p95"), "next_ms_max": end.get("next_ms_max"),
           "decisions": sorted({r["decision"] for r in live}), "action_kinds": sorted({r["action"]["kind"] for r in live if r.get("action")})}
    if first is not None:
        out["first_difference"] = {"index": first, "live": live[first], "dry": dry[first]}
    print(json.dumps(out, ensure_ascii=False))
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))
