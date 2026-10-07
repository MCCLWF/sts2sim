#!/usr/bin/env python3
"""Replay policy trajectories in the real game and compare every decision.

  python3 tools/nosl/oracle_replay.py --trajectories runs.jsonl --out artifacts/nosl-e01/batch1 [--jobs 4]

For each {seed, ascension, action_rows} line (python -m nosl.evaluate --trajectories):
  1. build/nosl_script turns the Env trajectory into an oracle action script;
  2. Sts2Oracle `run --script` makes the shipped game follow that script and
     dump its state at every decision;
  3. tests/l4/l4_run.py --trace replays the game's trace in the simulator and
     compares each decision (the existing L4 comparison, unchanged).

Statuses, one per trajectory (every trajectory is reported):
  identical     the game followed the whole trajectory to its death or victory
                and every compared decision matched
  prefix        the script stops before the run ends: the oracle driver cannot
                express an action (see nosl_script), or the trajectory itself was
                cut at the sampler's step limit; every scripted decision matched
  script_refused  the game refused a scripted action or asked for another kind
                of decision: the simulator and the game disagree there
  mismatch      the L4 comparison found a difference
  tool_error    a step of this pipeline failed
"""
import argparse
import collections
import concurrent.futures
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = ROOT / "tools" / "Sts2Oracle" / "bin" / "Release" / "net9.0" / "Sts2Oracle.dll"
SCRIPT_TOOL = Path(os.environ.get("NOSL_SCRIPT", ROOT / "build" / "nosl_script"))


def one(job):
    trajectory, out, timeout, test_mode = job
    seed = trajectory["seed"]
    work = out / seed
    work.mkdir(parents=True, exist_ok=True)
    row = {"seed": seed, "env_end": trajectory.get("end"), "floor": trajectory.get("floor"),
           "policy_decisions": len(trajectory["action_rows"]), "oracle_test_mode": test_mode == "on"}
    try:
        (work / "rows.txt").write_text(" ".join(str(r) for r in trajectory["action_rows"]))
        script = subprocess.run([str(SCRIPT_TOOL), "--seed", seed, "--ascension", str(trajectory["ascension"]),
                                 "--rows-file", str(work / "rows.txt"), "--out", str(work / "script.jsonl"),
                                 "--filter", str(trajectory.get("actor_filter", 0))],
                                capture_output=True, text=True, timeout=timeout)
        info = json.loads(script.stdout.strip().splitlines()[-1])
        row.update({"script": info["status"], "script_actions": info["script_actions"], "env_actions": info["env_actions"],
                    "script_reason": info["reason"]})
        if info["status"] == "error" or info["script_actions"] == 0:
            return {**row, "status": "tool_error", "detail": "nosl_script: " + info["reason"]}
        with (work / "oracle.jsonl").open("w") as trace, (work / "oracle.stderr.txt").open("w") as err:
            oracle = subprocess.run([DOTNET, str(ORACLE), "run", "--seed", seed, "--character", "SILENT",
                                     "--ascension", str(trajectory["ascension"]), "--script", str(work / "script.jsonl"),
                                     "--stop", "victory", "--oracle-test-mode", test_mode],
                                    stdout=trace, stderr=err, timeout=timeout)
        end = None
        begin = None
        for line in (work / "oracle.jsonl").read_text().splitlines():
            if '"event":"begin"' in line.replace(" ", ""):
                begin = json.loads(line)
            if '"event":"end"' in line.replace(" ", ""):
                end = json.loads(line)
        if begin is None or begin.get("oracle_test_mode") is not (test_mode == "on"):
            return {**row, "status": "tool_error", "detail": "oracle mode differs from requested mode or begin event is missing"}
        row["oracle_end"] = end.get("reason") if end else None
        if oracle.returncode != 0 or end is None:
            return {**row, "status": "tool_error", "detail": f"oracle exit {oracle.returncode}"}
        if end.get("reason") == "error":
            return {**row, "status": "script_refused", "detail": end.get("message", "")[:300]}
        compare = subprocess.run([sys.executable, str(ROOT / "tests" / "l4" / "l4_run.py"), "--trace", str(work / "oracle.jsonl"),
                                  "--result-json", str(work / "l4.json"), "--timeout", str(timeout)],
                                 capture_output=True, text=True, timeout=timeout * 2)
        (work / "l4.stdout.txt").write_text(compare.stdout + compare.stderr)
        result = json.loads((work / "l4.json").read_text())["results"][0]
        row["l4_status"] = result["status"]
        row["matched"] = result.get("matched")
        if result["status"] == "identical":
            return {**row, "status": "identical" if info["status"] == "complete" else "prefix"}
        # A script that stops before the run ends (an inexpressible action, or a
        # trajectory the sampler cut at its step limit) stops the game at its
        # last action: L4 calls that truncated. It is a clean prefix when
        # every scripted decision matched.
        unfinished = info["status"] == "inexpressible" or info["end"] == "running"
        if result["status"] == "truncated" and unfinished and result.get("matched") == info["script_actions"]:
            return {**row, "status": "prefix"}
        return {**row, "status": "mismatch", "detail": json.dumps(result.get("first"), ensure_ascii=False)[:400]}
    except Exception as exc:  # a broken step is a result, not a crash of the batch
        return {**row, "status": "tool_error", "detail": f"{type(exc).__name__}: {exc}"[:300]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--trajectories", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--timeout", type=float, default=300)
    ap.add_argument("--oracle-test-mode", choices=("off", "on"), default="off",
                    help="normal-rule qualification requires off (default); on is legacy diagnostics only")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    trajectories = [json.loads(line) for line in open(args.trajectories, encoding="utf-8") if line.strip()]
    if args.limit:
        trajectories = trajectories[:args.limit]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        rows = list(pool.map(one, [(t, out, args.timeout, args.oracle_test_mode) for t in trajectories]))
    with (out / "results.jsonl").open("w", encoding="utf-8") as stream:
        for row in rows:
            stream.write(json.dumps(row, ensure_ascii=False) + "\n")
    statuses = collections.Counter(r["status"] for r in rows)
    summary = {"trajectories": len(rows), "statuses": dict(statuses),
               "oracle_test_mode": args.oracle_test_mode == "on",
               "decisions_matched": sum(r.get("matched") or 0 for r in rows),
               "inexpressible_reasons": dict(collections.Counter(r["script_reason"] for r in rows if r.get("script") == "inexpressible")),
               "first_problems": [{k: r.get(k) for k in ("seed", "status", "detail")} for r in rows
                                  if r["status"] not in ("identical", "prefix")][:10]}
    (out / "summary.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False))
    print(json.dumps(summary, ensure_ascii=False))
    return 0 if set(statuses) <= {"identical", "prefix"} else 1


if __name__ == "__main__":
    sys.exit(main())
