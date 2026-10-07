#!/usr/bin/env python3
"""Freeze actual X1 training trajectories and test the pinned original teacher.

This is offline collection only. Student inputs are its public observation
tables; seeds, real-game roots and teacher plans are never actor inputs.
Run with PYTHONPATH=python and the project .venv.
"""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def collect(args):
    import numpy as np
    import torch
    from nosl import native
    from nosl.branch_credit import buffers, choose
    from nosl.model import load
    from nosl.vec import check_split

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / "collection.json").exists():
        raise ValueError("use a fresh collection directory")
    seeds = [f"{args.seed_prefix}{i:06d}" for i in range(args.first, args.first + args.count)]
    for seed in seeds:
        check_split("train", seed)
    torch.set_num_threads(1)
    model, blob = load(args.checkpoint, "cpu")
    model.eval()
    manifest = {"schema": "sts2.nosl.student-trajectories.v1", "split": "train", "seeds": seeds,
                "checkpoint_sha256": sha(args.checkpoint), "native_sha256": sha(ROOT / "build/libsts2nosl_c.so"),
                "actor_filter": int(blob.get("actor_filter", 0)), "greedy": True,
                "features": model.config["features"], "max_steps": args.max_steps,
                "timeout_seconds": args.timeout, "completed": False}
    (out / "collection.json").write_text(json.dumps(manifest, indent=2) + "\n")
    batch = native.Batch(len(seeds), 192, 160, args.max_steps)
    batch.set_filter(manifest["actor_filter"])
    data = buffers(len(seeds))
    histories = [[] for _ in seeds]
    for i, seed in enumerate(seeds):
        batch.reset(i, seed, 10)
    idle = np.full(len(seeds), -1, np.int32)
    batch.step(idle, *data.values())
    pending = set(range(len(seeds)))
    results = []
    began = time.monotonic()
    with (out / "trajectories.jsonl").open("w") as stream:
        while pending:
            if time.monotonic() - began > args.timeout:
                raise TimeoutError("student trajectory collection deadline")
            running = np.array([i for i in sorted(pending) if data["status"][i] == native.RUNNING], dtype=int)
            for i in sorted(pending - set(running)):
                info = batch.run_info(i)
                row = {"seed": seeds[i], "ascension": 10, "actor_filter": manifest["actor_filter"],
                       "end": native.STATUS_NAMES[int(data["status"][i])], **info, "action_rows": histories[i]}
                if data["status"][i] not in (native.DEATH, native.VICTORY):
                    row["error"] = batch.last_error(i)
                stream.write(json.dumps(row) + "\n")
                stream.flush()
                results.append(row)
                pending.remove(i)
            if not len(running):
                break
            choices = idle.copy()
            for i, action in zip(running, choose(model, data, running, "cpu")):
                histories[i].append(int(action))
                choices[i] = action
            batch.step(choices, *data.values())
    manifest.update(completed=True, wall_seconds=time.monotonic() - began,
                    ends=dict(collections.Counter(r["end"] for r in results)),
                    policy_decisions=sum(len(r["action_rows"]) for r in results))
    (out / "collection.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest))


def teacher(args):
    lock = json.loads((ROOT / "tools/combatsolver/upstream.json").read_text())
    source = ROOT / lock["checkout"]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if commit != lock["commit"]:
        raise ValueError("CombatSolver checkout differs from pinned teacher")
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=source, text=True)
    if dirty:
        raise ValueError("CombatSolver source has tracked modifications")
    dll = ROOT / "tools/NoSLTeacherHarness/bin/Release/net9.0/OfflineSearchHarness.dll"
    solver = source / ".godot/mono/temp/bin/Release/CombatSolver.dll"
    if not dll.exists() or not solver.exists():
        raise ValueError("build tools/NoSLTeacherHarness and pinned CombatSolver first")
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / "invocation.json").exists():
        raise ValueError("refusing to overwrite existing teacher evidence")
    command = [str(args.dotnet), str(dll), "--seed", args.seed, "--script", str(Path(args.script).resolve()),
               "--out", str(out), "--min-floor", str(args.min_floor), "--beam", str(args.beam),
               "--nodes", str(args.nodes), "--budget-ms", str(args.budget_ms)]
    if args.route:
        command += ["--route", str(Path(args.route).resolve())]
    if getattr(args, "root_decision", None) is not None:
        command += ["--root-decision", str(args.root_decision)]
    if getattr(args, "capture_only", False):
        command += ["--capture-only", "true"]
    manifest = {"schema": "sts2.nosl.student-root-invocation.v1", "upstream_commit": commit,
                "command": command, "timeout_seconds": args.timeout, "script_sha256": sha(args.script),
                "harness_sha256": sha(dll), "solver_sha256": sha(solver)}
    if args.route:
        manifest["route_sha256"] = sha(args.route)
    path = out / "invocation.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n")
    with (out / "harness.log").open("w") as log:
        try:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=args.timeout, check=False)
            manifest.update(returncode=result.returncode, status="completed" if result.returncode == 0 else "failed")
        except subprocess.TimeoutExpired:
            manifest["status"] = "timeout"
    path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"status": manifest["status"], "seed": args.seed, "out": str(out)}))
    return 0 if manifest["status"] == "completed" else 1


def verify(args):
    import importlib.util
    spec = importlib.util.spec_from_file_location("root_l4", ROOT / "tests/l4/l4_run.py")
    l4 = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(l4)
    out = Path(args.out).resolve()
    evidence = json.loads((out / "teacher.json").read_text())
    runs = l4.parse_runs((out / "oracle.jsonl").read_text().splitlines())
    if len(runs) != 1 or runs[0]["begin"].get("oracle_test_mode") is not False:
        raise ValueError("expected one normal-rule real-game trace")
    run = runs[0]
    decisions, end, error = l4.replay(str(ROOT / "build/sts2sim_cli"), run, verify_snapshots=True, timeout=args.timeout)
    comparison = l4.compare_run(run, decisions, end, error)
    native_route = evidence.get("status") == "native_route_replayed"
    # L4 intentionally rejects a partial whole run. Do not change its
    # comparator or call this a completed NoSL run. For this separate combat
    # contract require every decision AND the terminal run/RNG/resources.
    terminal_diff = l4.first_diff(run["end"]["run"], (end or {}).get("run"))
    qualified = (comparison["status"] == "truncated" if native_route else comparison["status"] == "identical")
    qualified = bool(qualified and comparison.get("matched") == comparison.get("total") and terminal_diff is None and error is None)
    result = {"schema": "sts2.nosl.teacher-env-contract.v1", "qualified": qualified,
              "scope": "real_combat_route_prefix" if native_route else "whole_frozen_student_run_after_search",
              "l4": comparison, "terminal_diff": terminal_diff}
    (out / "env-contract.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))
    return 0 if qualified else 1


def batch(args):
    collection = Path(args.collection).resolve()
    config = json.loads((collection / "collection.json").read_text())
    if config.get("completed") is not True or config.get("split") != "train":
        raise ValueError("expected a complete frozen training-seed collection")
    trajectories = {row["seed"]: row for row in map(json.loads, (collection / "trajectories.jsonl").read_text().splitlines())}
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / "batch.json").exists():
        raise ValueError("use a fresh teacher batch directory")
    manifest = {"schema": "sts2.nosl.student-root-batch.v1", "seeds": config["seeds"],
                "checkpoint_sha256": config["checkpoint_sha256"],
                "root_rule": f"first combat decision at total_floor >= {args.min_floor}",
                "min_floor": args.min_floor,
                "quality_gate": "qualified routes >= min_qualified and quality gains / all attempted seeds >= 25%",
                "min_qualified": args.min_qualified,
                "beam": 30, "nodes": 10000, "budget_ms": 10000, "timeout_seconds": 90,
                "completed": False}
    (out / "batch.json").write_text(json.dumps(manifest, indent=2) + "\n")
    results = []
    with (out / "results.jsonl").open("w") as stream:
        for seed in config["seeds"]:
            row = {"seed": seed, "status": "started"}
            directory = out / seed
            directory.mkdir()
            trajectory = trajectories[seed]
            rows_file = directory / "rows.txt"
            rows_file.write_text(" ".join(map(str, trajectory["action_rows"])))
            script = directory / "script.jsonl"
            try:
                translated = subprocess.run([str(ROOT / "build/nosl_script"), "--seed", seed, "--ascension", "10",
                                             "--rows-file", str(rows_file), "--out", str(script),
                                             "--filter", str(config["actor_filter"])], capture_output=True, text=True, timeout=90)
                (directory / "translation.log").write_text(translated.stdout + translated.stderr)
                info = json.loads(translated.stdout.strip().splitlines()[-1])
                if translated.returncode or info["status"] != "complete":
                    raise ValueError("frozen student action translation incomplete: " + str(info))
                search = directory / "search"
                options = argparse.Namespace(dotnet=args.dotnet, seed=seed, script=script, out=search,
                                             min_floor=args.min_floor, beam=30, nodes=10000, budget_ms=10000, timeout=90, route=None)
                code = teacher(options)
                evidence = json.loads((search / "teacher.json").read_text()) if (search / "teacher.json").exists() else {}
                row["search_status"] = evidence.get("status", "missing")
                if code or evidence.get("status") != "searched":
                    raise ValueError("search not qualified: " + str(row["search_status"]))
                if verify(argparse.Namespace(out=search, timeout=90)):
                    raise ValueError("student continuation changed after teacher search")
                row.update(search_qualified=True, all_enemies_dead=evidence["all_enemies_dead"],
                           wall_seconds=evidence["wall_seconds"], floor=evidence["floor"],
                           decision=evidence["decision"], projected_loss=evidence["metrics"]["projectedBattleHpLost"])
                if not evidence["all_enemies_dead"] or evidence["player_dead"]:
                    row["status"] = "incomplete_route"
                else:
                    replay_dir = directory / "native"
                    options.out, options.route = replay_dir, search / "teacher.json"
                    if teacher(options):
                        raise ValueError("teacher native route failed")
                    if verify(argparse.Namespace(out=replay_dir, timeout=90)):
                        raise ValueError("teacher route disagrees with Env")
                    native = json.loads((replay_dir / "teacher.json").read_text())
                    trace = [json.loads(line) for line in (search / "oracle.jsonl").read_text().splitlines()]
                    decisions = [event for event in trace if event["event"] == "decision"]
                    later = [event for event in decisions[evidence["decision"] + 1:]
                             if event["kind"] in {"rewards", "map", "event", "rest", "shop", "relic_select"}]
                    terminal = later[0]["run"] if later else evidence["student_end"]["run"]
                    if terminal["total_floor"] != evidence["floor"]:
                        raise ValueError("cannot identify student terminal for the same battle")
                    student_died = evidence["student_end"]["reason"] == "death" and terminal["hp"] <= 0
                    gain = native["terminal_hp"] - terminal["hp"]
                    row.update(status="route_qualified", teacher_terminal_hp=native["terminal_hp"],
                               student_terminal_hp=terminal["hp"], student_died=student_died,
                               terminal_hp_gain=gain, quality_gain=bool(student_died or gain >= 3),
                               actions_replayed=native["actions_replayed"], choices_replayed=native["choices_replayed"])
            except Exception as exc:
                row.update(status="failed", error=f"{type(exc).__name__}: {exc}")
            results.append(row)
            stream.write(json.dumps(row) + "\n")
            stream.flush()
            print(json.dumps(row), flush=True)
    searched = sum(bool(r.get("search_qualified")) for r in results)
    qualified = sum(r["status"] == "route_qualified" for r in results)
    gains = sum(bool(r.get("quality_gain")) for r in results)
    summary = {"attempts": len(results), "searched": searched, "qualified_routes": qualified,
               "quality_gains": gains, "gain_fraction_all_attempts": gains / len(results),
               "quality_gate_passed": qualified >= args.min_qualified and gains / len(results) >= .25,
               "statuses": dict(collections.Counter(r["status"] for r in results)),
               "source_collection": str(collection), "checkpoint_sha256": config["checkpoint_sha256"]}
    manifest.update(completed=True)
    (out / "batch.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))
    return 0


def audit_batch(args):
    """Recompute quality from saved real terminal boundaries, never rerun/search.

    Combat card_select decisions are inside the battle and cannot be used as
    the student's terminal HP. Preserve original measurements; write an
    audited table and the final quality summary from the correct boundary.
    """
    out = Path(args.out).resolve()
    manifest = json.loads((out / "batch.json").read_text())
    if manifest.get("completed") is not True:
        raise ValueError("wait for the frozen batch to finish")
    rows = [json.loads(line) for line in (out / "results.jsonl").read_text().splitlines()]
    for row in rows:
        if row["status"] != "route_qualified":
            continue
        directory = out / row["seed"]
        source = json.loads((directory / "search/teacher.json").read_text())
        native = json.loads((directory / "native/teacher.json").read_text())
        events = [json.loads(line) for line in (directory / "search/oracle.jsonl").read_text().splitlines()]
        decisions = [event for event in events if event["event"] == "decision"]
        later = [event for event in decisions[source["decision"] + 1:]
                 if event["kind"] in {"rewards", "map", "event", "rest", "shop", "relic_select"}]
        terminal = later[0]["run"] if later else source["student_end"]["run"]
        if terminal["total_floor"] != source["floor"]:
            raise ValueError("student terminal floor differs for " + row["seed"])
        died = source["student_end"]["reason"] == "death" and terminal["hp"] <= 0
        gain = native["terminal_hp"] - terminal["hp"]
        row.update(student_terminal_hp=terminal["hp"], student_died=died,
                   terminal_hp_gain=gain, quality_gain=bool(died or gain >= 3))
    (out / "audited-results.jsonl").write_text("".join(json.dumps(row) + "\n" for row in rows))
    searched = sum(bool(row.get("search_qualified")) for row in rows)
    gains = sum(bool(row.get("quality_gain")) for row in rows)
    summary = {"attempts": len(rows), "searched": searched,
               "qualified_routes": sum(row["status"] == "route_qualified" for row in rows),
               "quality_gains": gains, "gain_fraction_all_attempts": gains / len(rows),
               "quality_gate_passed": sum(row["status"] == "route_qualified" for row in rows) >= manifest.get("min_qualified", 12) and gains / len(rows) >= .25,
               "statuses": dict(collections.Counter(row["status"] for row in rows)),
               "summary_source": "audited-results.jsonl", "checkpoint_sha256": manifest["checkpoint_sha256"]}
    (out / "audited-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    c = sub.add_parser("collect")
    c.add_argument("--checkpoint", required=True)
    c.add_argument("--seed-prefix", default="NQ4N")
    c.add_argument("--first", type=int, default=0)
    c.add_argument("--count", type=int, default=16)
    c.add_argument("--max-steps", type=int, default=30000)
    c.add_argument("--timeout", type=float, default=300)
    c.add_argument("--out", required=True)
    c.set_defaults(func=collect)
    t = sub.add_parser("teacher")
    t.add_argument("--dotnet", type=Path, default=Path.home() / ".dotnet/dotnet")
    t.add_argument("--seed", required=True)
    t.add_argument("--script", required=True)
    t.add_argument("--out", required=True)
    t.add_argument("--min-floor", type=int, default=8)
    t.add_argument("--beam", type=int, default=30)
    t.add_argument("--nodes", type=int, default=10000)
    t.add_argument("--budget-ms", type=int, default=10000)
    t.add_argument("--timeout", type=float, default=90)
    t.add_argument("--route", help="replay this frozen teacher.json instead of searching")
    t.add_argument("--root-decision", type=int, help="capture this exact original decision, including same-floor combats")
    t.add_argument("--capture-only", action="store_true", help="stop before acting at the captured original-game root")
    t.set_defaults(func=teacher)
    v = sub.add_parser("verify")
    v.add_argument("--out", required=True)
    v.add_argument("--timeout", type=float, default=90)
    v.set_defaults(func=verify)
    b = sub.add_parser("batch")
    b.add_argument("--collection", required=True)
    b.add_argument("--out", required=True)
    b.add_argument("--dotnet", type=Path, default=Path.home() / ".dotnet/dotnet")
    b.add_argument("--min-qualified", type=int, default=12)
    b.add_argument("--min-floor", type=int, default=8)
    b.set_defaults(func=batch)
    a = sub.add_parser("audit")
    a.add_argument("--out", required=True)
    a.set_defaults(func=audit_batch)
    args = p.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
