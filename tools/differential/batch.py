#!/usr/bin/env python3
"""Isolated oracle batches, offline replay, frozen provenance and first-cause triage.

Examples (from the repository root):
  python3 tools/differential/batch.py run --out artifacts/diff/new --count 10 --jobs 4
  python3 tools/differential/batch.py replay --out artifacts/diff/regression --trace-dir /path/to/traces
  python3 tools/differential/batch.py run --out artifacts/diff/new --count 10 --jobs 4 --resume
"""
import argparse
import concurrent.futures
from contextlib import contextmanager
import fcntl
import hashlib
import importlib.util
import itertools
import json
import os
from pathlib import Path
import platform
import re
import secrets
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from differential.common import ProcessFailure, exit_code, run_process, summary, write_json


def module(level):
    path = ROOT / "tests" / level / ("l3_combat.py" if level == "l3" else "l4_run.py")
    spec = importlib.util.spec_from_file_location(level + "_batch_adapter", path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def stable_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def oracle_runtime_paths(dotnet, oracle):
    """Ask the built oracle which external data/localization roots it resolves."""
    command = [str(dotnet), str(oracle), "runtime-provenance"]
    try:
        completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, encoding="utf-8", timeout=30, check=False)
    except (OSError, subprocess.SubprocessError) as exc:
        raise ValueError(f"could not query oracle runtime provenance: {exc}") from exc
    if completed.returncode != 0:
        detail = completed.stderr.strip()[-2000:]
        raise ValueError(f"oracle runtime-provenance exited {completed.returncode}: {detail}")
    try:
        value = json.loads(completed.stdout)
    except (TypeError, ValueError) as exc:
        raise ValueError("oracle runtime-provenance did not return one JSON object") from exc
    if not isinstance(value, dict):
        raise ValueError("oracle runtime-provenance must return a JSON object")
    if type(value.get("schema_version")) is not int or value["schema_version"] != 1:
        raise ValueError("oracle runtime-provenance has an unsupported schema_version")
    if set(value) != {"schema_version", "game_data", "localization"}:
        raise ValueError("oracle runtime-provenance has missing or unknown fields")
    paths = {}
    for key in ("game_data", "localization"):
        raw = value.get(key)
        if not isinstance(raw, str) or not raw:
            raise ValueError(f"oracle runtime-provenance missing {key} path")
        path = Path(raw)
        if not path.is_absolute():
            raise ValueError(f"oracle runtime-provenance {key} path is not absolute")
        try:
            path = path.resolve(strict=True)
        except OSError as exc:
            raise ValueError(f"oracle runtime-provenance {key} path is unavailable: {raw}") from exc
        if not path.is_dir():
            raise ValueError(f"oracle runtime-provenance {key} path is not a directory: {path}")
        paths[key] = path
    return paths


def oracle_external_runtime(paths):
    game_data = paths["game_data"]
    localization = paths["localization"]
    required_assemblies = ("sts2.dll", "GodotSharp.dll", "0Harmony.dll")
    for name in required_assemblies:
        required = game_data / name
        if not required.is_file():
            raise ValueError(f"oracle runtime dependency is missing required file: {required}")
    # GameData includes native libraries and other runtime inputs alongside
    # managed assemblies. Freeze every top-level file and its relative name so
    # additions/removals and non-DLL runtime changes also invalidate resume.
    game_files = {path.relative_to(game_data).as_posix(): digest(path)
                  for path in sorted(game_data.iterdir()) if path.is_file()}
    if not game_files:
        raise ValueError(f"oracle GameData has no runtime files: {game_data}")
    required_localization = {"cards.json", "powers.json", "relics.json", "potions.json",
                             "events.json", "enchantments.json", "characters.json", "monsters.json"}
    localization_files = {path.name: digest(path) for path in sorted(localization.glob("*.json"))
                          if path.is_file()}
    missing_tables = sorted(required_localization - localization_files.keys())
    if missing_tables:
        raise ValueError("oracle localization is missing required English tables: " +
                         ", ".join(missing_tables))
    return {"game_data_path": str(game_data), "required_assemblies": list(required_assemblies),
            "game_data_files": game_files, "localization_path": str(localization),
            "localization_files": localization_files}


def provenance(cli, oracle, capture, dotnet=None):
    paths = subprocess.check_output(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=ROOT).decode().split("\0")
    roots = {"core", "engine", "game", "env", "content", "data", "tools", "tests", "scripts", "python", "search"}
    extensions = {".h", ".hpp", ".inl", ".inc", ".def", ".cpp", ".cxx", ".cc", ".c", ".cs", ".py", ".sh", ".json", ".csproj", ".props", ".targets", ".txt", ".tsv", ".csv"}
    sources = {p: digest(ROOT / p) for p in sorted(set(paths)) if p and (ROOT / p).is_file()
               and Path(p).suffix in extensions
               and (Path(p).parts[0] in roots or p == "CMakeLists.txt")}
    runtime = {"cli": {"path": str(cli), "sha256": digest(cli)}}
    if capture:
        # The oracle loads its shipped game assemblies dynamically from GameData
        # and reads English tables from the decompiled source tree. Freeze both
        # external inputs in addition to the oracle output directory.
        runtime["oracle"] = {"path": str(oracle), "files": {
            p.name: digest(p) for p in sorted(oracle.parent.iterdir())
            if p.is_file() and p.suffix in (".dll", ".json")}}
        host = Path(shutil.which(dotnet) or dotnet).resolve()
        resolved_oracle = Path(oracle).resolve(strict=True)
        runtime["dotnet"] = {"path": str(host), "sha256": digest(host),
                             "installed_runtimes": subprocess.check_output(
                                 [str(host), "--list-runtimes"], text=True, timeout=10).splitlines()}
        runtime["oracle_external"] = oracle_external_runtime(oracle_runtime_paths(host, resolved_oracle))
    return {"git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
            "source_sha256": stable_hash(sources), "source_files": sources,
            "dirty": subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).splitlines(),
            "runtime": runtime, "system": {"platform": platform.platform(), "machine": platform.machine()},
            "environment": {key: value for key, value in sorted(os.environ.items())
                            if key.startswith(("DOTNET_", "COMPlus_")) or key in ("DOTNET", "STS2_GAME_DIR", "STS2_SRC")},
            "oracle_qualification": "headless TestMode plus scoped production/presentation adapters; not full live certification"}


def compatible_manifest(old, current):
    for key in ("schema_version", "mode", "level", "config", "jobs"):
        if old.get(key) != current.get(key):
            raise ValueError(f"resume rejected: {key} changed; choose a new output directory")
    for key in ("git_commit", "source_sha256", "runtime", "environment", "system"):
        if old["provenance"].get(key) != current["provenance"].get(key):
            raise ValueError(f"resume rejected: {key} changed; choose a new output directory")


@contextmanager
def batch_lock(out):
    with (out / ".lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise ValueError("output directory is already used by another batch") from exc
        yield


def split(value):
    items = [p.strip() for p in value.split(",") if p.strip()]
    if not items or len(items) != len(set(items)):
        raise ValueError("lists must be nonempty and contain no duplicates")
    return items


def make_jobs(args, old=None):
    if old and not any(getattr(args, key, None) for key in ("traces", "trace_dir", "seeds", "seeds_file", "cases_file")):
        for job in old["jobs"]:
            if "trace" in job and Path(job["trace"]).is_file() and digest(job["trace"]) != job["trace_sha256"]:
                raise ValueError("resume rejected: input trace changed")
        return old["jobs"]
    if args.mode == "replay":
        paths = [Path(p).resolve() for p in args.traces or []]
        if args.trace_dir:
            directory = Path(args.trace_dir).resolve()
            paths += sorted(directory.rglob("*.oracle.jsonl"))
            paths += sorted(directory.rglob("oracle.stdout.jsonl"))
        paths = sorted(set(paths))
        if not paths:
            raise ValueError("replay requires --traces or a --trace-dir containing oracle JSONL")
        specs = [{"trace": str(p), "trace_sha256": digest(p),
                  "capture_provenance": "legacy/offline input; original oracle build not supplied"} for p in paths]
    elif args.cases_file:
        specs = json.loads(Path(args.cases_file).read_text())
        fields = {"seed", "character", "ascension", "policy_seed"}
        if not isinstance(specs, list) or not specs or any(
                not isinstance(job, dict) or set(job) != fields for job in specs):
            raise ValueError("--cases-file must contain nonempty seed/character/ascension/policy_seed objects")
        for job in specs:
            if (not isinstance(job["seed"], str) or not job["seed"].strip()
                    or job["character"] not in ("IRONCLAD", "SILENT", "DEFECT", "REGENT", "NECROBINDER")
                    or type(job["ascension"]) is not int or job["ascension"] not in range(11)
                    or type(job["policy_seed"]) is not int or not 0 <= job["policy_seed"] < 2147483648):
                raise ValueError("--cases-file contains an invalid identity")
    else:
        if args.seeds:
            seeds = split(args.seeds)
        elif args.seeds_file:
            path = Path(args.seeds_file)
            seeds = json.loads(path.read_text()) if path.suffix == ".json" else path.read_text().splitlines()
            if not isinstance(seeds, list) or not seeds or any(not isinstance(s, str) or not s.strip() for s in seeds):
                raise ValueError("--seeds-file must be a nonempty string list or one seed per line")
            if len(seeds) != len(set(seeds)):
                raise ValueError("duplicate seeds")
        elif old:
            seeds = old["seeds"]
        else:
            seeds = []
            while len(seeds) < args.count:
                seed = "D" + secrets.token_hex(6).upper()
                if seed not in seeds:
                    seeds.append(seed)
        specs = [{"seed": seed, "character": char, "ascension": asc, "policy_seed": policy}
                 for seed, char, asc, policy in itertools.product(seeds, split(args.characters),
                     [int(v) for v in split(args.ascensions)], [int(v) for v in split(args.policy_seeds)])]
        for job in specs:
            if job["ascension"] not in range(11):
                raise ValueError("ascensions must be between 0 and 10")
    jobs = []
    for index, spec in enumerate(specs):
        if args.mode == "run" and getattr(args, "level", "l4") == "l4":
            spec = dict(spec, policy_version=getattr(args, "policy_version", "coverage-v3"))
        jobs.append(dict(spec, index=index, id=stable_hash(spec)[:20]))
    if len({j["id"] for j in jobs}) != len(jobs):
        raise ValueError("duplicate job identities")
    return jobs


def adapter_args(config, job, cli, oracle):
    defaults = dict(seeds=None, characters="IRONCLAD", policy_seeds="1", ascension=0, max_decisions=10000,
                    no_neow=False, hp=0, deck=None, relics=None, relic_matrix=None, all_nonhero_relics=False,
                    force_sacrifice=0, policy_version="coverage-v3", stop="victory", verify_snapshots=True, max_actions=300, encounter=None,
                    potion=None, potion_target=None, probe_cards=None, probe_helper="STRIKE_IRONCLAD",
                    probe_after_card_turn=False, probe_mad_science=False, probe_potions=None,
                    starting_energy=None, starting_stars=None, take_special_rewards=False, sim_trace_output=None,
                    artifacts_dir=None, timeout=180, require_complete=True, require_probe_play=None)
    defaults.update(config)
    defaults.update(cli=str(cli), oracle=str(oracle))
    if "seed" in job:
        defaults.update(seeds=job["seed"], characters=job["character"], ascension=job["ascension"], policy_seeds=str(job["policy_seed"]))
    return argparse.Namespace(**defaults)


def artifacts_hashes(directory):
    return {str(p.relative_to(directory)): digest(p) for p in sorted(directory.rglob("*"))
            if p.is_file() and p.name != "result.json" and not p.name.endswith(".tmp")}


def validate_result(row, job, out):
    if row.get("id") != job["id"] or row.get("index") != job["index"]:
        raise ValueError("recorded result identity differs from manifest")
    directory = out / "jobs" / job["id"]
    if artifacts_hashes(directory) != row.get("artifacts"):
        raise ValueError("resume rejected: recorded artifacts changed for " + job["id"])
    result_file = directory / "result.json"
    if not result_file.is_file() or json.loads(result_file.read_text()) != row:
        raise ValueError("resume rejected: result.json differs from results.jsonl")


def recorded_results(out, jobs):
    path = out / "results.jsonl"
    if not path.exists():
        return {}
    by_id = {job["id"]: job for job in jobs}
    recorded = {}
    for number, line in enumerate(path.read_text().splitlines(), 1):
        try:
            row = json.loads(line)
        except ValueError as exc:
            raise ValueError(f"results.jsonl line {number} is incomplete/corrupt; preserve and repair evidence before resuming") from exc
        if row.get("id") not in by_id or row["id"] in recorded:
            raise ValueError("unknown or duplicated result in results.jsonl")
        validate_result(row, by_id[row["id"]], out)
        recorded[row["id"]] = row
    return recorded


def execute_job(job, config, level, cli, oracle, dotnet, out):
    directory = out / "jobs" / job["id"]
    saved_trace = directory / "oracle.stdout.jsonl"
    if "trace" in job and not Path(job["trace"]).is_file() and saved_trace.is_file():
        saved_input = saved_trace.read_bytes()
    else:
        saved_input = None
    if directory.exists():
        # An interruption can leave partial evidence without a completed row.
        # Archive it, including any completed-but-unappended result, then retry.
        interrupted = out / "interrupted"
        interrupted.mkdir(exist_ok=True)
        directory.rename(interrupted / (job["id"] + "-" + str(time.time_ns())))
    directory.mkdir(parents=True)
    start = time.monotonic()
    row = {"id": job["id"], "index": job["index"], "job": job, "level": level.upper(),
           "status": "tool_error", "matched": 0, "total": 0, "first": None}
    try:
        adapter = module(level)
        adapter.DOTNET, adapter.ORACLE = dotnet, str(oracle)
        opts = adapter_args(config, job, cli, oracle)
        opts.artifacts_dir = str(directory)
        if "trace" in job:
            trace = Path(job["trace"])
            data = saved_input if saved_input is not None else trace.read_bytes()
            if hashlib.sha256(data).hexdigest() != job["trace_sha256"]:
                raise ValueError("input trace changed after manifest creation")
            (directory / "oracle.stdout.jsonl").write_bytes(data)
            lines = data.decode("utf-8-sig").splitlines()
        else:
            lines = adapter.run_oracle(opts)
        runs = adapter.parse_runs(lines)
        if "trace" not in job and level == "l4":
            adapter.require_policy_version(runs, config["policy_version"])
        results = adapter.evaluate_runs(opts, runs)
        # A single input file can hold several runs. Preserve every run result
        # rather than silently labeling the entire file by its last combat.
        row.update(summary(results))
        row["runs"] = results
        row["status"] = next((r["status"] for r in results if r["status"] not in ("identical", "probe_identical")),
                             "probe_identical" if any(r["status"] == "probe_identical" for r in results) else "identical")
        row["first"] = next((r["first"] for r in results if r["first"] is not None), None)
        if len(results) == 1:
            row["identity"] = results[0]["identity"]
            row["endpoint"] = results[0]["endpoint"]
            row["scope"] = results[0]["scope"]
    except ProcessFailure as exc:
        row.update(status=exc.status, first=exc.detail)
    except (ValueError, KeyError, TypeError, UnicodeError) as exc:
        row.update(status="invalid_trace", first={"message": str(exc)})
    except Exception as exc:
        row.update(status="tool_error", first={"message": f"{type(exc).__name__}: {exc}"})
    row["elapsed_seconds"] = time.monotonic() - start
    row["artifacts"] = artifacts_hashes(directory)
    write_json(directory / "result.json", row)
    return row


def triage(rows):
    buckets = {}
    for row in sorted(rows, key=lambda r: r["index"]):
        for run in row.get("runs", [row]):
            if run["status"] in ("identical", "probe_identical"):
                continue
            first = run.get("first") or {}
            path = re.sub(r"\[\d+\]", "[*]", first.get("path", ""))
            message = first.get("message", "") if not path else ""
            message = re.sub(r"0x[0-9a-fA-F]+", "<address>", message)
            signature = stable_hash([run["status"], first.get("kind"), path, message])[:16]
            bucket = buckets.setdefault(signature, {"signature": signature, "status": run["status"],
                "kind": first.get("kind"), "path": path, "message": message, "count": 0,
                "representative": {"job_id": row["id"], "identity": run.get("identity", row["job"]), "first": first}})
            bucket["count"] += 1
    return sorted(buckets.values(), key=lambda b: (-b["count"], b["signature"]))


def write_summary(out, rows, manifest, elapsed):
    runs = [run for row in rows for run in row.get("runs", [row])]
    stats = summary(runs)
    stats.update(schema_version=1, finished_jobs=len(rows), planned_jobs=len(manifest["jobs"]),
                 wall_seconds=elapsed, job_seconds=sum(r["elapsed_seconds"] for r in rows),
                 mode=manifest["mode"], strict=manifest["config"]["require_complete"],
                 manifest_sha256=digest(out / "manifest.json"),
                 verify_snapshots=manifest["config"]["verify_snapshots"],
                 comparison="exported state/choice/RNG counters" +
                     (" and snapshot successor" if manifest["config"]["verify_snapshots"] else "") +
                     "; dual RNG draws/legal sets not yet certified")
    stats["complete_full_runs"] = sum(r["status"] == "identical" and r.get("endpoint", {}).get("reason") in ("death", "victory") for r in runs)
    stats["scopes"] = {scope: summary([r for r in runs if r.get("scope", "unknown") == scope])
                       for scope in sorted({r.get("scope", "unknown") for r in runs})}
    stats["buckets"] = len(triage(rows))
    write_json(out / "summary.json", stats)
    write_json(out / "triage.json", triage(rows))
    return stats


def validate_execution_options(parser, args):
    """Reject requested semantics that the selected adapter never executes."""
    level_only = {
        "l3": ("hp", "no_neow", "stop", "max_decisions", "policy_version", "no_verify_snapshots"),
        "l4": ("encounter", "starting_energy", "starting_stars", "probe_cards",
               "probe_helper", "probe_after_card_turn", "max_actions"),
    }
    unsupported = [key for key in level_only[args.level]
                   if getattr(args, key) is not None and getattr(args, key) is not False]
    if unsupported:
        parser.error(args.level.upper() + " does not support " +
                     ", ".join("--" + key.replace("_", "-") for key in unsupported))
    if args.mode == "replay":
        # L4 reconstructs all setup from begin. L3 additionally supports only
        # the legacy setup fallbacks actually passed to its native replay.
        capture_only = ("seeds", "seeds_file", "count", "characters", "ascensions", "policy_seeds",
                        "hp", "no_neow", "stop", "max_decisions", "max_actions", "encounter",
                        "probe_cards", "probe_helper", "probe_after_card_turn", "policy_version")
        if args.level == "l4":
            capture_only += ("deck", "relics")
        unsupported = [key for key in capture_only
                       if getattr(args, key) is not None and getattr(args, key) is not False]
        if unsupported:
            parser.error("replay uses frozen trace inputs and does not support " +
                         ", ".join("--" + key.replace("_", "-") for key in unsupported))
    if args.cases_file and any(value is not None for value in (args.count, args.characters, args.ascensions, args.policy_seeds)):
        parser.error("--cases-file fixes identities; do not combine it with --count/--characters/--ascensions/--policy-seeds")
    if args.mode == "run" and args.level == "l3" and not args.probe_cards:
        if args.probe_helper is not None or args.probe_after_card_turn:
            parser.error("--probe-helper/--probe-after-card-turn require --probe-cards")
    # Resolve effective defaults only after inspecting explicit requests. A
    # manifest must describe executed configuration, not shared parser defaults.
    args.characters = args.characters if args.characters is not None else "IRONCLAD,SILENT,DEFECT,REGENT,NECROBINDER"
    args.count = args.count if args.count is not None else 10
    args.ascensions = args.ascensions if args.ascensions is not None else "0,10"
    args.policy_seeds = args.policy_seeds if args.policy_seeds is not None else "1"
    args.hp = args.hp if args.hp is not None else 0
    args.max_decisions = (args.max_decisions if args.max_decisions is not None else 10000) if args.level == "l4" else None
    args.max_actions = (args.max_actions if args.max_actions is not None else 300) if args.level == "l3" else None
    args.stop = (args.stop or "victory") if args.level == "l4" else None
    args.probe_helper = (args.probe_helper if args.probe_helper is not None else "STRIKE_IRONCLAD") if args.level == "l3" else None
    args.policy_version = (args.policy_version or "coverage-v3") if args.mode == "run" and args.level == "l4" else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("run", "replay"))
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--level", choices=("l3", "l4"), default="l4")
    parser.add_argument("--seeds")
    parser.add_argument("--seeds-file")
    parser.add_argument("--cases-file", help="JSON array of exact seed/character/ascension/policy_seed identities")
    parser.add_argument("--count", type=int, help="new random seeds before character/ascension expansion (default 10)")
    parser.add_argument("--characters")
    parser.add_argument("--ascensions")
    parser.add_argument("--policy-seeds")
    parser.add_argument("--policy-version", choices=("legacy-v1", "coverage-v2", "coverage-v3"), default=None,
                        help="run L4 oracle strategy (default coverage-v3); recorded in capture identities")
    parser.add_argument("--traces", nargs="+")
    parser.add_argument("--trace-dir")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--max-decisions", type=int, help="L4 oracle limit (default 10000)")
    parser.add_argument("--max-actions", type=int, help="L3 oracle limit (default 300)")
    parser.add_argument("--stop", choices=("before-act1-boss", "after-act1", "after-act2", "after-loadout", "victory"), help="L4 oracle endpoint (default victory)")
    parser.add_argument("--hp", type=int, help="L4 test aid, marked injected in results")
    parser.add_argument("--no-neow", action="store_true")
    parser.add_argument("--deck")
    parser.add_argument("--relics")
    parser.add_argument("--encounter")
    parser.add_argument("--starting-energy", type=int)
    parser.add_argument("--starting-stars", type=int)
    parser.add_argument("--probe-cards")
    parser.add_argument("--probe-helper", help="L3 probe helper (default STRIKE_IRONCLAD)")
    parser.add_argument("--probe-after-card-turn", action="store_true")
    parser.add_argument("--no-verify-snapshots", action="store_true")
    parser.add_argument("--explore", action="store_true", help="Unported may exit 0; never count it as identical")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--cli", type=Path, default=ROOT / "build" / "sts2sim_cli")
    parser.add_argument("--oracle", type=Path, default=ROOT / "tools" / "Sts2Oracle" / "bin" / "Release" / "net9.0" / "Sts2Oracle.dll")
    parser.add_argument("--dotnet", default=os.environ.get("DOTNET", str(Path.home() / ".dotnet" / "dotnet")))
    args = parser.parse_args(argv)
    validate_execution_options(parser, args)
    if min(value for value in (args.jobs, args.count, args.max_decisions, args.max_actions, args.timeout) if value is not None) <= 0:
        parser.error("jobs/count/limits/timeout must be positive")
    if sum(bool(x) for x in (args.seeds, args.seeds_file, args.cases_file)) > 1:
        parser.error("choose --seeds, --seeds-file, or --cases-file")
    if args.mode == "replay" and args.cases_file:
        parser.error("--cases-file requires run mode")
    if args.mode == "run" and (args.traces or args.trace_dir):
        parser.error("trace inputs require replay mode")
    out = args.out.resolve()
    cli, oracle = args.cli.resolve(), args.oracle.resolve()
    out.mkdir(parents=True, exist_ok=True)
    config = {key: getattr(args, key) for key in ("count", "characters", "ascensions", "policy_seeds", "policy_version", "timeout", "max_decisions", "max_actions", "stop",
              "hp", "no_neow", "deck", "relics", "encounter", "starting_energy", "starting_stars", "probe_cards", "probe_helper", "probe_after_card_turn")}
    config.update(require_complete=not args.explore,
                  verify_snapshots=args.level == "l4" and not args.no_verify_snapshots, dotnet=args.dotnet)
    start = time.monotonic()
    with batch_lock(out):
        manifest_path = out / "manifest.json"
        old = json.loads(manifest_path.read_text()) if manifest_path.exists() else None
        if bool(old) != bool(args.resume):
            raise ValueError("existing batches require --resume; new batches must omit it")
        if not old and any(p.name != ".lock" for p in out.iterdir()):
            raise ValueError("new batch directory is not empty")
        jobs = make_jobs(args, old)
        identity = provenance(cli, oracle, args.mode == "run", args.dotnet)
        manifest = {"schema_version": 1, "mode": args.mode, "level": args.level, "config": config,
                    "jobs": jobs, "seeds": list(dict.fromkeys(j["seed"] for j in jobs if "seed" in j)),
                    "provenance": identity}
        if old:
            compatible_manifest(old, manifest)
            manifest = old
        else:
            write_json(manifest_path, manifest)  # Before executing any seed.
        (out / "jobs").mkdir(exist_ok=True)
        completed = recorded_results(out, jobs)
        pending = [j for j in jobs if j["id"] not in completed]
        print(f"{args.level.upper()} {args.mode}: {len(jobs)} planned, {len(completed)} recorded, {len(pending)} pending, {args.jobs} workers", flush=True)
        from differential.common import process_supervision
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool, process_supervision(
                on_cancel=lambda: pool.shutdown(wait=False, cancel_futures=True)):
            futures = {pool.submit(execute_job, job, config, args.level, cli, oracle, args.dotnet, out): job for job in pending}
            with (out / "results.jsonl").open("a", encoding="utf-8") as stream:
                for future in concurrent.futures.as_completed(futures):
                    try:
                        row = future.result()
                    except Exception as exc:
                        job = futures[future]
                        directory = out / "jobs" / job["id"]
                        directory.mkdir(parents=True, exist_ok=True)
                        row = {"id": job["id"], "index": job["index"], "job": job,
                               "level": args.level.upper(), "status": "tool_error", "matched": 0,
                               "total": 0, "elapsed_seconds": 0,
                               "first": {"message": f"worker failed: {type(exc).__name__}: {exc}"},
                               "artifacts": artifacts_hashes(directory)}
                        write_json(directory / "result.json", row)
                    stream.write(json.dumps(row, ensure_ascii=False) + "\n")
                    stream.flush()
                    os.fsync(stream.fileno())
                    completed[row["id"]] = row
                    if row["status"] not in ("identical", "probe_identical") or len(completed) % 10 == 0 or len(completed) == len(jobs):
                        print(f"{len(completed)}/{len(jobs)} {row['status']} job {row['index']} {row.get('identity', row['job'])}", flush=True)
        # Refuse to qualify a batch if source/binaries changed while it ran.
        after = provenance(cli, oracle, args.mode == "run", args.dotnet)
        for key in ("git_commit", "source_sha256", "runtime", "environment", "system"):
            if after.get(key) != identity.get(key):
                raise ValueError("batch source/runtime changed during execution; raw results preserved but not qualified")
        rows = sorted(completed.values(), key=lambda r: r["index"])
        stats = write_summary(out, rows, manifest, time.monotonic() - start)
        print(json.dumps(stats, ensure_ascii=False, sort_keys=True))
        return exit_code([r for row in rows for r in row.get("runs", [row])], config["require_complete"])


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as exc:
        print("batch rejected: " + str(exc), file=sys.stderr)
        sys.exit(2)
