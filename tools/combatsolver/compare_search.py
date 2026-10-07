#!/usr/bin/env python3
"""Run the unmodified upstream offline harness and the Env consumer.

The checked root fields are recorded explicitly; this is outcome evidence on
starting-deck fixtures, not a proof of complete state or search equivalence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from upstream import find_harness, resolve_upstream


def read(path):
    return json.loads(path.read_text())


def check_inputs(harness, character, encounter, seed, beam, nodes, budget_ms):
    scenario, budget = harness["scenario"], harness["budget"]
    expected_scenario = {"CharacterId": character, "EncounterId": encounter, "Seed": seed,
                         "Ascension": 0, "ActIndexForTest": 0}
    expected_budget = {"BeamWidth": beam, "MaxExpandedNodes": nodes, "BudgetMilliseconds": budget_ms,
                       "PotionPolicy": "Smart", "SearchMode": "Coordinator"}
    for actual, expected in ((scenario, expected_scenario), (budget, expected_budget)):
        for key, value in expected.items():
            if actual.get(key) != value:
                raise ValueError(f"upstream input mismatch: {key}={actual.get(key)!r}, expected {value!r}; rerun without --reuse-upstream")


def check_root(native, snapshot, upstream):
    stamp = upstream["rootContinuationStamp"]
    root = native["root"]
    checks = {}
    for key, value in (("hp", root["player"]["hp"]), ("max_hp", root["player"]["max_hp"]),
                       ("block", root["player"]["block"]), ("energy", root["player"]["energy"]),
                       ("stars", root["player"]["stars"]), ("turn", root["turn"])):
        match = re.search(r"(?:^|;)" + key + r"=(-?\d+)(?:;|$)", stamp)
        checks[key] = match is not None and int(match[1]) == value
    for letter, pile in (("H", "hand"), ("D", "draw"), ("C", "discard"), ("X", "exhaust")):
        section = re.search(r";" + letter + r"=(.*?)(?=;|$)", stamp)[1]
        cards = [name + ("+" + level if level != "0" else "")
                 for name, level in re.findall(r"([A-Z][A-Z_0-9]*)\+(\d+)/", section)]
        checks[pile] = cards == root["player"][pile]
    for i, enemy in enumerate(root["enemies"]):
        section = re.search(r";E" + str(i) + r"=(.*?)(?=;|$)", stamp)
        if not section:
            checks[f"enemy_{i}"] = False
            continue
        fields = section[1].split("/")
        checks[f"enemy_{i}"] = (fields[1] == enemy["id"] and int(fields[3]) == enemy["hp"]
                                  and int(fields[4]) == enemy["max_hp"] and int(fields[5]) == enemy["block"]
                                  and fields[6] == enemy["next_move"])
    checks["enemy_count"] = len(re.findall(r";E\d+=", stamp)) == len(root["enemies"])
    names = ["shuffle", "combat_card_generation", "combat_potion_generation", "combat_card_selection",
             "combat_energy_costs", "combat_targets", "combat_orbs", "monster_ai", "niche"]
    streams = dict(zip(snapshot["run_rng_streams"], snapshot["state"]["run"]["rng"]["rngs"]))
    source_streams = stamp.split(";R=", 1)[1].split("/")
    for name, source in zip(names, source_streams):
        rng = streams[name]
        checks["rng_" + name] = [rng[k] for k in ("counter", "s0", "s1", "s2", "s3")] == list(map(int, source.split(":")))
    return checks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream", type=Path, help="override the pinned original CombatSolver")
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--dotnet", type=Path, default=Path.home() / ".dotnet/dotnet")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--beam", type=int, default=30)
    parser.add_argument("--nodes", type=int, default=10000)
    parser.add_argument("--budget-ms", type=int, default=10000)
    parser.add_argument("--reuse-upstream", action="store_true")
    parser.add_argument("--baseline-native", type=Path)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--case", nargs=3, action="append", metavar=("CHARACTER", "ENCOUNTER", "SEED"),
                        help="run only these explicit A0 roots (repeatable); default is the historical 21 roots")
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    native = args.native.resolve()
    baseline = args.baseline_native.resolve() if args.baseline_native else None
    upstream, upstream_commit = resolve_upstream(args.upstream)
    harness = find_harness(upstream)
    if args.reuse_upstream:
        previous = read(args.out / "report.json")
        if previous.get("upstream_commit") != upstream_commit:
            raise ValueError("cached upstream results have a different source commit; use a new output directory")
    if not native.is_file() or not harness.is_file():
        raise SystemExit("build the native executable and the upstream OfflineSearchHarness first")
    rows = []
    manifest = {"schema": "sts2.combatsolver.comparison.v1", "native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
                "upstream_commit": upstream_commit,
                "beam": args.beam, "nodes": args.nodes, "budget_ms": args.budget_ms,
                "native_threads": args.threads,
                "root_check_scope": "HP, block, energy, stars, turn, ordered piles, enemy identities/HP/block/moves, nine combat RNG streams",
                "cases": rows}
    if baseline:
        manifest["baseline_native_sha256"] = hashlib.sha256(baseline.read_bytes()).hexdigest()
    fixtures = [("IRONCLAD", "CULTISTS_NORMAL", "CSPR0BE1")]
    fixtures += [(character, encounter, seed)
                 for encounter in ("CULTISTS_NORMAL", "FUZZY_WURM_CRAWLER_WEAK")
                 for character in ("IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT")
                 for seed in ("BMS0001", "BMS0002")]
    if args.case:
        fixtures = [tuple(case) for case in args.case]
    for character, encounter, seed in fixtures:
        case = args.out / f"{character}-{encounter}-{seed}"
        case.mkdir(exist_ok=True)
        row = {"character": character, "encounter": encounter, "seed": seed}
        try:
            common = ["--character", character, "--encounter", encounter, "--seed", seed,
                      "--beam", str(args.beam), "--nodes", str(args.nodes), "--budget-ms", str(args.budget_ms)]
            if not args.reuse_upstream or not (case / "upstream/result.json").is_file():
                with (case / "upstream.log").open("w") as log:
                    subprocess.run([str(args.dotnet), str(harness), *common, "--search-mode", "Coordinator",
                                    "--out", str(case / "upstream"), "--label", "source"], cwd=upstream,
                                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
            runs = [(native, "native", "root")]
            if baseline:
                runs.append((baseline, "baseline", "baseline-root"))
                # Alternate the order; all searches remain serial.
                if len(rows) % 2: runs.reverse()
            for binary, label, root_label in runs:
                with (case / (label + ".log")).open("w") as log:
                    thread_args = ["--threads", str(args.threads)] if label == "native" else []
                    subprocess.run([str(binary), *common, *thread_args, "--export-root", str(case / (root_label + ".json")),
                                    "--out", str(case / (label + ".json"))], stdout=log, stderr=subprocess.STDOUT,
                                   check=True, timeout=180)
            n = read(case / "native.json")
            u = read(case / "upstream/result.json")
            check_inputs(read(case / "upstream/harness-result.json"), character, encounter, seed,
                         args.beam, args.nodes, args.budget_ms)
            checks = check_root(n, read(case / "root.json"), u)
            row.update(root_checks=checks, root_checked=all(checks.values()), native=n["quality"],
                       upstream=read(case / "upstream/quality.json")["quality"], native_metrics=n["metrics"],
                       upstream_seconds=u["wallSeconds"], upstream_work=u["solverMetrics"])
            row["hp_loss_delta"] = row["native"]["battle_hp_lost"] - row["upstream"]["projectedBattleHpLost"]
            row["status"] = "compared" if row["root_checked"] else "root_mismatch"
            if baseline:
                b = read(case / "baseline.json")
                baseline_checks = check_root(b, read(case / "baseline-root.json"), u)
                row.update(baseline=b["quality"], baseline_metrics=b["metrics"],
                           baseline_root_checked=all(baseline_checks.values()),
                           same_route=n["actions"] == b["actions"], same_quality=n["quality"] == b["quality"],
                           same_root_hash=n["root_hash"] == b["root_hash"],
                           same_final_hash=n["final_search_hash"] == b["final_search_hash"])
                if not row["baseline_root_checked"] or not row["same_root_hash"]:
                    row["status"] = "root_mismatch"
        except (subprocess.SubprocessError, OSError, ValueError, KeyError, TypeError, IndexError, AttributeError) as error:
            row.update(status="error", error=str(error))
        rows.append(row)
        (args.out / "report.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(json.dumps({k: row[k] for k in ("character", "encounter", "seed", "status", "hp_loss_delta", "error") if k in row}), flush=True)
    good = [r for r in rows if r["status"] == "compared"]
    won = [r for r in good if r["native"]["won"] and r["upstream"]["won"]]
    manifest["summary"] = {"requested": len(rows), "compared": len(good),
        "native_wins": sum(r["native"]["won"] for r in good), "upstream_wins": sum(r["upstream"]["won"] for r in good),
        "lost_upstream_wins": sum(r["upstream"]["won"] and not r["native"]["won"] for r in good),
        "both_won": len(won),
        "mean_hp_loss_delta_when_both_won": sum(r["hp_loss_delta"] for r in won) / len(won) if won else None,
        "worst_hp_loss_delta_when_both_won": max((r["hp_loss_delta"] for r in won), default=None),
        "native_search_seconds": sum(r["native_metrics"]["elapsed_ms"] for r in good) / 1000,
        "upstream_search_seconds": sum(r["upstream_seconds"] for r in good)}
    if baseline:
        manifest["summary"].update(
            baseline_search_seconds=sum(r["baseline_metrics"]["elapsed_ms"] for r in good) / 1000,
            same_routes=sum(r["same_route"] for r in good),
            same_qualities=sum(r["same_quality"] for r in good),
            same_final_hashes=sum(r["same_final_hash"] for r in good))
    (args.out / "report.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest["summary"]), flush=True)
    return 0 if len(good) == len(rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
