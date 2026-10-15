#!/usr/bin/env python3
"""Do finer card-reward tables beat the context-free one? Measured on the collections' own random choices.

  tools/nosl/card_reward_cv.py --dir artifacts/nosl-r4 --generations 3,4,5 [--cache FILE.npz]

nosl.strategy_q collect takes a uniformly random action with a fixed probability. At a card reward that is a
randomized trial over the K options on offer. A value table is a policy (take the option with the highest value);
its value at one card reward, with the collecting policy everywhere else, is estimated without bias by the
K-weighted mean outcome of the exploratory decisions whose random action happened to be the table's pick. Two
tables are compared on the decisions where their picks differ.

The tables are fitted on all generations but one and tested on the one left out, rotated. Every table gives a card
the same kind of value as tools/nosl/card_table.py (mean outcome when taken minus when offered and not taken,
shrunk), within a cell: no cell (the table the rule uses), the act, the ascension, the deck size, low HP, how many
copies of the card the deck already holds. "pulled to context-free" shrinks a cell's value towards the context-free
one instead of towards zero.

It reads collection shards only (training data); no tuning or held-out seed is involved. The first run writes the
card-reward decisions (offer, deck, relics, outcome target) to --cache.
"""
import argparse
import collections
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from nosl.contexts import G_PROMPT, G_SOURCE, bucket  # noqa: E402
from nosl.shards import load_shard, shard_paths  # noqa: E402
from nosl.strategy_q import outcome_target  # noqa: E402

OPT, DECK, REL = 8, 96, 40


def extract(directory, generations, cache):
    """One row per card-reward choice of the collections `directory`/g<generation>/part*."""
    SRC, PROMPT = bucket("CardSelectCmd"), bucket("CardReward")
    out = {k: [] for k in ("gen", "run", "asc", "act", "floor", "hp", "maxhp", "gold", "K", "taken", "y", "explored", "opt", "optup", "deck", "deckup", "deckcount", "decksize", "relic", "step")}
    for gen in generations:
        for part in sorted(Path(directory, f"g{gen}").glob("part*")):
            for _, path in shard_paths([str(part)]):
                d = load_shard(path)
                g = d["g"]
                rows = np.flatnonzero((g[:, 1] == 7) & (g[:, G_SOURCE] == SRC) & (g[:, G_PROMPT] == PROMPT) & (d["na"] >= 2))
                if not len(rows): continue
                y = np.array([outcome_target(int(f), int(a), int(s)) for f, a, s in zip(d["floor"][rows], d["run_act"][rows], d["status"][rows])], np.float32)
                opt = np.zeros((len(rows), OPT), np.int16); optup = np.zeros((len(rows), OPT), np.int8)
                deck = np.zeros((len(rows), DECK), np.int16); deckup = np.zeros((len(rows), DECK), np.int8)
                deckcount = np.zeros((len(rows), DECK), np.int8); relic = np.zeros((len(rows), REL), np.int16)
                for j, r in enumerate(rows):
                    tok = d["tok"][d["tok_at"][r]:d["tok_at"][r] + d["nt"][r]]
                    act = d["actions"][d["act_at"][r]:d["act_at"][r] + d["na"][r]]
                    for k, a in enumerate(act[:OPT]):
                        if a[1] >= 0 and tok[a[1], 6] == 57:      # ChoiceOptionKind Card
                            opt[j, k] = tok[a[1], 1]; optup[j, k] = tok[a[1], 8]
                        else:
                            opt[j, k] = -1                          # the alternative: skip
                    dk = tok[tok[:, 0] == 5][:DECK]
                    deck[j, :len(dk)] = dk[:, 1]; deckup[j, :len(dk)] = dk[:, 8]; deckcount[j, :len(dk)] = dk[:, 9]   # the deck is a bag: one token per distinct card, n1 copies
                    rl = tok[tok[:, 0] == 8][:REL]
                    relic[j, :len(rl)] = rl[:, 2]
                out["gen"].append(np.full(len(rows), gen, np.int8)); out["run"].append(gen * 10_000_000 + int(part.name[4:]) * 1_000_000 + d["run"][rows].astype(np.int64))
                out["asc"].append(g[rows, 3].astype(np.int8)); out["act"].append(g[rows, 4].astype(np.int8)); out["floor"].append(g[rows, 6].astype(np.int16))
                out["hp"].append(g[rows, 7].astype(np.int16)); out["maxhp"].append(g[rows, 8].astype(np.int16)); out["gold"].append(g[rows, 10].astype(np.int32))
                out["K"].append(d["na"][rows].astype(np.int8)); out["taken"].append(d["choice"][rows].astype(np.int8)); out["y"].append(y)
                out["explored"].append(d["explored"][rows]); out["step"].append(d["step"][rows])
                out["opt"].append(opt); out["optup"].append(optup); out["deck"].append(deck); out["deckup"].append(deckup); out["deckcount"].append(deckcount); out["decksize"].append(g[rows, 21].astype(np.int16)); out["relic"].append(relic)
    np.savez_compressed(cache, **{k: np.concatenate(v) for k, v in out.items()})


def compare(cache, generations):
    with np.load(cache) as z:
        ex_all = z["explored"].astype(bool)
        y_all, gen_all = z["y"].astype(np.float64), z["gen"]
        rule_mean = {g: float(y_all[~ex_all & (gen_all == g)].mean()) for g in generations}
        total_decisions, rule_all = len(y_all), float(y_all[~ex_all].mean())
        d = {k: z[k][ex_all] for k in z.files}      # only the exploratory decisions are compared; the rest would not fit in memory
    del y_all, gen_all
    N = len(d["y"]); K = d["K"].astype(np.int64)
    opt = d["opt"].astype(np.int64); up = (d["optup"] > 0).astype(np.int64)
    key = np.where(opt > 0, opt * 2 + up, np.where(opt < 0, 1, 0))          # 0 pad, 1 skip, else card*2+upgraded
    valid = np.arange(opt.shape[1])[None, :] < K[:, None]
    key = np.where(valid, key, 0)
    taken = d["taken"].astype(np.int64)
    y = d["y"].astype(np.float64); gen = d["gen"]; asc = d["asc"]; act = d["act"].astype(np.int64)
    deck, deckcount, deckn = d["deck"], d["deckcount"].astype(np.int16), d["decksize"].astype(np.int64)
    # copies of each offered card already in the deck (any upgrade level); the deck is a bag of (card, upgrade) with counts
    copies = np.zeros_like(opt)
    for k in range(opt.shape[1]):
        o = d["opt"][:, [k]]
        copies[:, k] = (deckcount * ((deck == o) & (o > 0))).sum(1)
    ascg = np.select([asc == 0, asc == 5], [0, 1], 2)
    sizeg = np.select([deckn <= 15, deckn <= 20, deckn <= 26], [0, 1, 2], 3)
    hpg = (d["hp"] / np.maximum(1, d["maxhp"]) < 0.5).astype(np.int64)
    print(f"decisions {total_decisions}, exploratory {N}, per generation {[int((gen == g).sum()) for g in generations]}")
    print(f"mean outcome: exploratory {y.mean():.4f}  rule's own {rule_all:.4f}; K mean {K.mean():.2f}; skip offered {np.mean((key == 1).any(1)):.3f}")
    print(f"deck size quartiles {np.percentile(deckn, [25, 50, 75]).tolist()}; offered card already in the deck: {np.mean(copies[key > 1] > 0):.3f}")
    NK = int(key.max()) + 1
    CELLS = {
        "none": (np.zeros(N, np.int64), None), "act": (np.minimum(act, 2), None), "ascension": (ascg, None), "deck size": (sizeg, None), "hp<50%": (hpg, None),
        "copies": (None, np.minimum(copies, 2)), "act x ascension": (np.minimum(act, 2) * 3 + ascg, None),
    }
    def fit(rows, cell_state, cell_opt, shrink=300.0, prior=None, k=600.0):
        """value[cell, key]: (mean when taken - mean when offered and not taken) shrunk; skip: mean - mean of the others."""
        nc = int((cell_state if cell_state is not None else cell_opt).max()) + 1
        tn = np.zeros((nc, NK)); ts = np.zeros((nc, NK)); on = np.zeros((nc, NK)); os_ = np.zeros((nc, NK))
        r = rows
        for kk in range(key.shape[1]):
            c = cell_state[r] if cell_state is not None else cell_opt[r, kk]
            kcol = key[r, kk]; m = kcol > 0
            is_taken = taken[r] == kk
            np.add.at(tn, (c[m & is_taken], kcol[m & is_taken]), 1); np.add.at(ts, (c[m & is_taken], kcol[m & is_taken]), y[r][m & is_taken])
            np.add.at(on, (c[m & ~is_taken], kcol[m & ~is_taken]), 1); np.add.at(os_, (c[m & ~is_taken], kcol[m & ~is_taken]), y[r][m & ~is_taken])
        with np.errstate(invalid="ignore", divide="ignore"):
            raw = np.where((tn > 0) & (on > 0), ts / tn - os_ / on, 0.0)
        if prior is None:
            return raw * tn / (tn + shrink), tn
        return (tn * raw + k * prior[None, :]) / (tn + k), tn      # a finer cell is pulled to the context-free value
    def policy(value, cell_state, cell_opt, rows):
        v = np.full((len(rows), key.shape[1]), -9.0)
        for kk in range(key.shape[1]):
            c = cell_state[rows] if cell_state is not None else cell_opt[rows, kk]
            kcol = key[rows, kk]
            v[:, kk] = np.where(kcol > 0, value[c, kcol], -9.0)
        return v.argmax(1)
    def ips(rows, pick):
        m = taken[rows] == pick
        w = K[rows][m].astype(np.float64); yy = y[rows][m]
        mean = (w * yy).sum() / w.sum()
        se = np.sqrt((w ** 2 * (yy - mean) ** 2).sum()) / w.sum()
        return mean, se, int(m.sum())
    def versus(rows, a, b):
        """value(a) - value(b) per decision: on the decisions where they differ, times how often they differ."""
        diff = a != b
        if not diff.any(): return 0.0, 0.0, 0.0
        r = rows[diff]
        ma, sa, _ = ips(r, a[diff]); mb, sb, _ = ips(r, b[diff])
        f = diff.mean()
        return f * (ma - mb), f * np.hypot(sa, sb), f
    total = collections.defaultdict(lambda: [0.0, 0.0, 0.0, 0.0])
    for test in generations:
        tr = np.flatnonzero(gen != test); te = np.flatnonzero(gen == test)
        v0, _ = fit(tr, CELLS["none"][0], None)
        p0 = policy(v0, CELLS["none"][0], None, te)
        m0, s0, n0 = ips(te, p0)
        rule = rule_mean[test]; rnd = y[te].mean()
        print(f"\ntest generation {test}: random {rnd:.4f}  rule's own {rule:.4f}  context-free table {m0:.4f} ± {1.96 * s0:.4f} (n={n0})")
        total["table - rule"][0] += m0 - rule; total["table - rule"][1] += s0 ** 2; total["table - rule"][3] += 1
        for name, (cs, co) in CELLS.items():
            if name == "none": continue
            for label, kw in (("split", {}), ("pulled to context-free", {"prior": v0[0]})):
                v, _ = fit(tr, cs, co, **kw)
                p = policy(v, cs, co, te)
                dm, ds, f = versus(te, p, p0)
                print(f"  {name:16s} {label:24s} differs on {f:5.1%}  value - context-free {dm:+.4f} ± {1.96 * ds:.4f}")
                t = total[f"{name} / {label}"]; t[0] += dm; t[1] += ds ** 2; t[2] += f; t[3] += 1
    print("\nmean over the test generations (per decision; a run has about 7.7 card rewards):")
    for name, (s, var, f, n) in total.items():
        print(f"  {name:44s} {s / n:+.4f} ± {1.96 * np.sqrt(var) / n:.4f}   differs on {f / n:5.1%}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dir", required=True, help="holds g<generation>/part*/shards")
    p.add_argument("--generations", default="3,4,5")
    p.add_argument("--cache", default="")
    args = p.parse_args()
    generations = [int(x) for x in args.generations.split(",")]
    cache = args.cache or str(Path(args.dir, "card-decisions.npz"))
    if not Path(cache).exists():
        extract(args.dir, generations, cache)
    compare(cache, generations)


if __name__ == "__main__":
    main()
