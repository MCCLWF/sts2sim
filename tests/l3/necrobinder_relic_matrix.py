#!/usr/bin/env python3
"""Run source-triggered Oracle checks for all eight Necrobinder relics."""
import concurrent.futures
import json
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
L3 = ROOT / "tests" / "l3" / "l3_combat.py"
OUT = ROOT / "docs" / "reports" / "necrobinder-relic-matrix.jsonl"
RAW = ROOT / "docs" / "reports" / "necrobinder-relic-traces"

# Each case makes the relic hook relevant in a solo combat. Doom cases are
# intentionally included and require the shared DoomPower dependency.
CASES = [
    ("BIG_HAT", "POKE", 3),
    ("BONE_FLUTE", "POKE", 3),
    ("BOOK_REPAIR_KNIFE", "HAUNT,SCOURGE,SCOURGE,SCOURGE,SCOURGE,SCOURGE", 99),
    ("BOOKMARK", "REAP", 0),
    ("BOUND_PHYLACTERY", "UNLEASH", 3),
    ("FUNERARY_MASK", "POKE", 3),
    ("IVORY_TILE", "REAP", 3),
    ("UNDYING_SIGIL", "SCOURGE,SCOURGE,SCOURGE,SCOURGE,SCOURGE", 99),
]


def main():
    RAW.mkdir(parents=True, exist_ok=True)

    def run(case):
        relic, deck, energy = case
        seed = "NCR" + relic.replace("_", "")[:9]
        oracle_path = RAW / f"{relic.lower()}.oracle.jsonl"
        sim_path = RAW / f"{relic.lower()}.sim.jsonl"
        cmd = [sys.executable, str(L3), "--seeds", seed, "--characters", "NECROBINDER",
               "--policy-seeds", "1", "--encounter", "CULTISTS_NORMAL", "--max-actions", "160",
               "--deck", deck, "--relics", relic, "--starting-energy", str(energy), "--verbose",
               "--trace-output", str(oracle_path), "--sim-trace-output", str(sim_path)]
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
        return {"relic": relic, "seed": seed, "deck": deck, "starting_energy": energy,
                "command": cmd, "returncode": proc.returncode, "stdout": proc.stdout.strip(),
                "stderr": proc.stderr.strip(), "oracle_trace": str(oracle_path.relative_to(ROOT)),
                "sim_trace": str(sim_path.relative_to(ROOT))}

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(run, CASES))
    OUT.write_text("".join(json.dumps(row, ensure_ascii=False) + "\n" for row in results), encoding="utf-8")
    passed = sum(row["returncode"] == 0 and "1/1 combats identical" in row["stdout"] and
                 "0 unported" in row["stdout"] and "0 diverged" in row["stdout"] for row in results)
    for row in results:
        print(f"{row['relic']}: {row['stdout'].splitlines()[-1] if row['stdout'] else row['stderr']}")
    print(f"Necrobinder relic Oracle matrix: {passed}/{len(CASES)} passed; results: {OUT}")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    raise SystemExit(main())
