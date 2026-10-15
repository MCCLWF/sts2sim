#!/usr/bin/env python3
"""What kind of decisions did the rollout player change? (nosl_headroom --player rollout output)

  tools/nosl/rollout_changes.py ROLLOUT.jsonl [--json OUT.json]

Every changed decision is the policy's own action and the action the rollouts
preferred, with what the player saw (HP, block, energy, the damage the
enemies show). They are grouped by the kind of action on both sides (a card's
type for a played card); "gain" is the rollouts' mean worth difference in the
tool's units (surviving the combat = 3 by default, the whole HP bar = 1), so a
group's summed gain says where the policy's losses come from, not just how
often it was overruled.
"""
from __future__ import annotations

import argparse
import collections
import json


def kind(text):
    parts = text.split()
    if parts[0] != "PlayCard":
        return parts[0]
    return parts[-2] if parts[-1].startswith("@") else parts[-1]


def subject(text):
    parts = text.split()
    return parts[1] if parts[0] in ("PlayCard", "UsePotion") and len(parts) > 1 else parts[0]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("path")
    p.add_argument("--json", default="")
    args = p.parse_args()
    rows = [json.loads(line) for line in open(args.path, encoding="utf-8") if line.strip()]
    played = [r for r in rows if "offline" in r and not r["offline"]["error"]]
    changes = [c for r in played for c in r["offline"].get("changes", [])]
    groups = collections.defaultdict(lambda: [0, 0.0])
    for c in changes:
        a, b = kind(c["policy"]), kind(c["taken"])
        if a == b and subject(c["policy"]) == subject(c["taken"]):
            b += ", same card, other target"
        groups[f"{a} -> {b}"][0] += 1
        groups[f"{a} -> {b}"][1] += c["gain"]
    out = {"combats": len(played), "rolled_out": sum(r["offline"]["solves"] for r in played), "changed": len(changes),
           "changed_with_unblocked_incoming": sum(c["incoming"] > c["block"] for c in changes),
           "policy_played_rollouts_ended_turn": sum(kind(c["taken"]) == "EndTurn" for c in changes),
           "policy_ended_turn_rollouts_played": sum(kind(c["policy"]) == "EndTurn" for c in changes),
           "policy_potion_rollouts_other": sum(kind(c["policy"]) == "UsePotion" and kind(c["taken"]) != "UsePotion" for c in changes),
           "rollouts_potion_policy_other": sum(kind(c["taken"]) == "UsePotion" and kind(c["policy"]) != "UsePotion" for c in changes),
           "groups": {k: {"count": n, "gain": round(g, 3)} for k, (n, g) in sorted(groups.items(), key=lambda kv: -kv[1][1])}}
    print(f"combats {out['combats']}  decisions rolled out {out['rolled_out']}  changed {out['changed']}")
    for k, v in out["groups"].items():
        print(f"  {k:52s} {v['count']:4d}  {v['gain']:7.2f}")
    for k in ("changed_with_unblocked_incoming", "policy_played_rollouts_ended_turn", "policy_ended_turn_rollouts_played",
              "policy_potion_rollouts_other", "rollouts_potion_policy_other"):
        print(f"{k}: {out[k]}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as stream:
            json.dump(out, stream, indent=1)


if __name__ == "__main__":
    main()
