#!/usr/bin/env python3
"""Strict public Env consumer for the shipped Crystal Sphere event operation.

Records the entire source/Choice/final state, all RNGs and raw oracle output.
The test-only fixture injects an event root; no later operation uses C++ internals.
"""
from __future__ import annotations
import argparse
import ctypes
import importlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import traceback
import rf_operations as rf

ROOT = Path(__file__).resolve().parents[2]

def check_case(oracle, directory, fixture):
    rf.equal("sts2oracle.env_crystal_sphere", oracle["format"], "oracle.format")
    rf.equal(1, oracle["version"], "oracle.version")
    rf.equal(False, oracle["oracle_test_mode"], "oracle.production_mode")
    config = {key: oracle[key] for key in ("seed", "character", "ascension")}
    config["gold"] = oracle["fixture_gold"]
    payload = json.dumps(config, separators=(",", ":")).encode()
    diagnostics = rf.Env()
    output = ctypes.c_void_p(1)
    try:
        status = fixture.sts2_test_crystal_sphere_fixture(diagnostics._handle, payload, len(payload), ctypes.byref(output))
        if status or not output.value or output.value == 1:
            raise AssertionError(f"Crystal fixture failed ({status}); see recorded stderr")
    finally:
        diagnostics.__del__()
    env = rf.Env.__new__(rf.Env)
    env._handle = output.value
    handles = [env]
    try:
        rf.equal(3, env.decision, "source.event_decision")
        rf.equal(oracle["source"], rf.state(env), "source")
        rf.equal(None, env.export_run_queries()["event"]["crystal_sphere"], "source.uninitialized_board")
        operation = {"format": "sts2env.operation", "version": 1, "source_hash": env.state_hash,
                     "kind": "action", "stop": "operation_done",
                     "params": {"kind": 6, "index": oracle["event_option"], "target": -1, "col": 0, "row": 0}}
        env, previews, answers, prefixes, checks = rf.execute_checked(env, operation, oracle["choices"], directory, handles)
        rf.equal(oracle["final"], rf.state(env), "final")
        rf.equal(True, oracle["event_finished"], "oracle.finished")
        rf.equal(True, env.export_run_queries()["event"]["finished"], "native.finished")
        rf.equal("complete", previews[-1]["operation_status"], "final.operation_status")
        rf.equal(0, env.decision, "final.map_decision")
        snapshot = env.export_snapshot()
        rf.equal(None, snapshot["pending_operation"], "final.pending_operation")
        rf.equal(None, snapshot["pending"], "final.pending_action")
        restored = rf.Env.restore_snapshot(snapshot)
        try:
            rf.equal(snapshot, restored.export_snapshot(), "final.restore")
            rf.equal(env.export_run_queries(), restored.export_run_queries(), "final.restore_queries")
        finally:
            restored.__del__()
        rf.write_json(directory / "previews.json", previews)
        rf.write_json(directory / "final.json", {"snapshot": snapshot, "state": rf.state(env), "answers": answers})
        return {"seed": oracle["seed"], "case_id": oracle["case_id"], "passed": True,
                "native_prefixes": prefixes, "answers": answers, **checks}
    finally:
        for handle in reversed(handles):
            handle.__del__()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seeds", default="SEED0002,RFCRYSTAL0001,RFCRYSTAL0002,RFCRYSTAL0003")
    parser.add_argument("--character", default="IRONCLAD")
    parser.add_argument("--ascension", type=int, default=0)
    parser.add_argument("--event-options", default="0,1")
    parser.add_argument("--answer-orders", default="60,26,38,47,121,17,122,30;84,45,23,105,75,66;121,0,122,120,121,10,122,110,121,60,122,60")
    parser.add_argument("--gold", type=int, default=300)
    parser.add_argument("--dotnet", default=os.environ.get("DOTNET", str(Path.home() / ".dotnet" / "dotnet")))
    parser.add_argument("--oracle", type=Path, default=ROOT / "tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll")
    parser.add_argument("--fixture", type=Path, default=ROOT / "build/libenv_api_crystal_sphere_fixture.so")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--rng-trace", action="store_true", help="Explicitly request privileged RNG call diagnostics")
    parser.add_argument("--causal-trace", action="store_true", help="Explicitly request privileged model/Hook frames")
    args = parser.parse_args()
    rf.RNG_TRACE = args.rng_trace
    rf.CAUSAL_TRACE = args.causal_trace
    seeds, options, orders = args.seeds.split(","), args.event_options.split(","), args.answer_orders.split(";")
    for values in (seeds, options, orders):
        if not all(values) or len(values) != len(set(values)):
            parser.error("Matrices must be nonempty and unique")
    if set(options) - {"0", "1"} or args.gold < 100:
        parser.error("Event options are 0/1; fixture gold must be at least 100")
    for order in orders:
        try:
            indices = [int(i) for i in order.split(",")]
        except ValueError:
            parser.error("Answer orders contain integer indices")
        if any(i < 0 or i > 122 for i in indices) or sum(i < 121 for i in indices) < (6 if "1" in options else 3):
            parser.error("Answer indices must be 0..122 and supply all divinations")
    output = args.output_dir.resolve()
    if output.exists(): parser.error("Choose a new output directory; raw evidence is preserved")
    args.oracle, args.fixture = args.oracle.resolve(strict=True), args.fixture.resolve(strict=True)
    library = Path(os.environ.get("STS2SIM_ENV_LIB", str(ROOT / "build/libsts2env_c.so"))).resolve(strict=True)
    host = Path(shutil.which(args.dotnet) or args.dotnet).resolve(strict=True)
    def freeze():
        result = rf.provenance(ROOT / "build/sts2sim_cli", args.oracle, True, dotnet=str(host))
        result["oracle_qualification"] = "TestMode=false shipped DLL with scoped presentation/input adapters and injected event root; not live capture"
        for name, path in (("env_library", library), ("crystal_fixture", args.fixture)):
            result["runtime"][name] = {"path": str(path), "sha256": rf.digest(path)}
        return result
    initial = freeze()
    output.mkdir(parents=True)
    command = [str(host), str(args.oracle), "env-crystal-sphere", "--oracle-test-mode", "off",
               "--seeds", args.seeds, "--character", args.character, "--ascension", str(args.ascension),
               "--event-options", args.event_options, "--answer-orders", args.answer_orders, "--gold", str(args.gold)]
    rf.write_json(output / "manifest.json", {"command": command, "provenance": initial,
        "config": {"seeds": seeds, "options": options, "orders": orders, "character": args.character,
                   "ascension": args.ascension, "rng_trace_enabled": args.rng_trace,
                   "causal_trace_enabled": args.causal_trace}})
    sys.path.insert(0, str(ROOT / "python"))
    binding = importlib.import_module("sts2sim_env")
    rf.Env, rf.Action, rf.LIB = binding.Env, binding.Action, binding._LIB
    rf.equal(str(library), str(Path(rf.LIB._name).resolve()), "loaded_library")
    fixture = ctypes.CDLL(str(args.fixture))
    fixture.sts2_test_crystal_sphere_fixture.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
    fixture.sts2_test_crystal_sphere_fixture.restype = ctypes.c_int32
    start = time.monotonic()
    with (output / "oracle.jsonl").open("w") as stdout, (output / "oracle.stderr").open("w") as stderr:
        completed = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr, timeout=300, check=False)
    if completed.returncode:
        raise RuntimeError(f"Oracle exited {completed.returncode}; see {output / 'oracle.stderr'}")
    rows = [json.loads(line) for line in (output / "oracle.jsonl").read_text().splitlines() if line]
    rf.equal([(s, f"crystal:{o}:{a}") for s in seeds for o in options for a in orders],
             [(r["seed"], r["case_id"]) for r in rows], "oracle.case_matrix")
    results = []
    for index, oracle in enumerate(rows):
        directory = output / f"case-{index:03d}"
        directory.mkdir()
        rf.write_json(directory / "oracle.json", oracle)
        try:
            result = check_case(oracle, directory, fixture)
            print(f"PASS {oracle['seed']} {oracle['case_id']}: {result['native_prefixes']} prefixes", flush=True)
        except Exception as error:
            result = {"seed": oracle["seed"], "case_id": oracle["case_id"], "passed": False,
                      "error": str(error), "traceback": traceback.format_exc()}
            print(f"FAIL {oracle['seed']} {oracle['case_id']}: {error}", flush=True)
        rf.write_json(directory / "result.json", result)
        results.append(result)
    final = freeze()
    rf.write_json(output / "final-provenance.json", final)
    unchanged = all(initial[key] == final[key] for key in ("git_commit", "source_sha256", "runtime", "environment", "system"))
    summary = {"format": "sts2env.rf_crystal_sphere.summary", "version": 1,
        "qualified": unchanged, "passed": sum(r["passed"] for r in results), "cases": len(results),
        "native_prefixes": sum(r.get("native_prefixes", 0) for r in results), "results": results,
        "elapsed_seconds": time.monotonic() - start, "manifest_sha256": rf.digest(output / "manifest.json"),
        "oracle_sha256": rf.digest(output / "oracle.jsonl"), "oracle_test_mode": False,
        "qualification": "TestMode=false shipped DLL with UI input/presentation adapters and injected event root; not live capture"}
    rf.write_json(output / "summary.json", summary)
    print(f"{summary['passed']}/{len(results)} passed; {summary['native_prefixes']} prefixes; source/runtime unchanged={unchanged}")
    return 0 if unchanged and all(r["passed"] for r in results) else 1

if __name__ == "__main__":
    raise SystemExit(main())
