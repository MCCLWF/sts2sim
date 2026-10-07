"""Out-of-combat action values learned from whole-run outcomes (work item R1).

  .venv/bin/python -m nosl.strategy_q collect  --out DIR --combat X1.pt --prefix NR1G0T --runs 100000 --epsilon 0.15
  .venv/bin/python -m nosl.strategy_q shards   --data DIR[,DIR...]
  .venv/bin/python -m nosl.strategy_q train    --data DIR[,DIR...] --init X1.pt --out Q.pt
  .venv/bin/python -m nosl.strategy_q evaluate --combat X1.pt [--strategy Q.pt --margin 0.0] --prefix NR1TUNE --runs 512 --out FILE

Two public-observation policies share one run: inside combat (the public
in-combat flag) a frozen combat policy acts; outside it a strategic policy
acts. Nothing here simulates, searches or rolls back: `collect` and
`evaluate` play each run once from start to its real end.

collect   The behaviour policy outside combat is the strategic policy's
          greedy action, replaced with probability epsilon by a uniformly
          random legal action, so no legal action stops being tried. One line
          per attempt: seed, real end, the action rows and which of them were
          exploratory. The decisions taken outside combat are also written
          with their public observation to DIR/shards (nosl.shards).
shards    Rebuilds DIR/shards of a collection from its recorded runs: a run
          is a deterministic function of its seed and action rows, so
          replaying them (nosl.replay) gives back every observation. A replay
          whose end differs from the recorded end is an error, not a sample.
train     Regresses Q(observation, action taken) on the run's outcome target
          over the shards. The target is a declared training signal, not the
          product objective: floor / 50 + 0.5 * (acts completed) + 2 * victory.
          One run in every --validation-every is held out by its index.
evaluate  Greedy play on a fixed seed list. `--margin M`: outside combat the
          strategic policy's best action replaces the base policy's action
          only when its value is more than M above the base action's value.

Runs that end as anything but an original victory or death are counted and
never used as samples.
"""
from __future__ import annotations

import argparse
import collections
import copy
import gzip
import json
import math
import time
from pathlib import Path

import numpy as np
import torch

from . import native
from .model import load, save
from .replay import replay
from .shards import ShardWriter, batches, load_shard, shard_paths
from .vec import STATUS_NAMES, VecSampler

G_IN_COMBAT, G_DECISION = 11, 1
DECISION_NAMES = ("map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary")


def outcome_target(floor, act, status):
    """The training target of a finished run (see the module text)."""
    return floor / 50.0 + 0.5 * act + 2.0 * (status == native.VICTORY)


def wilson(successes, n, z=1.96):
    if n == 0:
        return (0.0, 1.0)
    p = successes / n
    centre = (p + z * z / (2 * n)) / (1 + z * z / n)
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / (1 + z * z / n)
    return (max(0.0, centre - half), min(1.0, centre + half))


def to_device(arrays, device):
    return [torch.from_numpy(np.ascontiguousarray(a)).to(device, non_blocking=True) for a in arrays]


def q_values(model, batch):
    """Per-action values of a strategic model: its action scores. Illegal slots are -inf."""
    return model(*batch)[0].float()


class Chooser:
    """Greedy actions for every sampler slot: the combat model inside combat, the strategic rule outside.

    The strategic rule is the base policy's action (`base`, or the combat model when none is given), replaced by
    the value model's best action when that is more than `margin` better by the value model's own estimate."""

    def __init__(self, combat, strategy=None, margin=0.0, device="cuda", base=None):
        self.combat, self.strategy, self.margin, self.device = combat, strategy, margin, torch.device(device)
        self.base = base

    @torch.no_grad()
    def __call__(self, A):
        """Returns (choice [runs] int64 tensor on device, outside mask tensor, deviated mask tensor)."""
        tokens_used = max(1, int(A["counts"][:, 0].max()))
        actions_used = max(1, int(A["counts"][:, 1].max()))
        batch = to_device((A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"]), self.device)
        logits = self.combat(*batch)[0].float()
        # Parked slots have no action row.
        logits = torch.where(torch.isinf(logits).all(dim=1, keepdim=True), torch.zeros_like(logits), logits)
        choice = logits.argmax(dim=1)
        outside = batch[0][:, G_IN_COMBAT] == 0
        deviated = torch.zeros_like(outside)
        rows = torch.nonzero(outside).flatten()
        if self.base is not None and len(rows):
            base_logits = self.base(*[x[rows] for x in batch])[0].float()
            base_logits = torch.where(torch.isinf(base_logits).all(dim=1, keepdim=True), torch.zeros_like(base_logits), base_logits)
            choice[rows] = base_logits.argmax(dim=1)
        if self.strategy is not None and len(rows):
            q = q_values(self.strategy, [x[rows] for x in batch])
            q = torch.where(torch.isinf(q).all(dim=1, keepdim=True), torch.zeros_like(q), q)
            best = q.argmax(dim=1)
            base = choice[rows]
            gain = q.gather(1, best.unsqueeze(1)).squeeze(1) - q.gather(1, base.unsqueeze(1)).squeeze(1)
            take = gain > self.margin
            choice[rows] = torch.where(take, best, base)
            deviated[rows] = take & (best != base)
        return choice, outside, deviated


def seeds_of(prefix, first, count):
    return [f"{prefix}{i:06d}" for i in range(first, first + count)]


def play(chooser, seeds, *, workers, epsilon=0.0, rng=None, keep_history=True, max_steps=30000, record=False):
    """Every seed once, to its real end. Yields one dict per attempt as it finishes.

    `record`: each row also carries "decisions", the run's decisions outside combat as
    (global, tokens, actions, action row taken, exploratory) with the tables unpadded."""
    per = max(1, min(80, -(-len(seeds) // workers)))  # slots per worker; each slot takes the next seed when its run ends
    with VecSampler(workers, per, split="train", seeds=seeds, max_steps=max_steps, keep_history=keep_history,
                    actor_filter=1) as vec:
        A = vec.arrays
        explored = [[] for _ in range(vec.runs)]
        decisions = [[] for _ in range(vec.runs)]
        deviations = np.zeros(vec.runs, np.int64)
        outside_count = np.zeros(vec.runs, np.int64)
        results = vec.reset()
        done = 0
        while True:
            for r in results:
                slot = r["slot"]
                row = {"seed": r["seed"], "status": r["status"], "end": STATUS_NAMES[r["status"]], "act": r["act"],
                       "floor": r["floor"], "hp": r["hp"], "max_hp": r["max_hp"], "gold": r["gold"], "deck": r["deck"],
                       "relics": r["relics"], "last_encounter": r["last_encounter"], "steps": r["steps"]}
                if "message" in r:
                    row["message"] = r["message"]
                if slot >= 0:
                    if keep_history:
                        row["action_rows"] = r.get("action_rows", [])
                    row["explored"] = explored[slot]
                    row["strategic"] = int(outside_count[slot])
                    row["deviations"] = int(deviations[slot])
                    if record:
                        row["decisions"] = decisions[slot]
                    decisions[slot] = []
                    explored[slot] = []
                    outside_count[slot] = 0
                    deviations[slot] = 0
                done += 1
                yield row
            if done >= len(seeds):
                return
            running = vec.running()
            if not running.any():
                raise RuntimeError("no run is active but some seeds have no result")
            choice, outside, deviated = chooser(A)
            choice = choice.cpu().numpy()
            outside = outside.cpu().numpy() & running
            deviations += deviated.cpu().numpy() & running
            outside_count += outside
            random_now = np.zeros(vec.runs, bool)
            if epsilon > 0:
                random_now = outside & (rng.random(vec.runs) < epsilon)
                for slot in np.flatnonzero(random_now):
                    choice[slot] = rng.integers(0, max(1, int(A["counts"][slot, 1])))
                    explored[slot].append(len(vec.history[slot]))
            if record:
                for slot in np.flatnonzero(outside):
                    nt, na = (int(x) for x in A["counts"][slot])
                    decisions[slot].append((A["global"][slot].copy(), A["tokens"][slot, :nt].copy(),
                                            A["actions"][slot, :na].copy(), int(choice[slot]), bool(random_now[slot])))
            results = vec.step(np.where(running, choice, -1).astype(np.int32))


def summarize(rows):
    finished = [r for r in rows if r["status"] in (native.VICTORY, native.DEATH)]
    n = len(finished)
    wins = sum(r["status"] == native.VICTORY for r in finished)
    floors = [r["floor"] for r in finished]
    target = [outcome_target(r["floor"], r["act"], r["status"]) for r in finished]
    act2 = sum(r["act"] >= 1 or r["status"] == native.VICTORY for r in finished)
    act3 = sum(r["act"] >= 2 or r["status"] == native.VICTORY for r in finished)
    low, high = wilson(wins, n)
    return {"attempts": len(rows), "ends": dict(collections.Counter(r["end"] for r in rows)), "terminals": n,
            "victories": wins, "win_rate": wins / n if n else None, "win_rate_ci95": [low, high],
            "act2": act2, "act3": act3, "act2_rate": act2 / n if n else None, "act3_rate": act3 / n if n else None,
            "floor_mean": float(np.mean(floors)) if n else None, "target_mean": float(np.mean(target)) if n else None,
            "strategic_per_run": float(np.mean([r.get("strategic", 0) for r in finished])) if n else None,
            "deviations_per_run": float(np.mean([r.get("deviations", 0) for r in finished])) if n else None}


def load_models(args, device):
    combat, blob = load(args.combat, device)
    if blob.get("actor_filter") != 1:
        raise ValueError("the combat checkpoint must be one trained behind actor filter 1")
    combat.eval().requires_grad_(False)
    strategy = base = None
    if getattr(args, "strategy", ""):
        strategy, sblob = load(args.strategy, device)
        if sblob.get("kind") != "strategy_q":
            raise ValueError("--strategy is not a strategy_q checkpoint")
        strategy.eval().requires_grad_(False)
    if getattr(args, "base", ""):
        base, bblob = load(args.base, device)
        if bblob.get("actor_filter") != 1:
            raise ValueError("the base checkpoint must be one trained behind actor filter 1")
        base.eval().requires_grad_(False)
    return combat, strategy, base


def collect(args):
    device = torch.device(args.device)
    combat, strategy, base = load_models(args, device)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if (out / "runs.jsonl.gz").exists():
        raise SystemExit(f"{out} already holds a collection")
    seeds = seeds_of(args.prefix, args.first, args.runs)
    rng = np.random.default_rng(args.seed)
    torch.manual_seed(args.seed)
    chooser = Chooser(combat, strategy, args.margin, device, base)
    config = {**vars(args), "target": "floor/50 + 0.5*act + 2*victory", "seeds": [seeds[0], seeds[-1]]}
    (out / "config.json").write_text(json.dumps(config, indent=1))
    began = time.time()
    rows, decisions = [], 0
    writer = ShardWriter(out / "shards")
    terminal = 0   # the run's index among the terminal runs of this collection, as read_runs lists them
    with gzip.open(out / "runs.jsonl.gz", "wt", encoding="utf-8") as stream:
        for row in play(chooser, seeds, workers=args.workers, epsilon=args.epsilon, rng=rng, record=True):
            taken = row.pop("decisions", [])
            if row["status"] in (native.VICTORY, native.DEATH) and "action_rows" in row:
                if taken:
                    writer.add(as_chunk(taken, row, terminal))
                terminal += 1
            stream.write(json.dumps(row) + "\n")
            decisions += len(row.get("action_rows", []))
            rows.append({k: v for k, v in row.items() if k != "action_rows"})
            if len(rows) % 5000 == 0:
                s = summarize(rows)
                print(json.dumps({"runs": len(rows), "wall_s": round(time.time() - began, 1), "decisions": decisions,
                                  "floor_mean": s["floor_mean"], "act2_rate": s["act2_rate"], "victories": s["victories"]}), flush=True)
    writer.flush()
    summary = {**summarize(rows), "decisions": decisions, "wall_s": round(time.time() - began, 1),
               "explored": int(sum(len(r.get("explored", [])) for r in rows)),
               "shards": writer.count, "shard_decisions": writer.decisions}
    (out / "summary.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps(summary))


def as_chunk(taken, row, run_index):
    """One run's recorded decisions as the padded arrays nosl.shards.ShardWriter takes."""
    n = len(taken)
    nt, na = max(len(d[1]) for d in taken), max(len(d[2]) for d in taken)
    chunk = {"global": np.stack([d[0] for d in taken]),
             "tokens": np.zeros((n, max(1, nt), native.TOKEN_WIDTH), np.int32),
             "actions": np.zeros((n, max(1, na), native.ACTION_WIDTH), np.int32),
             "counts": np.array([[len(d[1]), len(d[2])] for d in taken], np.int32),
             "choice": np.array([d[3] for d in taken]), "explored": np.array([d[4] for d in taken]),
             "floor": np.full(n, row["floor"]), "run_act": np.full(n, row["act"]),
             "status": np.full(n, row["status"]), "run": np.full(n, run_index)}
    for i, d in enumerate(taken):
        chunk["tokens"][i, :len(d[1])] = d[1]
        chunk["actions"][i, :len(d[2])] = d[2]
    return chunk


def build_shards(args):
    """DIR/shards of each collection, rebuilt by replaying its recorded runs."""
    for directory in args.data.split(","):
        runs, skipped = read_runs([directory])
        for r in runs:
            r["explored_set"] = set(r.get("explored", []))
            r["target"] = outcome_target(r["floor"], r["act"], r["status"])
        writer = ShardWriter(Path(directory) / "shards")
        began = time.time()
        for chunk in replay(runs, workers=args.workers, order_seed=1):
            writer.add(chunk)
        writer.flush()
        summary = {"runs": len(runs), "skipped_runs": dict(skipped), "shards": writer.count, "decisions": writer.decisions,
                   "expected_decisions": int(sum(r.get("strategic", 0) for r in runs)), "wall_s": round(time.time() - began, 1)}
        if summary["decisions"] != summary["expected_decisions"]:
            raise RuntimeError(f"{directory}: replay gave {summary['decisions']} decisions, the record says {summary['expected_decisions']}")
        (Path(directory) / "shards" / "manifest.json").write_text(json.dumps(summary, indent=1))
        print(json.dumps({"directory": directory, **summary}), flush=True)


def read_runs(directories):
    """Recorded runs of the given collections that ended in a real terminal."""
    runs, skipped = [], collections.Counter()
    for directory in directories:
        with gzip.open(Path(directory) / "runs.jsonl.gz", "rt", encoding="utf-8") as stream:
            for line in stream:
                r = json.loads(line)
                if r["status"] not in (native.VICTORY, native.DEATH) or "action_rows" not in r:
                    skipped[r["end"]] += 1
                    continue
                runs.append(r)
    return runs, skipped


def targets_of(data):
    """The outcome target of every decision of a loaded shard or of a batch from it."""
    return (data["floor"] / 50.0 + 0.5 * data["run_act"] + 2.0 * (data["status"] == native.VICTORY)).astype(np.float32)


def train(args):
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    paths = shard_paths(args.data.split(","))
    rng = np.random.default_rng(args.seed)

    def split(data):
        held_out = data["run"] % args.validation_every == 0
        return np.flatnonzero(~held_out), np.flatnonzero(held_out)

    first = load_shard(paths[0][1])
    mean_target = float(targets_of(first)[split(first)[0]].mean())
    if args.resume:
        model, blob = load(args.resume, device)
        if blob.get("kind") != "strategy_q":
            raise SystemExit("--resume is not a strategy_q checkpoint")
    else:
        model, blob = load(args.init, device)
        # The action score becomes a value: its output layer restarts at the mean target.
        torch.nn.init.zeros_(model.policy[-1].weight)
        torch.nn.init.constant_(model.policy[-1].bias, mean_target)
        torch.nn.init.constant_(model.value[-1].bias, mean_target)
    model.train().requires_grad_(True)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    log = out.with_suffix(".train.jsonl").open("a", encoding="utf-8")
    began = time.time()
    step = 0

    def forward(batch):
        inputs = to_device((batch["global"], batch["tokens"], batch["actions"], batch["counts"]), device)
        scores, values = model(*inputs)
        chosen = torch.from_numpy(batch["choice"].astype(np.int64)).to(device)
        return scores.float().gather(1, chosen.unsqueeze(1)).squeeze(1), values.float()

    def validate():
        model.eval()
        stats = collections.defaultdict(list)
        with torch.no_grad():
            for _, path in paths:
                data = load_shard(path)
                rows = split(data)[1]
                if not len(rows):
                    continue
                for batch in batches(data, rows, args.batch, args.token_budget, np.random.default_rng(7)):
                    q, v = forward(batch)
                    stats["q"].append(q.cpu().numpy())
                    stats["v"].append(v.cpu().numpy())
                    stats["y"].append(targets_of(batch))
                    stats["explored"].append(batch["explored"])
                    stats["kind"].append(batch["global"][:, G_DECISION])
        model.train()
        q, v, y, e, kind = (np.concatenate(stats[k]) for k in ("q", "v", "y", "explored", "kind"))
        result = {"validation_samples": int(len(y)), "q_mse": float(np.mean((q - y) ** 2)), "v_mse": float(np.mean((v - y) ** 2)),
                  "target_var": float(np.var(y)), "q_explained": float(1 - np.mean((q - y) ** 2) / np.var(y)),
                  "v_explained": float(1 - np.mean((v - y) ** 2) / np.var(y)),
                  "explored_share": float(e.mean()),
                  # Does Q say which exploratory actions turned out better than the state's value?
                  "explored_adv_corr": float(np.corrcoef((q - v)[e], (y - v)[e])[0, 1]) if e.sum() > 10 else None}
        for k in np.unique(kind):
            m = kind == k
            result[f"q_explained_{DECISION_NAMES[k]}"] = float(1 - np.mean((q[m] - y[m]) ** 2) / max(1e-9, np.var(y[m])))
        return result

    training_decisions = 0
    for epoch in range(args.epochs):
        recent = collections.deque(maxlen=200)
        samples = 0
        for which in rng.permutation(len(paths)):
            data = load_shard(paths[which][1])
            for batch in batches(data, split(data)[0], args.batch, args.token_budget, rng):
                q, v = forward(batch)
                target = torch.from_numpy(targets_of(batch)).to(device)
                weight = torch.ones_like(target)
                if args.explored_weight != 1.0:
                    weight = torch.where(torch.from_numpy(batch["explored"]).to(device), args.explored_weight * weight, weight)
                loss = (weight * torch.nn.functional.huber_loss(q, target, reduction="none", delta=1.0)).mean() \
                    + args.value_coef * torch.nn.functional.huber_loss(v, target, delta=1.0)
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), args.grad_norm)
                optimizer.step()
                step += 1
                samples += len(target)
                recent.append(float(torch.mean((q.detach() - target) ** 2)))
                if step % 500 == 0:
                    row = {"epoch": epoch, "step": step, "samples": samples, "wall_s": round(time.time() - began, 1),
                           "q_mse": round(float(np.mean(recent)), 5)}
                    log.write(json.dumps(row) + "\n")
                    log.flush()
                    if step % 5000 == 0:
                        print(json.dumps(row), flush=True)
            del data
        training_decisions = samples
        row = {"epoch": epoch, "step": step, "samples": samples, "wall_s": round(time.time() - began, 1), **validate()}
        log.write(json.dumps(row) + "\n")
        log.flush()
        print(json.dumps(row), flush=True)
        # One file per epoch: which epoch is used is decided from the validation numbers, by the plan's rule.
        for path in (out, out.with_suffix(f".e{epoch + 1}.pt")):
            save(model, path, kind="strategy_q", actor_filter=1, target="floor/50 + 0.5*act + 2*victory",
                 mean_target=mean_target, data=args.data, epochs_done=epoch + 1, training_decisions=training_decisions,
                 validation_every=args.validation_every, resumed_from=args.resume or None)
    log.close()


def evaluate(args):
    device = torch.device(args.device)
    combat, strategy, base = load_models(args, device)
    seeds = seeds_of(args.prefix, args.first, args.runs)
    chooser = Chooser(combat, strategy, args.margin, device, base)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    began = time.time()
    rows = []
    with out.open("w", encoding="utf-8") as stream:
        for row in play(chooser, seeds, workers=args.workers, keep_history=True):
            row.update({"ascension": 10, "actor_filter": 1})
            stream.write(json.dumps(row) + "\n")
            rows.append(row)
    summary = {"policy": {"combat": args.combat, "base": args.base or None, "strategy": args.strategy or None, "margin": args.margin},
               "prefix": args.prefix, "first": args.first, **summarize(rows), "wall_s": round(time.time() - began, 1),
               "device": str(device), "precision": "float32"}
    out.with_suffix(".summary.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps(summary))


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("mode", choices=("collect", "shards", "train", "evaluate"))
    p.add_argument("--out", default="", help="collect: directory; train: checkpoint; evaluate: result file")
    p.add_argument("--combat", default="", help="frozen combat checkpoint (collect, evaluate)")
    p.add_argument("--strategy", default="", help="strategy_q checkpoint; without it the base policy acts outside combat")
    p.add_argument("--base", default="", help="frozen base policy outside combat (default: the combat checkpoint itself)")
    p.add_argument("--margin", type=float, default=0.0)
    p.add_argument("--prefix", default="", help="run seeds are <prefix><6 digits>")
    p.add_argument("--first", type=int, default=0)
    p.add_argument("--runs", type=int, default=1000)
    p.add_argument("--epsilon", type=float, default=0.15)
    p.add_argument("--workers", type=int, default=6)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    p.add_argument("--data", default="", help="train: comma-separated collection directories")
    p.add_argument("--init", default="", help="train: checkpoint whose encoder the value model starts from")
    p.add_argument("--resume", default="", help="train: strategy_q checkpoint to continue")
    p.add_argument("--epochs", type=int, default=2)
    p.add_argument("--batch", type=int, default=1024)
    p.add_argument("--token-budget", type=int, default=48000, help="train: most padded tokens in one batch")
    p.add_argument("--lr", type=float, default=2e-4)
    p.add_argument("--value-coef", type=float, default=0.5)
    p.add_argument("--explored-weight", type=float, default=1.0)
    p.add_argument("--grad-norm", type=float, default=1.0)
    p.add_argument("--validation-every", type=int, default=20)
    args = p.parse_args(argv)
    if args.mode != "shards" and not args.out:
        raise SystemExit("--out is required")
    if args.mode in ("shards", "train") and not args.data:
        raise SystemExit("--data is required")
    if args.mode in ("collect", "evaluate"):
        if not args.combat or not args.prefix:
            raise SystemExit("--combat and --prefix are required")
        if args.prefix.startswith(("NOSLDEV", "NOSLTEST")):
            raise SystemExit("the registered dev/test splits are not for this tool")
    {"collect": collect, "shards": build_shards, "train": train, "evaluate": evaluate}[args.mode](args)


if __name__ == "__main__":
    main()
