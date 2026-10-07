#!/usr/bin/env python3
"""What a checkpoint would gain from one scripted habit, on seeds outside the dev and test sets.

  cd python && ../.venv/bin/python ../tools/nosl/override_eval.py --checkpoint CKPT --runs 2000 \\
      --smith-above 0.8 --out ../artifacts/nosl-q4f/override-smith.json

Every run is played twice on the same game seed: once by the policy alone and
once with the override. The override only replaces the policy's choice at the
named decision; everything else, including which card to upgrade, is the
policy's. It is a diagnostic of whether a behaviour the policy never tries is
worth anything, not a deployable rule.

  --smith-above F   at a rest site that offers both, upgrade instead of healing when hp >= F * max hp
  --hold-potions-above F   outside boss fights, never drink a potion while hp >= F * max hp: the policy's
                           best other action is taken instead (its most probable one)
  --avoid-elites    on the map, go to a node from which some path to the boss has the fewest elites (the map is
                    on screen); among equally good nodes the policy's own preference decides

Light on the machine by default: four sampler processes.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
from nosl import native  # noqa: E402
from nosl.model import load  # noqa: E402
from nosl.vec import VecSampler  # noqa: E402

G_DECISION, G_ACT, G_HP, G_MAX_HP, G_IN_COMBAT, G_ROOM_TYPE, ROOM_ELITE, ROOM_BOSS = 1, 4, 7, 8, 11, 26, 2, 3
DECISION_REST, ACTION_REST, ACTION_USE_POTION = 4, 7, 10   # env DecisionKind::RestSite, Action kinds
MISC_A, N_ENABLED = 6, 9                   # token columns: misc A, number 1
REST_HEAL, REST_SMITH = 41, 42             # misc::kRestOption + RestSiteOptionId
DECISION_MAP, ACTION_MAP_MOVE = 0, 1
TOKEN_MAP_NEXT, TOKEN_MAP_NODE = 10, 11
N_COL, N_ROW, N_CHILDREN = 8, 9, 12        # token numbers 0, 1 and 4 of a map node
MAP_ELITE = 1 + 6                          # misc::kMapPoint + MapPointType::Elite


def fewest_elites(tokens, count):
    """For every map node on screen: the fewest elites on any path from it to the end of the act, itself included."""
    nodes = {}
    for t in tokens[:count]:
        if t[0] == TOKEN_MAP_NODE:
            nodes[(int(t[N_COL]), int(t[N_ROW]))] = (int(t[MISC_A]) == MAP_ELITE, int(t[N_CHILDREN]))
    best = {}

    def cost(key):
        if key not in best:
            elite, children = nodes[key]
            ahead = [cost((c, key[1] + 1)) for c in range(31) if children >> c & 1 and (c, key[1] + 1) in nodes]
            best[key] = int(elite) + (min(ahead) if ahead else 0)
        return best[key]

    return {key: cost(key) for key in nodes}


def play(model, seeds, device, workers, greedy, policy_seed, smith_above, actor_filter, hold_potions_above=None,
         avoid_elites=False):
    torch.manual_seed(policy_seed)
    per = -(-len(seeds) // workers)
    rows, overridden = {}, 0
    elite_fights = 0  # act-1 elite fights entered, over all runs
    with VecSampler(workers, per, split="train", seeds=seeds, actor_filter=actor_filter) as vec:
        A = vec.arrays
        results = vec.reset()
        in_elite = np.zeros(vec.runs, bool)
        while True:
            now = (A["global"][:, G_IN_COMBAT] > 0) & (A["global"][:, G_ROOM_TYPE] == ROOM_ELITE) & (A["global"][:, G_ACT] == 0)
            elite_fights += int((now & ~in_elite & vec.running()).sum())
            in_elite = now
            for r in results:
                rows[r["seed"]] = r
            if len(rows) >= len(seeds):
                break
            running = vec.running()
            tokens_used = max(1, int(A["counts"][:, 0].max()))
            actions_used = max(1, int(A["counts"][:, 1].max()))
            batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device)
                     for a in (A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"])]
            with torch.no_grad(), torch.autocast("cuda", dtype=torch.bfloat16, enabled=device.type == "cuda"):
                logits, _ = model(*batch)
            logits = logits.float()
            logits = torch.where(torch.isinf(logits).all(dim=1, keepdim=True), torch.zeros_like(logits), logits)
            action = logits.argmax(dim=1) if greedy else torch.distributions.Categorical(logits=logits).sample()
            choice = np.where(running, action.cpu().numpy(), -1).astype(np.int32)
            if smith_above is not None:
                g = A["global"]
                rest = running & (g[:, G_DECISION] == DECISION_REST) & (g[:, G_HP] >= smith_above * g[:, G_MAX_HP])
                for slot in np.flatnonzero(rest):
                    option = {}
                    for row in range(int(A["counts"][slot, 1])):
                        kind, token = A["actions"][slot, row, 0], A["actions"][slot, row, 1]
                        if kind == ACTION_REST and token >= 0 and A["tokens"][slot, token, N_ENABLED]:
                            option[int(A["tokens"][slot, token, MISC_A])] = row
                    if REST_HEAL in option and REST_SMITH in option:
                        overridden += choice[slot] != option[REST_SMITH]
                        choice[slot] = option[REST_SMITH]
            if avoid_elites:
                scores = logits.cpu().numpy()
                for slot in np.flatnonzero(running & (A["global"][:, G_DECISION] == DECISION_MAP)):
                    tokens = A["tokens"][slot]
                    ahead = fewest_elites(tokens, int(A["counts"][slot, 0]))
                    options = []
                    for row in range(int(A["counts"][slot, 1])):
                        kind, token = int(A["actions"][slot, row, 0]), int(A["actions"][slot, row, 1])
                        if kind == ACTION_MAP_MOVE and token >= 0:
                            key = (int(tokens[token, N_COL]), int(tokens[token, N_ROW]))
                            options.append((ahead.get(key, 0), -float(scores[slot, row]), row))
                    if options:
                        fewest = min(o[0] for o in options)
                        chosen = next((o for o in options if o[2] == choice[slot]), None)
                        if chosen is None or chosen[0] > fewest:
                            choice[slot] = min(o for o in options if o[0] == fewest)[2]
                            overridden += 1
            if hold_potions_above is not None:
                g = A["global"]
                boss = (g[:, G_IN_COMBAT] > 0) & (g[:, G_ROOM_TYPE] == ROOM_BOSS)
                slots = np.arange(len(choice))
                chosen_kind = A["actions"][slots, np.maximum(choice, 0), 0]
                held = running & ~boss & (chosen_kind == ACTION_USE_POTION) & (g[:, G_HP] >= hold_potions_above * g[:, G_MAX_HP])
                if held.any():
                    other = logits.clone()
                    kinds = torch.from_numpy(np.ascontiguousarray(A["actions"][:, :actions_used, 0])).to(device)
                    other[kinds == ACTION_USE_POTION] = float("-inf")
                    best = other.argmax(dim=1).cpu().numpy()
                    usable = torch.isfinite(other).any(dim=1).cpu().numpy()
                    held &= usable  # nothing but potions on offer: leave the choice alone
                    overridden += int(held.sum())
                    choice = np.where(held, best, choice).astype(np.int32)
            results = vec.step(choice)
    play.elite_fights = elite_fights  # of the last call
    return rows, int(overridden)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--runs", type=int, default=2000)
    p.add_argument("--seed-prefix", default="NQDIAG", help="not a dev or test prefix")
    p.add_argument("--smith-above", type=float, default=None)
    p.add_argument("--hold-potions-above", type=float, default=None)
    p.add_argument("--avoid-elites", action="store_true")
    p.add_argument("--greedy", action="store_true")
    p.add_argument("--policy-seed", type=int, default=1)
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--out", default="")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()
    if args.smith_above is None and args.hold_potions_above is None and not args.avoid_elites:
        raise SystemExit("name an override (--smith-above, --hold-potions-above or --avoid-elites)")
    device = torch.device(args.device)
    model, blob = load(args.checkpoint, device)
    model.eval()
    actor_filter = int(blob.get("actor_filter", 0))
    seeds = [f"{args.seed_prefix}{i:05d}" for i in range(args.runs)]
    base, _ = play(model, seeds, device, args.workers, args.greedy, args.policy_seed, None, actor_filter)
    base_elites = play.elite_fights
    with_override, count = play(model, seeds, device, args.workers, args.greedy, args.policy_seed, args.smith_above, actor_filter,
                                args.hold_potions_above, args.avoid_elites)
    done = [s for s in seeds if base[s]["status"] in (native.VICTORY, native.DEATH)
            and with_override[s]["status"] in (native.VICTORY, native.DEATH)]
    a = np.array([base[s]["floor"] for s in done], float)
    b = np.array([with_override[s]["floor"] for s in done], float)
    difference = b - a
    summary = {
        "checkpoint": args.checkpoint, "greedy": args.greedy, "runs": len(seeds), "compared": len(done),
        "override": {"smith_above": args.smith_above, "hold_potions_above": args.hold_potions_above,
                     "avoid_elites": args.avoid_elites},
        "decisions_overridden": count,
        "floor_policy": round(float(a.mean()), 3), "floor_override": round(float(b.mean()), 3),
        "difference": round(float(difference.mean()), 3),
        "difference_se": round(float(difference.std(ddof=1) / math.sqrt(len(done))), 3),
        "act2_policy": round(float(np.mean([base[s]["act"] >= 1 for s in done])), 4),
        "act2_override": round(float(np.mean([with_override[s]["act"] >= 1 for s in done])), 4),
        "act1_elite_fights_per_run_policy": round(base_elites / len(seeds), 3),
        "act1_elite_fights_per_run_override": round(play.elite_fights / len(seeds), 3),
    }
    print(json.dumps(summary))
    if args.out:
        Path(args.out).write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
