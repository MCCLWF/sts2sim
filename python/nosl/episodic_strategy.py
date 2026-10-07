"""Offline episodic PPO for an independent public strategic policy.

Combat always uses the frozen public X1. The only learning observations are
outside-combat decisions; targets are sums of actual rewards to true terminals.
The inference wrapper only routes on the public in-combat flag.
"""
from __future__ import annotations

import collections
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch

from . import native
from .branch_credit import buffers, inputs
from .model import load, save
from .returns import RunRewards, reward_state
from .vec import check_split

START_SHA = '6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703'
G_IN_COMBAT = 11


def digest(model):
    h = hashlib.sha256()
    for key, tensor in sorted(model.state_dict().items()):
        h.update(key.encode())
        h.update(tensor.detach().cpu().contiguous().numpy().tobytes())
    return h.hexdigest()


def models(checkpoint, device):
    if hashlib.sha256(Path(checkpoint).read_bytes()).hexdigest() != START_SHA:
        raise ValueError('episodic pilot requires the frozen official X1')
    combat, blob = load(checkpoint, device)
    if blob['actor_filter'] != 1 or combat.features != 3:
        raise ValueError('unexpected X1 public contract')
    strategic = copy.deepcopy(combat)
    combat.requires_grad_(False)
    combat.eval()
    strategic.eval()
    return combat, strategic, blob


class RoutedPolicy(torch.nn.Module):
    def __init__(self, combat, strategic):
        super().__init__()
        self.combat, self.strategic = combat, strategic
        left = {x.data_ptr() for x in combat.parameters()}
        if any(x.data_ptr() in left for x in strategic.parameters()):
            raise ValueError('combat and strategic parameters must not alias')

    def forward(self, global_, tokens, actions, counts):
        # Only public tables and an existing public flag enter either model.
        logits, values = self.combat(global_, tokens, actions, counts)
        outside = torch.nonzero(global_[:, G_IN_COMBAT] == 0).flatten()
        if len(outside):
            other, value = self.strategic(global_[outside], tokens[outside], actions[outside], counts[outside])
            logits = logits.index_copy(0, outside, other)
            values = values.index_copy(0, outside, value)
        return logits, values


def public_row(data, slot):
    t, a = map(int, data['counts'][slot])
    return {'global': data['global'][slot].copy(), 'tokens': data['tokens'][slot, :t].copy(),
            'actions': data['actions'][slot, :a].copy(), 'counts': data['counts'][slot].copy()}


def pack(rows, indices, device):
    picked = [rows[int(i)] for i in indices]
    nt = max(1, max(int(r['counts'][0]) for r in picked))
    na = max(1, max(int(r['counts'][1]) for r in picked))
    data = buffers(len(picked), nt, na)
    for j, row in enumerate(picked):
        t, a = map(int, row['counts'])
        for key in ('global', 'counts'):
            data[key][j] = row[key]
        data['tokens'][j, :t] = row['tokens']
        data['actions'][j, :a] = row['actions']
    return inputs(data, np.arange(len(picked)), device)


def mc_returns(rewards):
    """Gamma=1 actual suffix returns; no value or terminal bootstrap."""
    return np.cumsum(np.asarray(rewards, np.float64)[::-1])[::-1]


def collect(policy, seeds, device, *, sample, deadline, keep_public=True):
    """One finite on-policy batch, with no reset or parameter updates mid-run."""
    for seed in seeds:
        check_split('train', seed)
        if sample and 'EVAL' in seed:
            raise ValueError('training refuses evaluation seeds')
    batch = native.Batch(len(seeds), 256, 256, 30000)
    batch.set_filter(1)
    data = buffers(len(seeds), 256, 256)
    for slot, seed in enumerate(seeds):
        if batch.reset(slot, seed, 10) != native.RUNNING:
            raise RuntimeError('normal run reset failed: ' + seed)
    batch.step(np.full(len(seeds), -1, np.int32), *data.values())
    histories, rewards, samples = ([[] for _ in seeds] for _ in range(3))
    routes = [collections.Counter() for _ in seeds]
    rewarder = RunRewards(len(seeds), gamma=1, boss_damage=.3)
    began = time.monotonic()
    step = 0
    while np.any(data['status'] == native.RUNNING) and time.monotonic() < deadline:
        live = np.flatnonzero(data['status'] == native.RUNNING)
        before = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
        with torch.no_grad():
            logits, values = policy(*inputs(data, live, device))
        outside = data['global'][live, G_IN_COMBAT] == 0
        choice = logits.float().argmax(-1)
        selected = torch.nonzero(torch.from_numpy(outside).to(device)).flatten()
        logps = torch.zeros(len(live), device=device)
        if sample and len(selected):
            dist = torch.distributions.Categorical(logits=logits[selected].float())
            chosen = dist.sample()
            choice[selected] = chosen
            logps[selected] = dist.log_prob(chosen)
        choices = np.full(len(seeds), -1, np.int32)
        chosen_np = choice.cpu().numpy()
        value_np, logp_np = values.float().cpu().numpy(), logps.cpu().numpy()
        for j, slot in enumerate(live):
            action = int(chosen_np[j])
            if not 0 <= action < data['counts'][slot, 1]:
                raise ValueError('model selected outside the public legal table')
            choices[slot] = action
            histories[slot].append(action)
            context = 'outside' if outside[j] else 'combat'
            routes[slot][context] += 1
            if keep_public and outside[j] and data['counts'][slot, 1] > 1:
                samples[slot].append({'public': public_row(data, slot), 'action': action,
                    'logp': float(logp_np[j]), 'value': float(value_np[j]), 'time': len(rewards[slot])})
        batch.step(choices, *data.values())
        finished = [{'slot': int(slot), 'seed': seeds[slot], 'status': int(data['status'][slot]),
                     **batch.run_info(int(slot))} for slot in live if data['status'][slot] != native.RUNNING]
        after = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
        reward, _ = rewarder.step(before, after, finished, seeds)
        for slot in live:
            rewards[slot].append(float(reward[slot]))
        step += 1
    observations, labels, logps, values, returns, outcomes = [], [], [], [], [], []
    for slot, seed in enumerate(seeds):
        status = int(data['status'][slot])
        good = status in (native.VICTORY, native.DEATH)
        row = {'seed': seed, 'status': status, 'end': native.STATUS_NAMES[status],
               **batch.run_info(slot), 'ascension': 10, 'actor_filter': 1,
               'action_rows': histories[slot], 'routing': dict(routes[slot]),
               'mc_qualified': good, 'strategic_rows': len(samples[slot]),
               'reward_sum': float(sum(rewards[slot])) if good else None}
        if not good:
            row['error'] = 'batch deadline' if status == native.RUNNING else batch.last_error(slot)
        if good:
            suffix = mc_returns(rewards[slot])
            for entry in samples[slot]:
                observations.append(entry['public'])
                labels.append(entry['action']);logps.append(entry['logp']);values.append(entry['value'])
                returns.append(suffix[entry['time']])
        outcomes.append(row)
    return {'public': observations, 'action': np.asarray(labels), 'logp': np.asarray(logps, np.float32),
            'value': np.asarray(values, np.float32), 'return': np.asarray(returns, np.float32),
            'outcomes': outcomes, 'rewards': rewards, 'samples': samples,
            'decisions': sum(len(h) for h in histories), 'ticks': step, 'seconds': time.monotonic()-began}


def update(strategic, optimizer, data, device, rng):
    count = len(data['public'])
    if not count:
        raise ValueError('no trustworthy strategic training rows')
    advantage = data['return'] - data['value']
    advantage = (advantage - advantage.mean()) / (advantage.std() + 1e-8)
    stats = collections.defaultdict(list)
    for epoch in range(2):
        order = rng.permutation(count)
        for begin in range(0, count, 256):
            ix = order[begin:begin+256]
            logits, values = strategic(*pack(data['public'], ix, device))
            dist = torch.distributions.Categorical(logits=logits.float())
            chosen = torch.as_tensor(data['action'][ix], device=device)
            old = torch.as_tensor(data['logp'][ix], device=device)
            new = dist.log_prob(chosen)
            ratio = torch.exp(new-old)
            adv = torch.as_tensor(advantage[ix], device=device)
            policy_loss = -torch.minimum(ratio*adv, ratio.clamp(.8, 1.2)*adv).mean()
            value_loss = .5*(values.float()-torch.as_tensor(data['return'][ix], device=device)).square().mean()
            entropy = dist.entropy().mean()
            loss = policy_loss + .5*value_loss - .05*entropy
            if not torch.isfinite(loss):
                raise ValueError('nonfinite strategic update')
            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            norm = torch.nn.utils.clip_grad_norm_(strategic.parameters(), .5)
            if not torch.isfinite(norm):
                raise ValueError('nonfinite strategic gradient')
            optimizer.step()
            for key, val in [('policy_loss',policy_loss),('value_loss',value_loss),('entropy',entropy),
                             ('approx_kl',(old-new).mean()),('grad_norm',norm)]:
                stats[key].append(float(val.detach()))
    rate = optimizer.param_groups[0]['lr']
    kl = float(np.mean(stats['approx_kl']))
    next_rate = rate/1.5 if kl>.02 else rate*1.5 if kl<.005 else rate
    for group in optimizer.param_groups:
        group['lr'] = min(.000133, max(.000133/30, next_rate))
    return {'rows':count, 'optimizer_steps':len(stats['grad_norm']), 'lr':rate,
            **{k:float(np.mean(v)) for k,v in stats.items()}}


def train(checkpoint, out, device, seed, *, updates=128, episodes=32, seconds=1800, prefix='NQ4YTRAIN'):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    combat, strategic, blob = models(checkpoint, device)
    policy = RoutedPolicy(combat, strategic)
    original = digest(combat)
    optimizer = torch.optim.Adam(strategic.parameters(), lr=.000133)
    deadline = time.monotonic()+seconds
    began = time.monotonic()
    config = {'source_commit': subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'checkpoint_sha256':START_SHA,'seed':seed,'updates':updates,'episodes_per_update':episodes,
        'seconds_limit':seconds,'precision':'float32','device':str(device),'actor_filter':1,
        'gamma':1,'credit':'actual whole-episode suffix return','reward':'existing floor/HP(.2/70)/boss(.3)/victory',
        'epochs':2,'minibatch':256,'lr':.000133,'entropy':.05,'clip':.2,'value_coef':.5,
        'combat_policy':'frozen greedy X1','combat_parameter_sha256':original,'prefix':prefix}
    (out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
    attempts = faults = decisions = strategic_rows = optimizer_steps = finished_updates = 0
    with (out/'train.jsonl').open('w') as log, (out/'episodes.jsonl').open('w') as ends:
        for iteration in range(updates):
            if time.monotonic()>=deadline:
                break
            seeds = [f'{prefix}{seed}{iteration:04d}{i:02d}' for i in range(episodes)]
            data = collect(policy, seeds, device, sample=True, deadline=deadline)
            for row in data['outcomes']:
                ends.write(json.dumps(row)+'\n')
            ends.flush()
            attempts += len(seeds)
            failures = sum(not r['mc_qualified'] for r in data['outcomes'])
            faults += failures
            decisions += data['decisions']
            if faults/max(1,attempts)>.01 or time.monotonic()>=deadline:
                break
            stats = update(strategic, optimizer, data, device, rng)
            assert digest(combat)==original
            strategic_rows += stats['rows'];optimizer_steps += stats['optimizer_steps'];finished_updates += 1
            row = {'iteration':iteration+1,'attempts':attempts,'decisions':decisions,'faults':faults,
                   'sample_seconds':data['seconds'],'wall_seconds':time.monotonic()-began,
                   'floor_mean':float(np.mean([r['floor'] for r in data['outcomes'] if r['mc_qualified']])),
                   'victories':sum(r['end']=='victory' for r in data['outcomes']),**stats}
            log.write(json.dumps(row)+'\n');log.flush()
            print(json.dumps(row),flush=True)
    assert digest(combat)==original
    status = 'complete' if finished_updates==updates and attempts==updates*episodes else 'incomplete'
    save(strategic,out/'checkpoint.pt',actor_filter=1,combat_checkpoint_sha256=START_SHA,
         learning_scope='outside only; must deploy with frozen combat X1',iteration=finished_updates,
         decisions=decisions,strategic_rows=strategic_rows)
    summary = {'status':status,'updates':finished_updates,'attempts':attempts,'faults':faults,
               'decisions':decisions,'strategic_rows':strategic_rows,'optimizer_steps':optimizer_steps,
               'wall_seconds':time.monotonic()-began,'combat_parameters_unchanged':True,
               'combat_parameter_sha256':original,'strategic_parameter_sha256':digest(strategic)}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    return summary
