"""Recorded runs replayed in worker processes; the selected decisions come back with their observation.

A recorded run is its seed and its action rows (nosl.strategy_q collect):
the sampler is deterministic, so replaying the rows rebuilds every public
observation of the run without storing any. Each worker owns one native
batch and advances its runs on its own, sending back only the decisions that
`select` asks for, so the parent never waits on a combat decision it does not
train on.

A run whose replay does not end as recorded (status, floor, act) is reported
as an error to the parent, which raises: a diverged replay is never a sample.
"""
from __future__ import annotations

import multiprocessing as mp
from multiprocessing.connection import wait

import numpy as np

from . import native

G_IN_COMBAT = 11
SMALL_TOKENS = 48  # decisions with at most this many tokens travel and train together


def _worker(conn, runs, config):
    slots, max_tokens, max_actions = config["slots"], config["max_tokens"], config["max_actions"]
    chunk, outside = config["chunk"], config["select"] == "outside"
    batch = native.Batch(slots, max_tokens, max_actions, config["max_steps"])
    batch.set_filter(1)
    g = np.zeros((slots, native.GLOBAL_WIDTH), np.int32)
    tk = np.zeros((slots, max_tokens, native.TOKEN_WIDTH), np.int32)
    ac = np.zeros((slots, max_actions, native.ACTION_WIDTH), np.int32)
    cn = np.zeros((slots, 2), np.int32)
    st = np.full(slots, native.DEATH, np.int32)
    sp = np.zeros(slots, np.int32)
    idle = np.full(slots, -1, np.int32)
    choice = np.full(slots, -1, np.int32)
    current = [None] * slots
    position = [0] * slots
    queue = list(reversed(runs))
    pending = {"small": [], "large": []}
    counts = {"small": 0, "large": 0}

    def flush(name, force=False):
        if counts[name] >= chunk or (force and counts[name]):
            parts = pending[name]
            merged = {k: np.concatenate([p[k] for p in parts]) for k in parts[0]}
            t = max(1, int(merged["counts"][:, 0].max()))
            a = max(1, int(merged["counts"][:, 1].max()))
            merged["tokens"] = np.ascontiguousarray(merged["tokens"][:, :t])
            merged["actions"] = np.ascontiguousarray(merged["actions"][:, :a])
            conn.send(("data", merged))
            pending[name], counts[name] = [], 0

    try:
        active = 0
        while True:
            # Start the next recorded run in every free slot.
            started = False
            for j in range(slots):
                if current[j] is None and queue:
                    run = queue.pop()
                    if batch.reset(j, run["seed"], 10) != native.RUNNING:
                        raise RuntimeError(f"replay of {run['seed']} did not start")
                    current[j], position[j] = run, 0
                    active += 1
                    started = True
            if active == 0:
                break
            if started:
                batch.step(idle, g, tk, ac, cn, st, sp)
            choice[:] = -1
            emit = []
            for j in range(slots):
                run = current[j]
                if run is None:
                    continue
                rows = run["action_rows"]
                if position[j] >= len(rows):
                    raise RuntimeError(f"replay of {run['seed']} outlived its record")
                choice[j] = rows[position[j]]
                if outside and g[j, G_IN_COMBAT] == 0:
                    emit.append(j)
            if emit:
                rows = np.array(emit)
                explored = np.array([position[j] in current[j]["explored_set"] for j in emit])
                target = np.array([current[j]["target"] for j in emit], np.float32)
                of_run = {key: np.array([current[j][source] for j in emit], np.int32)
                          for key, source in (("run", "id"), ("floor", "floor"), ("run_act", "act"), ("status", "status"))}
                small = cn[rows, 0] <= SMALL_TOKENS
                for name, mask in (("small", small), ("large", ~small)):
                    if mask.any():
                        pick = rows[mask]
                        pending[name].append({"global": g[pick].copy(), "tokens": tk[pick].copy(), "actions": ac[pick].copy(),
                                              "counts": cn[pick].copy(), "choice": choice[pick].astype(np.int64),
                                              "target": target[mask], "explored": explored[mask],
                                              **{key: value[mask] for key, value in of_run.items()}})
                        counts[name] += int(mask.sum())
                        flush(name)
            for j in range(slots):
                if current[j] is not None:
                    position[j] += 1
            batch.step(choice, g, tk, ac, cn, st, sp)
            for j in range(slots):
                run = current[j]
                if run is None or st[j] == native.RUNNING:
                    continue
                info = batch.run_info(j)
                if (int(st[j]), info["floor"], info["act"]) != (run["status"], run["floor"], run["act"]) \
                        or position[j] != len(run["action_rows"]):
                    raise RuntimeError(f"replay of {run['seed']} ended differently from its record")
                current[j] = None
                active -= 1
        flush("small", True)
        flush("large", True)
        conn.send(("done", len(runs)))
    except Exception as error:  # the parent must hear about it
        conn.send(("error", repr(error)))
    finally:
        conn.close()


def replay(runs, *, workers, order_seed, select="outside", slots=64, chunk=512, max_tokens=192, max_actions=160,
           max_steps=30000):
    """Yields chunks {global, tokens, actions, counts, choice, target, explored, run, floor, run_act, status}
    of the selected decisions; `run` is the index of the decision's run in `runs`.

    `runs`: recorded runs with seed, status, floor, act, action_rows, and the
    caller's `target` (float) and `explored_set` (set of action-row positions).
    Every run is replayed once; chunk order depends on process timing."""
    order = np.random.default_rng(order_seed).permutation(len(runs))
    fields = ("seed", "status", "floor", "act", "action_rows", "target", "explored_set")
    shuffled = [{**{k: runs[i][k] for k in fields}, "id": int(i)} for i in order]
    context = mp.get_context("spawn")
    config = {"slots": slots, "chunk": chunk, "max_tokens": max_tokens, "max_actions": max_actions,
              "max_steps": max_steps, "select": select}
    procs, conns = [], []
    for w in range(workers):
        part = shuffled[w::workers]
        if not part:
            continue
        parent, child = context.Pipe(duplex=False)
        proc = context.Process(target=_worker, args=(child, part, config), daemon=True)
        proc.start()
        child.close()
        procs.append(proc)
        conns.append(parent)
    replayed = 0
    try:
        live = list(conns)
        while live:
            for conn in wait(live):
                try:
                    kind, payload = conn.recv()
                except EOFError:
                    raise RuntimeError("a replay worker died without reporting")
                if kind == "data":
                    yield payload
                elif kind == "done":
                    replayed += payload
                    live.remove(conn)
                else:
                    raise RuntimeError("replay worker: " + payload)
        if replayed != len(runs):
            raise RuntimeError(f"replayed {replayed} of {len(runs)} runs")
    finally:
        for proc in procs:
            if proc.is_alive():
                proc.kill()
            proc.join()
