#!/usr/bin/env python3
"""Bounded basic RF acceptance, fresh captures and portable public-ABI replay.

Build the Env, Oracle, env_api_shop_fixture and env_api_crystal_sphere_fixture
first. This is an independent test consumer, not a game client or full RF matrix.
Every output directory is new; failures and their raw process logs are retained.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import rf_operations as rf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--library", type=Path, default=rf.ROOT / "build/libsts2env_c.so")
    parser.add_argument("--dotnet", default=os.environ.get("DOTNET", str(Path.home() / ".dotnet/dotnet")))
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    library = args.library.resolve(strict=True)
    host = Path(shutil.which(args.dotnet) or args.dotnet).resolve(strict=True)
    oracle = rf.ROOT / "tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll"
    shop = rf.ROOT / "build/libenv_api_shop_fixture.so"
    crystal = rf.ROOT / "build/libenv_api_crystal_sphere_fixture.so"
    def freeze():
        result = rf.provenance(rf.ROOT / "build/sts2sim_cli", oracle, True, dotnet=str(host))
        result["basic_rf_files"] = {str(p): rf.digest(p) for p in
                                    (library, shop, crystal, Path(__file__).resolve())}
        return result
    started = time.monotonic()
    baseline = freeze()
    env = dict(os.environ, STS2SIM_ENV_LIB=str(library), DOTNET=str(host))
    operations = [sys.executable, str(rf.ROOT / "tests/api/rf_operations.py"),
                  "--oracle-test-mode", "off", "--dotnet", str(host)]
    jobs = [
        ("relic", operations + ["--seeds", "BASICRF001,BASICRF002"]),
        ("transform", operations + ["--operation", "transform_cards", "--seeds", "BASICRFX001,BASICRFX002"]),
        ("merchant", operations + ["--operation", "merchant_purchase", "--seeds", "BASICRFM001,BASICRFM002"]),
        ("courier", operations + ["--operation", "merchant_purchase", "--setup-relics", "THE_COURIER",
                                  "--seeds", "BASICRFC001,BASICRFC002"]),
        ("reroll", operations + ["--relics", "ORRERY", "--setup-relics", "DRIFTWOOD",
                                 "--card-reward-policy", "reroll_once", "--seeds", "BASICRFR001,BASICRFR002"]),
        ("crystal", [sys.executable, str(rf.ROOT / "tests/api/rf_crystal_sphere.py"),
                     "--dotnet", str(host), "--seeds", "SEED0002,BASICRFS001"]),
    ]
    parents = [output / name for name, _ in jobs]
    jobs.append(("portable", [sys.executable, str(rf.ROOT / "tests/api/portable_rf_replay.py"),
                              "--library", str(library),
                              *[arg for parent in parents for arg in ("--input", str(parent))]]))
    rf.write_json(output / "manifest.json", {"scope": "basic RF fixture subset; not full RF or live capture",
                                             "provenance": baseline, "jobs": jobs})
    records = []
    qualified = False
    try:
        for name, command in jobs:
            command = command + ["--output-dir", str(output / name)]
            print(f"basic RF: {name}", flush=True)
            begin = time.monotonic()
            with (output / f"{name}.stdout.log").open("wb") as stdout, \
                 (output / f"{name}.stderr.log").open("wb") as stderr:
                result = subprocess.run(command, cwd=rf.ROOT, env=env, stdout=stdout, stderr=stderr)
            record = {"job": name, "command": command, "returncode": result.returncode,
                      "seconds": time.monotonic() - begin}
            records.append(record)
            rf.write_json(output / "processes.json", records)
            if result.returncode:
                raise AssertionError(f"{name} failed: see retained stdout/stderr")
            summary = json.loads((output / name / "summary.json").read_text())
            rf.equal(True, summary["qualified"], f"{name}.qualified")
            if name != "portable":
                rf.equal(summary["cases"], summary["passed"], f"{name}.complete_case_matrix")
        rf.equal(baseline, freeze(), "basic_rf.frozen_source_and_runtime")
        qualified = True
    finally:
        rf.write_json(output / "summary.json", {
            "qualified": qualified, "scope": "basic RF fixture subset; not full RF or live capture",
            "completed_jobs": len(records), "planned_jobs": len(jobs), "processes": records,
            "seconds": time.monotonic() - started,
        })


if __name__ == "__main__":
    main()
