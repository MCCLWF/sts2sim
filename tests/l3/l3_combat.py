#!/usr/bin/env python3
"""L3: replay oracle combats in sts2sim and diff every decision-point state.

The oracle (`Sts2Oracle combat`, tools/Sts2Oracle/OracleCombat.cs) plays a
combat with a seeded random policy and prints the state before every action.
This script feeds the same actions to `sts2sim_cli combat` and compares the
state lines and the final `end` line field by field. The first difference of
each combat is reported with its JSON path.

Usage:
  tests/l3/l3_combat.py --trace oracle.jsonl [--cli build/sts2sim_cli]
  tests/l3/l3_combat.py --seeds S1,S2 [--policy-seeds 1,2] [--ascension N]   (runs the oracle)
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = os.path.join(ROOT, "tools", "Sts2Oracle", "bin", "Release", "net9.0", "Sts2Oracle.dll")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from differential.common import (ProcessFailure, comparable, error_status, events, exit_code,
                                 fail, first_diff, native_events, process_supervision, result_base, run_process,
                                 summary, write_json)


def oracle_command(args):
    cmd = [DOTNET, ORACLE, "combat", "--seeds", args.seeds, "--characters", args.characters,
           "--policy-seeds", args.policy_seeds, "--max-actions", str(args.max_actions)]
    if args.ascension:
        cmd += ["--ascension", str(args.ascension)]
    if args.encounter:
        cmd += ["--encounter", args.encounter]
    if args.deck:
        cmd += ["--deck", args.deck]
    if args.relics:
        cmd += ["--relics", args.relics]
    if args.potion:
        cmd += ["--potion", args.potion]
    if args.potion_target:
        cmd += ["--potion-target", args.potion_target]
    if args.probe_cards:
        cmd += ["--probe-cards", args.probe_cards]
        cmd += ["--probe-helper", args.probe_helper]
        if args.probe_after_card_turn:
            cmd += ["--probe-after-card-turn", "1"]
    if args.probe_mad_science:
        cmd += ["--probe-mad-science", "1"]
        cmd += ["--probe-helper", args.probe_helper]
    if args.probe_potions:
        cmd += ["--probe-potions", args.probe_potions]
    if args.starting_energy is not None:
        cmd += ["--starting-energy", str(args.starting_energy)]
    if args.starting_stars is not None:
        cmd += ["--starting-stars", str(args.starting_stars)]
    if args.take_special_rewards:
        cmd += ["--take-special-rewards", "1"]
    return cmd


def run_oracle(args):
    prefix = os.path.join(args.artifacts_dir, "oracle") if getattr(args, "artifacts_dir", None) else None
    out, _, _ = run_process(oracle_command(args), timeout=getattr(args, "timeout", 180),
                            artifacts=prefix, role="oracle", allowed=(0,))
    return out.splitlines()


def parse_runs(lines):
    runs, cur = [], None
    for ev in events(lines, {"begin", "enter", "state", "decision", "action", "end", "rng"}):
        kind = ev["event"]
        if kind == "begin":
            if cur and cur["end"] is None and not is_probe(cur):
                raise ValueError("oracle begins another combat before ending the previous one")
            for key in ("seed", "character", "policy_seed"):
                if key not in ev:
                    raise ValueError("oracle begin missing " + key)
            cur = {"begin": ev, "states": [], "actions": [], "cards_played": [], "played_cards": [],
                   "decisions": [], "observations": [], "end": None,
                   "awaiting_action": False, "awaiting_selection": False}
            runs.append(cur)
        elif kind == "rng":
            continue  # Preserved raw, but no dual-ended ledger certification yet.
        elif cur is None or cur["end"] is not None:
            raise ValueError("oracle event outside an active combat")
        elif kind == "enter":
            if "coord" in cur or cur["states"]:
                raise ValueError("duplicate or late combat entry")
            cur["coord"] = ev["coord"]
            cur["encounter"] = ev.get("encounter")
        elif kind == "state":
            if cur["awaiting_action"] or cur["awaiting_selection"]:
                raise ValueError("oracle state before answering the preceding observation")
            if "coord" not in cur or not isinstance(ev.get("state"), dict):
                raise ValueError("oracle state missing combat entry or payload")
            cur["states"].append(ev["state"])
            cur["observations"].append(ev)
            cur["awaiting_action"] = True
        elif kind == "decision":
            if ev.get("kind") != "card_select":
                raise ValueError("unknown combat decision")
            if cur["awaiting_selection"]:
                raise ValueError("oracle card selection has no preceding answer")
            cur["decisions"].append(ev)
            cur["observations"].append(ev)
            cur["awaiting_selection"] = True
        elif kind == "action":
            if ev.get("kind") == "card_select":
                if not cur["awaiting_selection"]:
                    raise ValueError("oracle card selection action has no preceding decision")
                indices = ev.get("indices", [])
                cur["actions"].append("s%d%s" % (len(indices), ":" + ".".join(str(i) for i in indices) if indices else ""))
                cur["awaiting_selection"] = False
            elif ev["type"] == "play":
                if not cur["awaiting_action"] or cur["awaiting_selection"]:
                    raise ValueError("oracle play has no preceding state")
                target = ev["target"] if ev["target"] is not None else -1
                cur["actions"].append("p%d:%d" % (ev["hand_index"], target))
                cur["cards_played"].append(ev["card"])
                state = cur["states"][-1]
                player = state.get("player", {}) if isinstance(state, dict) else {}
                hand = player.get("hand", []) if isinstance(player, dict) else []
                hand_index = ev["hand_index"]
                hand_card = (hand[hand_index] if isinstance(hand, list) and
                             isinstance(hand_index, int) and 0 <= hand_index < len(hand) else None)
                cur["played_cards"].append({"card": ev["card"], "hand_index": hand_index,
                                            "hand_card": hand_card})
                cur["awaiting_action"] = False
            elif ev["type"] == "end_turn":
                if not cur["awaiting_action"] or cur["awaiting_selection"]:
                    raise ValueError("oracle end_turn has no preceding state")
                cur["actions"].append("e")
                cur["awaiting_action"] = False
            else:
                raise ValueError("unknown combat action")
        elif kind == "end":
            if cur["awaiting_selection"]:
                raise ValueError("oracle ends with an unanswered card selection")
            cur["end"] = ev
    if not runs:
        raise ValueError("oracle trace contains no combats")
    for run in runs:
        if not run["states"] or "coord" not in run:
            raise ValueError("oracle combat has no state or entry")
        selections = sum(action.startswith("s") for action in run["actions"])
        ordinary = len(run["actions"]) - selections
        if selections != len(run["decisions"]):
            raise ValueError("oracle card selection action/decision counts differ")
        if run["end"] is None:
            if not is_probe(run) or len(run["states"]) != ordinary + 1:
                raise ValueError("oracle combat is missing its end or probe successor")
            if run["begin"].get("probe_card") and not probe_card_was_played(run):
                raise ValueError("oracle probe card was not played")
        elif len(run["states"]) != ordinary:
            raise ValueError("oracle combat state/action counts differ")
    return runs


def is_probe(run):
    return bool(run["begin"].get("probe_card") or run["begin"].get("probe_potion"))


def card_spec_parts(card_spec):
    """Return the source card ID and upgrade level from CARD[+UPGRADE]."""
    if not isinstance(card_spec, str):
        return None, None
    card_id, separator, suffix = card_spec.rpartition("+")
    if separator and card_id and suffix.isdigit():
        return card_id, int(suffix)
    return card_spec, 0


def probe_card_was_played(run):
    """Match the specified probe copy, including its upgrade, to its play."""
    probe_id, probe_upgrade = card_spec_parts(run["begin"].get("probe_card"))
    if probe_id is None:
        return False
    for played in run.get("played_cards", []):
        if played.get("card") != probe_id:
            continue
        hand_card = played.get("hand_card")
        # The oracle action records CardId.Entry (the base ID); the immediately
        # preceding state carries the selected copy's upgrade level.
        if hand_card is None:
            if probe_upgrade == 0:
                return True
            continue
        if card_spec_parts(hand_card) == (probe_id, probe_upgrade):
            return True
    return False


def required_probe_matches(run, required_probe_play):
    probe_id, _ = card_spec_parts(run["begin"].get("probe_card"))
    required_ids = {card_spec_parts(item.strip())[0]
                    for item in required_probe_play.split(",") if item.strip()}
    return probe_id is not None and probe_id in required_ids


def replay(cli, run, ascension, deck=None, relics=None, potion=None, starting_energy=None, starting_stars=None,
           take_special_rewards=False, raw_output=None, potion_target=None, potion_auto_death=False,
           timeout=180, artifacts=None, diagnostics=None):
    b = run["begin"]
    ascension = b.get("ascension", ascension)
    cmd = [cli, "combat", "--seed", b["seed"], "--character", b["character"], "--ascension", str(ascension),
           "--coord", "%d,%d" % tuple(run["coord"]), "--actions", ",".join(run["actions"]) or "-"]
    if run.get("encounter"):
        cmd += ["--encounter", run["encounter"]]
    deck, relics, potion = b.get("deck", deck), b.get("relics", relics), b.get("potion", potion)
    starting_energy = b.get("starting_energy", starting_energy)
    starting_stars = b.get("starting_stars", starting_stars)
    take_special_rewards = b.get("take_special_rewards", take_special_rewards)
    if deck:
        cmd += ["--deck", deck]
    if relics:
        cmd += ["--relics", relics]
    if potion:
        cmd += ["--potion", potion]
    if potion_target:
        cmd += ["--potion-target", potion_target]
    if run["begin"].get("probe_type") and run["begin"].get("probe_rider"):
        cmd += ["--mad-science", ("%s:%s" % (run["begin"]["probe_type"], run["begin"]["probe_rider"])).upper()]
    if potion_auto_death:
        cmd += ["--potion-auto-death", "1"]
    if starting_energy is not None:
        cmd += ["--starting-energy", str(starting_energy)]
    if starting_stars is not None:
        cmd += ["--starting-stars", str(starting_stars)]
    if take_special_rewards:
        cmd += ["--take-special-rewards", "1"]
    process_error = None
    try:
        stdout, _, detail = run_process(cmd, timeout=timeout, artifacts=artifacts)
    except ProcessFailure as exc:
        stdout, detail = exc.stdout, exc.detail
        process_error = dict(detail, status=exc.status, unported=False)
    if raw_output is not None:
        raw_output.write(stdout)
    states, decisions, end, error = [], [], None, None
    parsed, error = native_events(stdout, {"state", "decision", "end", "error"})
    protocol_error = error
    observations = []
    for ev in parsed:
        if ev["event"] == "state":
            states.append(ev["state"])
            observations.append(ev)
        elif ev["event"] == "decision":
            observations.append(ev)
            if ev.get("kind") != "card_select":
                error = {"status": "native_error", "unported": False,
                         "message": "unexpected decision kind in L3 combat replay: " + str(ev.get("kind"))}
                break
            decisions.append(ev)
        elif ev["event"] == "end":
            end = ev
        elif ev["event"] == "error":
            error = ev
    error = process_error or protocol_error or error
    if detail["returncode"] != 0 and error is None:
        error = dict(detail, status="native_error", unported=False)
    if diagnostics is not None:
        diagnostics.update(observations=observations, process=detail, error=error)
    return states, decisions, end, error


def compare_run(run, observations, end, error, required_probe_play=None):
    want = run["observations"]
    row = result_base("L3", run, len(want))
    row["matched_states"] = 0
    row["total_states"] = len(run["states"])
    if error:
        row["native_diagnostic"] = error
    for index, expected in enumerate(want):
        if index >= len(observations):
            return fail(row, error_status(error) if error else "diverged",
                        error["message"] if error else "simulator observation missing", index=index,
                        kind=expected["event"])
        diff = first_diff(comparable(expected), comparable(observations[index]))
        if diff:
            return fail(row, "diverged", "combat observation differs", index=index,
                        kind=expected["event"], diff=diff)
        row["matched"] += 1
        row["matched_states"] += expected["event"] == "state"
    if len(observations) > len(want):
        index = len(want)
        return fail(row, "diverged", "unexpected native observation", index=index,
                    kind=observations[index]["event"])
    if error:
        return fail(row, error_status(error), error["message"])
    if len(observations) != len(want):
        return fail(row, "diverged", "different observation counts")
    if run["end"]:
        diff = first_diff(run["end"], end)
        if diff:
            return fail(row, "diverged", "combat end differs", kind="end", diff=diff)
    else:
        if end is not None:
            return fail(row, "diverged", "bounded probe unexpectedly ended combat", kind="end")
        # An explicit card/potion probe is a deliberately bounded test, not a
        # completed combat. Never include it in full-combat acceptance counts.
        row["status"] = "probe_identical"
    if (required_probe_play and required_probe_matches(run, required_probe_play)
            and not probe_card_was_played(run)):
        return fail(row, "diverged", "probe card was not played", kind="action",
                    context={"probe_card": run["begin"].get("probe_card"),
                             "required_probe_play": required_probe_play})
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace")
    ap.add_argument("--trace-output", help="save the raw Sts2Oracle JSONL when generating a trace")
    ap.add_argument("--sim-trace-output", help="save raw C++ replay JSONL for each Oracle run")
    ap.add_argument("--seeds")
    ap.add_argument("--characters", default="IRONCLAD")
    ap.add_argument("--policy-seeds", default="1")
    ap.add_argument("--ascension", type=int, default=0)
    ap.add_argument("--encounter", help="fight this encounter entry instead of the map's first room")
    ap.add_argument("--deck", help="replace the starting deck, CARD[+N],...")
    ap.add_argument("--relics", help="obtain extra relics, ID,...")
    ap.add_argument("--potion", help="procure and use a potion before the first state (focused model checks)")
    ap.add_argument("--potion-target", choices=("player", "enemy", "none"),
                    help="target for the focused potion use (default is the player)")
    ap.add_argument("--probe-cards", help="run a set of isolated card probes, CARD[+UPGRADE],...")
    ap.add_argument("--probe-mad-science", action="store_true", help="probe all nine TinkerTime Mad Science configurations at base and upgrade")
    ap.add_argument("--probe-helper", default="STRIKE_IRONCLAD",
                    help="card appended to each probe deck (empty string means no helper card)")
    ap.add_argument("--probe-after-card-turn", action="store_true",
                    help="for card probes, enqueue one player end turn and compare the next play phase")
    ap.add_argument("--probe-potions", help="run isolated acquisition/use probes for POTION_ID,...")
    ap.add_argument("--require-probe-play", help="comma-separated probe card IDs that must be played")
    ap.add_argument("--starting-energy", type=int, help="set combat energy before the first state")
    ap.add_argument("--starting-stars", type=int, help="set combat stars before the first state")
    ap.add_argument("--take-special-rewards", action="store_true", help="select returned special cards at combat end and compare the resulting deck")
    ap.add_argument("--max-actions", type=int, default=300)
    ap.add_argument("--cli", default=os.path.join(ROOT, "build", "sts2sim_cli"))
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--require-complete", action="store_true", help="reject Unported; explicit probes remain bounded tests")
    ap.add_argument("--timeout", type=float, default=180, help="maximum seconds per oracle or replay process")
    ap.add_argument("--artifacts-dir", help="save stdout, stderr, command, process result for every subprocess")
    ap.add_argument("--result-json", help="write structured results; probes are distinguished from complete combats")
    args = ap.parse_args()
    if args.timeout <= 0:
        ap.error("--timeout must be positive")
    if args.sim_trace_output:
        Path(args.sim_trace_output).write_text("", encoding="utf-8")
    if args.trace:
        lines = open(args.trace, encoding="utf-8").read().splitlines()
    elif args.seeds:
        lines = run_oracle(args)
    else:
        ap.error("--trace or --seeds is required")

    if args.trace_output:
        with open(args.trace_output, "w", encoding="utf-8") as raw:
            raw.write("\n".join(lines))
            raw.write("\n")

    runs = parse_runs(lines)
    results = evaluate_runs(args, runs)
    if args.result_json:
        write_json(args.result_json, {"schema_version": 1, "summary": summary(results), "results": results})
    for row in results:
        if row["status"] not in ("identical", "probe_identical") or args.verbose:
            print("%s %s: %s" % (row["status"], json.dumps(row["identity"]), json.dumps(row["first"], ensure_ascii=False)))
    counts = summary(results)["statuses"]
    print("L3: %d complete combats identical, %d bounded probes identical, %d inputs, %d unported, %d other failures, %d/%d states matched" %
          (counts.get("identical", 0), counts.get("probe_identical", 0), len(results), counts.get("unported", 0),
           sum(n for status, n in counts.items() if status not in ("identical", "probe_identical", "unported")),
           sum(r["matched_states"] for r in results), sum(r["total_states"] for r in results)))
    return exit_code(results, args.require_complete)


def evaluate_runs(args, runs):
    results = []
    for index, run in enumerate(runs):
        diagnostics = {}
        prefix = os.path.join(args.artifacts_dir, f"replay-{index:05d}") if getattr(args, "artifacts_dir", None) else None
        with open(args.sim_trace_output, "a", encoding="utf-8") if getattr(args, "sim_trace_output", None) else open(os.devnull, "w") as raw:
            _, _, end, error = replay(
                args.cli, run, args.ascension, run["begin"].get("deck", args.deck), args.relics,
                run["begin"].get("potion", args.potion), args.starting_energy, args.starting_stars,
                args.take_special_rewards, raw, run["begin"].get("potion_target", args.potion_target),
                run["begin"].get("probe_auto_death", False), timeout=getattr(args, "timeout", 180),
                artifacts=prefix, diagnostics=diagnostics)
        row = compare_run(run, diagnostics["observations"], end, error,
                          required_probe_play=getattr(args, "require_probe_play", None))
        row["identity"]["ascension"] = run["begin"].get("ascension", args.ascension)
        if (any(getattr(args, key, None) for key in ("deck", "relics", "potion", "encounter"))
                or any(getattr(args, key, None) is not None for key in ("starting_energy", "starting_stars"))):
            row["scope"] = "injected"
        results.append(row)
    return results


if __name__ == "__main__":
    try:
        with process_supervision():
            code = main()
        sys.exit(code)
    except (ValueError, KeyError, TypeError, OSError, ProcessFailure) as exc:
        print("L3 failure: " + str(exc), file=sys.stderr)
        sys.exit(2)
