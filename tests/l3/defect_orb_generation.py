#!/usr/bin/env python3
"""Oracle-check random-orb candidate order and generated-Status hooks."""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "l3"))
import l3_combat  # noqa: E402


def compare(lines: list[str], cli: str, ascension: int, deck: str,
            potion: str | None, starting_energy: int) -> tuple[int, int, str]:
    runs = l3_combat.parse_runs(lines)
    matched = total = 0
    for run in runs:
        states, decisions, end, error = l3_combat.replay(
            cli, run, ascension, deck, potion=potion,
            starting_energy=starting_energy,
        )
        total += len(run["states"])
        for i, expected in enumerate(run["states"]):
            if i >= len(states):
                return matched, total, "sim stopped at state %d: %s" % (
                    i, error.get("message", "no state") if error else "no state"
                )
            diff = l3_combat.first_diff(expected, states[i])
            if diff:
                return matched, total, "state %d %s oracle=%s sim=%s" % (
                    i, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )
            matched += 1
        if len(decisions) != len(run["decisions"]):
            return matched, total, "card-select decisions oracle=%d sim=%d" % (
                len(run["decisions"]), len(decisions)
            )
        for i, (expected, actual) in enumerate(zip(run["decisions"], decisions)):
            expected = json.loads(json.dumps(expected))
            actual = json.loads(json.dumps(actual))
            expected["options"].pop("source", None)
            actual["options"].pop("source", None)
            diff = l3_combat.first_diff(expected, actual)
            if diff:
                return matched, total, "decision %d %s oracle=%s sim=%s" % (
                    i, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )
        if run["end"] is not None:
            if end is None:
                return matched, total, "sim did not reach Oracle end"
            diff = l3_combat.first_diff(run["end"], end)
            if diff:
                return matched, total, "end %s oracle=%s sim=%s" % (
                    diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )
    return matched, total, ""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed-prefix", default="DFORB1110")
    parser.add_argument("--policy-seeds", default="1,2")
    parser.add_argument("--max-actions", type=int, default=240)
    parser.add_argument("--cli", default=str(ROOT / "build/sts2sim_cli"))
    parser.add_argument("--raw-jsonl", default="/tmp/defect-v0110-orb-generation-raw.jsonl")
    parser.add_argument("--results", default=str(ROOT / "docs/audits/defect-v0110-orb-generation-results.txt"))
    args = parser.parse_args()

    cases = [("DISCOVERY", "DISCOVERY", "DEFECT",
              "DISCOVERY,DISCOVERY,DISCOVERY,DISCOVERY,DISCOVERY", None, 20)]
    # Five Chaos cards plus two capacity slots expose only generated orb IDs,
    # so even LIGHTNING is checked as an RNG result rather than the starter relic.
    for i in range(4):
        cases.append(("CHAOS_%02d" % i, "CHAOS%02d" % i, "IRONCLAD",
                      "CHAOS,CHAOS,CHAOS,CHAOS,CHAOS", "POTION_OF_CAPACITY", 20))
    # Overclock generates Burn Status cards. TrashToTreasure's generation hook
    # channels a random orb for each Burn after the power is active.
    cases.append(("TRASH_TO_TREASURE_STATUS", "TTT00", "DEFECT",
                  "TRASH_TO_TREASURE,OVERCLOCK,OVERCLOCK,OVERCLOCK,OVERCLOCK",
                  None, 20))

    raw_path, result_path = Path(args.raw_jsonl), Path(args.results)
    raw_path.parent.mkdir(parents=True, exist_ok=True)
    result_path.parent.mkdir(parents=True, exist_ok=True)
    candidates: set[str] = set()
    total_states = matched_states = total_runs = passed = 0
    played: dict[str, int] = {}
    with raw_path.open("w", encoding="utf-8") as raw, result_path.open("w", encoding="utf-8") as results:
        for index, (label, seed, character, deck, potion, energy) in enumerate(cases):
            policy_seeds = "1,2,3" if label in {"DISCOVERY", "TRASH_TO_TREASURE_STATUS"} else args.policy_seeds
            oracle_args = argparse.Namespace(
                seeds=args.seed_prefix + seed,
                characters=character,
                policy_seeds=policy_seeds,
                max_actions=args.max_actions,
                ascension=0,
                encounter="BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER",
                deck=deck,
                relics=None,
                potion=potion,
                starting_energy=energy,
                starting_stars=None,
                take_special_rewards=False,
            )
            lines = l3_combat.run_oracle(oracle_args)
            raw.writelines(line + "\n" for line in lines)
            runs = l3_combat.parse_runs(lines)
            case_candidates: set[str] = set()
            case_played: dict[str, int] = {}
            case_choices = 0
            status_hook_seen = False
            for line in lines:
                if not line.startswith("{"):
                    continue
                event = json.loads(line)
                if event.get("event") == "action" and event.get("type") == "play":
                    card = event.get("card")
                    case_played[card] = case_played.get(card, 0) + 1
                elif event.get("event") == "state":
                    player = event["state"]["player"]
                    case_candidates.update(player["orbs"])
                    if any(power[0] == "TRASH_TO_TREASURE_POWER" for power in player["powers"]):
                        status_hook_seen |= any("BURN" in player[pile] for pile in ("hand", "draw", "discard", "exhaust", "play"))
                elif event.get("event") == "decision" and event.get("kind") == "card_select":
                    case_choices += 1
            matched, total, problem = compare(lines, args.cli, 0, deck, potion, energy)
            total_states += total
            matched_states += matched
            total_runs += len(runs)
            for key, count in case_played.items():
                played[key] = played.get(key, 0) + count
            if label.startswith("CHAOS"):
                candidates.update(case_candidates)
            if label == "DISCOVERY" and not case_choices:
                problem = problem or "Discovery did not reach a card-select choice"
            if label == "DISCOVERY" and any(
                len(json.loads(line)["options"].get("cards", [])) != 3
                for line in lines if line.startswith("{")
                and json.loads(line).get("event") == "decision"
                and json.loads(line).get("kind") == "card_select"
            ):
                problem = problem or "Discovery did not offer three distinct candidates"
            if label == "TRASH_TO_TREASURE_STATUS" and not status_hook_seen:
                problem = problem or "generated Burn was not observed while TrashToTreasurePower was active"
            status = "DIFF" if problem else "PASS"
            summary = "%s %s policies=%s states=%d/%d actions=%d%s%s" % (
                status, label, oracle_args.policy_seeds, matched, total,
                sum(case_played.values()), (" choices=" + str(case_choices) if label == "DISCOVERY" else ""),
                (" " + problem) if problem else "",
            )
            passed += not bool(problem)
            print(summary, flush=True)
            results.write(summary + "\n")

    expected = {"LIGHTNING_ORB", "FROST_ORB", "DARK_ORB", "PLASMA_ORB", "GLASS_ORB"}
    missing = sorted(expected - candidates)
    missing_triggers = []
    if not played.get("CHAOS"):
        missing_triggers.append("Chaos was never played")
    if not played.get("TRASH_TO_TREASURE") or not played.get("OVERCLOCK"):
        missing_triggers.append("TrashToTreasure/Overclock status-generation path was not played")
    summary = "Defect orb generation: %d/%d cases matched; states=%d/%d; runs=%d; candidate_orbs=%s%s" % (
        passed, len(cases), matched_states, total_states, total_runs,
        ",".join(sorted(candidates)),
        (" missing=" + ",".join(missing)) if missing else "",
    )
    print(summary)
    with result_path.open("a", encoding="utf-8") as results:
        results.write(summary + "\n")
    return 1 if passed != len(cases) or matched_states != total_states or missing or missing_triggers else 0


if __name__ == "__main__":
    sys.exit(main())
