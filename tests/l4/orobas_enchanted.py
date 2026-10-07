#!/usr/bin/env python3
"""Compare upgraded, enchanted Archaic Tooth preview/pickup against the game."""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = os.path.join(ROOT, "tools", "Sts2Oracle", "bin", "Release", "net9.0", "Sts2Oracle.dll")
CPP = os.path.join(ROOT, "build", "test_orobas_enchanted")
SEEDS = ["OROBASENCHANT", "OROBASENCHANT2"]


def main():
    oracle = subprocess.run([DOTNET, ORACLE, "orobas-enchanted", "--seeds", ",".join(SEEDS)],
                            check=True, capture_output=True, text=True)
    original = [json.loads(line) for line in oracle.stdout.splitlines() if line.startswith("{")]
    if len(original) != len(SEEDS):
        raise RuntimeError(f"expected {len(SEEDS)} oracle outputs, got {len(original)}")
    for expected in original:
        local = json.loads(subprocess.run([CPP, "--dump", expected["seed"]], check=True,
                                          capture_output=True, text=True).stdout)
        if local != expected:
            print(f"Archaic Tooth divergence on {expected['seed']}", file=sys.stderr)
            print(f"game: {expected}", file=sys.stderr)
            print(f"env:  {local}", file=sys.stderr)
            return 1
    print(f"Archaic Tooth enchanted starter: {len(SEEDS)}/{len(SEEDS)} game probes identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
