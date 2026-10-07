#!/usr/bin/env python3
"""Verify an optimization against the previous native search, including partial routes.

These A10 elite/boss starting-deck probes exercise search/forecast continuations;
they do not certify realistic late-game loadouts or wins against these encounters.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    binaries = {"baseline": args.baseline.resolve(), "native": args.native.resolve()}
    rows = []
    report = {"schema": "sts2.combatsolver.native-comparison.v1",
              "binaries": {k: hashlib.sha256(v.read_bytes()).hexdigest() for k, v in binaries.items()},
              "scope": "matched native roots, routes, qualities and complete Env hashes; A10 starting-deck elite/boss probes",
              "cases": rows}
    for encounter in ("BYGONE_EFFIGY_ELITE", "QUEEN_BOSS", "KAISER_CRAB_BOSS"):
        for character in ("IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT"):
            case = args.out / (character + "-" + encounter)
            case.mkdir(exist_ok=True)
            common = ["--character", character, "--encounter", encounter, "--seed", "CSPERFEXTRA01",
                      "--ascension", "10", "--act-index", "2", "--beam", "16", "--nodes", "3000",
                      "--budget-ms", "60000", "--max-turns", "12"]
            row = {"character": character, "encounter": encounter, "arguments": common}
            try:
                order = list(binaries)
                if len(rows) % 2: order.reverse()
                results = {}
                for label in order:
                    path = case / (label + ".json")
                    with (case / (label + ".log")).open("w") as log:
                        subprocess.run([str(binaries[label]), *common, "--out", str(path)], stdout=log,
                                       stderr=subprocess.STDOUT, check=True, timeout=180)
                    results[label] = json.loads(path.read_text())
                b, n = results["baseline"], results["native"]
                checks = {key: b[key] == n[key] for key in
                          ("root", "root_hash", "actions", "quality", "final_search_hash")}
                checks["expanded"] = b["metrics"]["expanded"] == n["metrics"]["expanded"]
                row.update(status="identical" if all(checks.values()) else "different", checks=checks,
                           baseline_metrics=b["metrics"], native_metrics=n["metrics"], quality=n["quality"])
            except (OSError, subprocess.SubprocessError, ValueError, KeyError) as error:
                row.update(status="error", error=str(error))
            rows.append(row)
            (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps({k: row[k] for k in ("character", "encounter", "status", "error") if k in row}), flush=True)
    good = [r for r in rows if r["status"] == "identical"]
    report["summary"] = {"requested": len(rows), "identical": len(good),
                         "native_seconds": sum(r["native_metrics"]["elapsed_ms"] for r in good) / 1000,
                         "baseline_seconds": sum(r["baseline_metrics"]["elapsed_ms"] for r in good) / 1000,
                         "won": sum(r["quality"]["won"] for r in good)}
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["summary"]), flush=True)
    return 0 if len(good) == len(rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
