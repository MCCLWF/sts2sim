#!/usr/bin/env python3
"""What are HP and a potion worth after a fight, relative to surviving it? Prices for the combat trainer's reward.

  tools/nosl/hp_exchange.py --data DIR[,DIR...] --out reward-table.json

nosl.combat_ppo --reward combat pays a survived combat (B + HP change / max HP - c * potions used) / B and a death
0: B is what surviving is worth in whole HP bars, c what a potion is worth in HP bars. They were set by hand (3 and
0.06). This tool measures them on a collection: one sample per (run, floor), the state a fight room is left in (its
first reward-screen decision). Per ascension and act, a ridge regression of the run's final outcome target on that
state: HP fraction and potions held enter once per floor bucket; the deck (which cards it holds), the relics, max
HP, the floor and the act's boss are controls, because a deck that loses less HP also goes further for other
reasons. With a_hp and a_pot the bucket's coefficients and D the mean of (final target - the target of dying on
that floor), B = D / a_hp and c = a_pot / a_hp. Observational: the prices are those of the collecting policy's
own runs.

Written to --out: per ascension the buckets [act, first floor, last floor, B, c] with B kept within [1, 5] (at
B < 1 a won fight could pay less than a death) and c within [0, 0.5], the raw estimates beside them, and the pair
for boss rooms, where what is left after the fight matters little (B 10, c 0: measured a_hp is about 0 there).
An ascension or act with too few fights takes the nearest lower ascension's buckets for that act.
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from nosl import native  # noqa: E402
from nosl.shards import load_shard, shard_paths  # noqa: E402

BUCKETS = {0: ((1, 5), (6, 10), (11, 15)), 1: ((18, 22), (23, 27), (28, 32)), 2: ((34, 40), (41, 47))}
BOSS = (10.0, 0.0)


def ridge(X, y, l2=1.0):
    X = torch.tensor(X, device=DEVICE, dtype=torch.float64)
    y = torch.tensor(y, device=DEVICE, dtype=torch.float64)
    A = X.T @ X + l2 * torch.eye(X.shape[1], device=DEVICE, dtype=torch.float64)
    A[0, 0] -= l2
    return torch.linalg.solve(A, X.T @ y).cpu().numpy()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()
    global DEVICE
    DEVICE = args.device
    rows = {k: [] for k in ("asc", "act", "floor", "hp", "maxhp", "pot", "boss", "y", "room")}
    decks, relics = [], []
    for directory in args.data.split(","):
        for _, path in shard_paths([directory]):
            d = load_shard(path)
            g = d["g"]
            m = np.flatnonzero((g[:, 1] == 2) & np.isin(g[:, 26], (1, 2, 3)))     # a reward screen in a fight room
            key = d["run"][m].astype(np.int64) * 100 + g[m, 6]
            order = np.lexsort((d["step"][m], key))
            m, key = m[order], key[order]
            m = m[np.r_[True, key[1:] != key[:-1]]]                                # the first one of each (run, floor)
            y = d["floor"][m] / 50 + 0.5 * d["run_act"][m] + 2.0 * (d["status"][m] == native.VICTORY)
            for k, v in (("asc", g[m, 3]), ("act", g[m, 4]), ("floor", g[m, 6]), ("hp", g[m, 7]), ("maxhp", g[m, 8]),
                         ("pot", g[m, 23]), ("boss", g[m, 25]), ("y", y), ("room", g[m, 26])):
                rows[k].append(np.asarray(v))
            for r in m:
                tok = d["tok"][d["tok_at"][r]:d["tok_at"][r] + d["nt"][r]]
                decks.append(np.unique(tok[tok[:, 0] == 5][:, 1]))
                relics.append(np.unique(tok[tok[:, 0] == 8][:, 2]))
    R = {k: np.concatenate(v) for k, v in rows.items()}
    ncard = int(max(d.max() for d in decks if len(d))) + 1
    nrel = int(max(r.max() for r in relics if len(r))) + 1
    result = {"data": args.data, "fight_ends": int(len(R["y"])), "boss": list(BOSS), "ascension": {}, "estimates": {}}
    print(f"fight ends {len(R['y'])}")
    for asc in (0, 5, 10):
        buckets, raw = [], []
        for act in sorted(BUCKETS):
            m = np.flatnonzero((R["asc"] == asc) & (R["act"] == act) & (R["room"] != 3))
            if len(m) < 3000:
                continue
            hp = R["hp"][m] / np.maximum(1, R["maxhp"][m])
            pot, fl = R["pot"][m].astype(np.float64), R["floor"][m]
            cols = [np.ones(len(m))]
            for lo, hi in BUCKETS[act]:
                b = (fl >= lo) & (fl <= hi)
                cols += [b * hp, b * pot]
            cols += [fl == f for f in np.unique(fl)[1:]] + [R["boss"][m] == b for b in np.unique(R["boss"][m])[1:]]
            cols.append(R["maxhp"][m] / 10)
            C = np.zeros((len(m), ncard), np.float32)
            Rl = np.zeros((len(m), nrel), np.float32)
            for j, i in enumerate(m):
                C[j, decks[i]] = 1
                Rl[j, relics[i]] = 1
            X = np.concatenate([np.stack(cols, 1), C[:, C.sum(0) >= 200], Rl[:, Rl.sum(0) >= 200]], 1)
            w = ridge(X, R["y"][m])
            for k, (lo, hi) in enumerate(BUCKETS[act]):
                b = (fl >= lo) & (fl <= hi)
                if b.sum() < 1000:
                    continue
                D = float((R["y"][m][b] - (fl[b] / 50 + 0.5 * act)).mean())
                a_hp, a_pot = float(w[1 + 2 * k]), float(w[2 + 2 * k])
                B = float(np.clip(D / a_hp, 1.0, 5.0)) if a_hp > 1e-6 else 5.0
                c = float(np.clip(a_pot / a_hp, 0.0, 0.5)) if a_hp > 1e-6 else 0.0
                buckets.append([act, lo, hi, round(B, 3), round(c, 3)])
                raw.append({"act": act, "floors": [lo, hi], "n": int(b.sum()), "alive_worth": D, "hp_bar": a_hp, "potion": a_pot,
                            "B_raw": D / a_hp if a_hp > 1e-6 else None, "c_raw": a_pot / a_hp if a_hp > 1e-6 else None})
                print(f"A{asc} act {act + 1} floors {lo}-{hi}: n={b.sum():6d}  alive {D:.3f}  HP bar {a_hp:+.3f}  potion {a_pot:+.3f}"
                      f"  ->  B {B:.2f} (raw {D / a_hp:.2f})  c {c:.2f} (raw {a_pot / a_hp:.2f})")
        result["ascension"][str(asc)] = buckets
        result["estimates"][str(asc)] = raw
    # an act an ascension has no estimate for takes the nearest lower ascension's
    for asc, lower in ((5, 0), (10, 5)):
        have = {b[0] for b in result["ascension"][str(asc)]}
        result["ascension"][str(asc)] += [b for b in result["ascension"][str(lower)] if b[0] not in have]
    Path(args.out).write_text(json.dumps(result, indent=1) + "\n")


if __name__ == "__main__":
    main()
