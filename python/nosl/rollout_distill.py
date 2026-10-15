"""Supervised distillation of rollout labels into the combat policy.

  .venv/bin/python -m nosl.rollout_distill --labels 'DIR/a2.*.bin,DIR/a1.*.bin' --init X1.pt --out D1.pt

The labels come from tools/nosl/nosl_rollout_labels: at combat decisions
along recorded runs, the mean worth of each of the policy's most probable
actions when the policy itself finishes the combat on resampled copies. The
target of a decision is the improved action: the candidate whose mean is
highest among those above the policy's own action by more than --z standard
errors of the paired difference, or the policy's own action when there is
none. The loss is cross-entropy to that action (expert iteration, Anthony et
al. 2017, with a one-step rollout as the expert).

--target advantage fits the worths themselves instead of naming a winner. A
candidate passes the --z test by chance about one time in six, so with three
alternatives a good share of the "improved actions" are noise, and a
classifier trained on them unlearns the policy's own choices without learning
the corrections (the first pipeline check: held-out agreement on changed
targets 8%, on unchanged ones down from 100% to 84%). The rollouts of one
decision share their resampled copies, so the difference between a
candidate's mean worth and the policy's own action's is a paired, unbiased
estimate of that candidate's advantage. The model's action scores are
regressed so that score(candidate) - score(policy's action) matches it
(squared error; the last policy layer starts at zero, so every advantage
starts at zero). Noise then averages out over similar decisions instead of
being voted on once per decision. The model is used as a rule beside the
policy, like the strategic value rule: the policy's action, unless a
candidate's predicted advantage is above a margin (nosl.strategy_q,
--combat-value). On held-out runs each epoch reports, per margin, the mean
labelled advantage of the action that rule would take: an unbiased estimate
of what the rule gains per labelled decision in the rollouts' own units,
because the held-out labels' noise is independent of the model.

Only the labelled decisions are trained, all of them inside combat. The
model still reads the public observation only; the labels are an offline
training signal with the rollout worth's own weights (surviving, HP,
potions), not the product objective.

Runs whose index is a multiple of 20 are held out. Each epoch reports, on the
held-out decisions, how often the model's most probable action is the target,
separately where the target differs from the labelling policy's action
("changed") and where it does not.
"""
from __future__ import annotations

import argparse
import ctypes
import glob
import gzip
import json
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F

from . import native
from .model import load, save
from .strategy_q import to_device

MAGIC = 0x4C4F524E
HEADER = 10
MAX_K = 8
MAX_ACTIONS = 160     # the sampler's action table (nosl.vec)
MARGINS = (0.0, 0.02, 0.05, 0.1, 0.2, 0.4)


def read_labels(patterns: str, z: float, runs_below: int = 0):
    """All complete records of the files matching the comma-separated patterns (plain or .gz). Tokens are kept
    unpadded (`tok`, with `tok_at` the first token row of each record): a padded table of several hundred thousand
    decisions does not fit in memory. `runs_below` > 0 keeps only records of runs with a smaller index."""
    paths = sorted(p for pattern in patterns.split(",") for p in glob.glob(pattern))
    if not paths:
        raise SystemExit(f"no label files match {patterns}")
    G, W, AW = native.GLOBAL_WIDTH, native.TOKEN_WIDTH, native.ACTION_WIDTH
    cols = {k: [] for k in ("global", "tok", "actions", "counts", "target", "policy", "run", "act", "fight", "gain",
                            "cand", "adv", "adv_se", "source")}
    truncated = 0
    for number, path in enumerate(paths):
        if path.endswith(".gz"):
            with gzip.open(path, "rb") as f:
                raw = f.read()
            buf = np.frombuffer(raw[:len(raw) // 4 * 4], dtype=np.int32)
        else:
            buf = np.fromfile(path, dtype=np.int32)
        at = 0
        while at + HEADER <= len(buf):
            magic, run, index, nt, na, k, copies, taken, act, fight = (int(x) for x in buf[at:at + HEADER])
            if magic != MAGIC:
                raise SystemExit(f"{path}: not a label record at int {at}")
            end = at + HEADER + G + nt * W + na * AW + 3 * k
            if end > len(buf):       # the tool was stopped inside this record
                truncated += 1
                break
            body = at + HEADER
            at = end
            if runs_below and run >= runs_below:
                continue
            tail = body + G + nt * W + na * AW
            rows = buf[tail:tail + k]
            mean = buf[tail + k:tail + 2 * k].view(np.float32)
            se = buf[tail + 2 * k:tail + 3 * k].view(np.float32)
            gain = mean[1:] - mean[0]
            better = (gain > 0) & (gain > z * se[1:])
            pick = int(np.argmax(np.where(better, gain, -np.inf))) + 1 if better.any() else 0
            actions = buf[body + G + nt * W:body + G + nt * W + na * AW].reshape(na, AW).copy()
            cand, adv, adv_se = np.full(MAX_K, -1, np.int64), np.zeros(MAX_K, np.float32), np.zeros(MAX_K, np.float32)
            cand[:k], adv[:k], adv_se[:k] = rows, mean - mean[0], se
            cols["global"].append(buf[body:body + G].copy())
            cols["tok"].append(buf[body + G:body + G + nt * W].reshape(nt, W).copy())
            cols["actions"].append(actions)
            cols["counts"].append((nt, na))
            cols["target"].append(int(rows[pick]))
            cols["policy"].append(int(rows[0]))
            cols["run"].append(run)
            cols["act"].append(act)
            cols["fight"].append(fight)
            cols["gain"].append(float(gain[pick - 1]) if pick else 0.0)
            cols["cand"].append(cand)
            cols["adv"].append(adv)
            cols["adv_se"].append(adv_se)
            cols["source"].append(number)
    n = len(cols["run"])
    if not n:
        raise SystemExit("no label record was read")
    counts = np.array(cols["counts"], np.int32)
    data = {"global": np.stack(cols["global"]), "tok": np.concatenate(cols["tok"]),
            "tok_at": np.concatenate([[0], np.cumsum(counts[:-1, 0], dtype=np.int64)]),
            "arow": np.concatenate(cols["actions"]),     # the action rows, unpadded like the tokens
            "arow_at": np.concatenate([[0], np.cumsum(counts[:-1, 1], dtype=np.int64)]), "counts": counts,
            "target": np.array(cols["target"], np.int64), "policy": np.array(cols["policy"], np.int64),
            "run": np.array(cols["run"], np.int64), "act": np.array(cols["act"], np.int64),
            "fight": np.array(cols["fight"], np.int64), "gain": np.array(cols["gain"], np.float32),
            # the tried rows (the policy's own first; -1 past a decision's count), each one's mean worth less the
            # policy's own row's, and the standard error of that paired difference
            "cand": np.stack(cols["cand"]), "adv": np.stack(cols["adv"]), "adv_se": np.stack(cols["adv_se"]),
            "source": np.array(cols["source"], np.int64)}
    del cols
    try:      # the lists of small arrays leave the heap fragmented: hand the freed pages back
        ctypes.CDLL("libc.so.6").malloc_trim(0)
    except OSError:
        pass
    return data, {"files": len(paths), "records": n, "truncated_files": truncated}


def batch_of(data, rows, device):
    counts = data["counts"][rows]
    tokens_used, actions_used = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
    tokens = np.zeros((len(rows), tokens_used, native.TOKEN_WIDTH), np.int32)
    actions = np.zeros((len(rows), actions_used, native.ACTION_WIDTH), np.int32)
    for i, row in enumerate(rows):
        first = data["tok_at"][row]
        tokens[i, :counts[i, 0]] = data["tok"][first:first + counts[i, 0]]
        first = data["arow_at"][row]
        actions[i, :counts[i, 1]] = data["arow"][first:first + counts[i, 1]]
    return to_device((data["global"][rows], tokens, actions, counts), device)


@torch.no_grad()
def agreement(model, data, rows, device, batch=2048):
    """Share of `rows` where the model's most probable action is the target: all, changed targets, unchanged ones."""
    model.eval()
    hit = np.zeros(len(rows), bool)
    for begin in range(0, len(rows), batch):
        part = rows[begin:begin + batch]
        logits = model(*batch_of(data, part, device))[0].float()
        hit[begin:begin + batch] = (logits.argmax(dim=1).cpu().numpy() == data["target"][part])
    changed = data["target"][rows] != data["policy"][rows]
    model.train()
    return {"all": float(hit.mean()) if len(rows) else None,
            "changed": float(hit[changed].mean()) if changed.any() else None,
            "unchanged": float(hit[~changed].mean()) if (~changed).any() else None,
            "n": int(len(rows)), "n_changed": int(changed.sum())}


def advantages(model, data, rows, device):
    """Predicted advantage of every tried row over the policy's own: [len(rows), MAX_K], 0 in column 0, -inf past a
    decision's candidates."""
    scores = model(*batch_of(data, rows, device))[0].float()
    cand = torch.from_numpy(data["cand"][rows]).to(device)
    valid = cand >= 0
    picked = scores.gather(1, cand.clamp(min=0))
    adv = picked - picked[:, :1]
    return torch.where(valid, adv, torch.full_like(adv, float("-inf"))), valid


@torch.no_grad()
def rule_value(model, data, rows, device, batch=2048):
    """On `rows`: how well the predicted advantages track the labelled ones, and for each margin what the rule
    "the policy's action unless a candidate's predicted advantage is above the margin" gains by the labels."""
    model.eval()
    pred = np.zeros((len(rows), MAX_K), np.float32)
    for begin in range(0, len(rows), batch):
        part = rows[begin:begin + batch]
        pred[begin:begin + batch] = advantages(model, data, part, device)[0].cpu().numpy()
    model.train()
    label, valid = data["adv"][rows], data["cand"][rows] >= 0
    others = valid.copy()
    others[:, 0] = False
    p, y = pred[others], label[others]
    out = {"n": int(len(rows)), "pairs": int(others.sum()),
           "explained": float(1 - np.mean((p - y) ** 2) / (np.var(y) + 1e-12)) if others.any() else None,
           "correlation": float(np.corrcoef(p, y)[0, 1]) if others.sum() > 2 and p.std() > 0 else None,
           "labelled_best_mean": float(np.where(others, label, -np.inf).max(axis=1).clip(min=0).mean())}
    masked = np.where(others, pred, -np.inf)
    best = masked.argmax(axis=1)
    best_pred = masked[np.arange(len(rows)), best]
    for margin in MARGINS:
        take = best_pred > margin
        gain = np.where(take, label[np.arange(len(rows)), best], 0.0)
        out[f"m{margin}"] = {"switched": float(take.mean()), "gain": float(gain.mean()),
                              "gain_se": float(gain.std() / np.sqrt(max(1, len(gain))))}
    return out


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--labels", required=True, help="comma-separated glob patterns of nosl_rollout_labels files")
    p.add_argument("--init", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--z", type=float, default=1.0)
    p.add_argument("--target", choices=("action", "advantage"), default="action")
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--batch", type=int, default=512)
    p.add_argument("--lr", type=float, default=1e-4)
    p.add_argument("--grad-norm", type=float, default=1.0)
    p.add_argument("--changed-weight", type=float, default=1.0, help="loss weight of decisions whose target is not the labelling policy's action")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--runs-below", type=int, default=0, help="train only on the runs with an index below this (a prefix "
                   "of the labelled runs: a point on the learning curve); the held-out runs are not cut")
    p.add_argument("--no-save", action="store_true", help="a measurement only: no checkpoint is written")
    p.add_argument("--decay", action="store_true", help="the learning rate falls to zero along a cosine over the whole training")
    p.add_argument("--save-best", action="store_true", help="--target advantage: the checkpoint holds the weights of the epoch "
                   "with the highest held-out explained variance, not the last epoch's (the held-out numbers of that epoch "
                   "are then a choice made on the held-out labels)")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args(argv)

    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    device = torch.device(args.device)
    data, read = read_labels(args.labels, args.z)
    held = data["run"] % 20 == 0
    # --runs-below N trains on a prefix of the runs; the held-out decisions are those of every held-out run, so that
    # the points of a learning curve are measured on the same decisions
    train_rows = np.flatnonzero(~held & ((data["run"] < args.runs_below) if args.runs_below else True))
    held_rows = np.flatnonzero(held)
    changed = data["target"] != data["policy"]
    summary = {**read, "train": int(len(train_rows)), "held_out": int(len(held_rows)), "runs": int(len(np.unique(data["run"]))),
               "changed_share": float(changed.mean()),
               "by_act": {str(a): {"n": int((data["act"] == a).sum()), "changed": float(changed[data["act"] == a].mean())}
                          for a in np.unique(data["act"])},
               "by_fight": {name: {"n": int((data["fight"] == i).sum()), "changed": float(changed[data["fight"] == i].mean())}
                            for i, name in enumerate(("weak", "normal", "elite", "boss")) if (data["fight"] == i).any()},
               "mean_gain_of_changed": float(data["gain"][changed].mean()) if changed.any() else None}
    pairs = data["cand"][train_rows, 1:] >= 0
    summary["train_pairs"] = int(pairs.sum())
    summary["train_advantage_variance"] = float(np.mean(data["adv"][train_rows, 1:][pairs] ** 2))   # what a loss of 0 explanation is
    noise = data["adv_se"][train_rows, 1:][pairs] ** 2
    summary["train_noise_share"] = float(noise.mean() / max(1e-12, np.var(data["adv"][train_rows, 1:][pairs])))
    print(json.dumps(summary), flush=True)

    model, blob = load(args.init, device)
    advantage = args.target == "advantage"
    if advantage:   # the scores become advantages: they start at zero, on the policy's features
        with torch.no_grad():
            model.policy[-1].weight.zero_()
            model.policy[-1].bias.zero_()
    model.train()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    log = Path(str(args.out) + ".train.jsonl").open("w", encoding="utf-8")
    measure = rule_value if advantage else agreement
    before = measure(model, data, held_rows, device)
    print(json.dumps({"epoch": 0, "held_out": before}), flush=True)
    log.write(json.dumps({"config": vars(args), "labels": summary, "epoch": 0, "held_out": before}) + "\n")
    weight = torch.from_numpy(np.where(changed, args.changed_weight, 1.0).astype(np.float32)).to(device)
    target = torch.from_numpy(data["target"]).to(device)
    began, step = time.time(), 0
    total_steps = args.epochs * -(-len(train_rows) // args.batch)
    best_explained, best_epoch, best_state = float("-inf"), 0, None
    for epoch in range(1, args.epochs + 1):
        # batches of similar size, so that padding is small; their order is random
        order = train_rows[np.argsort(data["counts"][train_rows, 0] + rng.random(len(train_rows)))]
        batches = [order[i:i + args.batch] for i in range(0, len(order), args.batch)]
        losses = []
        for which in rng.permutation(len(batches)):
            rows = np.sort(batches[which])
            where = torch.from_numpy(rows).to(device)
            if advantage:
                predicted, valid = advantages(model, data, rows, device)
                others = valid.clone()
                others[:, 0] = False     # column 0 is the policy's own action: its advantage is 0 by definition
                label = torch.from_numpy(data["adv"][rows]).to(device)
                loss = ((predicted[others] - label[others]) ** 2).mean()
            else:
                logits = model(*batch_of(data, rows, device))[0].float()
                loss = (F.cross_entropy(logits, target[where], reduction="none") * weight[where]).sum() / weight[where].sum()
            if args.decay:
                for group in optimizer.param_groups:
                    group["lr"] = args.lr * 0.5 * (1 + np.cos(np.pi * step / total_steps))
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), args.grad_norm)
            optimizer.step()
            losses.append(loss.item())
            step += 1
        row = {"epoch": epoch, "steps": step, "wall_s": round(time.time() - began, 1), "loss": round(float(np.mean(losses)), 5),
               "held_out": measure(model, data, held_rows, device)}
        print(json.dumps(row), flush=True)
        log.write(json.dumps(row) + "\n")
        log.flush()
        if args.save_best and advantage and row["held_out"]["explained"] > best_explained:
            best_explained, best_epoch = row["held_out"]["explained"], epoch
            best_state = {k: v.detach().clone() for k, v in model.state_dict().items()}
    if best_state is not None:
        model.load_state_dict(best_state)
    if not args.no_save:
        save(model, args.out, actor_filter=blob.get("actor_filter", 1), distilled_from=str(args.init), labels=summary,
             kind="combat_advantage" if advantage else "policy", epoch=best_epoch or args.epochs)
    log.close()


if __name__ == "__main__":
    main()
