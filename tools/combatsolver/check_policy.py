#!/usr/bin/env python3
"""Compare native primitives with compiled, unchanged upstream C# source.

No simulator expectations are generated here. Game-object mapping, root
history, threat, retention and whole-search quality require separate checks.
"""
import argparse
from collections import Counter
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = ROOT / "external/CombatSolver"
PIN = "d7b46203593beb51f792628a2587b04f0850d1c2"


def declaration(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        if source[end] == "{": depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0: return source[start:end + 1]
    raise ValueError("unclosed upstream data declaration: " + marker)


def contracts(target):
    settings = (UPSTREAM / "src/Runtime/SolverSettings.cs").read_text()
    progress = (UPSTREAM / "src/Runtime/SolverProgress.cs").read_text()
    target.write_text("// Extracted unchanged upstream data declarations; no algorithms.\nnamespace CombatSolver;\n"
                      + declaration(settings, "internal enum SolverPotionPolicy") + "\n"
                      + declaration(settings, "internal enum BossHpStrategy") + "\n"
                      + declaration(progress, "internal sealed record SolverInterimResult(") + "\n")


def cases():
    rng = random.Random(64937)
    rows = []
    def add(op, *fields):
        rows.append(" ".join([op, *map(str, fields)]))
    base = [1, 1, 0, 0, 10, 0, 0, 3, 10, 0, 0, 100, "0", -1, 0]
    alternatives = [[0, 1], [0, 1], [0, 1, 2], [0, 1, 20], [-10, 0, 9, 10, 11],
                    [0, 1, 10], [0, 1, 2], ["n", 0, 2, 3, 4, 2147483647],
                    [0, 9, 10, 11], [0, 9, 14, 18], [0, 1, 2], [0, 99, 100, 101],
                    ["NaN", "Infinity", "-Infinity", "-0", "0", "1", "-1"],
                    [-1, 0, 1], [-9223372036854775808, -1, 0, 1, 9223372036854775807]]
    # Isolated changes reach each late comparison key, rather than spending
    # the whole corpus on victory/death prefixes. Include both comparison directions.
    for candidate_theft, policy, projection in itertools.product([-1, 0, 1], [-1, 0, 1], range(4)):
        for index, values in enumerate(alternatives):
            for value in values:
                current = base.copy(); current[13] = candidate_theft
                candidate = current.copy(); candidate[index] = value
                add("R", projection, policy, *candidate, *current)
                add("R", projection, policy, *current, *candidate)
    for _ in range(2500):
        current = [rng.choice(values) for values in alternatives]
        candidate = current.copy()
        for _ in range(rng.randrange(1, 5)):
            index = rng.randrange(len(base)); candidate[index] = rng.choice(alternatives[index])
        for projection in range(4): add("R", projection, rng.choice([-1, 0, 1]), *candidate, *current)
        add("I", *candidate, *current)
    for fields in itertools.product([-1, 0, 1, 5], [0, 1], [0, 1], [-1, 0, 1, 100]): add("V", *fields)
    extremes = [-2147483648, -1, 0, 1, 9, 2147483647]
    for fields in itertools.product(extremes, repeat=4): add("D", *fields)
    for relief, act, final, value in itertools.product(range(3), range(2), range(2), extremes):
        add("A", relief, act, final, value, 12, 1, 21, 5)
    for _ in range(1200):
        add("A", rng.randrange(3), rng.randrange(2), rng.randrange(2), *[rng.choice(extremes) for _ in range(5)])
        add("H", *[rng.choice(extremes) for _ in range(5)], rng.randrange(2))
    for hp in [36, 37, 38, 39, 40, 74, 75, 76, 80]:
        for victory in [0, 1]: add("H", 6, 12, 50, hp, 75, victory)
    for _ in range(3000):
        add("P", rng.randrange(3), rng.randrange(5), rng.randrange(3), rng.choice([0, 1, 9, 14, 18, 36]),
            rng.choice([0, 1, 9, 14, 18, 36]), rng.randrange(-30, 120), rng.randrange(-30, 120),
            *[rng.randrange(2) for _ in range(4)], rng.randrange(3), rng.choice([0, 1, 75, 80, 81, 100]),
            rng.randrange(100), rng.randrange(100), rng.randrange(3))
    for value in extremes:
        add("P", 1, value, value, value, value, value, -value if value != -2147483648 else 0,
            1, 1, 1, 1, value, value, value, value, 1)
    add("P", 99, 1, 0, 9, 0, 10, 0, 1, 1, 1, 1, 0, 80, 50, 60, 0)
    for ms, nodes, minimum, reserve in itertools.product([-1, 0, 4, 5, 6, 2147483647],
            [-9223372036854775808, -1, 0, 1, 500, 9223372036854775807], [0, 5], [0, 5, 2147483647]):
        add("B", ms, nodes, minimum, 120000, 5000, reserve)
    # Explicit collapse / expansion / duplicate, followed by partial orders and IEEE edges.
    add("T0", 3, 3, 3, 3, 3, 3)
    for fields in [(2, 3, 3, 3, 3, 2), (0, 0, 0, 0, 0, 10), (3, 3, 3, 3, 3, 3), (1, 0, 0, 0, 0, 11)]: add("T", *fields)
    for _ in range(160):
        first = [rng.randrange(6) for _ in range(5)] + [rng.choice(["NaN", "Infinity", "-Infinity", -1, 0, 1, 10])]
        add("T0", *first)
        for step in range(64):
            label = first if step % 8 == 0 else [rng.randrange(6) for _ in range(5)] + [rng.choice(["NaN", "Infinity", "-Infinity", -1, 0, 1, 10])]
            add("T", *label)
    return rows


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, text=True, **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", type=Path, default=ROOT / "build/combatsolver_policy_probe")
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/combatsolver-policy-oracle")
    args = parser.parse_args()
    if not args.probe.is_file():
        raise SystemExit("build the native probe first: cmake --build build --target combatsolver_policy_probe")
    # The integrated Mod also carries live-capture fixes. Only the source
    # consumed by this policy oracle must remain identical to the pinned
    # upstream revision; unrelated Runtime changes do not change the oracle.
    project = ROOT / "tools/combatsolver/PolicyOracle/PolicyOracle.csproj"
    policy_sources = re.findall(r'<Compile Include="\$\(Upstream\)/([^"]+)"', project.read_text())
    policy_sources += ["src/Runtime/SolverSettings.cs", "src/Runtime/SolverProgress.cs"]
    changed = run(["git", "-C", UPSTREAM, "diff", PIN, "--", *policy_sources], capture_output=True).stdout
    if changed: raise SystemExit("policy oracle source differs from pinned upstream " + PIN)
    run(["python3", ROOT / "tools/combatsolver/gen_weights.py", "--check"])
    args.output.mkdir(parents=True, exist_ok=True)
    contract_path = args.output / "contracts.cs"
    contracts(contract_path)
    dotnet = os.environ.get("DOTNET") or shutil.which("dotnet") or str(Path.home() / ".dotnet/dotnet")
    project = ROOT / "tools/combatsolver/PolicyOracle/PolicyOracle.csproj"
    with (args.output / "build.log").open("w") as log:
        run([dotnet, "build", project, "--configuration", "Release", f"-p:Contracts={contract_path}"], stdout=log, stderr=subprocess.STDOUT)
    rows = cases()
    payload = "\n".join(rows) + "\n"
    (args.output / "cases.txt").write_text(payload)
    oracle = project.parent / "bin/Release/net9.0/PolicyOracle.dll"
    expected = run([dotnet, oracle], input=payload, capture_output=True).stdout
    actual = run([args.probe.resolve()], input=payload, capture_output=True).stdout
    (args.output / "expected.txt").write_text(expected)
    (args.output / "actual.txt").write_text(actual)
    expected_rows = expected.splitlines(); actual_rows = actual.splitlines()
    if len(expected_rows) != len(rows) or len(actual_rows) != len(rows):
        raise SystemExit(f"row count mismatch: input={len(rows)} C#={len(expected_rows)} C++={len(actual_rows)}")
    mismatches = [{"line":i + 1,"input":row,"expected":exp,"actual":act}
                  for i,(row,exp,act) in enumerate(zip(rows,expected_rows,actual_rows)) if exp != act]
    report = {"upstream_commit":PIN, "cases":len(rows), "operations":dict(Counter(row.split()[0] for row in rows)),
              "mismatches":len(mismatches), "case_sha256":hashlib.sha256(payload.encode()).hexdigest(),
              "scope":"pure policies, ordering, budget window, transposition labels; no Env field mapping or whole-search claim",
              "first_mismatches":mismatches[:12]}
    (args.output / "report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2) + "\n")
    print(json.dumps(report,ensure_ascii=False))
    if mismatches: raise SystemExit(1)


if __name__ == "__main__": main()
