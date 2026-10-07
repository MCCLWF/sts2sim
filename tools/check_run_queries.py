#!/usr/bin/env python3
"""Compare public run queries to original-game OracleRun facts, strictly.

This is an independent consumer of WriteRunQueries, not a comparison against
the simulator's own dump. Oracle bounds (TestMode/act-one stopping) still apply.
Missing decisions, errors, and Unported all fail; observed kinds are reported.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "l4"))
from l4_run import first_diff, parse_runs, run_oracle  # noqa: E402


def card_name(card):
    return card["model_id"]["entry"] + ("+" + str(card["upgrade"]) if card["upgrade"] else "")


def model_name(model):
    return model["model_id"]["entry"] if model else None


def reward_facts(reward):
    types = {1: "Card", 2: "Gold", 3: "Potion", 4: "Relic", 5: "RemoveCard", 6: "SpecialCard"}
    result = {"type": types[reward["type"]]}
    if result["type"] == "Card":
        result.update(cards=[card_name(c) for c in reward["cards"]],
                      can_skip=reward["can_skip"], can_reroll=reward["can_reroll"],
                      has_been_rerolled=reward["has_been_rerolled"])
    elif result["type"] == "Gold":
        result["amount"] = reward["gold"]
    elif result["type"] in ("Potion", "Relic"):
        result["id"] = model_name(reward[result["type"].lower()])
    elif result["type"] == "SpecialCard":
        result["card"] = card_name(reward["cards"][0])
    return result


def compare(want, got, label):
    difference = first_diff(want, got)
    if difference:
        path, expected, actual = difference
        raise ValueError(f"{label} {path}: oracle={expected!r}, query={actual!r}")


def check_decision(want, query):
    if query["format"] != "sts2env.run_queries" or query["version"] != 1:
        raise ValueError("unsupported query envelope")
    if query["future"] is not None or query["is_agent_observation"]:
        raise ValueError("default query visibility contract")
    player = query["player"]
    if want["kind"] == "combat":
        # OracleRun emits the combat dump without DumpRun at this boundary.
        # Compare every inventory/resource fact it actually observes; deck,
        # gold and floors are compared at the other room/choice boundaries.
        original = want["combat"]["player"]
        compare("combat", query["decision_name"], "decision kind")
        for field in ("hp", "max_hp", "block", "energy", "stars"):
            compare(original[field], player[field], field)
        compare([r["id"] for r in original["relics"]], [model_name(r) for r in player["relics"]], "combat relics")
        compare(original["potion_slots"], [model_name(p["item"]) for p in player["potions"]], "combat potion slots")
        return
    run = want["run"]
    for field in ("hp", "max_hp", "gold"):
        compare(run[field], player[field], field)
    for field, original in (("act_index", "act"), ("act_floor", "act_floor"), ("total_floor", "total_floor")):
        compare(run[original], query[field], field)
    compare(run["deck"], [card_name(c) for c in player["deck"]], "deck")
    compare([r["id"] for r in run["relics"]], [model_name(r) for r in player["relics"]], "relics")
    compare(run["potions"], [model_name(p["item"]) for p in player["potions"]], "potion slots")
    kinds = {"rest": "rest_site", "relic_select": "treasure", "card_select": "choice", "bundle_select": "choice"}
    kind = want["kind"]
    expected_kind = kinds.get(kind, kind)
    # RewardsCmd's inline screen is a pending operation rather than a room.
    if kind == "rewards" and query["choice"] and query["choice"]["source"] == "rewards":
        expected_kind = "choice"
    compare(expected_kind, query["decision_name"], "decision kind")
    if kind == "map":
        compare(want["options"], [[a["col"], a["row"]] for a in query["legal_actions"] if a["kind"] == 1], "map actions")
    elif kind == "rewards":
        compare(want["options"], [reward_facts(e["reward"]) for e in query["rewards"]["entries"]], "rewards")
    elif kind == "shop":
        stocked = sorted((e for e in query["shop"]["entries"] if e["stocked"]), key=lambda e: e["action_index"])
        compare(list(range(len(stocked))), [e["action_index"] for e in stocked], "shop action indexes")
        items = []
        for entry in stocked:
            category = entry["kind"]
            item_kind = "card" if category in ("character_card", "colorless_card") else "remove" if category == "card_removal" else category
            items.append({"kind": item_kind, "id": "remove" if item_kind == "remove" else model_name(entry["item"]),
                          "cost": entry["cost"], "affordable": entry["buyable"]})
        compare(want["options"], items, "shop facts")
    elif kind == "event":
        compare(want["options"]["event"], query["event"]["entry"], "event model")
        compare(want["options"]["options"], [{"key": o["text_key"], "locked": o["locked"]} for o in query["event"]["options"]], "event options")
    elif kind == "rest":
        compare(want["options"], [{"id": o["key"], "enabled": o["enabled"]} for o in query["rest_site"]["options"]], "rest options")
    elif kind == "relic_select":
        compare(want["options"]["relics"], [r["entry"] for r in query["treasure"]["relics"]], "treasure options")
    elif kind in ("card_select", "bundle_select"):
        choice, options = query["choice"], want["options"]
        if kind == "bundle_select":
            compare(options["bundles"], [[card_name(c) for c in o["cards"]] for o in choice["options"]], "bundle groups")
        else:
            compare(options["cards"], [card_name(c) for o in choice["options"] for c in o["cards"]], "choice cards")
            for original, field in (("min", "min_select"), ("max", "max_select"), ("can_skip", "can_skip")):
                compare(options[original], choice[field], "choice " + original)
            if "alternatives" in options:
                actual = [o["key"] for o in choice["options"] if o["kind"] == "alternative"]
                compare(options["alternatives"], actual, "choice alternatives")
                compare(options["can_reroll"], "REROLL" in actual, "choice reroll")
    else:
        raise ValueError("unsupported oracle decision kind " + kind)


def check_end_projection(end, query):
    if query["format"] != "sts2env.run_queries" or query["version"] != 1:
        raise ValueError("unsupported endpoint query envelope")
    if query["future"] is not None or query["is_agent_observation"]:
        raise ValueError("default endpoint query visibility contract")
    run = end["run"]
    player = query["player"]
    for field in ("hp", "max_hp", "gold"):
        compare(run[field], player[field], "end " + field)
    for field, original in (("act_index", "act"), ("act_floor", "act_floor"), ("total_floor", "total_floor")):
        compare(run[original], query[field], "end " + field)
    compare(run["deck"], [card_name(c) for c in player["deck"]], "end deck")
    compare([r["id"] for r in run["relics"]], [model_name(r) for r in player["relics"]], "end relics")
    compare(run["potions"], [model_name(p["item"]) for p in player["potions"]], "end potion slots")

    if run["room"] == "Map":
        compare("map", query["decision_name"], "end decision kind")
    boundary_state = end.get("boundary_state")
    if boundary_state and run["room"] == "Map":
        map_state = boundary_state["map"]
        coord = run["coord"]
        if coord is None:
            expected_actions = [map_state["start"]]
        else:
            point = next((p for p in map_state["points"] if p["coord"] == coord), None)
            if point is None:
                raise ValueError("oracle boundary map does not contain the current coordinate")
            expected_actions = point["children"]
            expected_actions.sort(key=lambda c: (c[0], c[1]))
        actual_actions = [[a["col"], a["row"]] for a in query["legal_actions"] if a["kind"] == 1]
        compare(expected_actions, actual_actions, "end map actions")


def replay_queries(run, cli):
    begin = run["begin"]
    command = [str(cli), "replay", "--run-queries", "1", "--seed", begin["seed"],
               "--character", begin["character"], "--ascension", str(begin.get("ascension", 0))]
    if not begin.get("neow", True):
        command += ["--no-neow", "1"]
    for option in ("hp", "deck", "relics"):
        if begin.get(option):
            command += ["--" + option, str(begin[option])]
    if begin.get("stop"):
        command += ["--stop", begin["stop"]]
    result = subprocess.run(command, input="\n".join(run["actions"]) + "\n", capture_output=True, text=True)
    queries, endpoint_query = [], None
    for line in result.stdout.splitlines():
        event = json.loads(line)
        if event["event"] == "error":
            raise ValueError("simulator query/replay error: " + json.dumps(event))
        if event["event"] == "run_query":
            queries.append(event["queries"])
        elif event["event"] == "end":
            endpoint_query = event.get("queries")
    if result.returncode or len(queries) != len(run["decisions"]):
        raise ValueError(f"query replay exit={result.returncode} decisions={len(queries)}/{len(run['decisions'])}: {result.stderr}")
    if endpoint_query is None:
        raise ValueError("simulator end is missing its public run query")
    return queries, endpoint_query


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--save-trace", type=Path)
    parser.add_argument("--seeds")
    parser.add_argument("--characters", default="IRONCLAD")
    parser.add_argument("--policy-seeds", default="1")
    parser.add_argument("--ascension", type=int, default=0)
    parser.add_argument("--max-decisions", type=int, default=3000)
    parser.add_argument("--no-neow", action="store_true")
    parser.add_argument("--hp", type=int, default=0)
    parser.add_argument("--deck")
    parser.add_argument("--relics")
    parser.add_argument("--stop", choices=("before-act1-boss", "after-act1", "after-act2", "victory"),
                        help="oracle/replay endpoint; omitted keeps the historical Act 1 boss boundary")
    parser.add_argument("--cli", type=Path, default=ROOT / "build" / "sts2sim_cli")
    args = parser.parse_args()
    if not args.trace and not args.seeds:
        parser.error("--trace or --seeds is required")
    lines = args.trace.read_text().splitlines() if args.trace else run_oracle(args)
    if args.save_trace:
        args.save_trace.write_text("\n".join(lines) + "\n")
    runs = parse_runs(lines)
    counts = Counter()
    reroll_options = 0
    endpoint_queries = 0
    for run in runs:
        queries, endpoint_query = replay_queries(run, args.cli)
        for index, (want, query) in enumerate(zip(run["decisions"], queries)):
            try:
                check_decision(want, query)
            except (ValueError, KeyError, TypeError) as error:
                raise ValueError(f"{run['begin']['seed']} policy {run['begin']['policy_seed']} decision {index}: {error}") from error
            counts[want["kind"]] += 1
            options = want.get("options", {})
            if isinstance(options, dict) and "REROLL" in options.get("alternatives", []):
                reroll_options += 1
        try:
            check_end_projection(run["end"], endpoint_query)
        except (ValueError, KeyError, TypeError) as error:
            raise ValueError(f"{run['begin']['seed']} policy {run['begin']['policy_seed']} endpoint: {error}") from error
        endpoint_queries += 1
    print(f"Run query differential: {len(runs)} runs, {sum(counts.values())} decisions, {endpoint_queries} endpoint queries, 0 differences, 0 Unported")
    print("Observed kinds: " + json.dumps(dict(sorted(counts.items())), sort_keys=True))
    print(f"Observed original REROLL candidate lists: {reroll_options}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError) as error:
        print("Run query differential failed: " + str(error), file=sys.stderr)
        sys.exit(1)
