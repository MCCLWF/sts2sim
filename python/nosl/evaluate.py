"""Whole-run evaluation of a checkpoint: observation in, action row out.

  .venv/bin/python -m nosl.evaluate --checkpoint artifacts/nosl-q2/run1/checkpoint.pt \\
      --split dev --first 0 --count 200 --out artifacts/nosl-q2/run1/dev.jsonl

No simulation, search or rollback happens here: the policy sees only the
observation tables. Every attempt gets one line, whatever its end type; the
format matches tools/nosl/nosl_run so tools/nosl/summarize.py reads both.
"""
from __future__ import annotations

import argparse
import collections
import json
import time
from pathlib import Path

import numpy as np
import torch

from . import native
from .model import load
from .vec import DEV_PREFIX, STATUS_NAMES, TEST_PREFIX, VecSampler

G_IN_COMBAT = 11  # nosl/observation.h, GlobalColumn


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--split", choices=("dev", "test"), default="dev")
    p.add_argument("--final-eval", action="store_true", help="required for the held-out test split")
    p.add_argument("--first", type=int, default=0)
    p.add_argument("--count", type=int, default=200)
    p.add_argument("--out", required=True)
    p.add_argument("--greedy", action="store_true", help="take the most probable action instead of sampling")
    p.add_argument("--greedy-out", action="store_true",
                   help="take the most probable action outside combat and sample inside it")
    p.add_argument("--trajectories", help="also write {seed, ascension, action_rows} per run, for tools/nosl/oracle_replay.py")
    p.add_argument("--policy-seed", type=int, default=1)
    p.add_argument("--ascension", type=int, default=10)
    p.add_argument("--workers", type=int, default=8)
    p.add_argument("--max-steps", type=int, default=30000)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args(argv)

    prefix = DEV_PREFIX if args.split == "dev" else TEST_PREFIX
    seeds = [f"{prefix}{i:04d}" for i in range(args.first, args.first + args.count)]
    device = torch.device(args.device)
    model, blob = load(args.checkpoint, device)
    model.eval()
    torch.manual_seed(args.policy_seed)
    name = (f"{Path(args.checkpoint).parent.name}@{blob.get('decisions', 0)}"
            f"{'-greedy' if args.greedy else '-greedy-out' if args.greedy_out else ''}")
    actor_filter = int(blob.get("actor_filter", 0))  # the policy is evaluated behind the filter it was trained with
    per = -(-len(seeds) // args.workers)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    rows, latency = [], []
    began = time.time()
    with VecSampler(args.workers, per, split=args.split, seeds=seeds, final_eval=args.final_eval,
                    ascension=args.ascension, max_steps=args.max_steps,
                    keep_history=bool(args.trajectories), actor_filter=actor_filter,
                    failure_log=out.with_suffix(".failures.jsonl")) as vec, out.open("w", encoding="utf-8") as stream:
        trajectories = open(args.trajectories, "w", encoding="utf-8") if args.trajectories else None
        A = vec.arrays
        results = vec.reset()
        while True:
            for r in results:
                row = {"seed": r["seed"], "policy": name, "policy_class": "public_no_search", "ascension": args.ascension,
                       "end": STATUS_NAMES[r["status"]], "act": r["act"], "floor": r["floor"], "hp": r["hp"],
                       "max_hp": r["max_hp"], "gold": r["gold"], "deck": r["deck"], "relics": r["relics"],
                       "last_encounter": r["last_encounter"], "steps": r["steps"], "steps_by_kind": {},
                       "ms_total": 0, "ms_legal": 0, "ms_step": 0, "ms_policy": 0}
                if "message" in r:
                    row["message"] = r["message"]
                stream.write(json.dumps(row) + "\n")
                stream.flush()
                rows.append(row)
                if trajectories and "action_rows" in r:
                    trajectories.write(json.dumps({"seed": r["seed"], "ascension": args.ascension, "actor_filter": actor_filter, "end": row["end"],
                                                   "floor": r["floor"], "action_rows": r["action_rows"]}) + "\n")
            if len(rows) >= len(seeds):
                break
            running = vec.running()
            if not running.any():
                raise RuntimeError("no run is active but some seeds have no result")
            tokens_used = max(1, int(A["counts"][:, 0].max()))
            actions_used = max(1, int(A["counts"][:, 1].max()))
            t0 = time.perf_counter()
            batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device)
                     for a in (A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"])]
            with torch.no_grad(), torch.autocast("cuda", dtype=torch.bfloat16, enabled=device.type == "cuda"):
                logits, _ = model(*batch)
            logits = logits.float()
            # Parked slots have no action row; give them a dummy logit so sampling stays defined.
            logits = torch.where(torch.isinf(logits).all(dim=1, keepdim=True), torch.zeros_like(logits), logits)
            action = logits.argmax(dim=1) if args.greedy else torch.distributions.Categorical(logits=logits).sample()
            if args.greedy_out:
                action = torch.where(batch[0][:, G_IN_COMBAT] == 0, logits.argmax(dim=1), action)
            choice = np.where(running, action.cpu().numpy(), -1).astype(np.int32)
            latency.append((time.perf_counter() - t0) * 1000)
            results = vec.step(choice)
        if trajectories:
            trajectories.close()
    ends = collections.Counter(r["end"] for r in rows)
    floors = sorted(r["floor"] for r in rows)
    latency.sort()
    summary = {"summary": True, "policy": name, "policy_class": "public_no_search", "split": args.split,
               "attempts": len(rows), "ends": dict(ends), "greedy": args.greedy, "greedy_out": args.greedy_out,
               "actor_filter": actor_filter,
               "ended_in_act": dict(sorted(collections.Counter(r["act"] + 1 for r in rows).items())),
               "floor_median": floors[len(floors) // 2], "floor_p90": floors[int(.9 * len(floors))], "floor_max": floors[-1],
               "floor_mean": round(sum(floors) / len(floors), 2), "wall_s": round(time.time() - began, 1),
               "batch_inference_ms_median": round(latency[len(latency) // 2], 2),
               "batch_inference_ms_p95": round(latency[int(.95 * len(latency))], 2), "batch_size": vec.runs}
    print(json.dumps(summary))
    out.with_suffix(".summary.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
