#!/usr/bin/env python3
"""Do the rollout advantages become predictable once an action's immediate effect is given?

  PYTHONPATH=python .venv/bin/python tools/nosl/advantage_features.py --labels 'DIR/a2.*.bin,...' --features 'DIR/a2.*.feat,...'

A diagnostic of why the policy network does not learn the lookahead's corrections. The labels are the paired
advantages of nosl_rollout_labels (a tried action's mean worth less the policy's own action's); the features are the
same tool's --afterstate numbers (what each tried action does at once: damage, kills, block, HP, energy). A small
network is fitted on features alone: for each tried action its own numbers, the policy's action's numbers, their
difference, and a few numbers of the situation (HP, block, energy, the damage the enemies show). Runs whose index is
a multiple of 20 are held out, as in nosl.rollout_distill, and the same held-out measures are printed, so that the
two can be compared: the policy network on the public observation, and this one on immediate effects only.

The afterstate numbers come from stepping a copy of the true Env, so they are an upper bound on what displayed card
values could give a policy; this tool says whether that kind of information is what is missing, nothing more.
"""
from __future__ import annotations

import argparse
import glob
import json
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
from nosl import native  # noqa: E402

MARGINS = (0.0, 0.02, 0.05, 0.1, 0.2)
F = 11


def read_features(patterns):
    out = {}
    for path in sorted(p for pattern in patterns.split(",") for p in glob.glob(pattern)):
        buf = np.fromfile(path, dtype=np.int32)
        at = 0
        while at + 4 <= len(buf):
            magic, run, index, k = (int(x) for x in buf[at:at + 4])
            assert magic == 0x4145464E, path
            if at + 4 + k + k * F > len(buf):
                break
            rows = buf[at + 4:at + 4 + k].copy()
            feats = buf[at + 4 + k:at + 4 + k + k * F].view(np.float32).reshape(k, F).copy()
            out[(run, index)] = (rows, feats)
            at += 4 + k + k * F
    return out


def read_labels(patterns):
    G, W, AW = native.GLOBAL_WIDTH, native.TOKEN_WIDTH, native.ACTION_WIDTH
    out = []
    for path in sorted(p for pattern in patterns.split(",") for p in glob.glob(pattern)):
        buf = np.fromfile(path, dtype=np.int32)
        at = 0
        while at + 10 <= len(buf):
            magic, run, index, nt, na, k, copies, taken, act, fight = (int(x) for x in buf[at:at + 10])
            end = at + 10 + G + nt * W + na * AW + 3 * k
            if end > len(buf):
                break
            g = buf[at + 10:at + 10 + G]
            tokens = buf[at + 10 + G:at + 10 + G + nt * W].reshape(nt, W)
            tail = end - 3 * k
            rows = buf[tail:tail + k].copy()
            mean = buf[tail + k:tail + 2 * k].view(np.float32)
            enemy = tokens[(tokens[:, 0] == 6) & (tokens[:, 15] != 0)]
            incoming = float((np.maximum(enemy[:, 11], 0) * np.maximum(enemy[:, 12], 1)).sum())
            context = np.array([g[7] / max(1, g[8]), g[9] / 50, g[12] / 3, incoming / 50, max(0.0, incoming - g[9]) / 50,
                                g[15] / 10, act, fight / 3, len(enemy) / 3, g[7] / 50], np.float32)
            out.append((run, index, rows, (mean - mean[0]).copy(), context))
            at = end
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--labels", required=True)
    p.add_argument("--features", required=True)
    p.add_argument("--epochs", type=int, default=60)
    p.add_argument("--seed", type=int, default=1)
    args = p.parse_args()
    feats = read_features(args.features)
    scale = np.array([20, 20, 1, 20, 20, 3, 3, 1, 1, 1, 10], np.float32)
    x, y, group, held, first = [], [], [], [], []
    decisions = missing = 0
    for run, index, rows, adv, context in read_labels(args.labels):
        if (run, index) not in feats:
            missing += 1
            continue
        frows, f = feats[(run, index)]
        if len(frows) != len(rows) or (frows != rows).any():
            missing += 1
            continue
        f = np.clip(f, -100, 200) / scale    # one enemy kind carries a billion HP
        for c in range(1, len(rows)):
            x.append(np.concatenate([f[c], f[0], f[c] - f[0], context]))
            y.append(adv[c])
            group.append(decisions)
            held.append(run % 20 == 0)
        decisions += 1
    x, y, group, held = np.stack(x), np.array(y, np.float32), np.array(group), np.array(held)
    print(json.dumps({"decisions": decisions, "pairs": int(len(y)), "unmatched_decisions": missing,
                      "held_out_pairs": int(held.sum()), "label_variance": float(y.var())}))
    torch.manual_seed(args.seed)
    device = torch.device("cpu")
    net = torch.nn.Sequential(torch.nn.Linear(x.shape[1], 128), torch.nn.ReLU(), torch.nn.Linear(128, 128), torch.nn.ReLU(),
                              torch.nn.Linear(128, 1)).to(device)
    optimizer = torch.optim.Adam(net.parameters(), lr=1e-3, weight_decay=1e-5)
    xt, yt = torch.from_numpy(x[~held]).to(device), torch.from_numpy(y[~held]).to(device)
    xh, yh, gh = torch.from_numpy(x[held]).to(device), y[held], group[held]
    rng = np.random.default_rng(args.seed)

    def measure():
        net.eval()
        with torch.no_grad():
            pred = net(xh).squeeze(1).cpu().numpy()
        net.train()
        out = {"explained": float(1 - np.mean((pred - yh) ** 2) / yh.var()), "correlation": float(np.corrcoef(pred, yh)[0, 1])}
        ids = np.unique(gh)
        for margin in MARGINS:
            gains = []
            for d in ids:      # per held-out decision: the rule's pick among its pairs
                sel = np.flatnonzero(gh == d)
                best = sel[np.argmax(pred[sel])]
                gains.append(yh[best] if pred[best] > margin else 0.0)
            gains = np.array(gains)
            out[f"m{margin}"] = {"switched": float((gains != 0).mean()), "gain": float(gains.mean()),
                                  "gain_se": float(gains.std() / np.sqrt(len(gains)))}
        out["decisions"] = int(len(ids))
        return out

    for epoch in range(1, args.epochs + 1):
        order = rng.permutation(len(xt))
        losses = []
        for begin in range(0, len(order), 1024):
            rows = torch.from_numpy(order[begin:begin + 1024]).to(device)
            loss = ((net(xt[rows]).squeeze(1) - yt[rows]) ** 2).mean()
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            losses.append(loss.item())
        if epoch in (1, 2, 5, 10, 20, 40, 60, args.epochs):
            print(json.dumps({"epoch": epoch, "train_loss": round(float(np.mean(losses)), 5), "held_out": measure()}))


if __name__ == "__main__":
    main()
