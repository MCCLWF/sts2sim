#!/usr/bin/env python3
"""What a checkpoint would gain from keeping its potions for the fights that decide a run.

  tools/nosl/potion_hold_eval.py --checkpoint X1.pt --prefix NR1DIAG --runs 512 --out DIR [--device cpu]

Every seed is played by the policy alone and again with one scripted habit
inside combat: a potion is not drunk while the fight is not of a listed room
type and HP is at or above a fraction of max HP; the policy's best other
action is taken instead. Everything else, including which potion to drink
when it may, is the policy's. The habit reads the public room type and HP
only. It is a diagnostic of whether potion timing is worth learning, not a
deployable rule and not a NoSL result of its own.

Arms: base; hold-A (free in elite and boss fights, elsewhere only below 50%
HP); hold-B (free in boss fights, elsewhere only below 50% HP); hold-C (free
in boss fights only, whatever the HP).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
from nosl.model import load  # noqa: E402
from nosl.strategy_q import Chooser, play, seeds_of, summarize, to_device  # noqa: E402

G_HP, G_MAX_HP, G_IN_COMBAT, G_ROOM_TYPE = 7, 8, 11, 26
ROOM_ELITE, ROOM_BOSS = 2, 3
ACTION_USE_POTION = 10
ARMS = {"base": None, "hold-A": ((ROOM_ELITE, ROOM_BOSS), 0.5), "hold-B": ((ROOM_BOSS,), 0.5), "hold-C": ((ROOM_BOSS,), 0.0)}


class HoldPotions(Chooser):
    def __init__(self, combat, device, free_rooms, hp_fraction):
        super().__init__(combat, None, 0.0, device)
        self.free_rooms, self.hp_fraction = free_rooms, hp_fraction

    @torch.no_grad()
    def __call__(self, A):
        tokens_used = max(1, int(A["counts"][:, 0].max()))
        actions_used = max(1, int(A["counts"][:, 1].max()))
        batch = to_device((A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"]), self.device)
        logits = self.combat(*batch)[0].float()
        logits = torch.where(torch.isinf(logits).all(dim=1, keepdim=True), torch.zeros_like(logits), logits)
        g = batch[0]
        outside = g[:, G_IN_COMBAT] == 0
        free = torch.zeros_like(outside)
        for room in self.free_rooms:
            free |= g[:, G_ROOM_TYPE] == room
        low = g[:, G_HP].float() < self.hp_fraction * g[:, G_MAX_HP].float()
        hold = ~outside & ~free & ~low
        potion = batch[2][:, :, 0] == ACTION_USE_POTION
        masked = logits.masked_fill(hold.unsqueeze(1) & potion, float("-inf"))
        usable = torch.isfinite(masked).any(dim=1, keepdim=True)
        logits = torch.where(usable, masked, logits)
        return logits.argmax(dim=1), outside, torch.zeros_like(outside)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--prefix", required=True)
    p.add_argument("--first", type=int, default=0)
    p.add_argument("--runs", type=int, default=512)
    p.add_argument("--out", required=True)
    p.add_argument("--arms", default=",".join(ARMS))
    p.add_argument("--workers", type=int, default=3)
    p.add_argument("--device", default="cpu")
    p.add_argument("--threads", type=int, default=2)
    args = p.parse_args()
    if args.prefix.startswith(("NOSLDEV", "NOSLTEST")):
        raise SystemExit("the registered dev/test splits are not for this tool")
    torch.set_num_threads(args.threads)
    device = torch.device(args.device)
    model, blob = load(args.checkpoint, device)
    if blob.get("actor_filter") != 1:
        raise SystemExit("the checkpoint must be one trained behind actor filter 1")
    model.eval().requires_grad_(False)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    seeds = seeds_of(args.prefix, args.first, args.runs)
    for arm in args.arms.split(","):
        rule = ARMS[arm]
        chooser = Chooser(model, None, 0.0, device) if rule is None else HoldPotions(model, device, *rule)
        rows = []
        with (out / f"{arm}.jsonl").open("w", encoding="utf-8") as stream:
            for row in play(chooser, seeds, workers=args.workers):
                row.update({"ascension": 10, "actor_filter": 1})
                stream.write(json.dumps(row) + "\n")
                rows.append(row)
        summary = {"arm": arm, "rule": rule, **summarize(rows)}
        (out / f"{arm}.summary.json").write_text(json.dumps(summary, indent=1))
        print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
