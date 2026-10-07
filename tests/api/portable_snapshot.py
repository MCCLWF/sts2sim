#!/usr/bin/env python3
"""Cross-process public-ABI restore, different registries, exact future states."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def worker(directory, reverse):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python"))
    from sts2sim_env import Env, Action, _LIB
    encounters = ["CULTISTS_NORMAL", "SLUMBERING_BEETLE_NORMAL", "NIBBITS_NORMAL"]
    if reverse:
        encounters.reverse()
    for encounter in encounters:
        warmup = Env()
        warmup.inspect_combat_named("PORTABLEWARMUP", encounter)
        warmup.close() if hasattr(warmup, "close") else warmup.__del__()
    cases = states = 0
    for path in sorted(Path(directory).glob("*.json")):
        data = json.loads(path.read_text())
        try:
            Env.restore_snapshot(data["local"])
        except RuntimeError as error:
            assert "runtime_id" in str(error), (path, error)
        else:
            raise AssertionError(f"foreign local v4 accepted: {path}")
        restored = Env.restore_snapshot(data["portable"])
        assert restored.export_portable_snapshot() == data["portable"], f"source differs: {path}"
        for continuation in data["continuations"]:
            restored.step(Action(**continuation["action"]))
            actual = restored.export_portable_snapshot()
            assert actual == continuation["expected"], f"future differs: {path}, state {states}"
            states += 1
        restored.__del__()
        cases += 1
    diagnostic = Env()
    diagnostic.inspect_combat_named("PORTABLEINVALID", "CULTISTS_NORMAL")
    original = diagnostic.export_snapshot()
    portable = diagnostic.export_portable_snapshot()
    rejected = 0
    for path in sorted((Path(directory) / "invalid").glob("*.json")):
        payload = path.read_bytes()
        successor = ctypes.c_void_p(123)
        status = _LIB.sts2_env_import_snapshot(diagnostic._handle, payload, len(payload), ctypes.byref(successor))
        assert status != 0 and not successor.value, path
        assert diagnostic.export_snapshot() == original, path
        rejected += 1
    for field in ("native_build_sha256", "native_runtime_abi", "wire_hash"):
        invalid = json.loads(json.dumps(portable))
        invalid[field] = invalid[field] ^ 1 if field == "wire_hash" else "incompatible"
        payload = json.dumps(invalid, separators=(",", ":")).encode()
        successor = ctypes.c_void_p(123)
        status = _LIB.sts2_env_import_snapshot(diagnostic._handle, payload, len(payload), ctypes.byref(successor))
        assert status != 0 and not successor.value
        assert diagnostic.export_snapshot() == original
    diagnostic.__del__()
    print(json.dumps({"cases": cases, "future_states": states, "invalid_cases": rejected + 3, "reverse_warmup": reverse}))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture")
    parser.add_argument("--library", required=True)
    parser.add_argument("--output-dir")
    parser.add_argument("--matrix", action="store_true")
    parser.add_argument("--worker")
    parser.add_argument("--reverse", action="store_true")
    args = parser.parse_args()
    os.environ["STS2SIM_ENV_LIB"] = args.library
    if args.worker:
        worker(args.worker, args.reverse)
        return
    with tempfile.TemporaryDirectory(prefix="sts2-portable-") as temporary:
        directory = args.output_dir or temporary
        command = [args.fixture, directory]
        if args.matrix:
            command.append("--matrix")
        subprocess.run(command, check=True)
        for reverse in (False, True):
            command = [sys.executable, __file__, "--library", args.library, "--worker", directory]
            if reverse:
                command.append("--reverse")
            subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
