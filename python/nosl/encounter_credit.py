"""Offline matched whole-run versus encounter returns for a combat-only actor.

The deployed router reads only public observations. Encounter boundaries,
actual HP transitions and reward records are training data, never actor input.
"""
from __future__ import annotations

import collections
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch

from . import native
from .branch_credit import buffers, inputs
from .episodic_strategy import START_SHA, digest, mc_returns, models, public_row, RoutedPolicy, update
from .model import save
from .returns import RunRewards, reward_state
from .vec import check_split

END_TURN = 3
HP_SCALE = .2 / 70
TURN_COST = .005
VALID_ENDS = ('survived', 'death', 'final_victory')


def combat_models(checkpoint, device):
    outside, combat, blob = models(checkpoint, device)
    return outside, combat, RoutedPolicy(combat, outside), blob


def collect(policy, seeds, device, *, sample, credit, deadline, keep_public=True, max_steps=30000):
    if credit not in ('C', 'R'):
        raise ValueError('expected registered credit C or R')
    for seed in seeds:
        check_split('train', seed)
        if sample and 'EVAL' in seed:
            raise ValueError('combat training refuses evaluation seeds')
    env = native.Batch(len(seeds), 256, 256, max_steps)
    env.set_filter(1)
    data = buffers(len(seeds), 256, 256)
    for slot, seed in enumerate(seeds):
        if env.reset(slot, seed, 10) != native.RUNNING:
            raise RuntimeError('normal run reset failed: ' + seed)
    env.step(np.full(len(seeds), -1, np.int32), *data.values())
    histories, samples, whole_rewards, local_rewards, transitions, encounters = (
        [[] for _ in seeds] for _ in range(6))
    active = [None] * len(seeds)
    routes = [collections.Counter() for _ in seeds]
    rewarder = RunRewards(len(seeds), gamma=1, boss_damage=.3)
    began = time.monotonic()
    while np.any(data['status'] == native.RUNNING) and time.monotonic() < deadline:
        live = np.flatnonzero(data['status'] == native.RUNNING)
        before = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
        hp_before = data['global'][:, native.G_HP].copy()
        inside = data['global'][:, 11].copy() > 0
        with torch.no_grad():
            logits, values = policy(*inputs(data, live, device))
        chosen = logits.float().argmax(-1)
        selected = torch.nonzero(torch.from_numpy(inside[live]).to(device)).flatten()
        logps = torch.zeros(len(live), device=device)
        if sample and len(selected):
            dist = torch.distributions.Categorical(logits=logits[selected].float())
            choices = dist.sample()
            chosen[selected] = choices
            logps[selected] = dist.log_prob(choices)
        choices = np.full(len(seeds), -1, np.int32)
        kinds = np.zeros(len(seeds), np.int32)
        selected_np = chosen.cpu().numpy()
        value_np, logp_np = values.float().cpu().numpy(), logps.float().cpu().numpy()
        for j, slot in enumerate(live):
            action = int(selected_np[j])
            if not 0 <= action < data['counts'][slot, 1]:
                raise ValueError('combat actor selected outside public legal table')
            choices[slot] = action
            kinds[slot] = data['actions'][slot, action, 0]
            at = len(histories[slot])
            if inside[slot] and active[slot] is None:
                segment = {'start': at, 'end': None, 'kind': 'unfinished',
                    'hp_start': int(hp_before[slot]), 'hp_end': None,
                    'act': int(data['global'][slot, 4]), 'floor': int(data['global'][slot, 6]),
                    'encounter': int(data['global'][slot, 25])}
                encounters[slot].append(segment)
                active[slot] = len(encounters[slot]) - 1
            histories[slot].append(action)
            routes[slot]['combat' if inside[slot] else 'outside'] += 1
            if keep_public and inside[slot] and data['counts'][slot, 1] > 1:
                samples[slot].append({'public': public_row(data, slot), 'action': action,
                    'logp': float(logp_np[j]), 'value': float(value_np[j]), 'time': at,
                    'encounter': active[slot]})
        env.step(choices, *data.values())
        finished = [{'slot': int(slot), 'seed': seeds[slot], 'status': int(data['status'][slot]),
                     **env.run_info(int(slot))} for slot in live if data['status'][slot] != native.RUNNING]
        after = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
        whole, _ = rewarder.step(before, after, finished, seeds)
        info_by_slot = {r['slot']: r for r in finished}
        for slot in live:
            hp_after = int(info_by_slot[slot]['hp'] if slot in info_by_slot else data['global'][slot, native.G_HP])
            status = int(data['status'][slot])
            reward = 0.0
            if inside[slot]:
                reward = HP_SCALE * (hp_after - int(hp_before[slot])) - TURN_COST * (kinds[slot] == END_TURN)
                closed = status != native.RUNNING or not data['global'][slot, 11]
                if closed:
                    kind = ('death' if status == native.DEATH else 'final_victory' if status == native.VICTORY
                            else 'survived' if status == native.RUNNING else 'fault')
                    bonus = -1.0 if kind == 'death' else 1.0 if kind in ('survived', 'final_victory') else 0.0
                    reward += bonus
                    segment = encounters[slot][active[slot]]
                    segment.update(end=len(histories[slot]), kind=kind, hp_end=hp_after)
                    active[slot] = None
            transitions[slot].append([int(inside[slot]), int(kinds[slot]), int(hp_before[slot]), hp_after])
            whole_rewards[slot].append(float(whole[slot]))
            local_rewards[slot].append(float(reward))
    observations, labels, logps, values, returns, outcomes = [], [], [], [], [], []
    for slot, seed in enumerate(seeds):
        status = int(data['status'][slot])
        good = status in (native.DEATH, native.VICTORY)
        expected = None
        if good:
            if any(s['kind'] not in VALID_ENDS or s['end'] is None for s in encounters[slot]):
                raise ValueError('true whole terminal has an unresolved combat segment')
            expected = mc_returns(whole_rewards[slot]) if credit == 'C' else np.zeros(len(histories[slot]), np.float64)
            if credit == 'R':
                for segment in encounters[slot]:
                    start, end = segment['start'], segment['end']
                    expected[start:end] = mc_returns(local_rewards[slot][start:end])
            for entry in samples[slot]:
                observations.append(entry['public']);labels.append(entry['action'])
                logps.append(entry['logp']);values.append(entry['value']);returns.append(expected[entry['time']])
        row = {'seed': seed, 'status': status, 'end': native.STATUS_NAMES[status], **env.run_info(slot),
            'ascension': 10, 'actor_filter': 1, 'action_rows': histories[slot], 'routing': dict(routes[slot]),
            'mc_qualified': good, 'combat_rows': len(samples[slot]), 'encounters': encounters[slot],
            'credit_steps': transitions[slot], 'whole_rewards': whole_rewards[slot],
            'learning_times': [s['time'] for s in samples[slot]],
            'learning_returns': [float(np.float32(expected[s['time']])) for s in samples[slot]] if good else None,
            'reward_sum': float(sum(whole_rewards[slot])) if good else None}
        if not good:
            row['error'] = 'batch deadline' if status == native.RUNNING else env.last_error(slot)
        outcomes.append(row)
    return {'public': observations, 'action': np.asarray(labels), 'logp': np.asarray(logps, np.float32),
        'value': np.asarray(values, np.float32), 'return': np.asarray(returns, np.float32),
        'outcomes': outcomes, 'samples': samples, 'local_rewards': local_rewards,
        'whole_rewards': whole_rewards, 'decisions': sum(map(len, histories)), 'seconds': time.monotonic() - began}


def train(checkpoint, out, device, seed, credit):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    torch.manual_seed(seed);rng = np.random.default_rng(seed)
    outside, combat, policy, _ = combat_models(checkpoint, device)
    original = digest(outside);optimizer = torch.optim.Adam(combat.parameters(), lr=.000133)
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'checkpoint_sha256': START_SHA, 'seed': seed, 'credit': credit, 'updates': 64, 'episodes_per_update': 32,
        'seconds_limit': 900, 'max_steps': 30000, 'device': device, 'precision': 'float32', 'actor_filter': 1,
        'prefix': 'NQ4ZTRAIN', 'outside_parameter_sha256': original, 'outside_policy': 'frozen greedy X1',
        'gamma': 1, 'hp_scale': HP_SCALE, 'end_turn_cost': TURN_COST, 'survived_bonus': 1, 'death_bonus': -1,
        'lr': .000133, 'clip': .2, 'entropy': .05, 'value_coef': .5, 'epochs': 2, 'minibatch': 256, 'grad_norm': .5}
    (out/'config.json').write_text(json.dumps(config, indent=2)+'\n')
    began = time.monotonic();deadline = began + 900
    attempts = faults = decisions = combat_rows = optimizer_steps = updates = 0
    with (out/'train.jsonl').open('w') as log, (out/'episodes.jsonl').open('w') as episodes:
        for iteration in range(64):
            if time.monotonic() >= deadline:
                break
            seeds = [f'NQ4ZTRAIN{seed}{iteration:04d}{slot:02d}' for slot in range(32)]
            data = collect(policy, seeds, device, sample=True, credit=credit, deadline=deadline)
            for row in data['outcomes']:
                episodes.write(json.dumps(row)+'\n')
            episodes.flush()
            attempts += len(seeds);faults += sum(not r['mc_qualified'] for r in data['outcomes'])
            decisions += data['decisions']
            if faults/max(1, attempts) > .01 or time.monotonic() >= deadline:
                break
            stats = update(combat, optimizer, data, device, rng)
            assert digest(outside) == original
            updates += 1;combat_rows += stats['rows'];optimizer_steps += stats['optimizer_steps']
            row = {'iteration': updates, 'attempts': attempts, 'faults': faults, 'decisions': decisions,
                'wall_seconds': time.monotonic()-began, 'sample_seconds': data['seconds'],
                'floor_mean': float(np.mean([r['floor'] for r in data['outcomes'] if r['mc_qualified']])),
                'victories': sum(r['end']=='victory' for r in data['outcomes']), **stats}
            log.write(json.dumps(row)+'\n');log.flush();print(json.dumps(row), flush=True)
    assert digest(outside) == original
    status = 'complete' if updates == 64 and attempts == 2048 else 'incomplete'
    save(combat, out/'checkpoint.pt', actor_filter=1, outside_checkpoint_sha256=START_SHA,
        learning_scope='combat only; must deploy with frozen outside X1', iteration=updates,
        decisions=decisions, combat_rows=combat_rows, credit=credit)
    summary = {'status': status, 'updates': updates, 'attempts': attempts, 'faults': faults,
        'decisions': decisions, 'combat_rows': combat_rows, 'optimizer_steps': optimizer_steps,
        'wall_seconds': time.monotonic()-began, 'outside_parameters_unchanged': True,
        'outside_parameter_sha256': original, 'combat_parameter_sha256': digest(combat)}
    (out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    return summary
