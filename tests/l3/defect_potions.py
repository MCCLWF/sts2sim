#!/usr/bin/env python3
"""Compare Defect potion effects through Sts2Oracle and the combat CLI."""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "l3"))
import l3_combat  # noqa: E402

DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = ROOT / "tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll"
POTIONS = ("ESSENCE_OF_DARKNESS", "FOCUS_POTION", "POTION_OF_CAPACITY")


def replay(cli: str, run: dict, potion: str, deck: str) -> tuple[list, list, dict | None, dict | None]:
    begin = run["begin"]
    command = [cli, "combat", "--seed", begin["seed"], "--character", begin["character"],
               "--ascension", "0", "--coord", "%d,%d" % tuple(run["coord"]),
               "--actions", ",".join(run["actions"]) or "-", "--deck", deck,
               "--potion", potion]
    if run.get("encounter"):
        command += ["--encounter", run["encounter"]]
    process = subprocess.run(command, capture_output=True, text=True)
    states, decisions, end, error = [], [], None, None
    for line in process.stdout.splitlines():
        event = json.loads(line)
        if event["event"] == "state":
            states.append(event["state"])
        elif event["event"] == "decision":
            decisions.append(event)
        elif event["event"] == "end":
            end = event
        elif event["event"] == "error":
            error = event
    if process.returncode not in (0, 1):
        error = {"message": "exit %d: %s" % (process.returncode, process.stderr.strip()[-500:])}
    return states, decisions, end, error


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed-prefix", default="DFPOT1110")
    parser.add_argument("--policy-seeds", default="1,2")
    parser.add_argument("--encounter", default="SLIMES_WEAK")
    parser.add_argument("--deck", default="CLAW,CLAW,CLAW,CLAW,CLAW")
    parser.add_argument("--max-actions", type=int, default=240)
    parser.add_argument("--cli", default=str(ROOT / "build/sts2sim_cli"))
    parser.add_argument("--raw-jsonl", default="/tmp/defect-v0110-potions-oracle-raw.jsonl")
    parser.add_argument("--results", default=str(ROOT / "docs/audits/defect-v0110-potions-oracle-results.txt"))
    args = parser.parse_args()

    raw_path, result_path = Path(args.raw_jsonl), Path(args.results)
    raw_path.parent.mkdir(parents=True, exist_ok=True)
    result_path.parent.mkdir(parents=True, exist_ok=True)
    total_states = matched_states = passed = 0
    failed = False
    with raw_path.open("w", encoding="utf-8") as raw, result_path.open("w", encoding="utf-8") as results:
        for index, potion in enumerate(POTIONS):
            seed = f"{args.seed_prefix}{index:02d}"
            oracle_command = [DOTNET, str(ORACLE), "combat", "--seeds", seed, "--characters", "DEFECT",
                              "--policy-seeds", args.policy_seeds, "--max-actions", str(args.max_actions),
                              "--encounter", args.encounter, "--deck", args.deck, "--potion", potion]
            process = subprocess.run(oracle_command, capture_output=True, text=True)
            if process.returncode:
                raise RuntimeError("Oracle failed for %s: %s" % (potion, process.stderr.strip()[-800:]))
            lines = process.stdout.splitlines()
            raw.writelines(line + "\n" for line in lines)
            runs = l3_combat.parse_runs(lines)
            case_states = case_matches = 0
            problem = ""
            for run in runs:
                states, decisions, end, error = replay(args.cli, run, potion, args.deck)
                case_states += len(run["states"])
                for state_index, expected in enumerate(run["states"]):
                    if state_index >= len(states):
                        problem = "sim stopped at state %d: %s" % (
                            state_index, error.get("message", "no state") if error else "no state"
                        )
                        break
                    diff = l3_combat.first_diff(expected, states[state_index])
                    if diff:
                        problem = "state %d %s oracle=%s sim=%s" % (
                            state_index, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                        )
                        break
                    case_matches += 1
                if problem:
                    break
                if len(decisions) != len(run["decisions"]):
                    problem = "selection count oracle=%d sim=%d" % (len(run["decisions"]), len(decisions))
                    break
                if run["end"] is not None:
                    if end is None:
                        problem = "sim did not reach Oracle end"
                        break
                    diff = l3_combat.first_diff(run["end"], end)
                    if diff:
                        problem = "end %s oracle=%s sim=%s" % (diff[0], json.dumps(diff[1]), json.dumps(diff[2]))
                        break
            total_states += case_states
            matched_states += case_matches
            passed += not bool(problem)
            failed |= bool(problem)
            result = "DIFF" if problem else "PASS"
            summary = "%s %s policies=%s states=%d/%d%s" % (
                result, potion, args.policy_seeds, case_matches, case_states,
                (" " + problem) if problem else ""
            )
            print(summary, flush=True)
            results.write(summary + "\n")
        summary = "Defect potions: %d/%d matched; states=%d/%d" % (
            passed, len(POTIONS), matched_states, total_states
        )
        print(summary)
        results.write(summary + "\n")
    return 1 if failed or matched_states != total_states else 0


if __name__ == "__main__":
    sys.exit(main())
