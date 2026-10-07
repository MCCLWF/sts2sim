#!/usr/bin/env python3
"""The sampler watchdog: a worker that stops answering costs its runs, not the sampler.

  PYTHONPATH=python .venv/bin/python tools/nosl/check_vec_watchdog.py

One worker is frozen with SIGSTOP in the middle of sampling. The check passes
when the sampler reports that worker's in-flight runs as `hang` with their
seeds and action histories, replaces the worker, and keeps sampling in every
slot. Exit code 1 otherwise.
"""
import json
import os
import signal
import sys
import tempfile
from pathlib import Path

import numpy as np

from nosl import native
from nosl.vec import HANG, VecSampler


def main():
    workers, per = 3, 8
    rng = np.random.default_rng(7)
    with tempfile.TemporaryDirectory() as temp:
        log = Path(temp) / "failures.jsonl"
        with VecSampler(workers, per, split="train", seed_prefix="NTWD", timeout=3.0, failure_log=log) as vec:
            vec.reset()

            def step():
                n = vec.arrays["counts"][:, 1]
                choice = np.where(vec.running(), (rng.random(vec.runs) * np.maximum(n, 1)).astype(np.int32), -1)
                return vec.step(choice.astype(np.int32))

            for _ in range(30):
                step()
            frozen = 1
            lo, hi = frozen * per, (frozen + 1) * per
            seeds = set(vec.seed_of[lo:hi])
            histories = {vec.seed_of[s]: len(vec.history[s]) + 1 for s in range(lo, hi)}
            old_pid = vec._procs[frozen].pid
            os.kill(old_pid, signal.SIGSTOP)
            results = step()
            hung = [r for r in results if r["status"] == HANG]
            problems = []
            if vec.hangs != 1:
                problems.append(f"hangs={vec.hangs}, expected 1")
            if {r["seed"] for r in hung} != seeds:
                problems.append("the hang rows are not exactly the frozen worker's in-flight runs")
            if not all(vec.arrays["ended"][r["slot"]] == HANG for r in hung):
                problems.append("ended[] does not mark the hung slots")
            if vec._procs[frozen].pid == old_pid or not vec._procs[frozen].is_alive():
                problems.append("the worker was not replaced")
            if not vec.running().all():
                problems.append("some slot is not running after the replacement")
            logged = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
            logged = {row["seed"]: row for row in logged if row["status_name"] == "hang"}
            if set(logged) != seeds or any(len(logged[s]["action_rows"]) != histories[s] for s in seeds):
                problems.append("the failure log lacks a seed or its full action history")
            for _ in range(30):
                step()
            if not vec.running().all() or vec.hangs != 1:
                problems.append("sampling did not continue normally")
    print(json.dumps({"hang_rows": len(hung), "frozen_runs": len(seeds), "problems": problems}))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
