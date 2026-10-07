"""Behaviour cloning of teacher combat demonstrations (tools/nosl/nosl_run --record-dir).

  .venv/bin/python -m nosl.bc --data artifacts/nosl-q3/demos --out artifacts/nosl-q3/bc/checkpoint.pt

The labels are one action per combat decision from a search that saw the
true future. Agreement with them is a diagnostic, not a quality measure: the
policy is judged by whole runs (nosl.evaluate). Runs are split, not rows:
every 20th run index is held out for validation.
"""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import numpy as np
import torch

from . import native
from .model import ActorCritic, load, save

MAGIC = 0x4E4F534C


def load_shards(directory):
    G, W, AW = native.GLOBAL_WIDTH, native.TOKEN_WIDTH, native.ACTION_WIDTH
    blobs = [np.fromfile(path, dtype=np.int32) for path in sorted(Path(directory).glob("shard-*.bin"))]
    index = []  # (blob, position)
    max_tokens = max_actions = 0
    for b, blob in enumerate(blobs):
        position = 0
        while position < len(blob):
            if blob[position] != MAGIC:
                raise ValueError(f"shard {b}: bad record at int {position}")
            tokens, actions = int(blob[position + 2 + G]), int(blob[position + 3 + G])
            index.append((b, position))
            max_tokens, max_actions = max(max_tokens, tokens), max(max_actions, actions)
            position += 5 + G + tokens * W + actions * AW
    n = len(index)
    data = {"run": np.zeros(n, np.int32), "global": np.zeros((n, G), np.int16),
            "tokens": np.zeros((n, max_tokens, W), np.int16), "actions": np.zeros((n, max_actions, AW), np.int16),
            "counts": np.zeros((n, 2), np.int16), "label": np.zeros(n, np.int64)}
    for i, (b, position) in enumerate(index):
        blob = blobs[b]
        tokens, actions, label = (int(v) for v in blob[position + 2 + G:position + 5 + G])
        body = position + 5 + G
        data["run"][i] = blob[position + 1]
        data["global"][i] = np.clip(blob[position + 2:position + 2 + G], -32768, 32767)
        data["tokens"][i, :tokens] = np.clip(blob[body:body + tokens * W].reshape(tokens, W), -32768, 32767)
        data["actions"][i, :actions] = np.clip(blob[body + tokens * W:body + tokens * W + actions * AW].reshape(actions, AW), -32768, 32767)
        data["counts"][i] = (tokens, actions)
        data["label"][i] = label
    return data


def batches(rows, counts, rng, size, token_budget):
    order = rows[np.argsort(counts[rows, 0] + rng.random(len(rows)))]
    out, begin = [], 0
    while begin < len(order):
        widest = int(counts[order[min(len(order), begin + size) - 1], 0])
        take = max(64, min(size, token_budget // max(1, widest)))
        out.append(np.sort(order[begin:begin + take]))
        begin += take
    return [out[i] for i in rng.permutation(len(out))]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--init", default="", help="start from this checkpoint instead of fresh weights")
    p.add_argument("--epochs", type=int, default=8)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--minibatch", type=int, default=2048)
    p.add_argument("--token-budget", type=int, default=160000)
    p.add_argument("--width", type=int, default=128)
    p.add_argument("--layers", type=int, default=3)
    p.add_argument("--features", type=int, default=1, help="model input features (nosl/model.py)")
    p.add_argument("--card-table", default="", help="tools/nosl/nosl_card_table output, for --features 2")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args(argv)

    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    device = torch.device(args.device)
    data = load_shards(args.data)
    held = data["run"] % 20 == 0
    train_rows, valid_rows = np.flatnonzero(~held), np.flatnonzero(held)
    model = load(args.init, device)[0] if args.init else ActorCritic(
        args.width, args.layers, features=args.features, card_table=args.card_table).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    total = args.epochs * len(batches(train_rows, data["counts"], rng, args.minibatch, args.token_budget))
    schedule = torch.optim.lr_scheduler.OneCycleLR(optimizer, max_lr=args.lr, total_steps=max(total, 1), pct_start=0.05)
    amp = device.type == "cuda"

    def run(rows, train):
        loss_sum = hit = seen = 0.0
        for chunk in batches(rows, data["counts"], rng, args.minibatch, args.token_budget):
            counts = data["counts"][chunk]
            tokens_used, actions_used = int(counts[:, 0].max()), int(counts[:, 1].max())
            batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device) for a in
                     (data["global"][chunk], data["tokens"][chunk, :tokens_used], data["actions"][chunk, :actions_used], counts)]
            label = torch.from_numpy(data["label"][chunk]).to(device)
            with torch.set_grad_enabled(train), torch.autocast("cuda", dtype=torch.bfloat16, enabled=amp):
                logits, _ = model(*batch)
            loss = torch.nn.functional.cross_entropy(logits.float(), label)
            if train:
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                optimizer.step()
                schedule.step()
            loss_sum += loss.item() * len(chunk)
            hit += (logits.argmax(dim=1) == label).sum().item()
            seen += len(chunk)
        return loss_sum / max(seen, 1), hit / max(seen, 1)

    log = []
    began = time.time()
    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss, train_hit = run(train_rows, True)
        model.eval()
        valid_loss, valid_hit = run(valid_rows, False) if len(valid_rows) else (float("nan"), float("nan"))
        row = {"epoch": epoch, "train_loss": round(train_loss, 4), "train_agreement": round(train_hit, 4),
               "valid_loss": round(valid_loss, 4), "valid_agreement": round(valid_hit, 4), "wall_s": round(time.time() - began, 1)}
        log.append(row)
        print(json.dumps(row), flush=True)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    save(model, out, iteration=0, decisions=0)
    summary = {"records": int(len(data["label"])), "train_records": int(len(train_rows)), "valid_records": int(len(valid_rows)),
               "runs": int(len(np.unique(data["run"]))), "epochs": log, "args": vars(args)}
    out.with_suffix(".bc.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
