#!/usr/bin/env python3
"""Oracle-backed M2 card coverage matrix.

Runs every Ironclad pool card in three Act-1 encounters, A0/A10, eight seeds,
and two random policies.  A run is accepted only when l3_combat reports no
unported content and no divergence.
"""
import argparse, concurrent.futures, json, subprocess, sys


def ironclad_cards():
    data = json.load(open("data/content.json", encoding="utf-8"))
    pool = next(x for x in data["models"] if x.get("id") == "CARD_POOL.IRONCLAD_CARD_POOL")
    return [x.split(".", 1)[1] for x in pool["props"]["AllCardIds"]]


def run_one(args):
    card, encounter, ascension, seeds, policies = args
    # Exhaust-heavy cards can remove every attack in a four-card deck on the
    # first turn. A longer deck keeps those Slimes combats finite while still
    # exercising the target card under the same random policies.
    strikes = 6 if card in ("FIEND_FIRE", "TRUE_GRIT") and encounter == "SLIMES_WEAK" else 1
    deck = ",".join([card] + ["STRIKE_IRONCLAD"] * strikes + ["DEFEND_IRONCLAD", "BASH"])
    cmd = [sys.executable, "tests/l3/l3_combat.py", "--seeds", seeds,
           "--characters", "IRONCLAD", "--policy-seeds", policies,
           "--ascension", str(ascension), "--encounter", encounter,
           "--deck", deck, "--max-actions", "120"]
    p = subprocess.run(cmd, capture_output=True, text=True)
    summary = (p.stdout + p.stderr).strip().splitlines()[-1] if (p.stdout or p.stderr) else ""
    return card, encounter, ascension, p.returncode, summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--encounters", default="TOADPOLES_WEAK,NIBBITS_WEAK,SLIMES_WEAK")
    ap.add_argument("--seeds", default=",".join(f"M2CARD{i:04d}" for i in range(1, 9)))
    ap.add_argument("--policy-seeds", default="1,2")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    work = [(c, e, a, args.seeds, args.policy_seeds)
            for e in args.encounters.split(",") for c in ironclad_cards() for a in (0, 10)]
    failures = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for card, encounter, ascension, rc, summary in pool.map(run_one, work):
            print(f"{encounter} {card} A{ascension} rc={rc} {summary}", flush=True)
            if rc != 0 or "0 unported, 0 diverged" not in summary:
                failures += 1
    print(f"M2 card matrix: {len(work) - failures}/{len(work)} cells passed; {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
