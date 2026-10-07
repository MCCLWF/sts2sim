#!/usr/bin/env python3
"""Native full-run regressions for nested continuations and turn history."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = (
    ("cheese-defect", "F87A20DE9374C77", "DEFECT", 0, 144745002),
    ("cheese-silent", "F87A20DE9374C77", "SILENT", 10, 783630005),
    ("rest-rewards", "F4C2E1F1DA91DCC", "SILENT", 10, 1041113654),
    ("history-course", "FF7429B102A14F2", "IRONCLAD", 0, 892152143),
)


def native_contract(path, case):
    events = [json.loads(line) for line in path.read_text().splitlines() if line.startswith("{")]
    decisions = [event for event in events if event.get("event") == "decision"]
    actions = [event for event in events if event.get("event") == "action"]
    if len(decisions) != len(actions):
        raise ValueError("regression native trace is not fully paired")
    observed = 0
    for i, (decision, action) in enumerate(zip(decisions, actions)):
        relics = {relic["id"] for relic in decision.get("run", {}).get("relics", [])}
        if case.startswith("cheese"):
            if (decision.get("kind") != "event" or
                decision.get("options", {}).get("event") != "ROOM_FULL_OF_CHEESE" or
                action.get("index") != 0 or "BING_BONG" not in relics):
                continue
            choice, selected = decisions[i + 1], actions[i + 1]
            if choice.get("kind") != "card_select" or len(selected.get("indices", [])) != 2:
                raise ValueError("Gorge did not select two native candidates")
            models = [choice["options"]["cards"][index] for index in selected["indices"]]
            if models[0] == models[1]:
                continue  # Distinct identities are needed to expose add/hook ordering.
            before, after = choice["run"]["deck"], decisions[i + 2]["run"]["deck"]
            if after != before + [models[0], models[0], models[1], models[1]]:
                raise ValueError("native Gorge/BingBong did not expose sequential-add order")
            observed += 1
        elif case == "rest-rewards":
            if (decision.get("kind") != "rest" or
                not {"MINIATURE_TENT", "TINY_MAILBOX"} <= relics or
                decision["options"][action["index"]]["id"] != "HEAL"):
                continue
            j = i + 1
            while j < len(decisions) and decisions[j].get("kind") == "rewards":
                j += 1
            if i + 1 < j < len(decisions) and actions[j - 1].get("proceed") and decisions[j].get("kind") == "map":
                observed += 1
        else:
            if decision.get("kind") != "combat":
                continue
            combat = decision["combat"]
            player = combat["player"]
            powers = {power[0] for power in player["powers"]}
            if (combat["turn"] != 15 or combat["test_subject_kills"] != 2 or
                not {"INFERNO_POWER", "HELLRAISER_POWER"} <= powers or
                "HISTORY_COURSE" not in {r["id"] for r in player["relics"]} or
                action.get("type") != "end_turn"):
                continue
            successor = decisions[i + 1]["combat"]
            if (successor["turn"] != 16 or combat["enemies"][0]["hp"] != 293 or
                successor["enemies"][0]["hp"] != 252 or
                combat["rng"]["CombatTargets"] != 45 or successor["rng"]["CombatTargets"] != 47 or
                "TWIN_STRIKE" not in successor["player"]["discard"]):
                raise ValueError("native History Course/Hellraiser overlap was not executed")
            observed += 1
    if not observed:
        raise ValueError(f"{case}: native route did not execute the required regression")
    return observed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--cli", type=Path)
    parser.add_argument("--artifacts-dir", type=Path)
    args = parser.parse_args()
    cli = args.cli or args.repo / "build/sts2sim_cli"
    with tempfile.TemporaryDirectory(prefix="sts2-continuation-regressions-") as temporary:
        output = args.artifacts_dir or Path(temporary)
        output.mkdir(parents=True, exist_ok=True)
        matched = total = 0
        for name, seed, character, ascension, policy in CASES:
            artifacts = output / name
            artifacts.mkdir(exist_ok=True)
            trace, result = artifacts / "native.jsonl", artifacts / "result.json"
            command = [sys.executable, str(args.repo / "tests/l4/l4_run.py"),
                       "--seeds", seed, "--characters", character, "--ascension", str(ascension),
                       "--policy-seeds", str(policy), "--policy-version", "coverage-v3",
                       "--hp", "10000", "--max-decisions", "10000", "--stop", "victory",
                       "--verify-snapshots", "--require-complete", "--cli", str(cli),
                       "--trace-output", str(trace), "--artifacts-dir", str(artifacts),
                       "--result-json", str(result)]
            subprocess.run(command, check=True)
            coverage = native_contract(trace, name)
            stats = json.loads(result.read_text())["summary"]
            matched += stats["matched"]
            total += stats["total"]
            print(f"{name}: {coverage} native regression executions", flush=True)
        print(f"Continuation regressions: {len(CASES)} native full runs; {matched}/{total} decisions identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
