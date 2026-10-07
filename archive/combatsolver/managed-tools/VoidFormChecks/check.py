#!/usr/bin/env python3
"""Compare VoidForm gameplay hooks with the shipped, TestMode=false assembly.

Run after `cmake --build build`. This is an explicit hook fixture, not installed
game acceptance or proof of the complete CombatSolver backend.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(command, cwd):
    completed = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if completed.returncode:
        raise RuntimeError(f"command failed ({completed.returncode}): {command}\n{completed.stdout}\n{completed.stderr}")
    return completed.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = root / args.build_dir
    dotnet = os.environ.get("DOTNET", str(Path.home() / ".dotnet/dotnet"))
    run([dotnet, "build", "tools/VoidFormChecks", "-c", "Release", "--nologo", "-v:minimal"], root)
    native_raw = run([dotnet, "tools/VoidFormChecks/bin/Release/net9.0/VoidFormChecks.dll"], root)
    native_documents = [json.loads(line) for line in native_raw.splitlines() if line.startswith('{"rows":')]
    if len(native_documents) != 1:
        raise RuntimeError("native hook fixture did not emit exactly one document")
    native = native_documents[0]
    with tempfile.TemporaryDirectory(prefix="sts2-voidform-") as temp:
        executable = Path(temp) / "hooks"
        libraries = [build / f"lib{name}.a" for name in ("sts2env", "sts2game", "sts2engine", "sts2content", "sts2core")]
        run([os.environ.get("CXX", "c++"), "-std=c++20", "-O2", "-ffp-contract=off", "-fno-fast-math", "-I", str(root),
             str(root / "tools/VoidFormChecks/hooks.cpp"), "-o", str(executable), *map(str, libraries)], root)
        actual = json.loads(run([str(executable)], root))
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / "native.json").write_text(json.dumps(native, indent=2) + "\n")
        (args.out / "env.json").write_text(json.dumps(actual, indent=2) + "\n")
    if native != actual:
        for expected, observed in zip(native["rows"], actual["rows"]):
            if expected != observed:
                raise RuntimeError(f"VoidForm mismatch at {expected['name']}: native={expected}; env={observed}")
        raise RuntimeError(f"VoidForm document mismatch: native={native}; env={actual}")
    print(f"VoidForm hooks: {len(native['rows'])} native rows matched (decimal bits, energy/stars, owner/piles, manual last-play counter, amount/reset); live counter freeze passed; TestMode=false; full_root=false")


if __name__ == "__main__":
    main()
