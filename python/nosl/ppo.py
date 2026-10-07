"""PPO over whole Silent runs in the native sampler.

  .venv/bin/python -m nosl.ppo --out artifacts/nosl-q2/run1 --decisions 2000000

Reward `floor-v1` (a declared training signal, not the product objective):
(floors climbed since the previous decision) / 50, plus 1 for an original
victory; death gives nothing further. Evaluation reports real outcomes.

--hp-potential C adds potential-based shaping on top of it:
gamma * phi(next) - phi(current) with phi = C * hp / max_hp and phi = 0 at
a terminal. It moves when the cost of lost HP is felt (at the hit, not at a
later death) without changing which policies are optimal for `floor-v1`.
--hp-potential-denominator D makes it phi = C * hp / D instead: with the
ratio, giving up max HP raises phi at once and the loss only shows later
(docs/plans/2026-10-04-nosl-q4d-reward-fix.md).

--boss-damage C adds a graded signal at boss fights: C times the share of
the boss fight's enemy HP removed by each action, C in total for a won boss
fight. It is not a potential: damage dealt before dying to the boss is paid
and kept, so a run that nearly kills the boss returns more than one that
barely scratches it (docs/plans/2026-10-04-nosl-q4f-steady-progress.md).

--entropy-out B is the entropy coefficient of decisions taken outside
combat (default: --entropy).

--explore-floor K keeps every legal action of a decision outside combat
above a floor while training, so none of them stops being sampled: with n
legal actions the floor is (best logit - K - log(n - 1)), which bounds the
floored actions together at exp(-K) of the best action's probability
whatever n is. The gradient passes
through the floor unchanged. A softmax policy gradient on its own scales
with the action's probability and cannot bring back an action it has stopped
sampling. Evaluation and the exported policy use the logits as they are.
What keeps a floored action from sinking without bound:
  --explore-band 0 (the first version, Q4-f arm V): a sample of a floored
    action with a negative advantage is left out of the policy loss. Biased:
    the sign of one sample's advantage is mostly noise, so floored actions are
    pushed up until they are well above the floor. Arm V degraded with it.
  --explore-band M > 0: every sample counts, and a quadratic penalty holds
    each outside-combat logit within M below its floor.

--resume with --features 2 or --critic separate on a checkpoint that lacks
them adds the new parts so that the outputs start unchanged (new input
layers at zero, the value encoder a copy of the policy's) with a fresh
optimizer.

--target-kl T adapts the learning rate to the size of the policy step: after
an iteration whose mean approximate KL is above 2T the rate is divided by
1.5, below T/2 it is multiplied by 1.5, within [--lr / 30, --lr]. With a fixed
rate the KL of one iteration grew five- to tenfold over 20M decisions in
every run so far.

--imitation-data DIR adds an auxiliary loss for the whole run: the cross
entropy of the policy on a random batch of teacher combat demonstrations
(tools/nosl/nosl_run --record-dir), times --imitation-coef, on every update
step. The demonstrations come from a search that saw the true future; the
reinforcement objective is unchanged and still decides the evaluation.

End types (plan E02): victory and death are terminals. A run that ends as
unported, error, capacity, step_limit or hang is not a terminal: its samples
in the current rollout are dropped from the update and it is counted.
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
from .model import ActorCritic, grow, load, save
from .returns import FLOOR_SCALE, G_IN_COMBAT, RunRewards, gae, reward_state, store_observation
from .vec import HANG, STATUS_NAMES, VecSampler


def to_device(arrays, device):
    return [torch.from_numpy(np.ascontiguousarray(a)).to(device, non_blocking=True) for a in arrays]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True)
    p.add_argument("--decisions", type=int, default=1_000_000, help="policy decisions to sample in this invocation")
    p.add_argument("--time-limit-s", type=float, default=0)
    p.add_argument("--workers", type=int, default=12)
    p.add_argument("--runs-per-worker", type=int, default=40)
    p.add_argument("--rollout", type=int, default=64)
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--minibatch", type=int, default=4096, help="most rows in one update batch")
    p.add_argument("--token-budget", type=int, default=160000, help="most padded tokens in one update batch")
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--target-kl", type=float, default=0.0,
                   help="adapt the learning rate to keep one iteration's approximate KL near this (0: fixed rate)")
    p.add_argument("--gamma", type=float, default=0.999)
    p.add_argument("--lam", type=float, default=0.95)
    p.add_argument("--clip", type=float, default=0.2)
    p.add_argument("--entropy", type=float, default=0.01)
    p.add_argument("--value-coef", type=float, default=0.5)
    p.add_argument("--hp-potential", type=float, default=0.0, help="coefficient C of the HP potential (0: plain floor-v1)")
    p.add_argument("--hp-potential-denominator", type=float, default=0.0,
                   help="0: phi = C * hp / max_hp; D > 0: phi = C * hp / D (a fixed scale)")
    p.add_argument("--boss-damage", type=float, default=0.0,
                   help="reward for the share of a boss fight's enemy HP an action removes (0: none)")
    p.add_argument("--entropy-out", type=float, default=None,
                   help="entropy coefficient of decisions outside combat (default: --entropy)")
    p.add_argument("--explore-floor", type=float, default=0.0,
                   help="outside combat, floored actions together keep exp(-K) of the best action's probability (0: off)")
    p.add_argument("--explore-band", type=float, default=0.0,
                   help="with --explore-floor: hold logits within this far below the floor and count every sample (0: first version)")
    p.add_argument("--imitation-data", default="", help="directory of teacher demonstration shards for the auxiliary loss")
    p.add_argument("--imitation-coef", type=float, default=0.5)
    p.add_argument("--imitation-batch", type=int, default=1024)
    p.add_argument("--grad-norm", type=float, default=0.5)
    p.add_argument("--width", type=int, default=128)
    p.add_argument("--layers", type=int, default=3)
    p.add_argument("--features", type=int, default=1, help="model input features (nosl/model.py); a resumed checkpoint keeps its own")
    p.add_argument("--power-context", action="store_true", help="pool visible powers onto their public player/enemy owner")
    p.add_argument("--card-table", default="", help="tools/nosl/nosl_card_table output, required for --features 2")
    p.add_argument("--critic", choices=("shared", "separate"), default="shared",
                   help="separate: the value has its own encoder (nosl/model.py); a resumed checkpoint keeps its own")
    p.add_argument("--ascension", type=int, default=10)
    p.add_argument("--seed", type=int, default=1, help="learner seed: initial weights and action sampling")
    p.add_argument("--seed-prefix", default="NT", help="training run seeds; never a dev/test prefix")
    p.add_argument("--max-tokens", type=int, default=192)
    p.add_argument("--max-actions", type=int, default=160)
    p.add_argument("--max-steps", type=int, default=30000)
    p.add_argument("--resume", default="")
    p.add_argument("--reset-optimizer", action="store_true",
                   help="resume weights with fresh Adam state and reseed after model construction (controlled warm-start experiments)")
    p.add_argument("--actor-filter", type=int, default=None,
                   help="actor filter version (nosl/actor_filter.h); default: the resumed checkpoint's, else 0")
    p.add_argument("--critic-warmup", type=int, default=0,
                   help="first iterations update only the value head (for a policy that was initialised by imitation)")
    p.add_argument("--save-every", type=int, default=20)
    p.add_argument("--keep-every", type=int, default=0,
                   help="also keep a copy of the checkpoint every this many iterations (checkpoint-<iteration>.pt)")
    p.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args(argv)

    if args.entropy_out is None:
        args.entropy_out = args.entropy
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    torch.manual_seed(args.seed)
    device = torch.device(args.device)
    torch.backends.cuda.matmul.allow_tf32 = True
    torch.backends.cudnn.allow_tf32 = True
    start_iteration, total_decisions = 0, 0
    upgraded = False
    if args.resume:
        model, blob = load(args.resume, device)
        start_iteration, total_decisions = blob.get("iteration", 0), blob.get("decisions", 0)
        grown = grow(model, max(args.features, model.features), args.card_table,
                     args.critic == "separate" or model.value_net is not None, args.power_context)
        model, upgraded = grown, grown is not model
    else:
        if args.features >= 2 and not args.card_table:
            raise SystemExit("--features 2 and above need --card-table")
        model = ActorCritic(args.width, args.layers, features=args.features, card_table=args.card_table,
                            critic=args.critic, power_context=args.power_context).to(device)
    if args.actor_filter is None:
        args.actor_filter = blob.get("actor_filter", 0) if args.resume else 0
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    if args.resume and "optimizer" in blob and not upgraded and not args.reset_optimizer:
        optimizer.load_state_dict(blob["optimizer"])
        for group in optimizer.param_groups:
            # The saved state carries the old rate. --lr is this invocation's, except that an adaptive
            # rate (--target-kl) goes on from where it was, never above --lr.
            group["lr"] = min(group["lr"], args.lr) if args.target_kl > 0 else args.lr
    value_optimizer = torch.optim.Adam((model.value_net or model.value).parameters(), lr=args.lr, eps=1e-5)
    if args.reset_optimizer:
        torch.manual_seed(args.seed)
    (out / "config.json").write_text(json.dumps({**vars(args), "floor_scale": FLOOR_SCALE,
                                                  "reward": ("floor-v1" if args.hp_potential == 0 else
                                                             "floor-v1+hp-potential" if args.hp_potential_denominator == 0 else
                                                             "floor-v1+hp-potential-fixed")
                                                  + ("+boss-damage" if args.boss_damage else ""),
                                                  "obs_schema": native.SCHEMA_VERSION,
                                                  "parameters": sum(x.numel() for x in model.parameters())}, indent=1))
    log = (out / "train.jsonl").open("a", encoding="utf-8")
    demos = None
    if args.imitation_data:
        from .bc import load_shards
        demos = load_shards(args.imitation_data)
        if not len(demos["label"]):
            raise ValueError("no demonstrations in " + args.imitation_data)

    vec = VecSampler(args.workers, args.runs_per_worker, split="train", ascension=args.ascension,
                     max_tokens=args.max_tokens, max_actions=args.max_actions, max_steps=args.max_steps,
                     # Each invocation gets its own seed stream: <prefix><learner seed><start iteration>.
                     seed_prefix=f"{args.seed_prefix}{args.seed % 10}{start_iteration:05d}",
                     actor_filter=args.actor_filter, failure_log=out / "failures.jsonl")
    N, L = vec.runs, args.rollout
    A = vec.arrays
    store = {
        "global": np.zeros((L, N, native.GLOBAL_WIDTH), np.int32),
        "tokens": np.zeros((L, N, args.max_tokens, native.TOKEN_WIDTH), np.int32),
        "actions": np.zeros((L, N, args.max_actions, native.ACTION_WIDTH), np.int32),
        "counts": np.zeros((L, N, 2), np.int32),
    }
    choice = np.zeros((L, N), np.int64)
    logp = np.zeros((L, N), np.float32)
    value = np.zeros((L + 1, N), np.float32)
    reward = np.zeros((L, N), np.float32)
    done = np.zeros((L, N), np.float32)
    valid = np.ones((L, N), bool)

    def observe(t=None):
        tokens_used = max(1, int(A["counts"][:, 0].max()))
        actions_used = max(1, int(A["counts"][:, 1].max()))
        arrays = (A["global"], A["tokens"][:, :tokens_used], A["actions"][:, :actions_used], A["counts"])
        if t is not None:
            for key, array in zip(("global", "tokens", "actions", "counts"), arrays):
                target = store[key][t]
                store_observation(target, array)
        return to_device(arrays, device)

    amp = device.type == "cuda"

    def forward(batch):
        with torch.autocast("cuda", dtype=torch.bfloat16, enabled=amp):
            logits, values = model(*batch)
        return logits.float(), values.float()

    def policy(batch):
        with torch.no_grad():
            return forward(batch)

    def floored(logits, global_):
        """The training-time policy: logits with --explore-floor applied, and which entries it raised."""
        if args.explore_floor <= 0:
            return logits, None, None
        legal = torch.isfinite(logits)
        outside = (global_[:, G_IN_COMBAT] == 0).unsqueeze(1)
        others = (legal.sum(dim=1, keepdim=True) - 1).clamp(min=1).float()
        floor = (logits.max(dim=1, keepdim=True).values - args.explore_floor - torch.log(others)).detach()
        finite = logits.masked_fill(~legal, 0.0)
        raised = legal & outside & (logits < floor)
        lift = torch.where(raised, floor - finite, torch.zeros_like(logits))
        # How far each outside-combat logit is below (floor - band), for the penalty of --explore-band.
        sunk = torch.relu(floor - args.explore_band - finite) * (legal & outside)
        return logits + lift.detach(), raised, sunk

    try:
        vec.reset()
        rewards = RunRewards(N, args.gamma, args.boss_damage)

        def state():
            return reward_state(A['global'], A['tokens'], A['counts'],
                                args.hp_potential, args.hp_potential_denominator)

        boss_recent = collections.deque(maxlen=2000)  # (share of the boss fight's HP removed, won) per boss fight
        episode_start = np.zeros(N, np.int64)  # first rollout step of each slot's current run
        began = time.time()
        decisions_this_call = 0
        recent = collections.deque(maxlen=2000)
        iteration = start_iteration
        while decisions_this_call < args.decisions and not (args.time_limit_s and time.time() - began > args.time_limit_s):
            iteration += 1
            t0 = time.time()
            valid[:] = True
            episode_start[:] = 0
            finished, failures = [], collections.Counter()
            boss_fights = []
            for t in range(L):
                before = state()
                batch = observe(t)
                logits, values = policy(batch)
                logits = floored(logits, batch[0])[0]
                dist = torch.distributions.Categorical(logits=logits)
                action = dist.sample()
                choice[t] = action.cpu().numpy()
                logp[t] = dist.log_prob(action).cpu().numpy()
                value[t] = values.float().cpu().numpy()
                seeds_before = list(vec.seed_of)
                results = vec.step(choice[t].astype(np.int32))
                reward[t], fights = rewards.step(before, state(), results, seeds_before)
                boss_fights.extend(fights)
                done[t] = 0
                for row in results:
                    slot = row["slot"]
                    if slot < 0 or row["seed"] != seeds_before[slot]:
                        failures["start_" + STATUS_NAMES[row["status"]]] += 1  # a run that never reached a decision
                        continue
                    done[t, slot] = 1
                    if row["status"] in (native.VICTORY, native.DEATH):
                        finished.append(row)
                        recent.append(row)
                    else:
                        valid[episode_start[slot]:t + 1, slot] = False
                        failures[STATUS_NAMES[row["status"]]] += 1
                    episode_start[slot] = t + 1
            _, values = policy(observe())
            value[L] = values.float().cpu().numpy()
            sample_s = time.time() - t0

            advantage = gae(reward, value, done, args.gamma, args.lam)
            returns = advantage + value[:L]

            t1 = time.time()
            index = np.flatnonzero(valid.reshape(-1))
            flat = {k: v.reshape(L * N, *v.shape[2:]) for k, v in store.items()}
            f_choice = torch.from_numpy(choice.reshape(-1)).to(device)
            f_logp = torch.from_numpy(logp.reshape(-1)).to(device)
            # Advantages are normalised over the rollout, not per minibatch:
            # minibatches below are grouped by observation size, so each one
            # is dominated by one kind of decision.
            valid_adv = advantage.reshape(-1)[index]
            f_adv = torch.from_numpy((advantage.reshape(-1) - valid_adv.mean()) / (valid_adv.std() + 1e-8)).to(device)
            f_ret = torch.from_numpy(returns.reshape(-1)).to(device)
            outside = flat["global"][:, G_IN_COMBAT] == 0
            adv_size = np.abs(f_adv.cpu().numpy())
            boss_recent.extend(boss_fights)
            stats = collections.defaultdict(list)
            rng = np.random.default_rng(args.seed * 1000003 + iteration)
            token_counts = flat["counts"][index, 0].astype(np.float64)
            for _ in range(args.epochs):
                # Rows sorted by token count (ties in random order) and cut
                # into batches of bounded padded size; batch order is random.
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
                    logits, raised, sunk = floored(logits, batch[0])
                    dist = torch.distributions.Categorical(logits=logits)
                    new_logp = dist.log_prob(f_choice[where])
                    ratio = torch.exp(new_logp - f_logp[where])
                    adv = f_adv[where]
                    surrogate = torch.min(ratio * adv, torch.clamp(ratio, 1 - args.clip, 1 + args.clip) * adv)
                    if raised is not None:
                        explored = raised.gather(1, f_choice[where].unsqueeze(1)).squeeze(1)
                        if args.explore_band <= 0:
                            surrogate = surrogate * (~(explored & (adv < 0))).float()
                        stats["_explored"].append((explored.float().sum().item(), int((batch[0][:, G_IN_COMBAT] == 0).sum().item())))
                    policy_loss = -surrogate.mean()
                    value_loss = 0.5 * (values - f_ret[where]).pow(2).mean()
                    entropies = dist.entropy()
                    entropy = entropies.mean()
                    out_rows = batch[0][:, G_IN_COMBAT] == 0
                    entropy_bonus = (torch.where(out_rows, args.entropy_out, args.entropy) * entropies).mean()
                    stats["_entropy_out"].append((entropies[out_rows].sum().item(), int(out_rows.sum().item())))
                    stats["_entropy_combat"].append((entropies[~out_rows].sum().item(), int((~out_rows).sum().item())))
                    imitation_loss = None
                    if demos is not None:
                        pick = np.sort(rng.choice(len(demos["label"]), size=min(args.imitation_batch, len(demos["label"])), replace=False))
                        d_counts = demos["counts"][pick]
                        d_batch = to_device((demos["global"][pick], demos["tokens"][pick, :int(d_counts[:, 0].max())],
                                             demos["actions"][pick, :int(d_counts[:, 1].max())], d_counts), device)
                        d_logits, _ = forward(d_batch)
                        d_label = torch.from_numpy(demos["label"][pick]).to(device)
                        imitation_loss = torch.nn.functional.cross_entropy(d_logits, d_label)
                        stats["imitation_loss"].append(imitation_loss.item())
                        stats["imitation_agreement"].append((d_logits.argmax(dim=1) == d_label).float().mean().item())
                    if iteration - start_iteration <= args.critic_warmup:
                        value_optimizer.zero_grad(set_to_none=True)
                        optimizer.zero_grad(set_to_none=True)
                        (args.value_coef * value_loss).backward()
                        value_optimizer.step()  # the actor and the encoder keep their weights
                    else:
                        loss = policy_loss + args.value_coef * value_loss - entropy_bonus
                        if raised is not None and args.explore_band > 0:
                            loss = loss + sunk.pow(2).sum(dim=1).mean()
                        if imitation_loss is not None:
                            loss = loss + args.imitation_coef * imitation_loss
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
            if args.target_kl > 0 and stats["approx_kl"]:
                kl = float(np.mean(stats["approx_kl"]))
                new_rate = rate / 1.5 if kl > 2 * args.target_kl else rate * 1.5 if kl < args.target_kl / 2 else rate
                new_rate = min(args.lr, max(args.lr / 30, new_rate))
                for group in optimizer.param_groups:
                    group["lr"] = new_rate

            decisions_this_call += L * N
            total_decisions += L * N
            v_flat, r_flat = value[:L].reshape(-1)[index], returns.reshape(-1)[index]
            row = {
                "iteration": iteration, "decisions": total_decisions, "wall_s": round(time.time() - began, 1),
                "sample_s": round(sample_s, 2), "update_s": round(update_s, 2),
                "decisions_per_s": round(L * N / (sample_s + update_s)), "sample_decisions_per_s": round(L * N / sample_s),
                "valid_samples": int(len(index)), "episodes": len(finished), "lr": rate,
                "floor_mean": round(float(np.mean([r["floor"] for r in finished])), 2) if finished else None,
                "floor_max": max((r["floor"] for r in finished), default=None),
                "recent_floor_mean": round(float(np.mean([r["floor"] for r in recent])), 2) if recent else None,
                "recent_act2_plus": round(float(np.mean([r["act"] >= 1 for r in recent])), 4) if recent else None,
                "victories": sum(r["status"] == native.VICTORY for r in finished),
                "failures": dict(failures), "hangs": vec.hangs,
                "explained_variance": round(float(1 - np.var(r_flat - v_flat) / (np.var(r_flat) + 1e-8)), 3),
                **{k: round(float(np.mean(v)), 5) for k, v in stats.items() if not k.startswith("_")},
                **{k[1:]: round(sum(x for x, _ in v) / max(1, sum(n for _, n in v)), 5)
                   for k, v in stats.items() if k.startswith("_")},
                "outside_share": round(float(outside[index].mean()), 4),
                "adv_abs_out": round(float(adv_size[index][outside[index]].mean()), 4) if outside[index].any() else None,
                "adv_abs_combat": round(float(adv_size[index][~outside[index]].mean()), 4) if (~outside[index]).any() else None,
                "boss_fights": len(boss_fights),
                "boss_damage_share": round(float(np.mean([s for s, _ in boss_fights])), 4) if boss_fights else None,
                "boss_wins": sum(w for _, w in boss_fights),
                "recent_boss_damage_share": round(float(np.mean([s for s, _ in boss_recent])), 4) if boss_recent else None,
                "recent_boss_win": round(float(np.mean([w for _, w in boss_recent])), 4) if boss_recent else None,
            }
            log.write(json.dumps(row) + "\n")
            log.flush()
            print(json.dumps(row), flush=True)
            if iteration % args.save_every == 0:
                save(model, out / "checkpoint.pt", optimizer=optimizer.state_dict(), iteration=iteration, decisions=total_decisions,
                     actor_filter=args.actor_filter)
            if args.keep_every and iteration % args.keep_every == 0:
                save(model, out / f"checkpoint-{iteration:05d}.pt", iteration=iteration, decisions=total_decisions,
                     actor_filter=args.actor_filter)
        save(model, out / "checkpoint.pt", optimizer=optimizer.state_dict(), iteration=iteration, decisions=total_decisions,
                     actor_filter=args.actor_filter)
    finally:
        vec.close()
        log.close()


if __name__ == "__main__":
    main()
