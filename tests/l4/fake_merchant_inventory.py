#!/usr/bin/env python3
"""Compare FakeMerchant.BeginEvent's six-item inventory against the game."""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
ORACLE = os.path.join(ROOT, "tools", "Sts2Oracle", "bin", "Release", "net9.0", "Sts2Oracle.dll")
CPP = os.path.join(ROOT, "build", "test_fake_merchant")
SEEDS = ["FM001", "FM002", "FM003", "FM004", "FM005", "FM006", "FM007", "FM008"]


def main():
    oracle = subprocess.run(
        [DOTNET, ORACLE, "fake-merchant", "--seeds", ",".join(SEEDS)],
        check=True, capture_output=True, text=True,
    )
    original = [json.loads(line) for line in oracle.stdout.splitlines() if line.startswith("{")]
    if len(original) != len(SEEDS):
        raise RuntimeError(f"expected {len(SEEDS)} oracle inventories, got {len(original)}")
    for expected in original:
        seed = expected["seed"]
        local = json.loads(subprocess.run([CPP, "--dump", seed], check=True, capture_output=True, text=True).stdout)
        if local != expected:
            print(f"Fake Merchant inventory divergence on {seed}", file=sys.stderr)
            print(f"game: {expected}", file=sys.stderr)
            print(f"env:  {local}", file=sys.stderr)
            return 1
    print(f"Fake Merchant inventory: {len(SEEDS)}/{len(SEEDS)} original-game seeds identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
