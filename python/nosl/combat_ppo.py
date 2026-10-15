"""PPO for the combat policy, paid at the end of each combat with the strategic value.

  .venv/bin/python -m nosl.combat_ppo --out DIR --init X1.pt --base X1.pt --strategy Q.pt --margin M --decisions 20000000

Whole normal runs are sampled. Outside combat a frozen strategic rule acts
(nosl.strategy_q: the base policy's action unless the value model prefers
another by more than the margin). Inside combat the learner samples, and only
those decisions are trained.

Reward (a declared training signal, not the product objective). A combat is
one episode with gamma 1. Its last action is paid
    value after - value before,
where "value before" is the strategic value model's estimate at the last
strategic decision before the combat and "value after" is its estimate at the
first strategic decision after it, or, when the run ends inside the combat,
the run's outcome target (nosl.strategy_q.outcome_target: no future is read,
the run really ended). Every other combat action is paid 0. The combat policy
is therefore asked to leave the run in the state the strategic value prefers:
what HP, potions, max HP and relic counters are worth is the value model's
estimate, not a fixed weight.

--death-penalty D subtracts D from the pay of a combat the run died in. With
the strategic value alone, dying costs only what the value model expected
the rest of the run to bring, and for a policy that expects to die soon that
is little: a gamble that risks the run is priced like a few floors. D prices
death as if the run were worth going on with. It is a shaping term and makes
the training objective differ from the outcome target; whether it helps is
decided by whole-run results, not by the training reward.

--reward combat pays a combat without the judge: a survived combat is paid
    (win_bonus + (HP after - HP before) / max HP - potion_cost * potions spent) / win_bonus
and a combat the run died in is paid minus the death penalty. HP and the
potion count are read from the public observation at the strategic decisions
around the combat. It is the plain "win the fight, keep your HP" pseudo-reward
(recursively optimal in Dietterich's sense, not hierarchically): it does not
depend on what the judge has seen, which matters where the judge has little
data (act 2 and later, other ascension levels).

The strategic value model is a training-time judge. The exported combat
policy reads the public observation only, as before.

--ascensions 0,5,10 samples runs of several ascension levels (each run's level
follows from its seed). It is a curriculum: lower levels reach the later acts
far more often, so the learner sees their fights. Results are only ever
reported from normal ascension-10 runs played by the exported policy.

--arena ROOTS.jsonl turns the first --arena-slots slots of every sampler
worker into a combat arena (nosl.vec.VecSampler): instead of whole runs they
play recorded combats (lines of {id, seed, ascension, rows, act, fight}: the
action rows that lead to the combat's first decision), each time with what
the player cannot see resampled. A run meets its act-2 boss at most once, and
most runs never get there; the arena is how the learner sees those fights
often. It is an injected scenario: its episodes are counted on their own
(`arena_*` in the log) and never as runs.

End types: a run that ends as anything but an original victory or death has
its samples in the current rollout dropped and is counted.
"""
from __future__ import annotations

import argparse
import collections
import json
import time
from pathlib import Path

import numpy as np
import torch

from . import native
from .model import grow, load, save
from .returns import gae, store_observation
from .contexts import Describer
from .readiness import readiness_override
from .strategy_q import G_IN_COMBAT, load_readiness, load_table, outcome_target, q_values, table_override, to_device

G_HP, G_MAX_HP, G_POTION_COUNT = 7, 8, 23  # nosl/observation.h GlobalColumn
from .vec import REFORK, STATUS_NAMES, VecSampler


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True)
    p.add_argument("--init", required=True, help="checkpoint the combat learner starts from")
    p.add_argument("--base", required=True, help="frozen base policy outside combat")
    p.add_argument("--strategy", required=True, help="frozen strategy_q checkpoint: the rule outside combat and the judge")
    p.add_argument("--margin", type=float, default=0.0)
    p.add_argument("--table", default="", help="the strategic rule's value table (nosl.strategy_q table_override)")
    p.add_argument("--table-delta", type=float, default=0.0)
    p.add_argument("--readiness", default="", help="the strategic rule's readiness rule (nosl.readiness), applied after the table")
    p.add_argument("--readiness-lambda", type=float, default=0.07)
    p.add_argument("--readiness-scope", default="cards,upgrade")
    p.add_argument("--readiness-acts", default="0")
    p.add_argument("--readiness-no-table", action="store_true")
    p.add_argument("--readiness-chain", action="store_true")
    p.add_argument("--readiness-shop-min", type=float, default=0.02)
    p.add_argument("--readiness-shop-add-only", action="store_true")
    p.add_argument("--death-penalty", type=float, default=0.0, help="subtracted from the pay of a combat the run died in")
    p.add_argument("--reward", choices=("judge", "combat"), default="judge")
    p.add_argument("--win-bonus", type=float, default=3.0, help="--reward combat: surviving, in units of the whole HP bar")
    p.add_argument("--potion-cost", type=float, default=0.06, help="--reward combat: one potion, in units of the whole HP bar")
    p.add_argument("--gradient-check", type=int, default=0, help="a diagnostic: sample this many iterations without "
                   "updating anything and report how alike their policy gradients are (see gradient_check below); "
                   "nothing is saved")
    p.add_argument("--reward-table", default="", help="--reward combat: win bonus and potion cost by ascension, act and floor "
                   "(tools/nosl/hp_exchange.py) instead of the two numbers above; fights it has no bucket for keep them")
    p.add_argument("--decisions", type=int, default=5_000_000, help="sampler decisions in this invocation, combat or not")
    p.add_argument("--time-limit-s", type=float, default=0)
    p.add_argument("--workers", type=int, default=6)
    p.add_argument("--runs-per-worker", type=int, default=80)
    p.add_argument("--rollout", type=int, default=64)
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--minibatch", type=int, default=4096)
    p.add_argument("--token-budget", type=int, default=160000)
    p.add_argument("--lr", type=float, default=1e-4)
    p.add_argument("--target-kl", type=float, default=0.01)
    p.add_argument("--lam", type=float, default=0.95)
    p.add_argument("--clip", type=float, default=0.2)
    p.add_argument("--entropy", type=float, default=0.003)
    p.add_argument("--value-coef", type=float, default=0.5)
    p.add_argument("--grad-norm", type=float, default=0.5)
    p.add_argument("--critic-warmup", type=int, default=20, help="first iterations update only the critic")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--seed-prefix", default="NR3T")
    p.add_argument("--ascensions", default="", help="a mixed-ascension training curriculum, e.g. 0,5,10 (default: ascension 10 only)")
    p.add_argument("--arena", default="", help="recorded combat roots for the arena slots (see the module text)")
    p.add_argument("--arena-slots", type=int, default=0, help="arena slots per sampler worker")
    p.add_argument("--arena-bank", type=int, default=48, help="roots each worker keeps parked")
    p.add_argument("--max-tokens", type=int, default=192)
    p.add_argument("--max-actions", type=int, default=160)
    p.add_argument("--save-every", type=int, default=20)
    p.add_argument("--keep-every", type=int, default=0)
    p.add_argument("--resume", default="")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args(argv)

    if len(args.seed_prefix) + 6 + 9 > 23:  # prefix + learner seed and iteration + the sampler's worker/generation/counter
        raise SystemExit("--seed-prefix is too long for a 23-character run seed")
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    torch.manual_seed(args.seed)
    device = torch.device(args.device)
    base, base_blob = load(args.base, device)
    judge, judge_blob = load(args.strategy, device)
    if base_blob.get("actor_filter") != 1 or judge_blob.get("kind") != "strategy_q":
        raise SystemExit("--base must be an actor-filter-1 policy and --strategy a strategy_q checkpoint")
    base.eval().requires_grad_(False)
    judge.eval().requires_grad_(False)
    table = load_table(args.table) if args.table else None
    readiness = load_readiness(args)
    price_table = json.loads(Path(args.reward_table).read_text()) if args.reward_table else None

    def prices(g):
        """(win bonus, potion cost) of --reward combat for the combats that just ended, from their rows of the global
        table: the flags' two numbers, or with --reward-table the ones of the ascension, act and floor the fight was on
        (tools/nosl/hp_exchange.py); a boss room has its own pair."""
        bonus = np.full(len(g), args.win_bonus, np.float32)
        potion = np.full(len(g), args.potion_cost, np.float32)
        if price_table is not None:
            for i, (ascension, act, floor, room) in enumerate(g[:, [3, 4, 6, 26]]):
                if room == 3:
                    bonus[i], potion[i] = price_table["boss"]
                    continue
                for bucket_act, first, last, b, c in price_table["ascension"].get(str(int(ascension)), ()):
                    if bucket_act == act and first <= floor <= last:
                        bonus[i], potion[i] = b, c
                        break
        return bonus, potion

    describer = Describer() if table or readiness is not None else None
    start_iteration, total_decisions = 0, 0
    model, blob = load(args.resume or args.init, device)
    if args.resume:
        start_iteration, total_decisions = blob.get("iteration", 0), blob.get("decisions", 0)
    # The critic gets its own encoder (a copy to begin with): its target is new, the policy's features are not.
    model = grow(model, model.features, "", True)
    model.train()
    checked = []    # --gradient-check: one policy gradient per iteration
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    value_optimizer = torch.optim.Adam(model.value_net.parameters(), lr=3e-4, eps=1e-5)
    if args.resume and "optimizer" in blob:
        optimizer.load_state_dict(blob["optimizer"])
    described = ("strategic value after - before, per combat" if args.reward == "judge"
                 else "(win bonus + HP change / max HP - potion cost * potions spent) / win bonus, per survived combat")
    (out / "config.json").write_text(json.dumps({**vars(args), "reward_description": described
                                                  + (", minus a death penalty" if args.death_penalty else ""),
                                                  "obs_schema": native.SCHEMA_VERSION,
                                                  "parameters": sum(x.numel() for x in model.parameters())}, indent=1))
    log = (out / "train.jsonl").open("a", encoding="utf-8")
    arena, root_of = None, {}
    if args.arena and args.arena_slots:
        roots = [json.loads(line) for line in open(args.arena, encoding="utf-8") if line.strip()]
        # another draw of the roots every invocation: a worker parks --arena-bank of them
        order = np.random.default_rng([args.seed, start_iteration]).permutation(len(roots))
        roots = [roots[i] for i in order[:args.workers * args.arena_bank]]
        root_of = {root["id"]: root for root in roots}
        arena = {"roots": [{k: root[k] for k in ("id", "seed", "ascension", "rows")} for root in roots],
                 "slots": args.arena_slots, "bank": args.arena_bank, "seed": args.seed * 100000 + start_iteration}
    vec = VecSampler(args.workers, args.runs_per_worker, split="train", max_tokens=args.max_tokens,
                     max_actions=args.max_actions, seed_prefix=f"{args.seed_prefix}{args.seed % 10}{start_iteration:05d}",
                     actor_filter=1, failure_log=out / "failures.jsonl",
                     ascensions=[int(x) for x in args.ascensions.split(",")] if args.ascensions else None, arena=arena,
                     timeout=600.0 if arena else 120.0)
    N, L = vec.runs, args.rollout
    A = vec.arrays
    store = {"global": np.zeros((L, N, native.GLOBAL_WIDTH), np.int32),
             "tokens": np.zeros((L, N, args.max_tokens, native.TOKEN_WIDTH), np.int32),
             "actions": np.zeros((L, N, args.max_actions, native.ACTION_WIDTH), np.int32),
             "counts": np.zeros((L, N, 2), np.int32)}
    choice = np.zeros((L, N), np.int64)
    logp = np.zeros((L, N), np.float32)
    value = np.zeros((L + 1, N), np.float32)
    reward = np.zeros((L, N), np.float32)
    done = np.zeros((L, N), np.float32)
    valid = np.zeros((L, N), bool)
    before = np.zeros(N, np.float32)          # strategic value at the last strategic decision of each slot's run
    was_combat = np.zeros(N, bool)            # the slot's previous decision was a combat decision of the same run
    hp_before = np.zeros((N, 3), np.float32)  # HP, max HP and potion count at that strategic decision (--reward combat)

    def observe(t=None):
        tokens_used = max(1, int(A["counts"][:, 0].max()))
        actions_used = max(1, int(A["counts"][:, 1].max()))
        arrays = (A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"])
        if t is not None:
            for key, array in zip(("global", "tokens", "actions", "counts"), arrays):
                store_observation(store[key][t], array)
        return to_device(arrays, device)

    @torch.no_grad()
    def act(batch):
        """Sampled combat actions, the strategic rule's actions, and the strategic value of the latter."""
        outside = batch[0][:, G_IN_COMBAT] == 0
        n = len(outside)
        action = torch.zeros(n, dtype=torch.long, device=device)
        log_prob = torch.zeros(n, device=device)
        values = torch.zeros(n, device=device)
        # The learner is asked only where it decides. Decisions outside combat are the wide ones (the map screen
        # has a hundred tokens and more); leaving them out keeps the learner's batch at the width of a combat.
        inside = torch.nonzero(~outside).flatten()
        if len(inside):
            counts = batch[3][inside]
            tokens_used, actions_used = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
            logits, inside_values = model(batch[0][inside], batch[1][inside, :tokens_used], batch[2][inside, :actions_used], counts)
            logits = logits.float()
            logits = torch.where(torch.isinf(logits).all(dim=1, keepdim=True), torch.zeros_like(logits), logits)
            dist = torch.distributions.Categorical(logits=logits)
            sampled = dist.sample()
            action[inside], log_prob[inside], values[inside] = sampled, dist.log_prob(sampled), inside_values.float()
        strategic = torch.zeros(n, device=device)
        # The strategic rule, in two groups of similar width for the same reason.
        wide = batch[3][:, 0] > 48
        for rows in (torch.nonzero(outside & ~wide).flatten(), torch.nonzero(outside & wide).flatten()):
            if not len(rows):
                continue
            counts = batch[3][rows]
            tokens_used, actions_used = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
            sub = [batch[0][rows], batch[1][rows, :tokens_used], batch[2][rows, :actions_used], counts]
            base_logits = base(*sub)[0].float()
            base_logits = torch.where(torch.isinf(base_logits).all(dim=1, keepdim=True), torch.zeros_like(base_logits), base_logits)
            q = q_values(judge, sub)
            q = torch.where(torch.isinf(q).all(dim=1, keepdim=True), torch.zeros_like(q), q)
            base_choice, best = base_logits.argmax(dim=1), q.argmax(dim=1)
            q_base = q.gather(1, base_choice.unsqueeze(1)).squeeze(1)
            q_best = q.gather(1, best.unsqueeze(1)).squeeze(1)
            take = (q_best - q_base) > args.margin
            action[rows] = torch.where(take, best, base_choice)
            strategic[rows] = torch.where(take, q_best, q_base)
        return action, log_prob, values, outside, strategic

    def forward(batch):
        logits, values = model(*batch)
        return logits.float(), values.float()

    try:
        vec.reset()
        was_combat[:] = False
        began = time.time()
        decisions_this_call = 0
        iteration = start_iteration
        recent_runs = collections.deque(maxlen=3000)
        recent_combats = collections.deque(maxlen=20000)   # (paid reward, died) of finished combats
        recent_arena = collections.deque(maxlen=6000)      # (root's act, root's fight kind, won) of finished arena combats
        in_arena = vec.arena_mask
        fresh = in_arena.copy()                            # the slot stands at a new root whose starting HP is not noted yet
        episode_start = np.zeros(N, np.int64)

        def arena_end(slot, won):
            root = root_of.get(vec.seed_of[slot]) or {}
            recent_arena.append((root.get("act", -1), root.get("fight", "?"), bool(won)))
        while decisions_this_call < args.decisions and not (args.time_limit_s and time.time() - began > args.time_limit_s):
            iteration += 1
            t0 = time.time()
            valid[:] = False
            reward[:] = 0
            done[:] = 0
            episode_start[:] = 0
            finished, failures = [], collections.Counter()
            for t in range(L + 1):
                batch = observe(t if t < L else None)
                action, log_prob, values, outside, strategic = act(batch)
                outside_np = outside.cpu().numpy()
                strategic_np = strategic.cpu().numpy()
                running = vec.running()
                # A combat that ended with the run still going: its last action is paid now.
                closed = was_combat & outside_np & running
                shown = A["global"][:, [G_HP, G_MAX_HP, G_POTION_COUNT]].astype(np.float32)
                if t > 0 and closed.any():
                    if args.reward == "judge":
                        paid = strategic_np[closed] - before[closed]
                    else:
                        was, now = hp_before[closed], shown[closed]
                        bonus, potion = prices(A["global"][closed])
                        paid = (bonus + (now[:, 0] - was[:, 0]) / np.maximum(1, was[:, 1])
                                - potion * np.maximum(0, was[:, 2] - now[:, 2])) / bonus
                    reward[t - 1, closed] = paid
                    done[t - 1, closed] = 1
                    recent_combats.extend((float(x), False) for x in paid[~in_arena[closed]])
                    for slot in np.flatnonzero(closed & in_arena):
                        arena_end(slot, True)
                before[outside_np & running] = strategic_np[outside_np & running]
                hp_before[outside_np & running] = shown[outside_np & running]
                # An arena slot at a new root: the combat starts here, so this is its starting HP.
                started_root = fresh & running & ~outside_np
                hp_before[started_root] = shown[started_root]
                fresh[started_root] = False
                value[t] = values.cpu().numpy()
                if t == L:
                    # The rollout stops here: an open combat bootstraps from the critic, a closed one was just paid.
                    was_combat = was_combat & ~outside_np
                    break
                in_combat = ~outside_np & running
                valid[t] = in_combat
                choice[t] = action.cpu().numpy()
                if table:   # the rule's answer outside combat, then the table's over it
                    table_override(table, args.table_delta, describer, A, np.flatnonzero(outside_np & running), choice[t])
                if readiness is not None:
                    readiness_override(readiness, table or {}, args.table_delta, describer, A,
                                       np.flatnonzero(outside_np & running), choice[t])
                logp[t] = log_prob.cpu().numpy()
                seeds_before = list(vec.seed_of)
                sent = np.where(running, choice[t], -1).astype(np.int32)
                leave = in_arena & outside_np & running    # an arena combat was won: the slot goes to a new root
                sent[leave] = REFORK
                fresh[leave] = True
                results = vec.step(sent)
                was_combat = in_combat.copy()
                for row in results:
                    slot = row["slot"]
                    if slot < 0 or row["seed"] != seeds_before[slot]:
                        failures["start_" + STATUS_NAMES[row["status"]]] += 1
                        continue
                    if row["status"] in (native.VICTORY, native.DEATH):
                        if row.get("arena"):
                            root = root_of.get(row["seed"]) or {}
                            recent_arena.append((root.get("act", -1), root.get("fight", "?"), row["status"] == native.VICTORY))
                            fresh[slot] = True
                        else:
                            finished.append(row)
                            recent_runs.append(row)
                        if in_combat[slot]:
                            if args.reward == "judge":
                                paid = outcome_target(row["floor"], row["act"], row["status"]) - before[slot]
                            else:  # a victory ends in the last combat: paid as a survived fight at its starting HP
                                paid = 1.0 if row["status"] == native.VICTORY else 0.0
                            if row["status"] == native.DEATH:
                                paid -= args.death_penalty
                            reward[t, slot] = paid
                            done[t, slot] = 1
                            if not row.get("arena"):
                                recent_combats.append((float(paid), row["status"] == native.DEATH))
                    else:
                        valid[episode_start[slot]:t + 1, slot] = False
                        failures[STATUS_NAMES[row["status"]]] += 1
                        done[t, slot] = 1
                        if row.get("arena"):
                            fresh[slot] = True
                    was_combat[slot] = False
                    before[slot] = 0
                    episode_start[slot] = t + 1
            sample_s = time.time() - t0

            advantage = gae(reward, value, done, 1.0, args.lam)
            returns = advantage + value[:L]
            t1 = time.time()
            index = np.flatnonzero(valid.reshape(-1))
            flat = {k: v.reshape(L * N, *v.shape[2:]) for k, v in store.items()}
            f_choice = torch.from_numpy(choice.reshape(-1)).to(device)
            f_logp = torch.from_numpy(logp.reshape(-1)).to(device)
            valid_adv = advantage.reshape(-1)[index]
            f_adv = torch.from_numpy((advantage.reshape(-1) - valid_adv.mean()) / (valid_adv.std() + 1e-8)).to(device)
            f_ret = torch.from_numpy(returns.reshape(-1)).to(device)
            stats = collections.defaultdict(list)
            rng = np.random.default_rng(args.seed * 1000003 + iteration)
            token_counts = flat["counts"][index, 0].astype(np.float64)
            warmup = iteration - start_iteration <= args.critic_warmup
            if args.gradient_check:
                # The gradient of the first PPO step on this batch: every sample once, ratio 1, the normalised
                # advantages the update would use. Nothing is changed; the vector is kept for the comparison.
                actor = [q for name, q in model.named_parameters() if not name.startswith("value_net.")]
                total = [torch.zeros_like(q) for q in actor]
                order = index[np.argsort(token_counts + rng.random(len(index)))]
                begin = 0
                while begin < len(order):
                    widest = int(flat["counts"][order[min(len(order), begin + args.minibatch) - 1], 0])
                    size = max(64, min(args.minibatch, args.token_budget // max(1, widest)))
                    rows = np.sort(order[begin:begin + size])
                    begin += size
                    counts = flat["counts"][rows]
                    tokens_used, actions_used = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
                    batch = to_device((flat["global"][rows], flat["tokens"][rows, :tokens_used],
                                       flat["actions"][rows, :actions_used], counts), device)
                    where = torch.from_numpy(rows).to(device)
                    logits, _ = forward(batch)
                    new_logp = torch.distributions.Categorical(logits=logits).log_prob(f_choice[where])
                    loss = -(new_logp * f_adv[where]).sum() / len(index)
                    grads = torch.autograd.grad(loss, actor, allow_unused=True)
                    for acc, gr in zip(total, grads):
                        if gr is not None:
                            acc += gr
                checked.append(torch.cat([t.reshape(-1) for t in total]).float().cpu())
                print(json.dumps({"gradient_check": len(checked), "combat_samples": int(len(index)),
                                  "norm": round(float(checked[-1].norm()), 6)}), flush=True)
                if len(checked) >= args.gradient_check:
                    G = torch.stack(checked).double()
                    unit = G / G.norm(dim=1, keepdim=True)
                    cos = unit @ unit.T
                    k = len(checked)
                    pairwise = float((cos.sum() - cos.diagonal().sum()) / (k * (k - 1)))
                    a, b = G[: k // 2].mean(0), G[k // 2:].mean(0)
                    halves = float(a @ b / (a.norm() * b.norm()))
                    print(json.dumps({"gradient_check": "done", "iterations": k, "samples_per_iteration": int(len(index)),
                                      "pairwise_cosine": pairwise, "cosine_of_half_means": halves,
                                      "signal_share_of_one_gradient": pairwise,
                                      "iterations_for_cosine_one_half": None if pairwise <= 0 else (1 - pairwise) / pairwise,
                                      "norm_mean": float(G.norm(dim=1).mean()), "norm_of_mean": float(G.mean(0).norm())}), flush=True)
                    return
                continue
            for _ in range(args.epochs):
                order = index[np.argsort(token_counts + rng.random(len(index)))]
                batches, begin = [], 0
                while begin < len(order):
                    widest = int(flat["counts"][order[min(len(order), begin + args.minibatch) - 1], 0])
                    size = max(64, min(args.minibatch, args.token_budget // max(1, widest)))
                    batches.append(order[begin:begin + size])
                    begin += size
                for which in rng.permutation(len(batches)):
                    rows = np.sort(batches[which])
                    if len(rows) < 2:
                        continue
                    counts = flat["counts"][rows]
                    tokens_used, actions_used = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
                    batch = to_device((flat["global"][rows], flat["tokens"][rows, :tokens_used],
                                       flat["actions"][rows, :actions_used], counts), device)
                    where = torch.from_numpy(rows).to(device)
                    logits, values = forward(batch)
                    dist = torch.distributions.Categorical(logits=logits)
                    new_logp = dist.log_prob(f_choice[where])
                    ratio = torch.exp(new_logp - f_logp[where])
                    adv = f_adv[where]
                    policy_loss = -torch.min(ratio * adv, torch.clamp(ratio, 1 - args.clip, 1 + args.clip) * adv).mean()
                    value_loss = 0.5 * (values - f_ret[where]).pow(2).mean()
                    entropy = dist.entropy().mean()
                    if warmup:
                        value_optimizer.zero_grad(set_to_none=True)
                        optimizer.zero_grad(set_to_none=True)
                        value_loss.backward()
                        value_optimizer.step()
                    else:
                        loss = policy_loss + args.value_coef * value_loss - args.entropy * entropy
                        optimizer.zero_grad(set_to_none=True)
                        loss.backward()
                        torch.nn.utils.clip_grad_norm_(model.parameters(), args.grad_norm)
                        optimizer.step()
                    stats["policy_loss"].append(policy_loss.item())
                    stats["value_loss"].append(value_loss.item())
                    stats["entropy"].append(entropy.item())
                    stats["approx_kl"].append((f_logp[where] - new_logp).mean().item())
                    stats["clip_fraction"].append(((ratio - 1).abs() > args.clip).float().mean().item())
            update_s = time.time() - t1
            rate = optimizer.param_groups[0]["lr"]
            if args.target_kl > 0 and stats["approx_kl"] and not warmup:
                kl = float(np.mean(stats["approx_kl"]))
                new_rate = rate / 1.5 if kl > 2 * args.target_kl else rate * 1.5 if kl < args.target_kl / 2 else rate
                for group in optimizer.param_groups:
                    group["lr"] = min(args.lr, max(args.lr / 30, new_rate))
            decisions_this_call += L * N
            total_decisions += L * N
            v_flat, r_flat = value[:L].reshape(-1)[index], returns.reshape(-1)[index]
            row = {"iteration": iteration, "decisions": total_decisions, "wall_s": round(time.time() - began, 1),
                   "sample_s": round(sample_s, 2), "update_s": round(update_s, 2),
                   "decisions_per_s": round(L * N / (sample_s + update_s)), "combat_samples": int(len(index)),
                   "warmup": warmup, "lr": rate, "episodes": len(finished),
                   "recent_floor_mean": round(float(np.mean([r["floor"] for r in recent_runs])), 2) if recent_runs else None,
                   "recent_act2_plus": round(float(np.mean([r["act"] >= 1 for r in recent_runs])), 4) if recent_runs else None,
                   "victories": sum(r["status"] == native.VICTORY for r in finished),
                   "recent_by_ascension": {str(a): {"runs": len(rs), "act2_plus": round(float(np.mean([r["act"] >= 1 for r in rs])), 4),
                                                    "act3_plus": round(float(np.mean([r["act"] >= 2 for r in rs])), 4),
                                                    "victories": sum(r["status"] == native.VICTORY for r in rs)}
                                           for a in sorted({r.get("ascension", 10) for r in recent_runs})
                                           for rs in [[r for r in recent_runs if r.get("ascension", 10) == a]]},
                   "arena_recent": {f"act{act + 1} {fight}": {"n": len(won), "win": round(float(np.mean(won)), 4)}
                                    for act, fight in sorted({(a, f) for a, f, _ in recent_arena})
                                    for won in [[w for a, f, w in recent_arena if (a, f) == (act, fight)]]},
                   "arena_samples": int(valid[:, in_arena].sum()),
                   "recent_combat_reward": round(float(np.mean([x for x, _ in recent_combats])), 5) if recent_combats else None,
                   "recent_combat_death": round(float(np.mean([d for _, d in recent_combats])), 5) if recent_combats else None,
                   "failures": dict(failures), "hangs": vec.hangs,
                   "explained_variance": round(float(1 - np.var(r_flat - v_flat) / (np.var(r_flat) + 1e-8)), 3),
                   **{k: round(float(np.mean(v)), 5) for k, v in stats.items()}}
            log.write(json.dumps(row) + "\n")
            log.flush()
            print(json.dumps(row), flush=True)
            extra = {"optimizer": optimizer.state_dict(), "iteration": iteration, "decisions": total_decisions, "actor_filter": 1}
            if iteration % args.save_every == 0:
                save(model, out / "checkpoint.pt", **extra)
            if args.keep_every and iteration % args.keep_every == 0:
                save(model, out / f"checkpoint-{iteration:05d}.pt", iteration=iteration, decisions=total_decisions, actor_filter=1)
        save(model, out / "checkpoint.pt", optimizer=optimizer.state_dict(), iteration=iteration, decisions=total_decisions,
             actor_filter=1)
    finally:
        vec.close()
        log.close()


if __name__ == "__main__":
    main()
