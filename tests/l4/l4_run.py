#!/usr/bin/env python3
"""L4: replay oracle runs in sts2sim and diff every decision point.

The oracle (`Sts2Oracle run`, tools/Sts2Oracle/OracleRun.cs) plays whole runs
with a seeded random policy and prints every decision point (state, options)
and the action taken. This script feeds the actions to `sts2sim_cli replay`
and compares the decision points one by one: kind, options, the run state and
(in combat) the combat state. The first difference of each run is reported
with its JSON path; a run the simulator cannot continue (content not ported)
is reported with how far it got.

Usage:
  tests/l4/l4_run.py --trace oracle.jsonl [--cli build/sts2sim_cli]
  tests/l4/l4_run.py --seeds S1,S2 [--policy-seeds 1,2] [--ascension N] [--no-neow]   (runs the oracle)
"""
import argparse
import concurrent.futures
import json
import os
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tests", "l3"))
sys.path.insert(0, os.path.join(ROOT, "tests"))
from l3_combat import first_diff  # noqa: E402
from coverage_nonhero_relics import nonhero_relic_inventory  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, "tools"))
from differential.common import (ProcessFailure, comparable, error_status, events, exit_code,
                                 fail, native_events, process_supervision, result_base, run_process, summary, write_json)

DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = os.path.join(ROOT, "tools", "Sts2Oracle", "bin", "Release", "net9.0", "Sts2Oracle.dll")


def oracle_command(args):
    cmd = [DOTNET, ORACLE, "run", "--seeds", args.seeds, "--characters", args.characters,
           "--policy-seeds", args.policy_seeds, "--max-decisions", str(args.max_decisions),
           "--policy-version", getattr(args, "policy_version", "coverage-v3")]
    if args.ascension:
        cmd += ["--ascension", str(args.ascension)]
    if args.no_neow:
        cmd += ["--no-neow", "1"]
    if args.hp:
        cmd += ["--hp", str(args.hp)]
    if args.deck:
        cmd += ["--deck", args.deck]
    if args.relics:
        cmd += ["--relics", args.relics]
    if args.relic_matrix:
        cmd += ["--relic-matrix", args.relic_matrix]
    if args.all_nonhero_relics:
        relic_matrix = ",".join(nonhero_relic_inventory()["in_scope"])
        cmd += ["--relic-matrix", relic_matrix]
    if getattr(args, "force_sacrifice", 0):
        cmd += ["--force-sacrifice", str(args.force_sacrifice)]
    if getattr(args, "stop", None):
        cmd += ["--stop", args.stop]
    return cmd


def run_oracle(args):
    # Async event exceptions may be logged by TaskHelper and swallowed by the
    # game. Such a trace is not ground truth, even if it has an ordinary end.
    prefix = os.path.join(args.artifacts_dir, "oracle") if getattr(args, "artifacts_dir", None) else None
    out, _, _ = run_process(oracle_command(args), timeout=getattr(args, "timeout", 180),
                            artifacts=prefix, role="oracle", allowed=(0,))
    return out.splitlines()


def action_line(ev):
    k = ev["kind"]
    if isinstance(ev.get("potion"), dict):
        # A scripted potion action on a screen outside combat (Sts2Oracle run --script).
        # (A combat action names its potion with a string in the same member.)
        if k not in ("map", "rewards", "event", "rest", "relic_select", "shop"):
            raise ValueError("potion action on a %s decision" % k)
        return ("discard %d" if ev["potion"]["type"] == "discard" else "potion %d -1") % ev["potion"]["slot"]
    if k == "map":
        return "map %d %d" % tuple(ev["coord"])
    if k == "combat":
        if ev["type"] == "play":
            return "play %d %d" % (ev["hand_index"], -1 if ev["target"] is None else ev["target"])
        if ev["type"] == "end_turn":
            return "end"
        if ev["type"] == "use_potion":
            return "potion %d %d" % (ev["slot"], -1 if ev["target"] is None else ev["target"])
        if ev["type"] == "discard_potion":
            return "discard %d" % ev["slot"]
        raise ValueError("unknown combat action " + ev["type"])
    if k == "rewards":
        return "proceed" if ev.get("proceed") else "reward %d" % ev["index"]
    if k == "card_select":
        return "select %d %s" % (len(ev["indices"]), " ".join(str(i) for i in ev["indices"]))
    if k == "event":
        return "event %d" % ev["index"]
    if k == "choice":
        return "choose %d" % ev["index"]
    if k == "rest":
        return "rest %d" % ev["index"]
    if k == "relic_select":
        return "relicskip" if ev.get("skip") else "relic %d" % ev["index"]
    if k == "bundle_select":
        return "bundle %d" % ev["index"]
    if k == "shop":
        return "leave" if ev.get("leave") else "shop %d" % ev["index"]
    raise ValueError("unknown action kind %s" % k)


def parse_runs(lines):
    runs, cur = [], None
    pending_kind = None
    for ev in events(lines, {"begin", "decision", "action", "end", "rng"}):
        kind = ev["event"]
        if kind == "begin":
            if cur is not None and cur["end"] is None:
                raise ValueError("oracle trace starts another run before ending the previous one")
            cur = {"begin": ev, "decisions": [], "actions": [], "end": None}
            runs.append(cur)
            pending_kind = None
            for key in ("seed", "character", "policy_seed"):
                if key not in ev:
                    raise ValueError("oracle begin missing " + key)
            if ev.get("policy_version", "legacy-v1") not in ("legacy-v1", "coverage-v2", "coverage-v3"):
                raise ValueError("oracle begin has unsupported policy_version")
        elif kind == "rng":
            continue
        elif kind == "decision":
            if cur is None or cur["end"] is not None:
                raise ValueError("oracle decision outside an active run")
            if pending_kind is not None:
                raise ValueError("oracle decision has no preceding action")
            if ev.get("kind") != "combat" and (not isinstance(ev.get("run"), dict) or "options" not in ev):
                raise ValueError("oracle decision missing options or run")
            if ev.get("kind") == "combat" and not isinstance(ev.get("combat"), dict):
                raise ValueError("oracle combat decision missing combat state")
            # A native rest continuation is a UI-flow boundary, not the old
            # one-option policy's automatic exit. Preserve it after the last
            # rest/card-selection/reward action. Marking an initial map entry
            # is unnecessary. The adapter cannot create a rest state: all
            # fields and the decision kind still undergo the same comparison.
            if ev.get("kind") == "rest" and cur["actions"] and cur["decisions"][-1]["kind"] != "map":
                cur["actions"][-1] += " rest_hold"
            pending_kind = ev["kind"]
            cur["decisions"].append(ev)
        elif kind == "action":
            if cur is None or cur["end"] is not None:
                raise ValueError("oracle action outside an active run")
            if pending_kind is None or pending_kind != ev.get("kind"):
                raise ValueError("oracle action has no matching decision")
            if cur["begin"].get("policy_version") == "coverage-v3" and pending_kind == "card_select":
                options = cur["decisions"][-1].get("options", {})
                if "alternatives" in options:
                    can_reroll = ev.get("native_reward_can_reroll")
                    legal = ev.get("legal_reward_indices")
                    if type(can_reroll) is not bool or not isinstance(legal, list):
                        raise ValueError("coverage-v3 reward action missing native legality evidence")
                    cards = options.get("cards", [])
                    expected = list(range(len(cards))) + [len(cards) + i for i, alternative in enumerate(options["alternatives"])
                                                          if alternative != "REROLL" or can_reroll]
                    if any(type(i) is not int for i in legal) or legal != expected:
                        raise ValueError("coverage-v3 reward legal indices differ from native availability")
                    selected = ev.get("indices")
                    if not isinstance(selected, list) or len(selected) > 1 or any(type(i) is not int or i not in legal for i in selected):
                        raise ValueError("coverage-v3 reward action selects an unavailable option")
            cur["actions"].append(action_line(ev))
            pending_kind = None
        elif kind == "end":
            if cur is None or cur["end"] is not None:
                raise ValueError("oracle end without an active run")
            if pending_kind is not None:
                raise ValueError("oracle ends with an unanswered decision")
            if not isinstance(ev.get("run"), dict):
                raise ValueError("oracle end missing run state")
            cur["end"] = ev
    if not runs:
        raise ValueError("oracle trace contains no runs")
    for run in runs:
        if (not run["decisions"] and run["begin"].get("stop") != "after-loadout") or run["end"] is None:
            raise ValueError("oracle run is empty or missing its end")
        if run["end"].get("reason") not in ("death", "boss", "boundary", "victory", "max_decisions"):
            raise ValueError("oracle run did not finish: " + json.dumps(run["end"]))
        if run["end"].get("reason") == "boundary":
            if run["begin"].get("stop") not in ("before-act1-boss", "after-act1", "after-act2", "after-loadout"):
                raise ValueError("oracle boundary has no matching explicit begin.stop: " + json.dumps(run["end"]))
            if run["end"].get("stop") != run["begin"].get("stop"):
                raise ValueError("oracle boundary stop does not match begin.stop: " + json.dumps(run["end"]))
            if not isinstance(run["end"].get("boundary_state"), dict):
                raise ValueError("oracle boundary is missing boundary_state: " + json.dumps(run["end"]))
        if run["end"].get("reason") == "victory":
            if run["begin"].get("stop") != "victory" or run["end"].get("stop") != "victory":
                raise ValueError("oracle victory does not match an explicit --stop victory: " + json.dumps(run["end"]))
        if len(run["actions"]) != len(run["decisions"]):
            raise ValueError("oracle run has different action and decision counts")
    return runs


def require_policy_version(runs, expected):
    for run in runs:
        if run["begin"].get("policy_version", "legacy-v1") != expected:
            raise ValueError("oracle capture policy_version does not match the requested strategy")


def replay(cli, run, verify_snapshots=False, timeout=180, artifacts=None):
    b = run["begin"]
    cmd = [cli, "replay", "--seed", b["seed"], "--character", b["character"], "--ascension", str(b.get("ascension", 0))]
    if not b.get("neow", True):
        cmd += ["--no-neow", "1"]
    if b.get("hp"):
        cmd += ["--hp", str(b["hp"])]
    if b.get("deck"):
        cmd += ["--deck", b["deck"]]
    if b.get("relics"):
        cmd += ["--relics", b["relics"]]
    if b.get("stop"):
        cmd += ["--stop", b["stop"]]
    if verify_snapshots:
        cmd += ["--verify-snapshots", "1"]
    process_error = None
    try:
        stdout, _, detail = run_process(cmd, stdin="\n".join(run["actions"]) + "\n",
                                        timeout=timeout, artifacts=artifacts)
    except ProcessFailure as exc:
        stdout, detail = exc.stdout, exc.detail
        process_error = dict(detail, status=exc.status, unported=False)
    decisions, end, error = [], None, None
    parsed, error = native_events(stdout, {"decision", "end", "error"})
    protocol_error = error
    for ev in parsed:
        if ev["event"] == "decision":
            decisions.append(ev)
        elif ev["event"] == "end":
            end = ev
        elif ev["event"] == "error":
            error = ev
    error = process_error or protocol_error or error
    if detail["returncode"] != 0 and error is None:
        error = dict(detail, status="native_error", unported=False)
    return decisions, end, error


def compare_run(run, decisions, end, error):
    row = result_base("L4", run, len(run["decisions"]))
    if error:
        row["native_diagnostic"] = error
    for index, want in enumerate(run["decisions"]):
        context = {key: want.get("run", {}).get(key) for key in ("act", "total_floor", "room")}
        if want["kind"] == "combat":
            context.update(round=want["combat"].get("round"), turn=want["combat"].get("turn"))
        context["action"] = run["actions"][index]
        if index >= len(decisions):
            return fail(row, error_status(error) if error else "diverged",
                        error["message"] if error else "simulator decision missing", index=index,
                        kind=want["kind"], context=context)
        diff = first_diff(comparable(want), comparable(decisions[index]))
        if diff:
            return fail(row, "diverged", "decision differs", index=index, kind=want["kind"],
                        diff=diff, context=context)
        row["matched"] += 1
    if error:
        return fail(row, error_status(error), error["message"])
    if len(decisions) != len(run["decisions"]):
        return fail(row, "diverged", "different decision counts")
    if end is None:
        return fail(row, "native_error", "simulator missing end")
    expected_reason = run["end"]["reason"]
    if expected_reason == "max_decisions":
        return fail(row, "truncated", "oracle reached max_decisions; matching a prefix is not completion")
    # Legacy captures omit begin.stop and call the Act 1 boundary 'boss'.
    # They certify only that historical boundary, never a completed full run.
    if expected_reason == "boss":
        if end.get("reason") not in ("actions", "boundary"):
            return fail(row, "diverged", "legacy boss boundary end differs")
    else:
        keys = ("reason", "stop") if "stop" in run["end"] else ("reason",)
        diff = first_diff({k: run["end"][k] for k in keys}, {k: end.get(k) for k in keys})
        if diff:
            return fail(row, "diverged", "end reason/stop differs", kind="end", diff=diff)
    if expected_reason == "boundary":
        diff = first_diff(run["end"]["boundary_state"], end.get("boundary_state"))
        if diff:
            return fail(row, "diverged", "boundary state differs", kind="end", diff=diff)
    diff = first_diff(run["end"]["run"], end.get("run"))
    if diff:
        return fail(row, "diverged", "end run state differs", kind="end", diff=diff)
    return row


def evaluate_runs(args, runs):
    rows = []
    for index, run in enumerate(runs):
        prefix = os.path.join(args.artifacts_dir, f"replay-{index:05d}") if getattr(args, "artifacts_dir", None) else None
        decisions, end, error = replay(args.cli, run, args.verify_snapshots,
                                       timeout=getattr(args, "timeout", 180), artifacts=prefix)
        rows.append(compare_run(run, decisions, end, error))
    return rows


def run_nonhero_relic_matrix(args):
    relic_ids = nonhero_relic_inventory()["in_scope"]
    if args.relics or args.relic_matrix:
        raise ValueError("--all-nonhero-relics cannot be combined with --relics or --relic-matrix")

    # The run oracle and replay CLI currently construct single-player runs.
    # Source explicitly disallows Massive Scroll in solo runs; forcing it
    # anyway makes the original game throw because its pool is multiplayer-only.
    solo_inapplicable = {"MASSIVE_SCROLL": "source IsAllowed requires more than one player"}
    test_ids = [relic for relic in relic_ids if relic not in solo_inapplicable]

    requested_artifacts = getattr(args, "artifacts_dir", None)
    if requested_artifacts:
        os.makedirs(requested_artifacts, exist_ok=True)
        artifact_root = tempfile.mkdtemp(prefix="nonhero-relic-matrix-", dir=requested_artifacts)
    else:
        artifact_root = tempfile.mkdtemp(prefix="sts2sim-l4-nonhero-relic-matrix-")
    artifact_root = os.path.abspath(artifact_root)
    print("MATRIX ARTIFACTS %s" % artifact_root, flush=True)

    def run_one(relic):
        relic_artifacts = os.path.join(artifact_root, "workers", relic)
        process_artifacts = os.path.join(artifact_root, "processes", relic)
        cmd = [sys.executable, __file__, "--seeds", args.seeds, "--characters", args.characters,
               "--policy-seeds", args.policy_seeds, "--max-decisions", str(args.max_decisions),
               "--relics", relic, "--stop", args.stop, "--cli", args.cli, "--require-complete",
               "--timeout", str(args.timeout), "--artifacts-dir", relic_artifacts,
               "--result-json", os.path.join(relic_artifacts, "result.json")]
        if args.ascension:
            cmd += ["--ascension", str(args.ascension)]
        if args.no_neow:
            cmd += ["--no-neow"]
        if args.hp:
            cmd += ["--hp", str(args.hp)]
        if args.deck:
            cmd += ["--deck", args.deck]
        if args.force_sacrifice:
            cmd += ["--force-sacrifice", str(args.force_sacrifice)]
        cmd += ["--policy-version", getattr(args, "policy_version", "coverage-v3")]
        if args.verify_snapshots:
            cmd += ["--verify-snapshots"]
        if args.verbose:
            cmd += ["--verbose"]
        try:
            stdout, stderr, detail = run_process(cmd, timeout=args.timeout, artifacts=process_artifacts,
                                                 role="matrix worker %s" % relic, allowed=(0, 1))
            return relic, detail["returncode"], stdout, stderr
        except ProcessFailure as exc:
            return relic, exc.detail.get("returncode", 2), exc.stdout, exc.stderr

    results = []
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.matrix_workers) as pool, process_supervision(
            on_cancel=lambda: pool.shutdown(wait=False, cancel_futures=True)):
        futures = [pool.submit(run_one, relic) for relic in test_ids]
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            results.append(result)
            done += 1
            if result[1] != 0:
                print("MATRIX DIFF/ERROR %s" % result[0])
                if result[2].strip():
                    print(result[2].strip())
                if result[3].strip():
                    print(result[3].strip()[-6000:])
            elif done % 25 == 0 or done == len(test_ids):
                print("single-player non-hero relic routes: %d/%d completed" % (done, len(test_ids)), flush=True)

    failures = sorted((r for r in results if r[1] != 0), key=lambda r: test_ids.index(r[0]))
    for relic, reason in solo_inapplicable.items():
        print("MATRIX SKIP %s: %s" % (relic, reason))
    print("L4 non-hero relic matrix: %d applicable relics, %d passed, %d failed; %d multiplayer-only relic skipped"
          % (len(test_ids), len(test_ids) - len(failures), len(failures), len(solo_inapplicable)))
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace")
    ap.add_argument("--trace-output", help="save the raw Sts2Oracle JSONL when generating a trace")
    ap.add_argument("--seeds")
    ap.add_argument("--characters", default="IRONCLAD")
    ap.add_argument("--policy-seeds", default="1")
    ap.add_argument("--policy-version", choices=("legacy-v1", "coverage-v2", "coverage-v3"), default=None,
                    help="oracle capture strategy (default coverage-v3); older strategies preserve historical behavior")
    ap.add_argument("--ascension", type=int, default=0)
    ap.add_argument("--max-decisions", type=int, default=3000)
    ap.add_argument("--no-neow", action="store_true", help="start without Neow (ExtraRunFields.StartedWithNeow)")
    ap.add_argument("--hp", type=int, default=0, help="test aid: start with this max HP (both sides)")
    ap.add_argument("--deck", help="replace the starting deck, CARD[+N],...")
    ap.add_argument("--relics", help="obtain extra relics, ID,...")
    ap.add_argument("--relic-matrix", help="run independent routes, one for each comma-separated relic ID")
    ap.add_argument("--all-nonhero-relics", action="store_true",
                    help="run an independent route for every relic in the shared/event/fallback/deprecated pools")
    ap.add_argument("--matrix-workers", type=int, default=4,
                    help="parallel isolated Oracle processes for --all-nonhero-relics")
    ap.add_argument("--force-sacrifice", type=int, default=0,
                    help="oracle test policy: select the first N Pael's Wing sacrifice alternatives")
    ap.add_argument("--stop", choices=("before-act1-boss", "after-act1", "after-act2", "after-loadout", "victory"),
                    help="oracle/replay endpoint; omitted keeps the historical Act 1 boss boundary")
    ap.add_argument("--cli", default=os.path.join(ROOT, "build", "sts2sim_cli"))
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--require-complete", action="store_true",
                    help="fail on Unported; all modes reject truncated traces and native/protocol errors")
    ap.add_argument("--verify-snapshots", action="store_true",
                    help="restore each decision and compare its recorded-action successor on a private branch")
    ap.add_argument("--timeout", type=float, default=180, help="maximum seconds per oracle or replay process")
    ap.add_argument("--artifacts-dir", help="save stdout, stderr, command, process result for every subprocess")
    ap.add_argument("--result-json", help="write structured results, including truncated/native failure categories")
    args = ap.parse_args()
    if args.timeout <= 0:
        ap.error("--timeout must be positive")
    if bool(args.trace) == bool(args.seeds):
        ap.error("exactly one of --trace or --seeds is required")
    if args.trace and args.policy_version is not None:
        ap.error("--policy-version controls oracle capture and is not supported with frozen --trace inputs")
    if args.seeds:
        args.policy_version = args.policy_version or "coverage-v3"
    if args.all_nonhero_relics:
        if not args.seeds:
            raise ValueError("--all-nonhero-relics requires --seeds")
        if args.stop is None:
            args.stop = "after-act1"
        if args.matrix_workers < 1:
            raise ValueError("--matrix-workers must be positive")
        return run_nonhero_relic_matrix(args)
    lines = open(args.trace, encoding="utf-8").read().splitlines() if args.trace else run_oracle(args)
    if args.trace_output:
        with open(args.trace_output, "w", encoding="utf-8") as raw:
            raw.write("\n".join(lines))
            raw.write("\n")
    runs = parse_runs(lines)
    if not args.trace:
        require_policy_version(runs, args.policy_version)
    if args.force_sacrifice:
        for run in runs:
            selected = 0
            for decision, action in zip(run["decisions"], run["actions"]):
                if decision["kind"] != "card_select":
                    continue
                options = decision["options"]
                alternatives = options.get("alternatives", [])
                if "SACRIFICE" not in alternatives:
                    continue
                sacrifice_index = len(options["cards"]) + alternatives.index("SACRIFICE")
                selected += action == f"select 1 {sacrifice_index}"
            if selected < args.force_sacrifice:
                raise ValueError(f"oracle selected only {selected}/{args.force_sacrifice} sacrifices")
    results = evaluate_runs(args, runs)
    if args.result_json:
        write_json(args.result_json, {"schema_version": 1, "summary": summary(results), "results": results})
    for row in results:
        if row["status"] != "identical" or args.verbose:
            print("%s %s: %s" % (row["status"], json.dumps(row["identity"]), json.dumps(row["first"], ensure_ascii=False)))
    stats = summary(results)
    counts = stats["statuses"]
    print("L4: %d runs: %d identical, %d unported, %d other failures; %d/%d decisions matched" %
          (len(results), counts.get("identical", 0), counts.get("unported", 0),
           sum(n for status, n in counts.items() if status not in ("identical", "unported")), stats["matched"], stats["total"]))
    print("L4 statuses: " + json.dumps(counts, sort_keys=True))
    return exit_code(results, args.require_complete)


if __name__ == "__main__":
    try:
        with process_supervision():
            code = main()
        sys.exit(code)
    except (ValueError, KeyError, TypeError, OSError, ProcessFailure) as exc:
        print("L4 failure: " + str(exc), file=sys.stderr)
        sys.exit(2)
