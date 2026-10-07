#!/usr/bin/env python3
"""Invoke the pinned original CombatSolver as an offline teacher/diagnostic.

Build instructions and dependency boundaries are in tools/combatsolver/README.md.
Explicit --upstream overrides allow comparisons with historical versions.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads(Path(__file__).with_suffix(".json").read_text())


def resolve_upstream(override=None):
    source = Path(override).resolve() if override else ROOT / LOCK["checkout"]
    if not source.is_dir():
        raise ValueError(f"missing CombatSolver checkout: {source}; see tools/combatsolver/README.md")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if override is None and commit != LOCK["commit"]:
        raise ValueError(f"pinned CombatSolver mismatch: {commit}, expected {LOCK['commit']}")
    return source, commit


def find_harness(source):
    # Upstream 0.50 moved tools and their outputs. Prefer its current layout.
    candidates = [source / ".local/tool-build/OfflineSearchHarness/bin/Release/net9.0/OfflineSearchHarness.dll",
                  source / "tools/search/OfflineSearchHarness/bin/Release/net9.0/OfflineSearchHarness.dll",
                  source / "tools/OfflineSearchHarness/bin/Release/net9.0/OfflineSearchHarness.dll"]
    for path in candidates:
        if path.is_file():
            return path
    raise ValueError(f"build the original OfflineSearchHarness in {source} first")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path)
    parser.add_argument("--dotnet", type=Path, default=Path.home() / ".dotnet/dotnet")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--timeout-s", type=int, default=120)
    parser.add_argument("arguments", nargs=argparse.REMAINDER, help="original harness options after --")
    args = parser.parse_args()
    source, commit = resolve_upstream(args.upstream)
    harness = find_harness(source)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / "invocation.json").exists():
        raise ValueError(f"refusing to overwrite existing teacher evidence: {out}")
    extra = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    if any(option == "--out" or option.startswith("--out=") for option in extra):
        raise ValueError("pass --out to this wrapper, not again to the original harness")
    command = [str(args.dotnet), str(harness), "--out", str(out), *extra]
    manifest = {"schema": "sts2.combatsolver.offline-teacher.v1", "upstream_commit": commit,
                "command": command, "timeout_seconds": args.timeout_s}
    (out / "invocation.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with (out / "harness.log").open("w") as log:
        try:
            result = subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=args.timeout_s, check=False)
            manifest["returncode"] = result.returncode
        except subprocess.TimeoutExpired:
            manifest["status"] = "timeout"
            (out / "invocation.json").write_text(json.dumps(manifest, indent=2) + "\n")
            raise
    manifest["status"] = "completed" if result.returncode == 0 else "failed"
    (out / "invocation.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"status": manifest["status"], "upstream_commit": commit, "out": str(out)}))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
