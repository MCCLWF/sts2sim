#!/usr/bin/env python3
"""Greedy strength of checkpoints on one fixed set of seeds outside the dev and test sets.

  cd python && ../.venv/bin/python ../tools/nosl/greedy_eval.py NAME=CKPT [NAME=CKPT ...] \\
      [--runs 2000] [--out ../artifacts/nosl-q4f/greedy-curve.json]

Every decision takes the most probable action. The training log's floor mean
is of the sampling policy, whose noise changes with the entropy and the
learning rate; this is the strength of the policy itself, with a standard
error near 0.11 floors at 2,000 runs (docs/plans/2026-10-04-nosl-q4f-steady-progress.md,
amendment 6). Results are added to --out, keyed by NAME.

Light on the machine by default: four sampler processes, one checkpoint at a time.
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))


def main() -> None:
    from nosl import native
    from nosl.model import load
    from override_eval import play

    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("checkpoints", nargs="+", help="NAME=PATH")
    p.add_argument("--runs", type=int, default=2000)
    p.add_argument("--seed-prefix", default="NQGRDY", help="not a dev or test prefix")
    p.add_argument("--seed-width", type=int, default=5)
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--out", default="")
    p.add_argument("--rows-dir", default="", help="save every attempted seed, including faults, as NAME.jsonl for paired comparisons")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()
    device = torch.device(args.device)
    seeds = [f"{args.seed_prefix}{i:0{args.seed_width}d}" for i in range(args.runs)]
    out_path = Path(args.out) if args.out else None
    table = json.loads(out_path.read_text()) if out_path and out_path.exists() else {}
    rows_dir = Path(args.rows_dir) if args.rows_dir else None
    if rows_dir:
        rows_dir.mkdir(parents=True, exist_ok=True)
    for spec in args.checkpoints:
        name, _, path = spec.partition("=")
        model, blob = load(path, device)
        model.eval()
        rows, _ = play(model, seeds, device, args.workers, True, 1, None, int(blob.get("actor_filter", 0)))
        if rows_dir:
            if Path(name).name != name:
                raise ValueError("checkpoint names must be filenames when --rows-dir is used")
            with (rows_dir / f"{name}.jsonl").open("w") as stream:
                for seed in seeds:
                    stream.write(json.dumps({"seed": seed, **rows[seed]}) + "\n")
        done = [r for r in rows.values() if r["status"] in (native.VICTORY, native.DEATH)]
        if not done:
            raise RuntimeError(f"{name}: none of the {len(seeds)} attempted runs reached a real terminal")
        floors = np.array([r["floor"] for r in done], float)
        from nosl.vec import STATUS_NAMES
        table[name] = {"checkpoint": path, "decisions": int(blob.get("decisions", 0)), "attempts": len(seeds),
                       "ends": dict(collections.Counter(STATUS_NAMES[r["status"]] for r in rows.values())), "runs": len(done),
                       "floor": round(float(floors.mean()), 3),
                       "se": round(float(floors.std(ddof=1) / len(floors) ** 0.5), 3),
                       "act2": round(float(np.mean([r["act"] >= 1 for r in done])), 4),
                       "act1_elite_fights_per_run": round(play.elite_fights / len(seeds), 3),
                       "victories": sum(r["status"] == native.VICTORY for r in done)}
        print(name, json.dumps(table[name]), flush=True)
        if out_path:
            out_path.write_text(json.dumps(table, indent=1))


if __name__ == "__main__":
    main()
