#!/usr/bin/env python3
"""Compare all eight v0.111.0 Defect relic paths against Sts2Oracle."""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "l3"))
import l3_combat  # noqa: E402

# The Defect's Cracked Core is its actual starting relic. Other cases procure
# exactly one pool relic. Separate decks target each relic's relevant trigger.
CASES = (
    ("CRACKED_CORE", None, "METEOR_STRIKE," * 9 + "METEOR_STRIKE", 1, "CULTISTS_NORMAL"),
    ("DATA_DISK", "DATA_DISK", "CLAW,CLAW,CLAW,CLAW,CLAW", 0, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
    ("EMOTION_CHIP", "EMOTION_CHIP", "METEOR_STRIKE," * 9 + "METEOR_STRIKE", 1, "CULTISTS_NORMAL"),
    ("GOLD_PLATED_CABLES", "GOLD_PLATED_CABLES", "BALL_LIGHTNING," * 9 + "BALL_LIGHTNING", 20, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
    ("POWER_CELL", "POWER_CELL", "CLAW," * 9 + "CLAW", 0, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
    ("METRONOME", "METRONOME", "VOLTAIC,VOLTAIC,BALL_LIGHTNING,BALL_LIGHTNING,BALL_LIGHTNING", 20, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
    ("RUNIC_CAPACITOR", "RUNIC_CAPACITOR", "BALL_LIGHTNING," * 9 + "BALL_LIGHTNING", 20, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
    ("SYMBIOTIC_VIRUS", "SYMBIOTIC_VIRUS", "BALL_LIGHTNING," * 9 + "BALL_LIGHTNING", 20, "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"),
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed-prefix", default="DFREL1110")
    parser.add_argument("--policy-seeds", default="1,2")
    parser.add_argument("--max-actions", type=int, default=240)
    parser.add_argument("--cli", default=str(ROOT / "build" / "sts2sim_cli"))
    parser.add_argument("--raw-jsonl", default="/tmp/defect-v0110-relic-oracle-raw.jsonl")
    parser.add_argument("--results", default=str(ROOT / "docs/audits/defect-v0110-relic-oracle-results.txt"))
    args = parser.parse_args()

    raw_path, result_path = Path(args.raw_jsonl), Path(args.results)
    raw_path.parent.mkdir(parents=True, exist_ok=True)
    result_path.parent.mkdir(parents=True, exist_ok=True)
    results: list[str] = []
    raw_path.write_text("")
    for index, (label, relic, deck, starting_energy, encounter) in enumerate(CASES):
        seed = f"{args.seed_prefix}_{index:02d}"
        oracle_args = argparse.Namespace(
            seeds=seed, characters="DEFECT", policy_seeds=args.policy_seeds,
            max_actions=args.max_actions, ascension=0, encounter=encounter,
            deck=deck, relics=relic, potion=None, starting_energy=starting_energy,
            starting_stars=None, take_special_rewards=False,
        )
        lines = l3_combat.run_oracle(oracle_args)
        with raw_path.open("a", encoding="utf-8") as raw:
            raw.writelines(line + "\n" for line in lines)
        runs = l3_combat.parse_runs(lines)
        matched_states = total_states = matched_choices = total_choices = 0
        detail = ""
        for run in runs:
            states, decisions, end, error = l3_combat.replay(
                args.cli, run, 0, deck, relic, starting_energy=starting_energy,
            )
            total_states += len(run["states"])
            total_choices += len(run["decisions"])
            for state_index, expected in enumerate(run["states"]):
                if state_index >= len(states):
                    detail = "sim stopped at state %d: %s" % (
                        state_index, error.get("message", "no state") if error else "no state"
                    )
                    break
                diff = l3_combat.first_diff(expected, states[state_index])
                if diff:
                    detail = "state %d %s oracle=%s sim=%s" % (
                        state_index, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                    )
                    break
                matched_states += 1
            if detail:
                break
            if len(decisions) != len(run["decisions"]):
                detail = "card-select decisions oracle=%d sim=%d" % (
                    len(run["decisions"]), len(decisions)
                )
                break
            matched_choices += len(decisions)
            if run["end"] is not None:
                if end is None:
                    detail = "sim did not reach Oracle end"
                    break
                diff = l3_combat.first_diff(run["end"], end)
                if diff:
                    detail = "end %s oracle=%s sim=%s" % (
                        diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                    )
                    break
        status = "DIFF" if detail else "PASS"
        line = "%s %s policies=%s states=%d/%d choices=%d/%d%s" % (
            status, label, args.policy_seeds, matched_states, total_states,
            matched_choices, total_choices, (" " + detail) if detail else "",
        )
        print(line, flush=True)
        results.append(line)

    summary = "Defect relic pool: %d/%d matched" % (
        sum(line.startswith("PASS ") for line in results), len(CASES)
    )
    print(summary)
    results.append(summary)
    result_path.write_text("\n".join(results) + "\n", encoding="utf-8")
    return 1 if any(line.startswith("DIFF ") for line in results) else 0


if __name__ == "__main__":
    sys.exit(main())
