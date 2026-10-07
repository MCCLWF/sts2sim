#!/usr/bin/env python3
"""Oracle replay for every Necrobinder pool card at base and +1.

This deliberately injects one card into a solo fight so the content body is
reached independently of random card rewards. Multiplayer-only cards are
included: their solo forced-card execution is useful model coverage, while
pool eligibility remains governed by the source multiplayer constraint.
"""
import concurrent.futures
import json
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CONTENT = ROOT / "data" / "content.json"
L3 = ROOT / "tests" / "l3" / "l3_combat.py"
OUT = ROOT / "docs" / "reports" / "necrobinder-card-matrix.jsonl"


def main():
    models = json.loads(CONTENT.read_text(encoding="utf-8"))["models"]
    by_id = {m["id"]: m for m in models}
    card_ids = by_id["CARD_POOL.NECROBINDER_CARD_POOL"]["props"]["AllCardIds"]
    jobs = [(card_id.removeprefix("CARD."), upgrade, index)
            for index, (card_id, upgrade) in enumerate(
                (pair for card_id in card_ids for pair in ((card_id, 0), (card_id, 1))))]

    def run(job):
        card, upgrade, index = job
        seed = f"NCBMAT{index:03d}"
        deck = card + (f"+{upgrade}" if upgrade else "")
        cmd = [sys.executable, str(L3), "--seeds", seed, "--characters", "NECROBINDER",
               "--policy-seeds", "1", "--encounter", "CULTISTS_NORMAL", "--max-actions", "120",
               "--deck", deck, "--starting-energy", "99", "--verbose"]
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
        return {"card": card, "upgrade": upgrade, "seed": seed, "command": cmd,
                "returncode": proc.returncode, "stdout": proc.stdout.strip(),
                "stderr": proc.stderr.strip()}

    OUT.parent.mkdir(parents=True, exist_ok=True)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        for result in pool.map(run, jobs):
            results.append(result)
            if result["returncode"] or "0 diverged" not in result["stdout"]:
                print(json.dumps(result, ensure_ascii=False), flush=True)
            elif len(results) % 20 == 0:
                print(f"Oracle card forms: {len(results)}/{len(jobs)} passed", flush=True)
    OUT.write_text("".join(json.dumps(result, ensure_ascii=False) + "\n" for result in results),
                   encoding="utf-8")
    passed = sum(r["returncode"] == 0 and "1/1 combats identical" in r["stdout"]
                 and "0 unported" in r["stdout"] and "0 diverged" in r["stdout"] for r in results)
    print(f"Necrobinder card Oracle matrix: {passed}/{len(jobs)} forms passed; raw summaries: {OUT}")
    return 0 if passed == len(jobs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
