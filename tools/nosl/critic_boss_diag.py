#!/usr/bin/env python3
"""Does the critic tell a strong deck from a weak one before a boss fight?

  cd python && ../.venv/bin/python ../tools/nosl/critic_boss_diag.py --checkpoint CKPT --fights 3000

Plays training-distribution runs with the policy. At the first decision of
every boss fight it records the critic's value, the player's HP, deck size,
potions and relics, and afterwards how the fight went: the share of the
fight's enemy HP removed (1 for a win). It then reports how much of that
outcome the value explains, and how much is left once HP alone is accounted
for. If the critic adds nothing beyond HP, card and relic choices get no
credit from the boss fight through it.

Light on the machine by default: four sampler processes.
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
from nosl.vec import VecSampler  # noqa: E402

G_HP, G_MAX_HP, G_IN_COMBAT, G_DECK, G_RELICS, G_POTIONS, G_ROOM_TYPE, ROOM_BOSS = 7, 8, 11, 21, 22, 23, 26, 3
TOKEN_ENEMY, T_HP, T_MAX_HP, T_ALIVE = 6, 8, 9, 15


def r2(x: np.ndarray, y: np.ndarray) -> float:
    """Share of the variance of y explained by a least-squares fit on the columns of x (with an intercept)."""
    design = np.column_stack([np.ones(len(y)), x])
    fitted = design @ np.linalg.lstsq(design, y, rcond=None)[0]
    return float(1 - np.var(y - fitted) / max(np.var(y), 1e-12))


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--fights", type=int, default=3000)
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--runs-per-worker", type=int, default=120)
    p.add_argument("--seed-prefix", default="NQCRIT", help="not a dev or test prefix")
    p.add_argument("--policy-seed", type=int, default=1)
    p.add_argument("--out", default="")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()
    device = torch.device(args.device)
    model, blob = load(args.checkpoint, device)
    model.eval()
    torch.manual_seed(args.policy_seed)
    records = []
    with VecSampler(args.workers, args.runs_per_worker, split="train", seed_prefix=args.seed_prefix,
                    actor_filter=int(blob.get("actor_filter", 0))) as vec:
        A = vec.arrays
        vec.reset()
        N = vec.runs
        ref = np.zeros(N)       # enemy HP the open boss fight started with (0: none)
        paid = np.zeros(N)      # share removed so far
        opened = [None] * N     # the record of the open fight

        def state():
            used = max(1, int(A["counts"][:, 0].max()))
            tokens = A["tokens"][:, :used]
            living = ((tokens[..., 0] == TOKEN_ENEMY) & (tokens[..., T_ALIVE] > 0)
                      & (np.arange(used)[None, :] < A["counts"][:, :1]))
            inside = (A["global"][:, G_IN_COMBAT] > 0) & (A["global"][:, G_ROOM_TYPE] == ROOM_BOSS)
            return inside, (tokens[..., T_HP] * living).sum(axis=1), (tokens[..., T_MAX_HP] * living).sum(axis=1)

        while len(records) < args.fights:
            inside, hp, max_hp = state()
            tokens_used = max(1, int(A["counts"][:, 0].max()))
            actions_used = max(1, int(A["counts"][:, 1].max()))
            batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device)
                     for a in (A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"])]
            with torch.no_grad(), torch.autocast("cuda", dtype=torch.bfloat16, enabled=device.type == "cuda"):
                logits, values = model(*batch)
            values = values.float().cpu().numpy()
            for slot in np.flatnonzero(inside & (ref == 0)):
                g = A["global"][slot]
                ref[slot], paid[slot] = max(max_hp[slot], hp[slot], 1), 0.0
                opened[slot] = {"value": float(values[slot]), "hp": int(g[G_HP]), "max_hp": int(g[G_MAX_HP]),
                                "deck": int(g[G_DECK]), "relics": int(g[G_RELICS]), "potions": int(g[G_POTIONS])}
            action = torch.distributions.Categorical(logits=logits.float()).sample().cpu().numpy().astype(np.int32)
            seeds_before = list(vec.seed_of)
            results = vec.step(action)
            inside_now, hp_now, _ = state()
            ended = np.zeros(N, bool)
            for row in results:
                if row["slot"] >= 0 and row["seed"] == seeds_before[row["slot"]]:
                    ended[row["slot"]] = True
            fighting = (ref > 0) & ~ended
            won = fighting & ~inside_now
            share = np.where(fighting, np.minimum(np.maximum(hp - hp_now, 0) / np.maximum(ref, 1), 1.0 - paid), 0.0)
            paid += share
            for slot in np.flatnonzero(won | (ended & (ref > 0))):
                records.append({**opened[slot], "share": 1.0 if won[slot] else float(paid[slot]), "won": bool(won[slot])})
                ref[slot], opened[slot] = 0, None

    value = np.array([r["value"] for r in records])
    hp = np.array([r["hp"] for r in records], float)
    share = np.array([r["share"] for r in records])
    won = np.array([r["won"] for r in records], float)
    basics = np.column_stack([hp, [r["max_hp"] for r in records], [r["deck"] for r in records],
                              [r["relics"] for r in records], [r["potions"] for r in records]])
    residual_share = share - np.column_stack([np.ones(len(hp)), hp]) @ np.linalg.lstsq(
        np.column_stack([np.ones(len(hp)), hp]), share, rcond=None)[0]
    residual_value = value - np.column_stack([np.ones(len(hp)), hp]) @ np.linalg.lstsq(
        np.column_stack([np.ones(len(hp)), hp]), value, rcond=None)[0]
    summary = {
        "checkpoint": args.checkpoint, "fights": len(records), "boss_win": round(float(won.mean()), 4),
        "share_mean": round(float(share.mean()), 4), "share_sd": round(float(share.std()), 4),
        "r2_share_from_value": round(r2(value[:, None], share), 4),
        "r2_share_from_hp": round(r2(hp[:, None], share), 4),
        "r2_share_from_hp_deck_relics_potions": round(r2(basics, share), 4),
        "r2_share_from_value_and_hp": round(r2(np.column_stack([value, hp]), share), 4),
        "correlation_value_share_given_hp": round(float(np.corrcoef(residual_value, residual_share)[0, 1]), 4),
        "correlation_value_hp": round(float(np.corrcoef(value, hp)[0, 1]), 4),
        "r2_win_from_value": round(r2(value[:, None], won), 4),
        "r2_win_from_hp": round(r2(hp[:, None], won), 4),
    }
    print(json.dumps(summary))
    if args.out:
        Path(args.out).write_text(json.dumps({"summary": summary, "records": records}))


if __name__ == "__main__":
    main()
