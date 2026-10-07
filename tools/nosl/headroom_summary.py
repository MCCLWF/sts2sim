#!/usr/bin/env python3
"""Same-root combat comparison from nosl_headroom output, by act and fight type.

  tools/nosl/headroom_summary.py HEADROOM.jsonl [--json OUT.json]

Every line of the input is one combat: the recorded policy's result and the
offline player's result from the same root. Combats the offline player did
not play, or where it reported an error, are counted and left out of the
comparison. "HP lost" is over the combats both survived, so that a death is
not read as a cheap fight.
"""
from __future__ import annotations

import argparse
import json


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("path")
    p.add_argument("--json", default="")
    args = p.parse_args()
    rows = [json.loads(line) for line in open(args.path, encoding="utf-8") if line.strip()]
    played = [r for r in rows if "offline" in r]
    errors = [r for r in played if r["offline"]["error"]]
    good = [r for r in played if not r["offline"]["error"]]
    out = {"runs": len({r["seed"] for r in rows}), "combats": len(rows), "played_offline": len(played),
           "offline_errors": len(errors), "groups": {}}

    def group(label, selected):
        if not selected:
            return
        n = len(selected)
        both = [r for r in selected if r["recorded"]["survived"] and r["offline"]["survived"]]
        out["groups"][label] = {
            "combats": n,
            "recorded_win": sum(r["recorded"]["survived"] for r in selected) / n,
            "offline_win": sum(r["offline"]["survived"] for r in selected) / n,
            "only_offline_wins": sum(r["offline"]["survived"] and not r["recorded"]["survived"] for r in selected),
            "only_recorded_wins": sum(r["recorded"]["survived"] and not r["offline"]["survived"] for r in selected),
            "both_win": len(both),
            "recorded_hp_lost": sum(r["hp_before"] - r["recorded"]["hp_after"] for r in both) / max(1, len(both)),
            "offline_hp_lost": sum(r["hp_before"] - r["offline"]["hp_after"] for r in both) / max(1, len(both)),
            "recorded_potions": sum(r["recorded"]["potions"] for r in selected) / n,
            "offline_potions": sum(r["offline"]["potions"] for r in selected) / n,
            "offline_seconds": sum(r["offline"]["ms"] for r in selected) / n / 1000,
        }

    for act in sorted({r["act"] for r in good}):
        for kind in ("weak", "normal", "elite", "boss"):
            group(f"act{act + 1} {kind}", [r for r in good if r["act"] == act and r["fight"] == kind])
    group("all", good)
    print(f"runs {out['runs']}  combats {out['combats']}  played offline {out['played_offline']}  offline errors {out['offline_errors']}")
    print(f"{'group':14s} {'n':>5s}  {'win rec/off':>13s}  {'only off/rec':>12s}  {'HP lost rec/off (both win)':>27s}  {'potions rec/off':>15s}")
    for label, g in out["groups"].items():
        print(f"{label:14s} {g['combats']:5d}  {g['recorded_win']:6.1%}/{g['offline_win']:6.1%}  "
              f"{g['only_offline_wins']:5d}/{g['only_recorded_wins']:<5d}  "
              f"{g['recorded_hp_lost']:9.1f}/{g['offline_hp_lost']:<6.1f} (n={g['both_win']:4d})  "
              f"{g['recorded_potions']:6.2f}/{g['offline_potions']:<6.2f}")
    if args.json:
        open(args.json, "w", encoding="utf-8").write(json.dumps(out, indent=1) + "\n")


if __name__ == "__main__":
    main()
