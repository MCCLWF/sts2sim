#!/usr/bin/env python3
"""Fresh-process public-ABI operations of qualified TestMode=false RF captures.

The parent RF consumers compare these exact prefixes and final facts to the
shipped DLL. This tool executes every captured primary operation, including
operations without a Choice, and checks portable continuations with the frozen
native library, after independently warming registries in two opposite orders.
It preserves parent hashes and never treats this replay as a new Oracle capture.
"""
from __future__ import annotations

import argparse
import importlib
import json
import os
from pathlib import Path
import subprocess
import sys

import rf_operations as rf


def inputs(roots, library):
    files = {str(library): rf.digest(library)}
    for path in (Path(__file__).resolve(), Path(rf.__file__).resolve(), rf.ROOT / "python/sts2sim_env.py"):
        files[str(path)] = rf.digest(path)
    for root in roots:
        for path in sorted(root.rglob("*.json")):
            files[str(path)] = rf.digest(path)
        summary = json.loads((root / "summary.json").read_text())
        manifest = json.loads((root / "manifest.json").read_text())
        rf.equal(True, summary["qualified"], "parent.qualified")
        if summary["format"] == "sts2env.rf_operations.summary":
            rf.equal("off", summary["oracle_test_mode"], "parent.test_mode")
        elif summary["format"] == "sts2env.rf_crystal_sphere.summary":
            rf.equal(False, summary["oracle_test_mode"], "parent.test_mode")
        else:
            raise AssertionError("Unknown RF capture summary format")
        rf.equal(summary["cases"], summary["passed"], "parent.all_cases_passed")
        rf.equal(summary["manifest_sha256"], rf.digest(root / "manifest.json"), "parent.manifest_hash")
        rf.equal(summary["oracle_sha256"], rf.digest(root / "oracle.jsonl"), "parent.oracle_hash")
        rf.equal(manifest["provenance"]["runtime"]["env_library"]["sha256"],
                 rf.digest(library), "parent.identical_library")
        files[str(root / "oracle.jsonl")] = rf.digest(root / "oracle.jsonl")
        native_rows = [json.loads(line) for line in (root / "oracle.jsonl").read_text().splitlines()]
        cases = sorted(root.glob("case-*"))
        rf.equal(summary["cases"], len(cases), "parent.case_directories")
        rf.equal(len(native_rows), len(cases), "parent.native_case_count")
        for case, native in zip(cases, native_rows):
            rf.equal(native, json.loads((case / "oracle.json").read_text()), "parent.exact_native_case")
    return files


def restore_operation_state(path):
    data = json.loads(path.read_text())
    restored = rf.Env.restore_snapshot(data["portable"])
    try:
        check_operation_state(restored, data)
    except BaseException:
        restored.__del__()
        raise
    return restored, data


def check_operation_state(env, data):
    rf.equal("sts2env.rf_operation_state", data["format"], "operation_state.format")
    rf.equal(1, data["version"], "operation_state.version")
    rf.equal(data["portable"], env.export_portable_snapshot(), "fresh.canonical_state")
    queries = dict(data["queries"])
    rf.equal(data["snapshot"]["state_hash"], queries["state_hash"], "parent.query_hash")
    queries["state_hash"] = env.state_hash
    rf.equal(queries, env.export_run_queries(), "fresh.complete_queries")


def check_trace(captured, preview):
    if rf.RNG_TRACE:
        for field in ("rng_trace_enabled", "rng_trace_visibility", "rng_calls"):
            rf.equal(captured[field], preview[field], f"fresh.{field}")
    if rf.CAUSAL_TRACE:
        for field in ("causal_trace_enabled", "causal_trace_visibility", "frames"):
            rf.equal(captured[field], preview[field], f"fresh.{field}")


def replay_initial(directory, answers, oracle_final, previews=None, rejection=None):
    restored, data = restore_operation_state(directory / "initial.json")
    successor = None
    try:
        captured_operation = data["operation"]
        rf.equal(data["snapshot"]["state_hash"], captured_operation["source_hash"], "parent.bound_operation")
        operation = dict(captured_operation, source_hash=restored.state_hash)
        if rejection is not None:
            rf.equal([], answers, "rejection.answers")
            successor, diagnostic = rf.native_operation(restored, operation, expected_status=rejection["status"])
            rf.equal(None, successor, "fresh.rejected_successor")
            rf.equal(rejection, diagnostic, "fresh.rejection_diagnostic")
            check_operation_state(restored, json.loads((directory / "completed.json").read_text()))
            rf.equal(oracle_final, rf.state(restored), "fresh.rejected_native_state")
            return
        if not previews:
            raise AssertionError("Missing captured initial preview")
        rf.equal(captured_operation, previews[0]["operation"], "parent.initial_request")
        successor, preview = rf.native_operation(restored, operation)
        check_trace(previews[0], preview)
        rf.equal(previews[0]["operation_result"], preview["operation_result"], "fresh.initial_result")
        if answers:
            expected = json.loads((directory / "prefix-000.json").read_text())["portable"]
            rf.equal(expected, successor.export_portable_snapshot(), "fresh.initial_choice_state")
            rf.equal("needs_choice", preview["operation_status"], "fresh.initial_choice_status")
        else:
            check_operation_state(successor, json.loads((directory / "completed.json").read_text()))
            rf.equal(oracle_final, rf.state(successor), "fresh.initial_native_final")
            rf.equal("complete", preview["operation_status"], "fresh.initial_complete_status")
            rf.equal(None, successor.export_snapshot()["pending_operation"], "fresh.initial_complete_pending")
    finally:
        if successor is not None:
            successor.__del__()
        restored.__del__()


def replay_operation(directory, answers, oracle_final, completed_result=None, expected_previews=None):
    prefixes = sorted(directory.glob("prefix-*.json"))
    rf.equal(len(answers), len(prefixes), "prefix.answer_count")
    for index, path in enumerate(prefixes):
        rf.equal(f"prefix-{index:03d}.json", path.name, "prefix.contiguous")
        data = json.loads(path.read_text())
        restored = rf.Env.restore_snapshot(data["portable"])
        successor = None
        try:
            rf.equal(data["portable"], restored.export_portable_snapshot(), "fresh.canonical_source")
            expected_queries = dict(data["queries"])
            # Native source hashes bind the local registry, whereas canonical
            # portable snapshots above compare all state/definitions exactly.
            rf.equal(data["snapshot"]["state_hash"], expected_queries["state_hash"], "parent.query_hash")
            expected_queries["state_hash"] = restored.state_hash
            rf.equal(expected_queries, restored.export_run_queries(), "fresh.queries")
            operation = restored.export_snapshot()["pending_operation"]
            successor, preview = rf.answer(restored, answers[index], operation)
            if expected_previews is None:
                raise AssertionError("Missing captured continuation previews")
            check_trace(expected_previews[index + 1], preview)
            if index + 1 < len(prefixes):
                expected = json.loads(prefixes[index + 1].read_text())["portable"]
                rf.equal(expected, successor.export_portable_snapshot(), "fresh.canonical_successor")
                rf.equal("needs_choice", preview["operation_status"], "fresh.pending_status")
            else:
                rf.equal(oracle_final, rf.state(successor), "fresh.oracle_final")
                rf.equal("complete", preview["operation_status"], "fresh.complete_status")
                rf.equal(None, successor.export_snapshot()["pending_operation"], "fresh.complete_pending")
                if completed_result is not None:
                    rf.equal(completed_result, preview["operation_result"], "fresh.operation_result")
                check_operation_state(successor, json.loads((directory / "completed.json").read_text()))
        finally:
            if successor is not None:
                successor.__del__()
            restored.__del__()
    return len(prefixes)


def worker(roots, library, reverse):
    sys.path.insert(0, str(rf.ROOT / "python"))
    binding = importlib.import_module("sts2sim_env")
    rf.Env, rf.Action, rf.LIB = binding.Env, binding.Action, binding._LIB
    rf.equal(str(library), str(Path(rf.LIB._name).resolve()), "loaded_library")
    encounters = ["CULTISTS_NORMAL", "SLUMBERING_BEETLE_NORMAL", "NIBBITS_NORMAL"]
    for encounter in reversed(encounters) if reverse else encounters:
        warmup = rf.Env()
        try:
            warmup.inspect_combat_named("PORTABLERFWARMUP", encounter)
        finally:
            warmup.__del__()
    case_count = operation_count = prefix_count = initial_count = no_choice_count = rejected_count = 0
    for root in roots:
        manifest = json.loads((root / "manifest.json").read_text())
        rf.RNG_TRACE = manifest["config"].get("rng_trace_enabled", False)
        rf.CAUSAL_TRACE = manifest["config"].get("causal_trace_enabled", False)
        summary = json.loads((root / "summary.json").read_text())
        cases = sorted(root.glob("case-*"))
        rf.equal(summary["cases"], len(cases), "parent.case_directories")
        for case in cases:
            result = json.loads((case / "result.json").read_text())
            rf.equal(True, result["passed"], "parent.case_passed")
            oracle = json.loads((case / "oracle.json").read_text())
            if oracle.get("kind") == "merchant_purchase":
                operations = json.loads((case / "operations.json").read_text())
                rf.equal(len(oracle["steps"]), len(operations), "parent.shop_steps")
                for index, operation in enumerate(operations):
                    directory = case / f"step-{index:03d}"
                    if "rejected_request" in operation:
                        replay_initial(directory, [], oracle["steps"][index]["after"], rejection=operation["rejected_request"])
                        rejected_count += 1
                        continue
                    replay_initial(directory, operation["answers"], oracle["steps"][index]["after"], operation["previews"])
                    initial_count += 1
                    no_choice_count += not operation["answers"]
                    prefixes = replay_operation(directory, operation["answers"],
                        oracle["steps"][index]["after"], operation["previews"][-1]["operation_result"], operation["previews"])
                    prefix_count += prefixes
                    operation_count += bool(operation["answers"])
            else:
                previews = json.loads((case / "previews.json").read_text())
                replay_initial(case, result["answers"], oracle["final"], previews)
                initial_count += 1
                no_choice_count += not result["answers"]
                if result["answers"]:
                    prefix_count += replay_operation(case, result["answers"], oracle["final"],
                                                     previews[-1]["operation_result"], previews)
                    operation_count += 1
            case_count += 1
    expected = sum(json.loads((root / "summary.json").read_text()).get("native_choice_prefixes",
        json.loads((root / "summary.json").read_text()).get("native_prefixes", 0)) for root in roots)
    rf.equal(expected, prefix_count, "parent.all_prefixes_replayed")
    return {"cases_inspected": case_count, "operations_with_choices": operation_count,
            "initial_operations_executed": initial_count, "no_choice_operations_executed": no_choice_count,
            "rejected_requests_checked": rejected_count, "prefixes_restored": prefix_count,
            "successors_checked": prefix_count + initial_count,
            "reverse_warmup": reverse}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, action="append", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--reverse", action="store_true")
    args = parser.parse_args()
    roots = [path.resolve(strict=True) for path in args.input]
    library = args.library.resolve(strict=True)
    os.environ["STS2SIM_ENV_LIB"] = str(library)
    if args.worker:
        print(json.dumps(worker(roots, library, args.reverse)))
        return
    if not args.output_dir or args.output_dir.exists():
        parser.error("Choose a new --output-dir; raw evidence is preserved")
    initial = inputs(roots, library)
    args.output_dir.mkdir(parents=True)
    rf.write_json(args.output_dir / "manifest.json", {"inputs_sha256": initial,
        "qualification": "Fresh-process replay of frozen TestMode=false RF captures; injected fixture scope, not live capture"})
    results = []
    for reverse in (False, True):
        command = [sys.executable, __file__, "--library", str(library), "--worker"]
        for root in roots:
            command += ["--input", str(root)]
        if reverse:
            command.append("--reverse")
        name = "reverse" if reverse else "forward"
        with (args.output_dir / f"{name}.stdout").open("w") as stdout, (args.output_dir / f"{name}.stderr").open("w") as stderr:
            process = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=600, check=False)
        rf.write_json(args.output_dir / f"{name}.process.json", {"command": command, "exit_code": process.returncode})
        if process.returncode:
            raise RuntimeError(f"{name} replay exited {process.returncode}; see saved stderr")
        result = json.loads((args.output_dir / f"{name}.stdout").read_text())
        results.append(result)
        print(json.dumps(result), flush=True)
    rf.equal(initial, inputs(roots, library), "frozen_inputs_unchanged")
    rf.write_json(args.output_dir / "summary.json", {"qualified": True, "results": results})


if __name__ == "__main__":
    main()
