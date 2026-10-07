#!/usr/bin/env python3
"""Run CombatSolver's native offline harness and sts2sim Env in parallel.

The two searches intentionally use different policies.  A different first
action is therefore a policy divergence; a root mismatch is a simulator/env
divergence.  Solver timings exclude process startup and are read from each
solver's own timing field.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any


ENEMY_RE = re.compile(r"(?P<monster>[A-Z0-9_]+)#(?P<id>-?\d+):(?P<hp>-?\d+)/(?P<max_hp>-?\d+)@")
ROOT_RE = re.compile(
    r"energy=(?P<energy>-?\d+) hp=(?P<hp>-?\d+)/(?P<max_hp>-?\d+) "
    r"hand=\[(?P<hand>[^]]*)\].*?enemies=\[(?P<enemies>[^]]*)\]"
)
TARGET_RE = re.compile(r"target=(?P<target>-?\d+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-harness", required=True, type=Path,
                        help="OfflineSearchHarness.dll built from CombatSolver")
    parser.add_argument("--dotnet", type=Path, default=Path.home() / ".dotnet/dotnet")
    parser.add_argument("--env-solver", type=Path, default=Path("build/sts2sim_env_solver"))
    parser.add_argument("--seeds", default="DUAL001")
    parser.add_argument("--character", default="IRONCLAD")
    parser.add_argument("--encounter", default="CULTISTS_NORMAL")
    parser.add_argument("--ascension", type=int, default=0)
    parser.add_argument("--act-index", type=int, default=0)
    parser.add_argument("--native-profile", default="Low")
    parser.add_argument("--native-budget-ms", type=int, default=500)
    parser.add_argument("--env-node-budget", "--env-iterations", dest="env_node_budget", type=int, default=1000)
    parser.add_argument("--env-beam-width", type=int, default=8)
    parser.add_argument("--env-depth", "--env-rollout-depth", dest="env_depth", type=int, default=16)
    parser.add_argument("--out", type=Path, default=Path("build/dual-solver-report.json"))
    return parser.parse_args()


def run_process(command: list[str]) -> tuple[int, str, str, float]:
    started = time.perf_counter()
    proc = subprocess.run(command, text=True, capture_output=True)
    return proc.returncode, proc.stdout, proc.stderr, time.perf_counter() - started


def parse_env(stdout: str) -> dict[str, Any]:
    lines = [line for line in stdout.splitlines() if line.strip()]
    if not lines:
        raise ValueError("env solver produced no JSON")
    return json.loads(lines[-1])


def parse_native(out_dir: Path) -> dict[str, Any]:
    result_path = out_dir / "harness-result.json"
    if not result_path.exists():
        raise ValueError(f"native harness did not write {result_path}")
    return json.loads(result_path.read_text())


def parse_native_root(text: str) -> dict[str, Any]:
    match = ROOT_RE.search(text)
    if not match:
        raise ValueError(f"cannot parse native root: {text}")
    hand = [item for item in match.group("hand").split(",") if item]
    enemies = [
        {
            "monster": item.group("monster"),
            "id": int(item.group("id")),
            "hp": int(item.group("hp")),
            "max_hp": int(item.group("max_hp")),
        }
        for item in ENEMY_RE.finditer(match.group("enemies"))
    ]
    return {
        "hand": hand,
        "enemies": enemies,
        "energy": int(match.group("energy")),
        "hp": int(match.group("hp")),
        "max_hp": int(match.group("max_hp")),
    }


def parse_native_action(native: dict[str, Any]) -> dict[str, Any] | None:
    actions = native.get("search", {}).get("planActions", [])
    if not actions:
        return None
    text = actions[0]
    pieces = text.split(":", 3)
    if len(pieces) < 3:
        return {"raw": text}
    target_match = TARGET_RE.search(text)
    return {
        "raw": text,
        "turn": int(pieces[0]),
        "kind": pieces[1],
        "card": pieces[2] if pieces[2] != "-" else None,
        "target": int(target_match.group("target")) if target_match else None,
    }


def normalize_env_root(env: dict[str, Any]) -> dict[str, Any]:
    root = env["root"]
    return {
        "hand": root["hand"],
        "enemies": root["enemies"],
        "energy": root["energy"],
        "hp": root["hp"],
        # Native root has the same scalar but does not expose legal actions.
        "max_hp": None,
    }


def root_match(env: dict[str, Any], native: dict[str, Any]) -> bool:
    left = normalize_env_root(env)
    right = {key: native[key] for key in left}
    if left["hand"] != right["hand"] or left["energy"] != right["energy"] or left["hp"] != right["hp"]:
        return False
    left_enemies = [
        {key: item[key] for key in ("monster", "id", "hp", "max_hp")}
        for item in left["enemies"]
    ]
    return left_enemies == right["enemies"]


def action_match(env: dict[str, Any], native: dict[str, Any] | None) -> bool:
    if native is None:
        return False
    action = env["action"]
    return action.get("kind") == native.get("kind") \
        and action.get("card") == native.get("card") \
        and action.get("target") == native.get("target")


def one_case(args: argparse.Namespace, seed: str, root_dir: Path) -> dict[str, Any]:
    case_dir = root_dir / seed
    native_out = case_dir / "native"
    native_workspace = case_dir / "workspace"
    native_out.mkdir(parents=True)
    native_workspace.mkdir(parents=True)
    native_cmd = [
        str(args.dotnet), str(args.native_harness), "--label", seed,
        "--character", args.character, "--encounter", args.encounter,
        "--seed", seed, "--ascension", str(args.ascension),
        "--act-index", str(args.act_index), "--profile", args.native_profile,
        "--budget-ms", str(args.native_budget_ms), "--out", str(native_out),
        "--workspace", str(native_workspace),
    ]
    env_cmd = [
        str(args.env_solver), "--seed", seed, "--character", args.character,
        "--encounter", args.encounter, "--ascension", str(args.ascension),
        "--act-index", str(args.act_index), "--node-budget", str(args.env_node_budget),
        "--beam-width", str(args.env_beam_width), "--rollout-depth", str(args.env_depth),
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        native_future = pool.submit(run_process, native_cmd)
        env_future = pool.submit(run_process, env_cmd)
        native_code, native_stdout, native_stderr, native_process_seconds = native_future.result()
        env_code, env_stdout, env_stderr, env_process_seconds = env_future.result()

    result: dict[str, Any] = {
        "seed": seed,
        "native_process_exit": native_code,
        "env_process_exit": env_code,
        "native_process_seconds": native_process_seconds,
        "env_process_seconds": env_process_seconds,
        "native_stderr": native_stderr[-4000:],
        "env_stderr": env_stderr[-4000:],
    }
    if env_code == 0:
        result["env"] = parse_env(env_stdout)
    if native_code == 0:
        native = parse_native(native_out)
        result["native"] = {
            "root_text": native.get("root"),
            "root": parse_native_root(native["root"]),
            "action": parse_native_action(native),
            "wall_seconds": native.get("wallSeconds"),
            "solver_metrics": native.get("solverMetrics", {}),
        }
    if "env" in result and "native" in result:
        result["root_match"] = root_match(result["env"], result["native"]["root"])
        result["action_match"] = action_match(result["env"], result["native"]["action"])
        native_ms = result["native"]["solver_metrics"].get("TotalElapsedMilliseconds")
        if native_ms is None and result["native"].get("wall_seconds") is not None:
            native_ms = float(result["native"]["wall_seconds"]) * 1000.0
        env_ms = result["env"].get("elapsed_ms")
        result["performance"] = {
            "env_solver_ms": env_ms,
            "native_solver_ms": native_ms,
            "native_over_env_speed_ratio": (native_ms / env_ms) if env_ms and native_ms else None,
            "env_node_budget": result["env"].get("node_budget"),
            "env_beam_width": result["env"].get("beam_width"),
            "native_expanded_nodes": result["native"]["solver_metrics"].get("SelectedExpanded"),
        }
    return result


def main() -> int:
    args = parse_args()
    if not args.native_harness.exists():
        raise SystemExit(f"native harness not found: {args.native_harness}")
    if not args.env_solver.exists():
        raise SystemExit(f"env solver not found: {args.env_solver}")
    seeds = [seed for seed in args.seeds.split(",") if seed]
    with tempfile.TemporaryDirectory(prefix="sts2sim-dual-") as temp:
        cases = [one_case(args, seed, Path(temp)) for seed in seeds]
    report = {
        "schema_version": 1,
        "character": args.character,
        "encounter": args.encounter,
        "ascension": args.ascension,
        "native_profile": args.native_profile,
        "native_budget_ms": args.native_budget_ms,
        "env_node_budget": args.env_node_budget,
        "env_beam_width": args.env_beam_width,
        "env_depth": args.env_depth,
        "cases": cases,
        "summary": {
            "cases": len(cases),
            "root_matches": sum(case.get("root_match") is True for case in cases),
            "action_matches": sum(case.get("action_match") is True for case in cases),
            "root_mismatches": sum(case.get("root_match") is False for case in cases),
            "action_mismatches": sum(case.get("action_match") is False for case in cases),
        },
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps(report["summary"], ensure_ascii=False))
    for case in cases:
        perf = case.get("performance", {})
        print(f"{case['seed']}: root={'MATCH' if case.get('root_match') else 'MISMATCH'} "
              f"action={'MATCH' if case.get('action_match') else 'DIFF'} "
              f"env_ms={perf.get('env_solver_ms')} native_ms={perf.get('native_solver_ms')} "
              f"native/env={perf.get('native_over_env_speed_ratio')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
