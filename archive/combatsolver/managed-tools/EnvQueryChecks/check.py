#!/usr/bin/env python3
"""Compare query JSON with shipped TestMode=false getters on complete live roots.

Uses the built static query implementation and the public managed API.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(command, root):
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def compare(expected, actual, path="queries"):
    if expected == actual:
        return
    if isinstance(expected, dict) and isinstance(actual, dict) and expected.keys() == actual.keys():
        for key in expected:
            compare(expected[key], actual[key], path + "." + key)
    elif isinstance(expected, list) and isinstance(actual, list) and len(expected) == len(actual):
        for index, (left, right) in enumerate(zip(expected, actual)):
            compare(left, right, path + f"[{index}]")
    raise RuntimeError(f"getter mismatch at {path}: native={expected}; env={actual}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = root / args.build_dir
    dotnet = os.environ.get("DOTNET", str(Path.home() / ".dotnet/dotnet"))
    with tempfile.TemporaryDirectory(prefix="sts2-queries-") as temp:
        output = args.out.resolve() if args.out else Path(temp)
        output.mkdir(parents=True, exist_ok=True)
        run([dotnet, "build", "tools/CombatSolverEnv/CombatSolverEnv.csproj", "-c", "Release", "--nologo", "-v:minimal"], root)
        run([dotnet, "build", "tools/EnvQueryChecks", "-c", "Release", "--nologo", "-v:minimal"], root)
        print(run([dotnet, "tools/EnvQueryChecks/bin/Release/net9.0/EnvQueryChecks.dll", str(output)], root).strip())
        executable = Path(temp) / "queries"
        libraries = [build / f"lib{name}.a" for name in ("sts2env", "sts2game", "sts2engine", "sts2content", "sts2core")]
        run([os.environ.get("CXX", "c++"), "-std=c++20", "-O2", "-ffp-contract=off", "-fno-fast-math", "-I", str(root),
             str(root / "tools/EnvQueryChecks/query.cpp"),
             "-o", str(executable), *map(str, libraries)], root)
        for scenario in ("baseline", "empty_resources", "blocked_hook", "play_pile"):
            expected = json.loads((output / f"{scenario}.native.json").read_text())
            actual = json.loads(run([str(executable), str(output / f"{scenario}.root.json")], root))
            (output / f"{scenario}.env.json").write_text(json.dumps(actual, indent=2) + "\n")
            compare(expected, actual)
            print(f"passed {scenario}: five piles/card identity/keywords/costs/playability/targets/resources/raw intents; source/hash/clone purity and Unported refusal passed")
            if scenario != "play_pile":
                native_policy = json.loads((output / f"{scenario}.native-policy.json").read_text())
                env_policy = json.loads((output / f"{scenario}.public-policy-probe.json").read_text())
                (output / f"{scenario}.env-policy-probe.json").write_text(json.dumps(env_policy, indent=2) + "\n")
                assert env_policy["format"] == "combatsolver.env_policy_probe" and env_policy["version"] == 1
                assert env_policy["prediction_hash"] == actual["prediction_hash"]
                assert env_policy["policy_projection_complete"] is False
                compare(native_policy, env_policy["cards"], f"policy.{scenario}")
                print(f"passed {scenario}: Mod computes original CanPlayCard core from generic getter facts; policy still incomplete")
                native_calculated = json.loads((output / f"{scenario}.native-calculated.json").read_text())
                env_calculated = json.loads(run([str(executable), str(output / f"{scenario}.root.json"), "--calculated"], root))
                (output / f"{scenario}.env-calculated.json").write_text(json.dumps(env_calculated, indent=2) + "\n")
                compare(native_calculated, env_calculated, f"calculated.{scenario}")
                print(f"passed {scenario}: calculated card values match game and original prediction; exact decimal bits")
                native_orbs = json.loads((output / f"{scenario}.native-orbs.json").read_text())
                env_orbs = json.loads(run([str(executable), str(output / f"{scenario}.root.json"), "--orbs"], root))
                (output / f"{scenario}.env-orbs.json").write_text(json.dumps(env_orbs, indent=2) + "\n")
                compare(native_orbs, env_orbs, f"orbs.{scenario}")
                print(f"passed {scenario}: original orb key inputs; exact decimal bits and original queue fingerprint")
                native_powers = json.loads((output / f"{scenario}.native-powers.json").read_text())
                env_powers = json.loads(run([str(executable), str(output / f"{scenario}.root.json"), "--powers"], root))
                (output / f"{scenario}.env-powers.json").write_text(json.dumps(env_powers, indent=2) + "\n")
                compare(native_powers, env_powers, f"powers.{scenario}")
                print(f"passed {scenario}: individual original power key inputs and fingerprints; full state key incomplete")
        expected_growth = json.loads((output / "growth-downgrade.native.json").read_text())
        downgraded = json.loads(run([str(executable), str(output / "growth-before.root.json"), "--downgrade"], root))
        actual_growth = []
        for card in expected_growth:
            value = next(slot["value"] for slot in downgraded["state"]["cards"]["slots"]
                         if slot["index"] == card["card_ref"])
            compare(card["value"], value, f"growth_downgrade.card[{card['card_ref']}]")
            actual_growth.append(value)
        (output / "growth-downgrade.env.json").write_text(json.dumps(actual_growth, indent=2) + "\n")
        print("passed Rampage/Thrash downgrade: all native card fields and exact fractional growth matched")
        baseline = json.loads((output / "baseline.native.json").read_text())["piles"][0]["cards"][2]
        policy = json.loads((output / "baseline.native-policy.json").read_text())
        whirlwind = next(row for row in policy if row["pile_index"] == 0 and row["index"] == 2)
        defend = next(row for row in policy if row["pile_index"] == 0 and row["index"] == 1)
        assert (baseline["model_id"]["entry"] == whirlwind["card_id"] == "WHIRLWIND"
                and baseline["energy_cost"]["effective"] == 0 and whirlwind["energy_cost"] == 2
                and defend["card_id"] == "DEFEND_IRONCLAD" and defend["energy_cost"] == 0)
        print("Original CombatSolver policy baseline captured: X cost uses owner energy; early unplayable returns zero costs; gameplay getters are a distinct contract")
    print("Query checks: TestMode=false; three full live roots plus one explicit native silent Play move; policy_projection_complete=false; threat_projection_complete=false; installed_game_acceptance=false")


if __name__ == "__main__":
    main()
