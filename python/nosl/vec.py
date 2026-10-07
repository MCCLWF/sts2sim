"""Process-isolated vector sampler over the native NoSL batch.

Each worker process owns one single-threaded native batch and writes
observations into shared memory. A worker that does not answer within the
timeout is killed and replaced, and its in-flight runs are reported as
`hang` with their seeds and action histories, so a native endless loop or a
crash costs those runs instead of the whole sampler.
"""
from __future__ import annotations

import json
import multiprocessing as mp
import time
from multiprocessing import shared_memory
from pathlib import Path

import numpy as np

from . import native

HANG = 7
STATUS_NAMES = native.STATUS_NAMES + ("hang",)

# Registered data split (docs/plans/2026-10-03-nosl-q1-foundation.md).
DEV_PREFIX, TEST_PREFIX = "NOSLDEV", "NOSLTEST"


def check_split(split: str, seed: str, final_eval: bool = False) -> None:
    dev, test = seed.startswith(DEV_PREFIX), seed.startswith(TEST_PREFIX)
    if split == "train" and (dev or test):
        raise ValueError(f"training may not use the held-out seed {seed}")
    if split == "dev" and not dev:
        raise ValueError(f"{seed} is not a dev seed")
    if split == "test" and not (test and final_eval):
        raise ValueError("the test split needs NOSLTEST seeds and final_eval=True")
    if split not in ("train", "dev", "test"):
        raise ValueError(f"unknown split {split}")


def _arrays(runs: int, max_tokens: int, max_actions: int):
    return {
        "choice": (runs,),
        "global": (runs, native.GLOBAL_WIDTH),
        "tokens": (runs, max_tokens, native.TOKEN_WIDTH),
        "actions": (runs, max_actions, native.ACTION_WIDTH),
        "counts": (runs, 2),
        "status": (runs,),
        "steps": (runs,),
        "ended": (runs,),
    }


def _worker(conn, index, lo, hi, shm_names, shapes, config, seeds):
    """seeds: a list of seeds to play once, or None for an endless training stream.

    A new run costs milliseconds (map generation), so runs are prepared in
    spare slots while the worker waits for its next command and swapped in
    when a run ends."""
    blocks = {k: shared_memory.SharedMemory(name=n) for k, n in shm_names.items()}
    view = {k: np.ndarray(shapes[k], dtype=np.int32, buffer=blocks[k].buf)[lo:hi] for k in shapes}
    n, spares = hi - lo, config["spares"]
    batch = native.Batch(n + spares, config["max_tokens"], config["max_actions"], config["max_steps"])
    batch.set_active(n)
    batch.set_filter(config["actor_filter"])
    current = [None] * n
    spare_seed = [None] * spares
    pending = []  # runs that failed or ended inside their reset
    counter = 0
    queue = list(seeds) if seeds is not None else None
    idle = np.full(n, -1, dtype=np.int32)

    def next_seed():
        nonlocal counter
        if queue is not None:
            return queue.pop(0) if queue else None
        counter += 1
        return f"{config['seed_prefix']}{index:02d}{config['generation'] % 10}{counter:06d}"

    def result(j, status):
        row = {"slot": lo + j, "seed": current[j], "status": int(status), "steps": int(view["steps"][j]), **batch.run_info(j)}
        if status in (native.UNPORTED, native.ERROR, native.CAPACITY):
            row["message"] = batch.last_error(j)[:300]
        return row

    def prepare(s):
        """Reset spare `s` on the next seed. False when no seed is left."""
        for _ in range(50):
            seed = next_seed()
            if seed is None:
                return False
            status = batch.reset(n + s, seed, config["ascension"])
            if status == native.RUNNING:
                spare_seed[s] = seed
                return True
            row = {"slot": -1, "seed": seed, "status": int(status), "steps": 0, **batch.run_info(n + s)}
            if status in (native.UNPORTED, native.ERROR, native.CAPACITY):
                row["message"] = batch.last_error(n + s)[:300]
            pending.append(row)
        raise RuntimeError("50 consecutive runs failed to start")

    def take(j, started):
        ready = next((s for s in range(spares) if spare_seed[s] is not None), None)
        if ready is None and prepare(0):
            ready = 0
        if ready is None:
            current[j] = None  # no seed left: the slot stays parked on its ended run
            return
        batch.swap(j, n + ready)
        current[j], spare_seed[ready] = spare_seed[ready], None
        started.append((lo + j, current[j]))

    def write():
        batch.step(idle, view["global"], view["tokens"], view["actions"], view["counts"],
                   view["status"], view["steps"], config["auto_forced"])

    try:
        while True:
            while not conn.poll(0):
                empty = next((s for s in range(spares) if spare_seed[s] is None), None)
                if empty is None or not prepare(empty):
                    break
            command = conn.recv()
            results, started = [], []
            if command == "reset":
                view["ended"][:] = 0
                for j in range(n):
                    take(j, started)
                write()
            elif command == "step":
                batch.step(view["choice"], view["global"], view["tokens"], view["actions"], view["counts"],
                           view["status"], view["steps"], config["auto_forced"])
                view["ended"][:] = 0
                restarted = False
                for j in range(n):
                    status = int(view["status"][j])
                    if status == native.RUNNING or current[j] is None:
                        continue
                    view["ended"][j] = status
                    results.append(result(j, status))
                    take(j, started)
                    restarted = True
                if restarted:
                    write()
            elif command == "close":
                break
            results.extend(pending)
            pending.clear()
            conn.send((results, started))
    finally:
        for block in blocks.values():
            block.close()


class VecSampler:
    """`workers * runs_per_worker` runs. Arrays are views of shared memory:
    copy what must outlive the next step."""

    def __init__(self, workers: int, runs_per_worker: int, *, split: str, ascension: int = 10,
                 max_tokens: int = 192, max_actions: int = 160, max_steps: int = 30000,
                 seed_prefix: str = "NT", seeds: list[str] | None = None, final_eval: bool = False,
                 timeout: float = 120.0, auto_forced: bool = True, spares: int = 4, keep_history: bool = False,
                 actor_filter: int = 0,
                 failure_log: str | Path | None = None):
        if seeds is None:
            check_split(split, seed_prefix + "00")
            if split != "train":
                raise ValueError("an endless seed stream is only for training")
        else:
            for seed in seeds:
                check_split(split, seed, final_eval)
        self.workers, self.per = workers, runs_per_worker
        self.runs = workers * runs_per_worker
        self.timeout = timeout
        self.keep_history = keep_history  # attach each run's action rows to its result
        self.failure_log = Path(failure_log) if failure_log else None
        self.config = {"max_tokens": max_tokens, "max_actions": max_actions, "max_steps": max_steps,
                       "ascension": ascension, "seed_prefix": seed_prefix, "auto_forced": auto_forced, "generation": 0,
                       "spares": spares, "actor_filter": actor_filter}
        self.shapes = _arrays(self.runs, max_tokens, max_actions)
        self._blocks = {k: shared_memory.SharedMemory(create=True, size=int(np.prod(s)) * 4) for k, s in self.shapes.items()}
        self.arrays = {k: np.ndarray(s, dtype=np.int32, buffer=self._blocks[k].buf) for k, s in self.shapes.items()}
        for array in self.arrays.values():
            array[:] = 0
        self.arrays["choice"][:] = -1
        self._context = mp.get_context("spawn")
        self._seed_lists = None if seeds is None else [seeds[w::workers] for w in range(workers)]
        self._procs, self._conns = [None] * workers, [None] * workers
        self.seed_of = [None] * self.runs
        self.history = [[] for _ in range(self.runs)]
        self.hangs = 0
        self._done = set()
        for w in range(workers):
            self._spawn(w, self._seed_lists[w] if self._seed_lists else None)

    def _spawn(self, w, seeds):
        parent, child = self._context.Pipe()
        lo, hi = w * self.per, (w + 1) * self.per
        names = {k: b.name for k, b in self._blocks.items()}
        proc = self._context.Process(target=_worker, args=(child, w, lo, hi, names, self.shapes, dict(self.config), seeds),
                                     daemon=True)
        proc.start()
        child.close()
        self._procs[w], self._conns[w] = proc, parent

    def _log(self, row):
        if self.failure_log:
            with self.failure_log.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(row) + "\n")

    def _collect(self, command):
        for conn in self._conns:
            conn.send(command)
        results = []
        deadline = time.monotonic() + self.timeout
        for w, conn in enumerate(self._conns):
            lo, hi = w * self.per, (w + 1) * self.per
            answered = False
            try:
                answered = conn.poll(max(0.0, deadline - time.monotonic()))
                if answered:
                    worker_results, started = conn.recv()
            except (EOFError, ConnectionError):
                answered = False
            if answered:
                for row in worker_results:
                    slot = row["slot"]  # -1: the run never reached a decision
                    if row["status"] not in (native.VICTORY, native.DEATH):
                        self._log({**row, "status_name": STATUS_NAMES[row["status"]],
                                   "action_rows": list(self.history[slot]) if slot >= 0 else []})
                    if slot >= 0:
                        if self.keep_history:
                            row["action_rows"] = self.history[slot]
                        self.history[slot] = []
                    results.append(row)
                for slot, seed in started:
                    self.seed_of[slot] = seed
                continue
            # No answer: the worker is stuck in native code or died.
            self.hangs += 1
            self._procs[w].kill()
            self._procs[w].join()
            remaining = None
            if self._seed_lists is not None:
                finished = {r["seed"] for r in results}
                remaining = [s for s in self._seed_lists[w] if s not in self._done and s not in finished
                             and s not in self.seed_of[lo:hi]]
            hung = [slot for slot in range(lo, hi) if self.seed_of[slot] is not None]
            for slot in hung:
                row = {"slot": slot, "seed": self.seed_of[slot], "status": HANG, "steps": len(self.history[slot]),
                       "act": -1, "floor": -1, "hp": -1, "max_hp": -1, "gold": -1, "deck": -1, "relics": -1, "last_encounter": 0,
                       "message": "worker did not answer within the timeout"}
                self._log({**row, "status_name": "hang", "action_rows": list(self.history[slot])})
                self.history[slot] = []
                results.append(row)
            self.config["generation"] += 1
            self._spawn(w, remaining)
            self._conns[w].send("reset")
            if not self._conns[w].poll(self.timeout):
                raise RuntimeError("a replacement sampler worker did not start")
            _, started = self._conns[w].recv()
            for slot in range(lo, hi):
                self.seed_of[slot] = None
            for slot, seed in started:
                self.seed_of[slot] = seed
            self.arrays["ended"][hung] = HANG  # after the replacement's reset, which clears its slice
        for row in results:
            self._done.add(row["seed"])
        return results

    def reset(self):
        return self._collect("reset")

    def step(self, choice: np.ndarray):
        """Take action row choice[i] in every running slot (-1: none). Returns the
        runs that ended in this step; `ended` marks their slots, whose
        observation already belongs to the next run."""
        active = (self.arrays["status"] == native.RUNNING) & (choice >= 0)
        for slot in np.nonzero(active)[0]:
            self.history[slot].append(int(choice[slot]))
        self.arrays["choice"][:] = np.where(active, choice, -1)
        return self._collect("step")

    def running(self) -> np.ndarray:
        return self.arrays["status"] == native.RUNNING

    def close(self):
        for conn, proc in zip(self._conns, self._procs):
            try:
                conn.send("close")
            except (BrokenPipeError, OSError):
                pass
            proc.join(timeout=2)
            if proc.is_alive():
                proc.kill()
        for block in self._blocks.values():
            block.close()
            block.unlink()
        self._blocks = {}

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
