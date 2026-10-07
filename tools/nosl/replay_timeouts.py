#!/usr/bin/env python3
"""Replay recorded NoSL runs sequentially, killing each timed-out native process.

  taskset -c 0 nice -n 10 .venv/bin/python tools/nosl/replay_timeouts.py \
      --trajectories artifacts/nosl-q4f/kt1/failures.jsonl --status-name hang \
      --ascension 10 --actor-filter 1 --out artifacts/nosl-native-hang/replay

The input's ascension/actor_filter take precedence over the explicit fallbacks.
nosl_decision_diag applies the observation table, actor filter and auto_forced
semantics. A successful replay may end at a live prefix; it does not finish
unrecorded decisions. before_step/after_step progress is flushed by the native
tool, so a timeout retains its last action boundary.
"""
from __future__ import annotations

import argparse
import collections
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--trajectories", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--status-name")
    parser.add_argument("--ascension", type=int, default=10)
    parser.add_argument("--actor-filter", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=5)
    parser.add_argument("--diag", type=Path, default=ROOT / "build/nosl_decision_diag")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    args.out.mkdir(parents=True, exist_ok=True)
    statuses = collections.Counter()
    with args.trajectories.open() as source, (args.out / "results.jsonl").open("w") as results:
        for index, line in enumerate(source):
            if not line.strip():
                continue
            original = json.loads(line)
            if args.status_name and original.get("status_name") != args.status_name:
                continue
            trajectory = {"seed": original["seed"], "action_rows": original["action_rows"],
                          "ascension": original.get("ascension", args.ascension),
                          "actor_filter": original.get("actor_filter", args.actor_filter)}
            work = args.out / f"run-{index:05d}"
            work.mkdir(exist_ok=True)
            input_path = work / "trajectory.jsonl"
            input_path.write_text(json.dumps(trajectory) + "\n")
            progress_path = work / "progress.jsonl"
            progress_path.unlink(missing_ok=True)
            started = time.monotonic()
            row = {"index": index, "seed": trajectory["seed"], "rows": len(trajectory["action_rows"])}
            with (work / "stdout.txt").open("w") as stdout, (work / "stderr.txt").open("w") as stderr:
                try:
                    process = subprocess.run([str(args.diag), "--trajectories", str(input_path),
                                              "--out", str(work / "diag.jsonl"), "--threads", "1",
                                              "--progress", str(progress_path)],
                                             stdout=stdout, stderr=stderr, timeout=args.timeout)
                    row.update(status="ok" if process.returncode == 0 else "error", returncode=process.returncode)
                except subprocess.TimeoutExpired:
                    # subprocess.run kills and waits for this process before the next run.
                    row["status"] = "timeout"
            row["seconds"] = round(time.monotonic() - started, 6)
            if progress_path.exists():
                last = None
                with progress_path.open() as progress:
                    for boundary in progress:
                        last = json.loads(boundary)
                row["last_boundary"] = last
            results.write(json.dumps(row) + "\n")
            results.flush()
            statuses[row["status"]] += 1
            if row["status"] != "ok":
                print(json.dumps(row), flush=True)
    summary = {"runs": sum(statuses.values()), "statuses": dict(statuses), "timeout_seconds": args.timeout}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))
    return int(bool(statuses["error"] or statuses["timeout"]))


if __name__ == "__main__":
    raise SystemExit(main())
