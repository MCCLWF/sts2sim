#!/usr/bin/env python3
"""Directed original-DLL L3 matrix for Expertise, HiddenDaggers and LeadingStrike."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import json

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests/l3"))
from l3_combat import parse_runs  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-dir", type=Path)
    args = parser.parse_args()
    trace_dir = args.trace_dir or Path(tempfile.mkdtemp(prefix="mcc-silent-shivs-"))
    trace_dir.mkdir(parents=True, exist_ok=True)
    oracle = ROOT / "tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll"
    dotnet = os.environ.get("DOTNET", str(Path.home() / ".dotnet/dotnet"))
    total_combats = total_states = total_plays = 0
    for card in ("EXPERTISE", "HIDDEN_DAGGERS", "LEADING_STRIKE"):
        for upgrade in (0, 1):
            spec = card + ("+1" if upgrade else "")
            deck = ",".join([spec, spec] + ["STRIKE_SILENT"] * 4
                            + ["DEFEND_SILENT"] * 4 + ["NEUTRALIZE"] * 2)
            for asc in (0, 10):
                trace = trace_dir / f"{card.lower()}-u{upgrade}-a{asc}.jsonl"
                original = subprocess.run(
                    [dotnet, str(oracle), "combat", "--seeds", "SHIVCHECK01,SHIVCHECK02",
                     "--characters", "SILENT", "--policy-seeds", "1,2", "--ascension", str(asc),
                     "--encounter", "CULTISTS_NORMAL", "--deck", deck, "--max-actions", "300"],
                    check=True, capture_output=True, text=True)
                if "Exception:" in original.stderr:
                    raise RuntimeError(original.stderr[-3000:])
                trace.write_text(original.stdout)
                runs = parse_runs(original.stdout.splitlines())
                if len(runs) != 4 or any(run["end"] is None for run in runs):
                    raise RuntimeError("missing original combat/end")
                # Nested card-selection actions do not emit a new combat state.
                # Read the actual hand at the preceding original state event.
                hand = None
                plays = 0
                for line in original.stdout.splitlines():
                    if not line.startswith("{"):
                        continue
                    event = json.loads(line)
                    if event["event"] == "state":
                        hand = event["state"]["player"]["hand"]
                    elif event["event"] == "action" and event.get("type") == "play":
                        plays += hand[event["hand_index"]] == spec
                if plays == 0:
                    raise RuntimeError(f"{spec} A{asc}: original never played target")
                comparison = subprocess.run(
                    [sys.executable, str(ROOT / "tests/l3/l3_combat.py"), "--trace", str(trace),
                     "--ascension", str(asc), "--deck", deck, "--verbose"],
                    check=True, capture_output=True, text=True)
                if "4/4 combats identical, 0 unported, 0 diverged" not in comparison.stdout:
                    raise RuntimeError(comparison.stdout)
                print(f"{spec} A{asc}: {plays} original plays", flush=True)
                print(comparison.stdout, end="", flush=True)
                total_combats += len(runs)
                total_states += sum(len(run["states"]) for run in runs)
                total_plays += plays
    print(f"Directed card batch: {total_combats} combats, {total_states} states, "
          f"{total_plays} original target plays; 0 divergence/Unported")
    print(f"Original traces: {trace_dir}")


if __name__ == "__main__":
    main()
