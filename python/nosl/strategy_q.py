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
train     Regresses Q(observation, action taken) over the shards. The run's
          outcome target is a declared training signal, not the product
          objective: floor / 50 + 0.5 * (acts completed) + 2 * victory.
          --td-lambda 1 (default) regresses on that outcome itself. Below 1
          the target of a decision is the lambda-return along the run's own
          later strategic decisions, G = (1 - lambda) * V(next) + lambda * G(next),
          ending in the run's outcome; V is the value head of --value-from, a
          frozen earlier model. What happened after the next decision then
          enters only through V's average, not through this one run's luck.
          The value head itself is always regressed on the outcome.
          One run in every --validation-every is held out by its index.
          A value model that was fitted on a run has partly memorised that
          run's outcome, which leaks back into the bootstrap. --value-from A,B
          cross-fits: A must have been trained with --only-runs 2:0 (runs of
          even index) and B with --only-runs 2:1; each run is then valued by
          the model that never saw it.
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
from .shards import ShardWriter, batches, load_group, load_shard, shard_paths
from .contexts import Describer
from .guards import Guards, parse as parse_guards, parse_low
from .readiness import Readiness, readiness_override
from .vec import STATUS_NAMES, VecSampler

G_IN_COMBAT, G_DECISION = 11, 1
DECISION_NAMES = ("map", "combat", "rewards", "event", "rest", "shop", "treasure", "choice", "ended", "boundary")
COMBAT_CANDIDATES = 4    # the combat advantage rule looks at the policy's most probable actions, as its labels did
SCORE_SMALL_TOKENS = 40  # slots with at most this many tokens are scored in one batch, the rest in another


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


def load_table(path):
    """tools/nosl/card_table.py output: {context: {label: value}} in the words of nosl.contexts."""
    return json.loads(Path(path).read_text())["contexts"]


def load_readiness(args):
    """--readiness and its options as a nosl.readiness.Readiness, or None."""
    if not args.readiness:
        return None
    return Readiness(args.readiness, args.readiness_lambda, [x for x in args.readiness_scope.split(",") if x],
                     [int(x) for x in args.readiness_acts.split(",")], not args.readiness_no_table, args.readiness_chain,
                     args.readiness_shop_min, args.readiness_shop_add_only)


def table_override(table, delta, describer, A, rows, choice, deviated=None, scores=None):
    """The table rule, applied over the actions already chosen for the sampler slots `rows` (all outside combat).

    A decision whose context the table holds: every action gets its label's value (0 for a label the table does
    not hold, the value of an average action; none for an action without a label). When the best value is more
    than `delta` above the value of the chosen action's own label, the chosen action is replaced by an action with
    the best label; among several, the one the rule scored highest (`scores[i]`: per-action scores of rows[i]), else
    the first. A decision the rule already answered with the best label is left alone."""
    g = A["global"]
    for position, row in enumerate(rows):
        if int(g[row, G_DECISION]) == 1:
            continue
        count = int(A["counts"][row, 1])
        if count < 2:
            continue
        context, labels = describer(g[row], A["tokens"][row], A["actions"][row, :count])
        entries = table.get(context)
        if entries is None:
            continue
        values = np.array([np.nan if label is None else entries.get(label, 0.0) for label in labels])
        if np.isnan(values).all():
            continue
        own = int(choice[row])
        own_value = 0.0 if own >= count or np.isnan(values[own]) else values[own]
        top = np.nanmax(values)
        if top - own_value <= delta:
            continue
        candidates = np.flatnonzero(values == top)
        if scores is not None and len(candidates) > 1:
            ranked = scores[position][candidates]
            best = int(candidates[int(np.argmax(ranked))])
        else:
            best = int(candidates[0])
        if best != own:
            choice[row] = best
            if deviated is not None:
                deviated[row] = True


def q_values(model, batch):
    """Per-action values of a strategic model: its action scores. Illegal slots are -inf."""
    return model(*batch)[0].float()


class Chooser:
    """Greedy actions for every sampler slot: the combat model inside combat, the strategic rule outside.

    The strategic rule is the base policy's action (`base`, or the combat model when none is given), replaced by
    the value model's best action when that is more than `margin` better by the value model's own estimate.

    `prior` > 0 is another rule: the action with the highest value plus `prior` times the base policy's log
    probability. A small value difference then decides where the base policy is unsure, and a larger one is
    needed where it is sure; `margin` is not used.

    `strategy` may be a list of value models: their values are averaged. Models fitted on the same runs in a
    different order, or on different runs, disagree by about as much as the advantages being compared."""

    def __init__(self, combat, strategy=None, margin=0.0, device="cuda", base=None, prior=0.0, style=None, table=None,
                 table_delta=0.0, readiness=None, combat_value=None, combat_margin=0.0, style_last=False, guards=None, guards_low=(-1, None)):
        self.guards = Guards(guards, guards_low[1], guards_low[0]) if guards or guards_low[1] else None      # nosl.guards: applied last
        self.combat, self.margin, self.device = combat, margin, torch.device(device)
        # nosl.rollout_distill --target advantage: inside combat the policy's action, unless another of its most
        # probable actions has a predicted advantage above combat_margin. One more forward pass, no simulation.
        self.combat_value, self.combat_margin = combat_value, combat_margin
        self.style, self.style_last = style or {}, style_last
        self.table, self.table_delta = table or {}, table_delta
        self.readiness = readiness      # nosl.readiness.Readiness: applied last, over the table rule
        self.describer = Describer() if self.table or readiness is not None else None
        self.strategy = strategy if strategy is None or isinstance(strategy, (list, tuple)) else [strategy]
        self.base, self.prior = base, prior

    @torch.no_grad()
    def __call__(self, A):
        """Returns (choice [runs] int64 tensor on device, outside mask tensor, replaced mask tensor)."""
        actions_used = max(1, int(A["counts"][:, 1].max()))
        outside_np = A["global"][:, G_IN_COMBAT] == 0
        logits = self.scores(self.combat, A, np.arange(len(outside_np)), actions_used)
        choice = logits.argmax(dim=1)
        inside_np = np.flatnonzero(~outside_np)
        if self.combat_value is not None and len(inside_np):
            inside = torch.from_numpy(inside_np).to(self.device)
            # column 0 is the policy's own action; equal logits (two copies of a card) keep their row order, as in nosl/actor.cpp
            ranked = torch.sort(logits[inside], dim=1, descending=True, stable=True)
            top = torch.return_types.topk((ranked.values[:, :COMBAT_CANDIDATES], ranked.indices[:, :COMBAT_CANDIDATES]))
            models = self.combat_value if isinstance(self.combat_value, (list, tuple)) else [self.combat_value]
            advantage = 0
            for model in models:     # several fits of the same labels: their predicted advantages are averaged
                scores = self.scores(model, A, inside_np, actions_used).gather(1, top.indices)
                advantage = advantage + (scores - scores[:, :1]) / len(models)
            advantage = torch.where(torch.isinf(top.values), torch.full_like(advantage, float("-inf")), advantage)
            best = advantage.argmax(dim=1, keepdim=True)
            take = advantage.gather(1, best).squeeze(1) > self.combat_margin
            choice[inside] = torch.where(take, top.indices.gather(1, best).squeeze(1), choice[inside])
        outside = torch.from_numpy(outside_np).to(self.device)
        deviated = torch.zeros_like(outside)
        table_scores = None
        rows_np = np.flatnonzero(outside_np)
        rows = torch.from_numpy(rows_np).to(self.device)
        base_logits = logits[rows] if len(rows_np) else None
        if self.base is not None and len(rows_np):
            base_logits = self.scores(self.base, A, rows_np, actions_used)
            choice[rows] = base_logits.argmax(dim=1)
        if self.strategy is not None and len(rows_np) and self.prior > 0:
            q = self.values(A, rows_np, actions_used)
            base = choice[rows]
            mixed = q + self.prior * torch.log_softmax(base_logits, dim=1)   # -inf stays -inf for illegal rows
            choice[rows] = mixed.argmax(dim=1)
            deviated[rows] = choice[rows] != base
        elif self.strategy is not None and len(rows_np):
            q = self.values(A, rows_np, actions_used)
            best = q.argmax(dim=1)
            base = choice[rows]
            gain = q.gather(1, best.unsqueeze(1)).squeeze(1) - q.gather(1, base.unsqueeze(1)).squeeze(1)
            take = gain > self.margin
            choice[rows] = torch.where(take, best, base)
            deviated[rows] = take & (best != base)
            if self.table or self.readiness is not None:
                table_scores = torch.nan_to_num(q, neginf=-1e9).cpu().numpy()
        if self.style and len(rows_np) and not self.style_last:
            self.apply_style(A, rows_np, choice, deviated)
        if self.table and len(rows_np):
            self.apply_table(A, rows_np, choice, deviated, table_scores)
        if self.readiness is not None and len(rows_np):
            readiness_override(self.readiness, self.table, self.table_delta, self.describer, A, rows_np, choice, deviated,
                               table_scores)
        if self.style and len(rows_np) and self.style_last:     # the habit holds against the table and the readiness rule
            self.apply_style(A, rows_np, choice, deviated)
        if self.guards is not None:
            position = {int(slot): i for i, slot in enumerate(rows_np)}
            def scores_of(slot):      # the acting policy's scores: the base policy outside combat, the combat network inside
                return (base_logits[position[slot]] if slot in position else logits[slot]).cpu().numpy()
            picked = choice.cpu().numpy()
            changed = self.guards(A, picked, scores_of)
            if changed:
                where = torch.as_tensor(np.array(changed), device=self.device)
                choice[where] = torch.as_tensor(picked[changed], device=self.device)
                deviated[where] = outside[where]
        return choice, outside, deviated

    def apply_table(self, A, rows, choice, deviated, scores=None):
        table_override(self.table, self.table_delta, self.describer, A, rows, choice, deviated, scores)

    def apply_style(self, A, rows, choice, deviated):
        """A diagnostic, not a learned rule: fixed dispositions kept for a whole run, applied over whatever the rule
        chose. `smith`: at a rest site, upgrade whenever HP is at least that fraction of the maximum and the option
        is there. `elite`: on the map, enter an elite node whenever one is offered and HP is at least that fraction.
        They ask what the one-decision-at-a-time comparisons cannot: whether a habit pays when it is kept up."""
        g = A["global"]
        hp = g[:, 7] / np.maximum(1, g[:, 8])
        for row in rows:
            decision, wanted = int(g[row, G_DECISION]), None
            if decision == 4 and "smith" in self.style and hp[row] >= self.style["smith"]:
                wanted = (7, 41 + 1)      # Action::RestOption on a token whose misc is the Smith option
            elif decision == 0 and "elite" in self.style and hp[row] >= self.style["elite"]:
                wanted = (1, 1 + 6)       # Action::MapMove onto an elite node
            if wanted is None:
                continue
            actions = A["actions"][row, :int(A["counts"][row, 1])]
            for index, (kind, token, _, _) in enumerate(actions):
                if kind == wanted[0] and token >= 0 and A["tokens"][row, token, 6] == wanted[1] and \
                        (decision != 4 or A["tokens"][row, token, 9]):
                    if int(choice[row]) != index:
                        choice[row] = index
                        deviated[row] = True
                    break

    def values(self, A, rows, actions_used):
        """The mean of the value models' action values for the sampler slots `rows`."""
        total = self.scores(self.strategy[0], A, rows, actions_used)
        for model in self.strategy[1:]:
            total = total + self.scores(model, A, rows, actions_used)
        return total / len(self.strategy)

    def scores(self, model, A, rows, actions_used):
        """`model`'s action scores for the sampler slots `rows`: [len(rows), actions_used], -inf past a slot's
        action count and 0 for a parked slot. Slots are scored in two groups of similar observation size; the map
        screen's hundred tokens would otherwise set the padded width of every slot in the batch."""
        out = torch.full((len(rows), actions_used), float("-inf"), device=self.device)
        tokens = A["counts"][rows, 0]
        for group in (np.flatnonzero(tokens <= SCORE_SMALL_TOKENS), np.flatnonzero(tokens > SCORE_SMALL_TOKENS)):
            if not len(group):
                continue
            pick = rows[group]
            width = max(1, int(tokens[group].max()))
            batch = to_device((A["global"][pick], A["tokens"][pick, :width], A["actions"][pick, :actions_used],
                               A["counts"][pick]), self.device)
            out[torch.from_numpy(group).to(self.device)] = model(*batch)[0].float()
        # Parked slots have no action row.
        return torch.where(torch.isinf(out).all(dim=1, keepdim=True), torch.zeros_like(out), out)


def seeds_of(prefix, first, count):
    return [f"{prefix}{i:06d}" for i in range(first, first + count)]


def play(chooser, seeds, *, workers, epsilon=0.0, rng=None, keep_history=True, max_steps=30000, record=False,
         ascension=10, ascensions=None):
    """Every seed once, to its real end. Yields one dict per attempt as it finishes.

    `record`: each row also carries "decisions", the run's decisions outside combat as
    (global, tokens, actions, action row taken, exploratory, position among the action rows), tables unpadded."""
    per = max(1, min(80, -(-len(seeds) // workers)))  # slots per worker; each slot takes the next seed when its run ends
    with VecSampler(workers, per, split="train", seeds=seeds, max_steps=max_steps, keep_history=keep_history,
                    actor_filter=1, ascension=ascension, ascensions=ascensions) as vec:
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
                       "relics": r["relics"], "last_encounter": r["last_encounter"], "steps": r["steps"],
                       "ascension": r.get("ascension", ascension)}
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
                                            A["actions"][slot, :na].copy(), int(choice[slot]), bool(random_now[slot]),
                                            len(vec.history[slot])))
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
        strategy = []
        for path in args.strategy.split(","):
            model, sblob = load(path, device)
            if sblob.get("kind") != "strategy_q":
                raise ValueError("--strategy is not a strategy_q checkpoint")
            strategy.append(model.eval().requires_grad_(False))
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
    chooser = Chooser(combat, strategy, args.margin, device, base, args.prior, None,
                      load_table(args.table) if args.table else None, args.table_delta, load_readiness(args))
    config = {**vars(args), "target": "floor/50 + 0.5*act + 2*victory", "seeds": [seeds[0], seeds[-1]]}
    (out / "config.json").write_text(json.dumps(config, indent=1))
    began = time.time()
    rows, decisions = [], 0
    writer = ShardWriter(out / "shards")
    terminal = 0   # the run's index among the terminal runs of this collection, as read_runs lists them
    with gzip.open(out / "runs.jsonl.gz", "wt", encoding="utf-8") as stream:
        mixed = [int(x) for x in args.ascensions.split(",")] if args.ascensions else None
        for row in play(chooser, seeds, workers=args.workers, epsilon=args.epsilon, rng=rng, record=True, max_steps=args.max_steps,
                        ascension=args.ascension, ascensions=mixed):
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
             "status": np.full(n, row["status"]), "run": np.full(n, run_index),
             "step": np.array([d[5] for d in taken])}
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


def lambda_returns(paths, value_models, lam, args, device):
    """For every shard, the lambda-return of each of its decisions (see the module text).

    `value_models`: one model, or two cross-fitted ones [trained on even runs, trained on odd runs]."""
    collections_seen = {}
    parts = []
    for collection, path in paths:
        data = load_shard(path)
        v = np.zeros(len(data["choice"]), np.float32)
        with torch.no_grad():
            for batch in batches(data, np.arange(len(v)), args.batch, args.token_budget, np.random.default_rng(0)):
                inputs = to_device((batch["global"], batch["tokens"], batch["actions"], batch["counts"]), device)
                if len(value_models) == 1:
                    v[batch["rows"]] = value_models[0](*inputs)[1].float().cpu().numpy()
                else:
                    # A run of even index is valued by the model fitted on the odd runs, and the reverse.
                    seen_even = value_models[0](*inputs)[1].float().cpu().numpy()
                    seen_odd = value_models[1](*inputs)[1].float().cpu().numpy()
                    v[batch["rows"]] = np.where(batch["run"] % 2 == 0, seen_odd, seen_even)
        parts.append((np.full(len(v), collections_seen.setdefault(collection, len(collections_seen)), np.int64),
                      data["run"].astype(np.int64), data["step"].astype(np.int64), v, targets_of(data)))
    collection, run, step, v, y = (np.concatenate([p[i] for p in parts]) for i in range(5))
    order = np.lexsort((step, run, collection))
    same_run = np.zeros(len(order), bool)
    same_run[:-1] = (collection[order][:-1] == collection[order][1:]) & (run[order][:-1] == run[order][1:])
    v_sorted, y_sorted, linked = v[order].tolist(), y[order].tolist(), same_run.tolist()
    g_sorted = [0.0] * len(order)
    for i in range(len(order) - 1, -1, -1):
        g_sorted[i] = (1.0 - lam) * v_sorted[i + 1] + lam * g_sorted[i + 1] if linked[i] else y_sorted[i]
    g = np.empty(len(order), np.float32)
    g[order] = np.asarray(g_sorted, np.float32)
    out, at = [], 0
    for part in parts:
        out.append(g[at:at + len(part[0])])
        at += len(part[0])
    return out, {"decisions": int(len(g)), "bootstrap_minus_outcome_var": float(np.var(g - y)), "outcome_var": float(np.var(y)),
                 "target_var": float(np.var(g))}


def train(args):
    device = torch.device(args.device)
    torch.manual_seed(args.seed)
    paths = shard_paths(args.data.split(","))
    rng = np.random.default_rng(args.seed)
    bootstrapped, bootstrap_stats = None, None
    if args.td_lambda < 1.0:
        if not args.value_from:
            raise SystemExit("--td-lambda below 1 needs --value-from")
        value_models = []
        for expected, path in enumerate(args.value_from.split(",")):
            value_model, vblob = load(path, device)
            if vblob.get("kind") != "strategy_q":
                raise SystemExit("--value-from is not a strategy_q checkpoint")
            if "," in args.value_from and vblob.get("only_runs") != f"2:{expected}":
                raise SystemExit("cross-fitting needs --value-from EVEN,ODD trained with --only-runs 2:0 and 2:1")
            value_models.append(value_model.eval().requires_grad_(False))
        bootstrapped, bootstrap_stats = lambda_returns(paths, value_models, args.td_lambda, args, device)
        bootstrap_stats["cross_fitted"] = len(value_models) == 2
        print(json.dumps({"lambda": args.td_lambda, **bootstrap_stats}), flush=True)
        del value_models

    fold = tuple(int(x) for x in args.only_runs.split(":")) if args.only_runs else None

    def split(data):
        held_out = data["run"] % args.validation_every == 0
        training = ~held_out if fold is None else ~held_out & (data["run"] % fold[0] == fold[1])
        return np.flatnonzero(training), np.flatnonzero(held_out)

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
    # --ema D keeps an exponential average of the weights and validates and saves that average. A run's outcome is
    # a noisy target shared by all its decisions; the last iterate follows whichever runs it saw last.
    average = {k: v.detach().clone() for k, v in model.state_dict().items() if v.dtype.is_floating_point} if args.ema > 0 else None

    class averaged:
        """The model with its averaged weights inside the block."""
        def __enter__(self):
            if average is not None:
                self.live = {k: v.detach().clone() for k, v in model.state_dict().items() if k in average}
                model.load_state_dict(average, strict=False)
        def __exit__(self, *exc):
            if average is not None:
                model.load_state_dict(self.live, strict=False)

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
        order = rng.permutation(len(paths))
        for begin in range(0, len(order), args.shard_group):
            members = order[begin:begin + args.shard_group]
            data = load_group([paths[i][1] for i in members])
            group_target = None if bootstrapped is None else np.concatenate([bootstrapped[i] for i in members])
            if args.lr_final > 0:
                # A linear decay over the whole training: late steps average the outcome noise instead of chasing it.
                progress = (epoch + begin / len(order)) / args.epochs
                for group in optimizer.param_groups:
                    group["lr"] = args.lr + (args.lr_final - args.lr) * progress
            for batch in batches(data, split(data)[0], args.batch, args.token_budget, rng, by_run=args.batch_by_run):
                q, v = forward(batch)
                target = torch.from_numpy(targets_of(batch)).to(device)
                q_target = target if group_target is None else torch.from_numpy(group_target[batch["rows"]]).to(device)
                weight = torch.ones_like(target)
                if args.explored_weight != 1.0:
                    weight = torch.where(torch.from_numpy(batch["explored"]).to(device), args.explored_weight * weight, weight)
                loss = (weight * torch.nn.functional.huber_loss(q, q_target, reduction="none", delta=1.0)).mean() \
                    + args.value_coef * torch.nn.functional.huber_loss(v, target, delta=1.0)
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), args.grad_norm)
                optimizer.step()
                if average is not None:
                    with torch.no_grad():
                        for k, v in model.state_dict().items():
                            if k in average:
                                average[k].lerp_(v, 1.0 - args.ema)
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
        with averaged():
            row = {"epoch": epoch, "step": step, "samples": samples, "wall_s": round(time.time() - began, 1), **validate()}
            log.write(json.dumps(row) + "\n")
            log.flush()
            print(json.dumps(row), flush=True)
            # One file per epoch: which epoch is used is decided from the validation numbers, by the plan's rule.
            for path in (out, out.with_suffix(f".e{epoch + 1}.pt")):
                save(model, path, kind="strategy_q", actor_filter=1, target="floor/50 + 0.5*act + 2*victory",
                     mean_target=mean_target, data=args.data, epochs_done=epoch + 1, training_decisions=training_decisions,
                     validation_every=args.validation_every, resumed_from=args.resume or None, td_lambda=args.td_lambda,
                     value_from=args.value_from or None, bootstrap=bootstrap_stats, only_runs=args.only_runs or None,
                     ema=args.ema, lr_final=args.lr_final, batch_by_run=args.batch_by_run, shard_group=args.shard_group)
    log.close()


def evaluate(args):
    device = torch.device(args.device)
    combat, strategy, base = load_models(args, device)
    seeds = seeds_of(args.prefix, args.first, args.runs)
    style = {key: float(value) for key, value in (item.split("=") for item in args.style.split(",") if item)}
    combat_value = None
    if args.combat_value:
        combat_value = []
        for path in args.combat_value.split(","):
            model, vblob = load(path, device)
            if vblob.get("kind") != "combat_advantage" or vblob.get("actor_filter") != 1:
                raise ValueError("--combat-value is not a combat advantage checkpoint behind actor filter 1")
            combat_value.append(model.eval().requires_grad_(False))
    chooser = Chooser(combat, strategy, args.margin, device, base, args.prior, style,
                      load_table(args.table) if args.table else None, args.table_delta, load_readiness(args),
                      combat_value=combat_value, combat_margin=args.combat_margin, style_last=args.style_last,
                      guards=parse_guards(args.guards), guards_low=parse_low(args.guards_low))
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    began = time.time()
    rows = []
    with out.open("w", encoding="utf-8") as stream:
        for row in play(chooser, seeds, workers=args.workers, keep_history=True, ascension=args.ascension, max_steps=args.max_steps):
            row.update({"actor_filter": 1})
            stream.write(json.dumps(row) + "\n")
            rows.append(row)
    summary = {"policy": {"combat": args.combat, "base": args.base or None, "strategy": args.strategy or None, "margin": args.margin,
                          "prior": args.prior, "style": style or None, "style_last": args.style_last,
                          "combat_value": args.combat_value or None, "combat_margin": args.combat_margin if args.combat_value else None,
                          "guards": args.guards or None, "guards_low": args.guards_low or None, "table": args.table or None,
                          "table_delta": args.table_delta if args.table else None,
                          "readiness": None if not args.readiness else {
                              "model": args.readiness, "lambda": args.readiness_lambda, "scope": args.readiness_scope,
                              "acts": args.readiness_acts, "no_table": args.readiness_no_table, "chain": args.readiness_chain,
                              "shop_min": args.readiness_shop_min, "shop_add_only": args.readiness_shop_add_only}},
               "prefix": args.prefix, "first": args.first, "ascension": args.ascension, **summarize(rows),
               "guards_fired": dict(chooser.guards.fired) if chooser.guards is not None else None,
               "wall_s": round(time.time() - began, 1),
               "device": str(device), "precision": "float32"}
    out.with_suffix(".summary.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps(summary))


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("mode", choices=("collect", "shards", "train", "evaluate"))
    p.add_argument("--out", default="", help="collect: directory; train: checkpoint; evaluate: result file")
    p.add_argument("--combat", default="", help="frozen combat checkpoint (collect, evaluate)")
    p.add_argument("--strategy", default="", help="strategy_q checkpoint, or several separated by commas whose values are "
                                                  "averaged; without it the base policy acts outside combat")
    p.add_argument("--base", default="", help="frozen base policy outside combat (default: the combat checkpoint itself)")
    p.add_argument("--margin", type=float, default=0.0)
    p.add_argument("--prior", type=float, default=0.0,
                   help="above 0: outside combat take the action with the highest value + PRIOR * log base probability")
    p.add_argument("--prefix", default="", help="run seeds are <prefix><6 digits>")
    p.add_argument("--first", type=int, default=0)
    p.add_argument("--runs", type=int, default=1000)
    p.add_argument("--epsilon", type=float, default=0.15)
    p.add_argument("--ascension", type=int, default=10, help="collect, evaluate: the runs' ascension level (the product is 10)")
    p.add_argument("--ascensions", default="", help="collect: a mixed-ascension curriculum, e.g. 0,5,10; each run's level "
                                                    "follows from its seed (nosl.vec.ascension_of) and is recorded")
    p.add_argument("--workers", type=int, default=6)
    p.add_argument("--table", default="", help="collect, evaluate: a value table over the rule (tools/nosl/card_table.py; "
                   "table_override)")
    p.add_argument("--table-delta", type=float, default=0.0)
    p.add_argument("--readiness", default="", help="collect, evaluate: the boss-door model (tools/nosl/boss_readiness.py) for "
                   "the readiness rule over the table rule (nosl.readiness)")
    p.add_argument("--readiness-lambda", type=float, default=0.07, help="outcome-target units per unit of log-odds")
    p.add_argument("--readiness-scope", default="cards,upgrade", help="of cards, upgrade, remove, rest")
    p.add_argument("--readiness-acts", default="0", help="act indices the readiness rule is used in")
    p.add_argument("--readiness-no-table", action="store_true", help="judge by the change in log-odds alone")
    p.add_argument("--readiness-chain", action="store_true", help="the chain of doors with the model file's own weights; "
                   "--readiness-lambda scales their sum and --readiness-acts is not used")
    p.add_argument("--readiness-shop-min", type=float, default=0.02, help="scope shop: the least score a purchase is made for")
    p.add_argument("--readiness-shop-add-only", action="store_true", help="scope shop: never undo the rule's own purchase")
    p.add_argument("--style", default="", help="evaluate: fixed run-long dispositions over the rule, e.g. smith=0.5,elite=0.7 "
                   "(Chooser.apply_style; a diagnostic)")
    p.add_argument("--style-last", action="store_true", help="evaluate: the dispositions are applied after the table and "
                   "the readiness rule instead of before them, so that those do not undo them")
    p.add_argument("--combat-value", default="", help="evaluate: a combat advantage checkpoint (nosl.rollout_distill --target "
                   "advantage), or several separated by commas whose predictions are averaged: inside combat the policy's "
                   "action, unless another of its most probable actions is predicted to be more than --combat-margin better")
    p.add_argument("--combat-margin", type=float, default=0.02)
    p.add_argument("--guards", default="", help="evaluate: corrections of plainly wasteful actions, applied last (nosl.guards), "
                   "e.g. nodiscard,nobuyfull,takerewards,hold=0.5")
    p.add_argument("--guards-low", default="", help="evaluate: LEVEL:GUARDS, the set used instead in runs at ascension LEVEL or below")
    p.add_argument("--max-steps", type=int, default=30000, help="collect, evaluate: decisions after which a run is cut and "
                   "reported as step_limit, never as a death. A greedy policy can repeat a free card that returns to "
                   "its hand without end (Particle Wall made free by Touch of Insanity did, 2026-10-08)")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    p.add_argument("--data", default="", help="train: comma-separated collection directories")
    p.add_argument("--init", default="", help="train: checkpoint whose encoder the value model starts from")
    p.add_argument("--resume", default="", help="train: strategy_q checkpoint to continue")
    p.add_argument("--epochs", type=int, default=2)
    p.add_argument("--batch", type=int, default=1024)
    p.add_argument("--token-budget", type=int, default=48000, help="train: most padded tokens in one batch")
    p.add_argument("--shard-group", type=int, default=6, help="train: shards mixed in memory at a time (nosl.shards.load_group)")
    p.add_argument("--lr-final", type=float, default=0.0, help="train: above 0, the rate decays linearly from --lr to this")
    p.add_argument("--batch-by-run", action="store_true", help="train: batches hold whole runs (nosl.shards.batches)")
    p.add_argument("--ema", type=float, default=0.0, help="train: above 0, validate and save an exponential average of the weights with this decay")
    p.add_argument("--td-lambda", type=float, default=1.0, help="train: below 1, Q regresses on the lambda-return (module text)")
    p.add_argument("--value-from", default="", help="train: strategy_q checkpoint whose value head bootstraps the lambda-return; "
                                                    "EVEN,ODD cross-fits (module text)")
    p.add_argument("--only-runs", default="", help="train: M:R trains on the runs whose index is R modulo M only")
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
