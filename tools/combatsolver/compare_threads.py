#!/usr/bin/env python3
"""Serial process measurements of one search at several thread counts.

Inputs are fixed argument manifests. GNU time records whole-process CPU/RSS;
the solver records search latency and verifies every selected route by replay.
Quality changes are reported per case, never hidden by a speedup average.
"""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess


def standard_cases():
    inputs = [("IRONCLAD", "CULTISTS_NORMAL", "CSPR0BE1")]
    inputs += [(c, e, s) for e in ("CULTISTS_NORMAL", "FUZZY_WURM_CRAWLER_WEAK")
               for c in ("IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT")
               for s in ("BMS0001", "BMS0002")]
    return [{"name": f"{c}-{e}-{s}", "arguments": ["--character", c, "--encounter", e, "--seed", s,
             "--beam", "30", "--nodes", "10000", "--budget-ms", "10000"]} for c, e, s in inputs]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cases", type=Path, help="JSON array of {name, arguments}, e.g. snapshot probes")
    parser.add_argument("--baseline", type=Path, help="optional frozen single-thread native consumer")
    parser.add_argument("--thread-counts", default="1,2,4,8")
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--summarize-existing", action="store_true", help="rebuild only the summary from saved measurements")
    args = parser.parse_args()
    counts = list(dict.fromkeys(int(v) for v in args.thread_counts.split(",")))
    if 1 not in counts or min(counts) < 1 or args.repeats < 1:
        parser.error("include thread count 1 and use positive counts/repeats")
    cases = json.loads(args.cases.read_text()) if args.cases else standard_cases()
    args.out.mkdir(parents=True, exist_ok=True)
    native = args.native.resolve()
    baseline_binary = args.baseline.resolve() if args.baseline else None
    labels = (["baseline"] if baseline_binary else []) + counts
    rows = []
    report = {"schema": "sts2.combatsolver.thread-comparison.v1", "native_sha256": hashlib.sha256(native.read_bytes()).hexdigest(),
              "scope": "same per-request node/time caps; serial processes; complete native root hashes and replay",
              "thread_counts": counts, "repeats": args.repeats, "cases": rows}
    if baseline_binary:
        report["baseline_sha256"] = hashlib.sha256(baseline_binary.read_bytes()).hexdigest()
    if args.summarize_existing:
        report = json.loads((args.out / "report.json").read_text())
        if report["native_sha256"] != hashlib.sha256(native.read_bytes()).hexdigest():
            parser.error("saved results belong to a different native binary")
        rows = report["cases"]
        counts = report["thread_counts"]
        labels = (["baseline"] if report.get("baseline_sha256") else []) + counts
        cases = []
    for index, case in enumerate(cases):
        common = list(case["arguments"])
        if case.get("setup_arguments"):
            snapshot = args.out / f"root-{index:03d}.json"
            with (args.out / f"root-{index:03d}.log").open("w") as log:
                subprocess.run([str(native), *case["setup_arguments"], "--export-only", "1", "--export-root-format", "portable", "--export-root", str(snapshot)],
                               stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
            common += ["--snapshot", str(snapshot)]
        for repeat in range(args.repeats):
            directory = args.out / f"case-{index:03d}-repeat-{repeat}"
            directory.mkdir(exist_ok=True)
            row = {"name": case["name"], "arguments": common, "setup_arguments": case.get("setup_arguments"),
                   "expected_error": case.get("expected_error"), "repeat": repeat, "runs": {}}
            order = labels if (index + repeat) % 2 == 0 else list(reversed(labels))
            try:
                for threads in order:
                    result = directory / f"threads-{threads}.json"
                    usage = directory / f"threads-{threads}-time.json"
                    with (directory / f"threads-{threads}.log").open("w") as log:
                        binary = baseline_binary if threads == "baseline" else native
                        thread_args = [] if threads == "baseline" else ["--threads", str(threads)]
                        process = subprocess.run(["/usr/bin/time", "-f", '{"wall_seconds":%e,"user_seconds":%U,"system_seconds":%S,"max_rss_kib":%M}',
                                        "-o", str(usage), str(binary), *common, *thread_args, "--out", str(result)],
                                       stdout=log, stderr=subprocess.STDOUT, timeout=300)
                    if case.get("expected_error"):
                        message = (directory / f"threads-{threads}.log").read_text()
                        if process.returncode == 0 or case["expected_error"] not in message:
                            raise ValueError("expected budget rejection was not observed")
                        row["runs"][str(threads)] = {"expected_error": case["expected_error"], "exit_code": process.returncode}
                        continue
                    if process.returncode:
                        raise subprocess.CalledProcessError(process.returncode, process.args)
                    data = json.loads(result.read_text())
                    row["runs"][str(threads)] = {k: data[k] for k in ("root_hash", "final_search_hash", "quality", "metrics", "actions")}
                    row["runs"][str(threads)]["usage"] = json.loads(usage.read_text())
                if case.get("expected_error"):
                    row["status"] = "expected_error"
                else:
                    compare_runs(row)
            except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
                row.update(status="error", error=str(error))
            rows.append(row)
            (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(json.dumps({k: row[k] for k in ("name", "repeat", "status", "error") if k in row}), flush=True)
    good = [r for r in rows if r["status"] == "compared"]
    expected = sum(r["status"] == "expected_error" for r in rows)
    report["expected_rejections"] = expected
    summary = {}
    for threads in labels:
        runs = [r["runs"][str(threads)] for r in good]
        cpu = sum(r["usage"]["user_seconds"] + r["usage"]["system_seconds"] for r in runs)
        wall = sum(r["usage"]["wall_seconds"] for r in runs)
        summary[str(threads)] = {"compared": len(runs), "wins": sum(r["quality"]["won"] for r in runs),
            "lost_wins": sum(r["lost_win"] for r in runs), "same_qualities": sum(r["same_quality"] for r in runs),
            "lost_baseline_wins": sum(row["runs"]["baseline"]["quality"]["won"] and not row["runs"][str(threads)]["quality"]["won"]
                                       for row in good) if "baseline" in labels else 0,
            "same_routes": sum(r["same_route"] for r in runs), "battle_hp_lost": sum(r["quality"]["battle_hp_lost"] for r in runs),
            "expanded": sum(r["metrics"]["expanded"] for r in runs), "native_predictions": sum(r["metrics"]["native_predictions"] for r in runs),
            "search_seconds": sum(r["metrics"]["elapsed_ms"] for r in runs) / 1000,
            "cpu_seconds": cpu, "process_wall_seconds": wall, "mean_cpu_cores": cpu / wall if wall else None,
            "median_max_rss_mib": statistics.median(r["usage"]["max_rss_kib"] / 1024 for r in runs) if runs else None,
            "max_rss_mib": max((r["usage"]["max_rss_kib"] / 1024 for r in runs), default=0),
            "max_concurrent_passes": max((r["metrics"].get("peak_concurrent_passes", 1) for r in runs), default=0)}
    report["summary"] = summary
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(summary), flush=True)
    return 0 if len(good) + expected == len(rows) and all(summary[str(t)]["lost_wins"] == 0 and summary[str(t)]["lost_baseline_wins"] == 0
                                                          for t in counts) else 1


def compare_runs(row):
    baseline = row["runs"]["1"]
    for run in row["runs"].values():
        run["same_root"] = run["root_hash"] == baseline["root_hash"]
        run["same_quality"] = run["quality"] == baseline["quality"]
        run["same_route"] = run["actions"] == baseline["actions"]
        run["hp_loss_delta"] = run["quality"]["battle_hp_lost"] - baseline["quality"]["battle_hp_lost"]
        run["lost_win"] = baseline["quality"]["won"] and not run["quality"]["won"]
    row["status"] = "compared" if all(r["same_root"] for r in row["runs"].values()) else "root_mismatch"

if __name__ == "__main__":
    raise SystemExit(main())
