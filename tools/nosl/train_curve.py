#!/usr/bin/env python3
"""Training curves of nosl.ppo runs as window means (docs/plans/2026-10-04-nosl-q4f-steady-progress.md).

  tools/nosl/train_curve.py artifacts/nosl-q4f/k1 artifacts/nosl-q4f/e1 [--marks 1,5,10,15,20] [--window 1]

The window mean at a mark is the mean floor of every training run that ended
in the `window` million decisions before it: several thousand runs, where
`recent_floor_mean` in the log is the last 2,000. --relative counts decisions
from the start of each log (for a run resumed from a checkpoint).

--verdict checks each run against the three "steady progress" conditions of
the Q4-f plan (window means at 10M, 15M and 20M decisions).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def load(path: Path) -> list[dict]:
    file = path / "train.jsonl" if path.is_dir() else path
    return [json.loads(line) for line in file.open(encoding="utf-8") if line.strip()]


def window(rows: list[dict], mark: float, width: float) -> dict | None:
    """Rows whose decision count is in (mark - width, mark], in millions."""
    chosen = [r for r in rows if (mark - width) * 1e6 < r["decisions"] <= mark * 1e6]
    if not chosen or rows[-1]["decisions"] < mark * 1e6 - 30720:
        return None
    episodes = sum(r["episodes"] for r in chosen)
    out = {"episodes": episodes,
           "floor": sum(r["floor_mean"] * r["episodes"] for r in chosen if r["episodes"]) / max(1, episodes),
           "act2": chosen[-1].get("recent_act2_plus")}
    fights = sum(r.get("boss_fights", 0) for r in chosen)
    if fights:
        out["boss_fights"] = fights
        out["boss_share_of_runs"] = fights / max(1, episodes)
        out["boss_damage"] = sum(r["boss_damage_share"] * r["boss_fights"] for r in chosen if r.get("boss_fights")) / fights
        out["boss_win"] = sum(r.get("boss_wins", 0) for r in chosen) / fights
    for key in ("entropy_out", "entropy_combat", "explained_variance", "approx_kl"):
        values = [r[key] for r in chosen if r.get(key) is not None]
        if values:
            out[key] = sum(values) / len(values)
    return out


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("runs", nargs="+")
    p.add_argument("--marks", default="1,2,3,5,7,10,12,15,17,20", help="decision counts, in millions")
    p.add_argument("--window", type=float, default=1.0, help="window width, in millions of decisions")
    p.add_argument("--relative", action="store_true", help="count decisions from the first row of each log")
    p.add_argument("--json", action="store_true")
    p.add_argument("--verdict", action="store_true", help="the Q4-f steady-progress conditions, per run")
    args = p.parse_args()
    marks = [10.0, 15.0, 20.0] if args.verdict else [float(m) for m in args.marks.split(",")]
    table = {}
    for run in args.runs:
        rows = load(Path(run))
        if args.relative and rows:
            start = rows[0]["decisions"] - (rows[1]["decisions"] - rows[0]["decisions"] if len(rows) > 1 else 0)
            rows = [{**r, "decisions": r["decisions"] - start} for r in rows]
        table[run] = {m: window(rows, m, args.window) for m in marks}
    if args.verdict:
        for run, by in table.items():
            w10, w15, w20 = by[10.0], by[15.0], by[20.0]
            if not (w10 and w15 and w20):
                print(f"{Path(run).name}: not at 20M decisions yet")
                continue
            checks = (("20M - 10M >= 1.0 floors", w20["floor"] - w10["floor"], w20["floor"] - w10["floor"] >= 1.0),
                      ("20M - 15M >= 0.3 floors", w20["floor"] - w15["floor"], w20["floor"] - w15["floor"] >= 0.3),
                      ("act 2 share at 20M >= 5% and above 10M", w20["act2"], w20["act2"] >= 0.05 and w20["act2"] > w10["act2"]))
            print(f"{Path(run).name}: floors {w10['floor']:.2f} / {w15['floor']:.2f} / {w20['floor']:.2f} at 10M / 15M / 20M, "
                  f"act 2 share {w10['act2']:.3f} -> {w20['act2']:.3f}")
            for label, value, passed in checks:
                print(f"    {'pass' if passed else 'FAIL'}  {label}: {value:+.3f}" if "floors" in label
                      else f"    {'pass' if passed else 'FAIL'}  {label}: {value:.3f}")
            print(f"    => {'steady progress on this seed' if all(c[2] for c in checks) else 'not steady progress'}")
        return
    if args.json:
        print(json.dumps({run: {str(m): w for m, w in by.items()} for run, by in table.items()}, indent=1))
        return
    columns = (("floor", "{:6.2f}"), ("act2", "{:6.3f}"), ("boss_share_of_runs", "{:6.3f}"), ("boss_damage", "{:6.3f}"),
               ("boss_win", "{:6.3f}"), ("entropy_out", "{:6.3f}"), ("entropy_combat", "{:6.3f}"))
    for key, fmt in columns:
        if not any(w and w.get(key) is not None for by in table.values() for w in by.values()):
            continue
        print(key)
        print(" " * 28 + "".join(f"{m:>6g}M" for m in marks))
        for run, by in table.items():
            cells = "".join((" " + fmt.format(w[key])) if w and w.get(key) is not None else "      -" for w in by.values())
            print(f"{Path(run).name[:27]:27} {cells}")


if __name__ == "__main__":
    main()
