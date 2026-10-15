#!/usr/bin/env python3
"""The two pipelines of R4 (docs/plans/2026-10-08-nosl-r4-alternating-scale-up.md), each as one long-running driver.

  tools/nosl/alternate.py init --dir artifacts/nosl-r4 --combat CHECKPOINT.pt --resume TRAINER.pt --strategy RULE.pt --margin 0.02 --rule-name f2-m0.02
  tools/nosl/alternate.py p --dir artifacts/nosl-r4          # combat PPO, segment after segment
  tools/nosl/alternate.py s --dir artifacts/nosl-r4 --first-generation 3   # strategic generations

The drivers share two small files in DIR and nothing else:
  rule.json    the strategic rule in force: {name, strategy, margin}
  combat.json  the newest combat checkpoint: {iteration, checkpoint (no critic, for acting), trainer}
P reads rule.json when a segment starts and writes combat.json when one ends. S reads both when a generation starts
and when it compares candidate rules, and replaces rule.json when a candidate wins. Both append to DIR/events.jsonl.

One GPU: P's training holds most of it. Every other GPU job except S's collection (value-model training, every
evaluation) takes DIR/gpu-aux.lock first, so at most one of them runs beside P at a time.

A file DIR/STOP makes each driver finish its current step and exit. A step that fails is logged and tried again
(the PPO resumes from its last saved checkpoint; a partial collection is set aside and started over).
"""
from __future__ import annotations

import argparse
import fcntl
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PYTHON = str(ROOT / ".venv/bin/python")
SEGMENT = 130          # PPO iterations between checkpoints
PER_ITERATION = 30720  # 480 runs x 64 steps
MARGINS = ("0", "0.02", "0.05")
ADOPT = 0.01           # a candidate rule replaces the current one when its tuning target is this much higher
MAX_STEPS = 6000       # decisions after which a run is cut (step_limit): a victory takes about 800, an endless loop took 30000
TABLE_DELTA = 0.01     # the table replaces the rule's answer when its best label is this much above the answer's own
TABLE_KINDS = "choice,rest"  # context kinds of the table rule (tools/nosl/card_table.py --kinds): the two that
                             # raised the tuning target when added; map and event tables lowered it (R4 report)


def table_args(rule):
    """A rule may carry a value table over the value network (nosl.strategy_q table_override)."""
    return ["--table", rule["table"], "--table-delta", rule.get("table_delta", TABLE_DELTA)] if rule.get("table") else []


def now():
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


class Driver:
    def __init__(self, directory, x1):
        self.dir = Path(directory).resolve()
        self.x1 = str(Path(x1).resolve())
        self.env = {**os.environ, "PYTORCH_CUDA_ALLOC_CONF": "expandable_segments:True"}
        for sub in ("p", "tune", "diag", "logs"):
            (self.dir / sub).mkdir(parents=True, exist_ok=True)

    # -- shared files
    def read(self, name):
        return json.loads((self.dir / name).read_text())

    def write(self, name, value):
        tmp = self.dir / (name + ".tmp")
        tmp.write_text(json.dumps(value, indent=1))
        os.replace(tmp, self.dir / name)

    def event(self, **row):
        row = {"time": now(), **row}
        with open(self.dir / "events.jsonl", "a", encoding="utf-8") as stream:
            fcntl.flock(stream, fcntl.LOCK_EX)
            stream.write(json.dumps(row) + "\n")
        print(json.dumps(row)[:400], flush=True)

    def stop(self, driver=""):
        """DIR/STOP stops both drivers; DIR/STOP.p or DIR/STOP.s only that one."""
        return (self.dir / "STOP").exists() or bool(driver and (self.dir / f"STOP.{driver}").exists())

    @contextmanager
    def gpu(self):
        with open(self.dir / "gpu-aux.lock", "w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            yield

    # -- processes
    def module(self, args, log, nice=5):
        """python -m nosl.run ARGS from python/, output appended to DIR/logs/LOG. Returns the exit code."""
        with open(self.dir / "logs" / log, "a", encoding="utf-8") as stream:
            stream.write(f"\n== {now()} {' '.join(map(str, args))}\n")
            stream.flush()
            return subprocess.run(["nice", "-n", str(nice), PYTHON, "-B", "-W", "ignore", "-m", "nosl.run", *map(str, args)],
                                  cwd=ROOT / "python", env=self.env, stdout=stream, stderr=subprocess.STDOUT).returncode

    def start(self, args, log, nice=5):
        """The same, not waited for: returns the process."""
        stream = open(self.dir / "logs" / log, "a", encoding="utf-8")
        stream.write(f"\n== {now()} {' '.join(map(str, args))}\n")
        stream.flush()
        return subprocess.Popen(["nice", "-n", str(nice), PYTHON, "-B", "-W", "ignore", "-m", "nosl.run", *map(str, args)],
                                cwd=ROOT / "python", env=self.env, stdout=stream, stderr=subprocess.STDOUT)

    def snippet(self, code, *args):
        return subprocess.check_output([PYTHON, "-B", "-W", "ignore", "-c", code, *map(str, args)], cwd=ROOT / "python", env=self.env).decode().strip()

    def iteration_of(self, checkpoint):
        return int(self.snippet("import sys, torch; print(torch.load(sys.argv[1], map_location='cpu', weights_only=False)['iteration'])", checkpoint))

    def strip(self, checkpoint, out):
        """A copy of a trainer checkpoint without its separate critic: half the forward pass for acting."""
        self.snippet("""
import sys, torch
blob = torch.load(sys.argv[1], map_location='cpu', weights_only=False)
config = {k: v for k, v in blob['config'].items() if k != 'critic'}
state = {k: v for k, v in blob['state'].items() if not k.startswith('value_net.')}
torch.save({'config': config, 'state': state, 'actor_filter': 1, 'iteration': blob.get('iteration'), 'stripped_from': sys.argv[1]}, sys.argv[2])
""", checkpoint, out)

    def evaluate(self, out, combat, rule, log, prefix="NR4TUNE", runs=1024, ascension=10):
        """One greedy whole-run evaluation; returns its summary (None when it failed). Skipped when already done."""
        summary = Path(str(out) + ".summary.json")
        if not summary.exists():
            with self.gpu():
                code = self.module(["strategy_q", "evaluate", "--out", str(out) + ".jsonl", "--combat", combat, "--base", self.x1,
                                    "--strategy", rule["strategy"], "--margin", rule["margin"], *table_args(rule), "--prefix", prefix,
                                    "--runs", runs, "--ascension", ascension, "--workers", 12, "--max-steps", MAX_STEPS], log)
            if code != 0 or not summary.exists():
                return None
        return json.loads(summary.read_text())


KEEP = ("attempts", "ends", "victories", "act2", "act3", "act2_rate", "act3_rate", "floor_mean", "target_mean")


def brief(summary):
    return {k: summary[k] for k in KEEP} if summary else None


def run_p(d: Driver, args):
    failures, last_rule, reporters = 0, None, []
    while not d.stop("p") and failures < 5:
        rule, combat = d.read("rule.json"), d.read("combat.json")
        resume = d.dir / "p/checkpoint.pt"
        if not resume.exists():
            resume = Path(combat["trainer"])
        at = d.iteration_of(resume)
        target = (at // SEGMENT + 1) * SEGMENT
        kept = d.dir / f"p/checkpoint-{target:05d}.pt"
        warm = 5 if last_rule is not None and last_rule != rule["name"] else 0
        code = d.module(["combat_ppo", "--out", d.dir / "p", "--init", d.x1, "--resume", resume, "--base", d.x1,
                         "--strategy", rule["strategy"], "--margin", rule["margin"], *table_args(rule), "--reward", "combat",
                         "--decisions", (target - at) * PER_ITERATION, "--workers", 12, "--runs-per-worker", 40,
                         "--minibatch", 2048, "--token-budget", 64000, "--critic-warmup", warm, "--save-every", 26,
                         "--keep-every", SEGMENT, "--seed-prefix", "NR4P", "--ascensions", "0,5,10",
                         *(["--arena", Path(args.arena).resolve(), "--arena-slots", args.arena_slots, "--arena-bank", args.arena_bank]
                           if args.arena else [])], "p.train.log")
        if code != 0 or not kept.exists():
            failures += 1
            d.event(driver="p", step="segment_failed", exit=code, resume=str(resume), started_at=at, target=target, failures=failures)
            time.sleep(60)
            continue
        failures, last_rule = 0, rule["name"]
        acting = d.dir / f"p/combat-{target:05d}.pt"
        d.strip(kept, acting)
        d.write("combat.json", {"iteration": target, "checkpoint": str(acting), "trainer": str(d.dir / "p/checkpoint.pt")})
        rows = [json.loads(line) for line in open(d.dir / "p/train.jsonl")]
        last = rows[-1]
        victories = sum(r["victories"] for r in rows if r["iteration"] > target - SEGMENT)

        def report(target=target, rule=rule, acting=acting, last=last, victories=victories):
            # The evaluation waits for the auxiliary GPU slot; the next segment does not wait for the evaluation.
            summary = d.evaluate(d.dir / f"tune/p{target:05d}-{rule['name']}", str(acting), rule, "p.eval.log")
            d.event(driver="p", step="segment", iteration=target, decisions=last["decisions"], rule=rule["name"], tune=brief(summary),
                    training_recent=last.get("recent_by_ascension"), training_victories_in_segment=victories)

        reporter = threading.Thread(target=report)
        reporter.start()
        reporters.append(reporter)
    for reporter in reporters:
        reporter.join()
    d.event(driver="p", step="exit", stopped=d.stop("p"), failures=failures)


def free_gib(path):
    usage = shutil.disk_usage(path)
    return usage.free / 2 ** 30


def run_s(d: Driver, args):
    generation = args.start_generation or args.first_generation
    failures = 0
    while not d.stop("s") and failures < 3:
        if free_gib(d.dir) < 3:
            d.event(driver="s", step="disk_low", free_gib=round(free_gib(d.dir), 2))
            break
        rule, combat = d.read("rule.json"), d.read("combat.json")
        data = d.dir / f"g{generation}"
        parts = [data / f"part{i}" for i in range(args.collectors)]
        if not all((part / "summary.json").exists() for part in parts):
            if data.exists():
                data.rename(d.dir / f"g{generation}-partial-{int(time.time())}")
            data.mkdir()
            d.event(driver="s", step="collect_start", generation=generation, combat=combat["iteration"], rule=rule["name"],
                    collectors=args.collectors)
            per = args.runs // args.collectors
            # Several collectors side by side: one collector's Python loop, not the GPU or the cores, is what limits it.
            running = [d.start(["strategy_q", "collect", "--out", part, "--combat", combat["checkpoint"], "--base", d.x1,
                                "--strategy", rule["strategy"], "--margin", rule["margin"], *table_args(rule),
                                "--prefix", f"NR4G{generation}T",
                                "--first", i * per, "--runs", per, "--epsilon", 0.15, "--ascensions", "0,5,10,10",
                                "--workers", args.collector_workers, "--seed", generation * 10 + i, "--max-steps", MAX_STEPS],
                               f"s.g{generation}.collect{i}.log")
                       for i, part in enumerate(parts)]
            codes = [process.wait() for process in running]
            if any(codes) or not all((part / "summary.json").exists() for part in parts):
                failures += 1
                d.event(driver="s", step="collect_failed", generation=generation, exits=codes, failures=failures)
                time.sleep(60)
                continue
            (data / "source.json").write_text(json.dumps({"combat": combat, "rule": rule}, indent=1))
        summaries = [json.loads((part / "summary.json").read_text()) for part in parts]
        collected = {"attempts": sum(x["attempts"] for x in summaries),
                     "ends": {k: sum(x["ends"].get(k, 0) for x in summaries) for k in sorted({k for x in summaries for k in x["ends"]})},
                     "victories": sum(x["victories"] for x in summaries), "act2": sum(x["act2"] for x in summaries),
                     "act3": sum(x["act3"] for x in summaries), "decisions": sum(x["decisions"] for x in summaries),
                     "shard_decisions": sum(x["shard_decisions"] for x in summaries), "wall_s": max(x["wall_s"] for x in summaries),
                     "floor_mean": sum(x["floor_mean"] * x["attempts"] for x in summaries) / sum(x["attempts"] for x in summaries)}
        check = data / "shard-check.json"
        if not check.exists():
            results = []
            for part in parts:
                result = subprocess.run([PYTHON, "-B", "tools/nosl/check_shards.py", "--data", str(part), "--runs", "150", "--workers", "6"],
                                        cwd=ROOT, env=d.env, capture_output=True, text=True)
                if result.returncode != 0:
                    d.event(driver="s", step="shard_check_failed", generation=generation, part=str(part), output=result.stdout[-600:])
                    return
                results.append(json.loads(result.stdout))
            check.write_text(json.dumps(results))
        d.event(driver="s", step="collected", generation=generation, summary=collected, shard_check=json.loads(check.read_text()))
        source = json.loads((data / "source.json").read_text())["rule"]
        effects = d.dir / f"g{generation}-effects.json"
        comparing = None
        if not effects.exists():
            comparing = subprocess.Popen(["nice", "-n", "10", PYTHON, "-B", "tools/nosl/exploration_effects.py", "--data",
                                          ",".join(str(part) for part in parts), "--out", str(effects), "--workers", "6", "--split-ascension"],
                                         cwd=ROOT, env=d.env, stdout=open(d.dir / "logs" / f"s.g{generation}.effects.log", "w"),
                                         stderr=subprocess.STDOUT)
        def directories(first, last):
            return ",".join(str(part) for g in range(first, last + 1) for part in sorted((d.dir / f"g{g}").glob("part*")))
        candidates = {"a": ["--data", directories(args.first_generation, generation), "--init", d.x1],
                      "c": ["--data", directories(generation, generation), "--resume", source["strategy"]]}
        trained = True
        for name, extra in candidates.items():
            out = d.dir / f"f{generation}{name}.pt"
            if out.exists():
                continue
            with d.gpu():
                code = d.module(["strategy_q", "train", "--out", out, *extra, "--epochs", 2, "--batch", 1024, "--lr", "2e-4", "--seed", 1,
                                 "--shard-group", 6, "--batch-by-run"], f"s.g{generation}.train-{name}.log")
            if code != 0 or not out.exists():
                trained = False
                d.event(driver="s", step="train_failed", generation=generation, candidate=name, exit=code)
        if not trained:
            failures += 1
            time.sleep(60)
            continue
        failures = 0
        rule, combat = d.read("rule.json"), d.read("combat.json")   # the newest: P has moved on meanwhile
        tag = f"s{generation}-p{combat['iteration']:05d}"
        current = d.evaluate(d.dir / f"tune/{tag}-cur-{rule['name']}", combat["checkpoint"], rule, f"s.g{generation}.eval.log")
        best = None
        table = {}
        carried = {k: rule[k] for k in ("table", "table_delta") if rule.get(k)}   # the table in force goes with every candidate
        suffix = "+" + Path(rule["table"]).stem if rule.get("table") else ""
        for name in candidates:
            for margin in MARGINS:
                cand = {"name": f"f{generation}{name}-m{margin}{suffix}", "strategy": str(d.dir / f"f{generation}{name}.pt"),
                        "margin": float(margin), **carried}
                summary = d.evaluate(d.dir / f"tune/{tag}-{cand['name']}", combat["checkpoint"], cand, f"s.g{generation}.eval.log")
                table[cand["name"]] = brief(summary)
                if summary and (best is None or summary["target_mean"] > best[1]["target_mean"]):
                    best = (cand, summary)
        # A table pooled from every generation's randomized comparison, over the better of the current network
        # and the best new one.
        if comparing is not None and comparing.wait() != 0:
            d.event(driver="s", step="effects_failed", generation=generation)
        pooled = [str(d.dir / f"g{g}-effects.json") for g in range(args.first_generation, generation + 1)
                  if (d.dir / f"g{g}-effects.json").exists()]
        if pooled:
            new_table = d.dir / f"table-g{generation}.json"
            if not new_table.exists():
                subprocess.run([PYTHON, "-B", "tools/nosl/card_table.py", "--effects", ",".join(pooled), "--kinds", TABLE_KINDS,
                                "--out", str(new_table)], cwd=ROOT, env=d.env, check=True, capture_output=True)
            under = best[0] if best and current and best[1]["target_mean"] > current["target_mean"] else rule
            cand = {"name": under["name"].split("+")[0] + "+" + new_table.stem, "strategy": under["strategy"], "margin": under["margin"],
                    "table": str(new_table), "table_delta": TABLE_DELTA}
            summary = d.evaluate(d.dir / f"tune/{tag}-{cand['name']}", combat["checkpoint"], cand, f"s.g{generation}.eval.log")
            table[cand["name"]] = brief(summary)
            if summary and (best is None or summary["target_mean"] > best[1]["target_mean"]):
                best = (cand, summary)
        adopt = bool(best and current and best[1]["target_mean"] >= current["target_mean"] + ADOPT)
        if adopt:
            d.write("rule.json", best[0])
        d.event(driver="s", step="rule", generation=generation, combat=combat["iteration"], current={rule["name"]: brief(current)},
                candidates=table, adopted=best[0]["name"] if adopt else None)
        rule = d.read("rule.json")
        side = d.evaluate(d.dir / f"diag/g{generation}-p{combat['iteration']:05d}-{rule['name']}-a0", combat["checkpoint"], rule,
                          f"s.g{generation}.eval.log", prefix="NR4DIAG", runs=512, ascension=0)
        d.event(driver="s", step="ascension0_side_diagnostic", generation=generation, combat=combat["iteration"], rule=rule["name"], result=brief(side))
        generation += 1
    d.event(driver="s", step="exit", stopped=d.stop("s"), failures=failures, next_generation=generation)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("mode", choices=("init", "p", "s"))
    p.add_argument("--dir", required=True)
    p.add_argument("--x1", default=str(ROOT / "artifacts/nosl-q4u/start.pt"), help="the base policy outside combat")
    p.add_argument("--combat", default="", help="init: the combat checkpoint the loop starts from (a kept trainer checkpoint)")
    p.add_argument("--resume", default="", help="init: the trainer checkpoint with optimizer state that PPO resumes")
    p.add_argument("--strategy", default="", help="init: the strategic value model in force")
    p.add_argument("--margin", type=float, default=0.02)
    p.add_argument("--rule-name", default="")
    p.add_argument("--first-generation", type=int, default=3, help="s: the first generation of this loop (candidate A pools from it)")
    p.add_argument("--start-generation", type=int, default=0, help="s: the generation to go on with after a restart (default: the first)")
    p.add_argument("--arena", default="", help="p: recorded combat roots; half of each worker's slots then play them (nosl.combat_ppo)")
    p.add_argument("--arena-slots", type=int, default=20)
    p.add_argument("--arena-bank", type=int, default=48)
    p.add_argument("--runs", type=int, default=100000, help="s: runs per generation")
    p.add_argument("--collectors", type=int, default=3, help="s: collection processes side by side")
    p.add_argument("--collector-workers", type=int, default=8, help="s: sampler processes of each collector")
    args = p.parse_args()
    d = Driver(args.dir, args.x1)
    if args.mode == "init":
        if (d.dir / "rule.json").exists() or (d.dir / "combat.json").exists():
            raise SystemExit(f"{d.dir} is already initialised")
        start = Path(args.combat).resolve()
        iteration = d.iteration_of(start)
        acting = d.dir / f"p/combat-{iteration:05d}.pt"
        d.strip(start, acting)
        d.write("rule.json", {"name": args.rule_name, "strategy": str(Path(args.strategy).resolve()), "margin": args.margin})
        d.write("combat.json", {"iteration": iteration, "checkpoint": str(acting), "trainer": str(Path(args.resume).resolve())})
        d.event(driver="init", rule=d.read("rule.json"), combat=d.read("combat.json"))
    elif args.mode == "p":
        run_p(d, args)
    else:
        run_s(d, args)


if __name__ == "__main__":
    sys.exit(main())
