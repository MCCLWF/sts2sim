#!/usr/bin/env python3
"""The chance of getting past an act's boss, from the state a run reaches that boss in.

  tools/nosl/boss_readiness.py --data g3:DIR,DIR g4:DIR,DIR g5:DIR,DIR --out readiness.json [--doors FILE.npz]
                               [--segments]

Every recorded run of nosl.strategy_q collect that reaches a boss gives one "door": the state at its last stored
decision on the floor before the boss (the rest site) and whether the run entered the next act. With the combat
policy fixed, what happens in the boss fight depends on nothing but that state, so a model of the door is a
low-noise answer to "what should a run bring to this boss": a card is judged on every door whose deck holds it,
one fight later, not on the few runs in which an exploratory action picked it and on where those runs ended.

The model is a logistic regression per act (acts 1 and 2; boss floors 17 and 33). Features, all public:
ascension, which boss, HP fraction (plus the 30% a final Heal adds), maximum HP, potions held, whether the last
rest action was Smith (that upgrade is not in the recorded deck), for each card whether the deck holds at least
1 .. 5 copies and at least 1 .. 3 upgraded copies, each relic (the rare ones share one coefficient), and for common cards and relics their
interaction with the boss. It is observational: the doors are the states the collecting policies happened to
reach, and a combination no run reached has no evidence behind it.

--segments fits a chain of five doors instead of the two bosses (see SEGMENTS): the second half of act 1, the act 1
boss, the first and second halves of act 2, the act 2 boss. A hallway door is the first stored decision from a
floor on, and it is passed when the run reaches the floor the segment ends at. For every door and ascension the
output also holds how often it is passed and how much higher the outcome target of the runs that passed it is than
of those that did not: the weights of nosl.readiness's chain rule. The two boss doors are also written under
"acts", so a chain file serves the boss-only rule as well.

The groups are generations; the last one is held out for the report (log loss, AUC, how often the doors the
model rates highest really got past), then the model is fitted on all of them and written to --out as a table
of coefficients indexed by the observation's ids (nosl.readiness reads it).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
from nosl import native  # noqa: E402
from nosl.contexts import enum_names  # noqa: E402
from nosl.shards import load_shard, shard_paths  # noqa: E402

DECK, REL, POT = 96, 40, 6
BOSS_FLOOR = {0: 17, 1: 33}
# name, act, which decision is the door ("last" on that floor, or "first" from that floor on), the floor that passes it
# (None: entering the next act)
BOSS_SEGMENTS = (("S2", 0, "last", 16, None), ("S5", 1, "last", 32, None))
SEGMENTS = (("S1", 0, "first", 9, 17), ("S2", 0, "last", 16, None), ("S3", 1, "first", 18, 26), ("S4", 1, "first", 26, 33),
            ("S5", 1, "last", 32, None))
MIN_COUNT, MIN_INTERACTION = 300, 3000
N_LEVELS, U_LEVELS = 5, 3   # card features: at least 1..5 copies, at least 1..3 upgraded copies
SCALARS = ("asc", "act", "hp", "maxhp", "gold", "boss", "decksize", "won", "last_kind", "last_rest", "segment", "target100")


def extract(groups, out, segments):
    """One row per (run, segment) whose door the run reached."""
    cols = {k: [] for k in ("group",) + SCALARS + ("deck", "deckup", "deckcount", "relic", "potion")}
    for index, (_, directories) in enumerate(groups):
        for directory in directories:
            best = {}  # (run, segment) -> (step, record)
            for _, path in shard_paths([directory]):
                d = load_shard(path)
                g = d["g"]
                final_floor, final_act = d["floor"].astype(np.int64), d["run_act"].astype(np.int64)
                for number, (_, act, which, floor, passes) in enumerate(segments):
                    if which == "last":      # the floor before a boss: the run must have walked onto the boss floor
                        rows = np.flatnonzero((g[:, 4] == act) & (g[:, 6] == floor) & (final_act >= act)
                                              & ((final_act > act) | (final_floor >= floor + 1)))
                    else:
                        rows = np.flatnonzero((g[:, 4] == act) & (g[:, 6] >= floor) & (g[:, 6] < (passes or 99)))
                    for r in rows:
                        key, step = (int(d["run"][r]), number), int(d["step"][r])
                        if key in best and (best[key][0] >= step if which == "last" else best[key][0] <= step):
                            continue
                        tok = d["tok"][d["tok_at"][r]:d["tok_at"][r] + d["nt"][r]]
                        action = d["actions"][d["act_at"][r] + d["choice"][r]]
                        rest = int(tok[action[1], 6] - 41) if (which == "last" and g[r, 1] == 4 and action[1] >= 0) else -1
                        won = final_act[r] > act or (passes is not None and final_act[r] == act and final_floor[r] >= passes)
                        target = final_floor[r] / 50 + 0.5 * final_act[r] + 2.0 * (d["status"][r] == native.VICTORY)
                        deck, relic, potion = tok[tok[:, 0] == 5][:DECK], tok[tok[:, 0] == 8][:REL], tok[tok[:, 0] == 9][:POT]
                        best[key] = (step, (g[r, 3], act, g[r, 7], g[r, 8], g[r, 10], g[r, 25], g[r, 21], bool(won),
                                            g[r, 1] if which == "last" else -1, rest, number, round(100 * target),
                                            deck[:, 1].copy(), deck[:, 8].copy(), deck[:, 9].copy(),
                                            relic[:, 2].copy(), potion[:, 3].copy()))
            n = len(best)
            wide = {"deck": np.zeros((n, DECK), np.int16), "deckup": np.zeros((n, DECK), np.int8),
                    "deckcount": np.zeros((n, DECK), np.int8), "relic": np.zeros((n, REL), np.int16),
                    "potion": np.zeros((n, POT), np.int16)}
            scalar = {k: np.zeros(n, np.int32) for k in SCALARS}
            for j, (_, record) in enumerate(best.values()):
                for k, v in zip(SCALARS, record[:len(SCALARS)]):
                    scalar[k][j] = int(v)
                for k, v in zip(("deck", "deckup", "deckcount", "relic", "potion"), record[len(SCALARS):]):
                    wide[k][j, :len(v)] = v
            cols["group"].append(np.full(n, index, np.int8))
            for k in SCALARS:
                cols[k].append(scalar[k])
            for k in wide:
                cols[k].append(wide[k])
            print(f"{directory}: {n} doors", flush=True)
    np.savez_compressed(out, **{k: np.concatenate(v) for k, v in cols.items()})


def features(d, mask):
    """The design of one act: (X, names, layout). `layout` says which column is which coefficient of the table."""
    deck, count, up, relic = d["deck"][mask], d["deckcount"][mask].astype(np.float32), d["deckup"][mask] > 0, d["relic"][mask]
    hp = d["hp"][mask] / np.maximum(1, d["maxhp"][mask])
    hp_door = np.minimum(1.0, hp + 0.3 * (d["last_rest"][mask] == 0))
    smithed = (d["last_rest"][mask] == 1) | (d["last_kind"][mask] == 7)
    boss, asc = d["boss"][mask], d["asc"][mask]
    bosses = [int(b) for b in np.unique(boss)]
    cols, layout = [], []

    def add(key, v):
        v = np.asarray(v)
        cols.append(v if v.dtype == np.bool_ else v.astype(np.float32))   # indicators stay one byte until the matrix is built
        layout.append(key)

    add(("asc", 5), asc == 5)
    add(("asc", 10), asc == 10)
    for b in bosses[1:]:
        add(("boss", b), boss == b)
    add(("hp10",), hp_door * 10)
    add(("maxhp10",), d["maxhp"][mask] / 10)
    add(("smithed",), smithed)
    add(("potion",), (d["potion"][mask] > 0).sum(1))
    has_card, has_relic = {}, {}
    for cid in (int(c) for c in np.unique(deck) if c > 0):
        held = deck == cid
        n, u = (count * held).sum(1), (count * (held & up)).sum(1)
        if (n > 0).sum() < MIN_COUNT:
            continue
        has_card[cid] = n >= 1
        # a threshold is a feature only where doors on both sides of it are common (every deck starts with five
        # Strikes: "at least 3" tells nothing, "at least 5" tells whether one was removed)
        for k in range(1, N_LEVELS + 1):
            if MIN_COUNT <= (n >= k).sum() <= len(n) - MIN_COUNT or (k == 1 and (n >= 1).sum() >= MIN_COUNT):
                add(("card_n", cid, k), n >= k)
        for k in range(1, U_LEVELS + 1):
            if (u >= k).sum() >= MIN_COUNT:
                add(("card_u", cid, k), u >= k)
    other = np.zeros(len(relic), np.float32)      # relics too rare for a feature of their own: one shared coefficient
    for rid in (int(r) for r in np.unique(relic) if r > 0):
        held = (relic == rid).any(1)
        if held.sum() >= MIN_COUNT:
            has_relic[rid] = held
            add(("relic", rid), held)
        else:
            other += held
    add(("relic_other",), other)
    for kind, table in (("card_boss", has_card), ("relic_boss", has_relic)):
        for item, held in table.items():
            if held.sum() >= MIN_INTERACTION:
                for b in bosses[1:]:
                    add((kind, b, item), held & (boss == b))
    X = np.empty((len(boss), len(cols)), np.float32)
    for j, column in enumerate(cols):
        X[:, j] = column
    return X, layout, bosses


def fit(X, y, l2):
    X, y = torch.tensor(X, device=DEVICE), torch.tensor(y, device=DEVICE)
    w = torch.zeros(X.shape[1], device=DEVICE, requires_grad=True)
    b = torch.zeros(1, device=DEVICE, requires_grad=True)
    opt = torch.optim.LBFGS([w, b], lr=1.0, max_iter=500, history_size=20, line_search_fn="strong_wolfe")

    def closure():
        opt.zero_grad()
        loss = torch.nn.functional.binary_cross_entropy_with_logits(X @ w + b, y) + l2 * (w ** 2).sum()
        loss.backward()
        return loss

    opt.step(closure)
    return w.detach().cpu().numpy(), float(b.detach().cpu())


def log_loss(z, y):
    p = np.clip(1 / (1 + np.exp(-z)), 1e-6, 1 - 1e-6)
    return float(-(y * np.log(p) + (1 - y) * np.log(1 - p)).mean())


def auc(z, y):
    order = np.argsort(z)
    rank = np.empty(len(z))
    rank[order] = np.arange(1, len(z) + 1)
    pos = y > 0.5
    return float((rank[pos].sum() - pos.sum() * (pos.sum() + 1) / 2) / max(1, pos.sum() * (~pos).sum()))


def as_table(w, b, layout, bosses):
    """The coefficients by what they belong to; ids are the observation's ids, as strings (JSON keys)."""
    t = {"intercept": b, "asc": {}, "boss": {str(bosses[0]): 0.0}, "card": {}, "relic": {}, "card_boss": {}, "relic_boss": {}}
    for key, value in zip(layout, (float(x) for x in w)):
        kind = key[0]
        if kind in ("hp10", "maxhp10", "smithed", "potion", "relic_other"):
            t[kind] = value
        elif kind in ("asc", "boss", "relic"):
            t[kind][str(key[1])] = value
        elif kind in ("card_n", "card_u"):
            entry = t["card"].setdefault(str(key[1]), {"n": [0.0] * N_LEVELS, "u": [0.0] * U_LEVELS})
            entry["n" if kind == "card_n" else "u"][key[2] - 1] = value
        else:
            t[kind].setdefault(str(key[1]), {})[str(key[2])] = value
    return t


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--data", nargs="+", required=True, help="NAME:DIR[,DIR...] per generation; the last one is held out for the report")
    p.add_argument("--out", required=True)
    p.add_argument("--doors", default="", help="where the extracted doors are kept (default: OUT with .doors.npz)")
    p.add_argument("--segments", action="store_true", help="the chain of five doors instead of the two bosses")
    p.add_argument("--extract-only", action="store_true", help="write the doors file and stop")
    p.add_argument("--weights-from", default="", help="the group whose doors alone give the chain weights (pass rates and "
                   "gains); default: all groups")
    p.add_argument("--l2", type=float, default=5e-5)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()
    global DEVICE
    DEVICE = args.device
    segments = SEGMENTS if args.segments else BOSS_SEGMENTS
    groups = [(item.split(":", 1)[0], item.split(":", 1)[1].split(",")) for item in args.data]
    doors = Path(args.doors or str(Path(args.out).with_suffix("")) + ".doors.npz")
    if not doors.exists():
        extract(groups, doors, segments)
    if args.extract_only:
        return
    d = dict(np.load(doors))
    cards, relics = enum_names("CardId"), enum_names("RelicId")
    result = {"sources": {name: dirs for name, dirs in groups}, "held_out_group": groups[-1][0], "l2": args.l2,
              "weights_from": args.weights_from or "all groups",
              "segments": {}, "order": [name for name, *_ in segments], "weights": {}, "acts": {}, "report": {}}
    for number, (name, act, which, floor, passes) in enumerate(segments):
        mask = d["segment"] == number
        y, group, asc = d["won"][mask].astype(np.float32), d["group"][mask], d["asc"][mask]
        X, layout, bosses = features(d, mask)
        test = group == len(groups) - 1
        w, b = fit(X[~test], y[~test], args.l2)
        z = X[test] @ w + b
        report = {"doors": int(mask.sum()), "won": float(y.mean()), "features": len(layout), "test_doors": int(test.sum()),
                  "test_log_loss": log_loss(z, y[test]), "test_auc": auc(z, y[test]), "by_ascension": {}}
        for level in (0, 5, 10):
            s = asc[test] == level
            if s.sum() < 200:
                continue
            cuts = np.quantile(z[s], [0.0, 0.5, 0.8, 0.9, 0.95])
            report["by_ascension"][str(level)] = {
                "test_doors": int(s.sum()), "won": float(y[test][s].mean()), "log_loss": log_loss(z[s], y[test][s]),
                "won_in_top": {key: float(y[test][s][z[s] >= cut].mean()) for key, cut in zip(("100%", "50%", "20%", "10%", "5%"), cuts)}}
        # the chain rule's weights: how often the door is passed, and what passing it is worth, per ascension
        target = d["target100"][mask] / 100.0
        weigh = np.ones(len(asc), bool) if not args.weights_from else group == [n for n, _ in groups].index(args.weights_from)
        for level in np.unique(asc[weigh]):
            s = (asc == level) & weigh
            passed = y[s] > 0.5
            result["weights"].setdefault(str(int(level)), {})[name] = {
                "p": float(passed.mean()), "gain": float(target[s][passed].mean() - target[s][~passed].mean()), "doors": int(s.sum())}
        w, b = fit(X, y, args.l2)
        table = as_table(w, b, layout, bosses)
        table["act"] = act
        table["door"] = [which, floor, passes]
        result["segments"][name] = table
        if which == "last":
            result["acts"][str(act)] = table
        result["report"][name] = report
        what = f"act {act + 1} boss" if which == "last" else f"act {act + 1}, from floor {floor} to floor {passes}"
        print(f"\n{name} ({what}): {report['doors']} doors, {report['won']:.3f} passed; {report['features']} features; "
              f"held-out log loss {report['test_log_loss']:.4f}, AUC {report['test_auc']:.4f}")
        for level, r in report["by_ascension"].items():
            weight = result["weights"][level][name]
            print(f"  A{level}: {r['test_doors']} held-out doors, {r['won']:.3f} passed; of the model's top "
                  + ", ".join(f"{k} {v:.3f}" for k, v in r["won_in_top"].items() if k != "100%")
                  + f"; passing is worth {weight['gain']:+.3f}")
        print(f"  HP +10% {table['hp10']:+.3f}   potion {table['potion']:+.3f}   max HP +10 {table['maxhp10']:+.3f}   "
              + "  ".join(f"A{k} {v:+.2f}" for k, v in table["asc"].items()))
        first = sorted(((v["n"][0], cards[int(k)]) for k, v in table["card"].items()), reverse=True)
        print("  first copy, most: " + ", ".join(f"{card} {value:+.2f}" for value, card in first[:14]))
        print("  first copy, least: " + ", ".join(f"{card} {value:+.2f}" for value, card in first[-10:]))
        upgrade = sorted(((v["u"][0], cards[int(k)]) for k, v in table["card"].items() if v["u"][0]), reverse=True)
        print("  first upgrade, most: " + ", ".join(f"{card} {value:+.2f}" for value, card in upgrade[:12]))
        relic = sorted(((v, relics[int(k)]) for k, v in table["relic"].items()), reverse=True)
        print("  relics, most: " + ", ".join(f"{item} {value:+.2f}" for value, item in relic[:10]))
    Path(args.out).write_text(json.dumps(result, indent=1) + "\n")


if __name__ == "__main__":
    main()
