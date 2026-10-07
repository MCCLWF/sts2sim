"""Offline binary EndTurn comparisons to real terminals, then public learning.

Only this collector owns forked environments. Its future rewards and seed
metadata are targets/diagnostics, never additional actor inputs.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch
import torch.nn.functional as F

from . import native
from .branch_credit import KEYS, buffers, inputs
from .endturn import (EPOCHS, FEATURES, FORMAT, MINIBATCH, START_SHA,
                      binary_metrics, gate_features, load_gate, scope, write)
from .episodic_strategy import digest, public_row
from .returns import reward_state, RunRewards
from .teacher_distill import padded_stack, sha
from .vec import check_split

HEAD_SHA = {1: '563fa7fc93d029e26fe395e4f136f13bf176c62987ed8b1705cbb9d70fd209ad',
            3: '51be7593886af4e6cc05285270a8112cdfa130d84ad369bd1f06681251c65239'}
REWARD_COLUMNS = ('floor0', 'hp0', 'boss0', 'enemy_hp0', 'enemy_max_hp0',
                  'floor1', 'hp1', 'boss1', 'enemy_hp1', 'enemy_max_hp1', 'status1', 'reward')
TIE_EPSILON = 1e-5


def policies(start, heads, device):
    result = {}
    for seed in (1, 3):
        path = Path(heads) / f'E{seed}/checkpoint.pt'
        if sha(path) != HEAD_SHA[seed]:
            raise ValueError('credit collection requires the frozen Q4-ac head')
        result[seed] = load_gate(path, start, device)[0]
    return result


def split(seed):
    import hashlib
    return int.from_bytes(hashlib.sha256(seed.encode()).digest()[:8], 'little') % 5 == 0


def infer(models, data, slots, source, device):
    result = {}
    for seed in (1, 3):
        selected = np.asarray([slot for slot in slots if source[slot] == seed], np.int64)
        if not len(selected):continue
        batch = inputs(data, selected, device)
        with torch.no_grad():logits, _ = models[seed](*batch)
        if not torch.isfinite(logits).any(1).all():
            raise ValueError('live branch has no finite legal action')
        active, end, other = scope(batch[0], batch[2], batch[3])
        choice = logits.argmax(1).cpu().numpy()
        non_end = logits.masked_fill(~other, float('-inf')).argmax(1).cpu().numpy()
        end_index = end.long().argmax(1).cpu().numpy()
        for j, slot in enumerate(selected):
            result[int(slot)] = {'choice': int(choice[j]), 'active': bool(active[j]),
                                 'end_action': int(end_index[j]), 'continue_action': int(non_end[j])}
    return result


def packed(rows):
    return padded_stack(rows)


def public_digest(row):
    """Hash only used public rows; transport padding is not an actor fact."""
    digest = hashlib.sha256()
    for key in KEYS:
        value = row[key]
        if key in ('tokens', 'actions'):
            value = value[:int(row['counts'][0 if key == 'tokens' else 1])]
        value = np.ascontiguousarray(value, dtype=np.int32)
        digest.update(key.encode())
        digest.update(np.asarray(value.shape, dtype=np.int64).tobytes())
        digest.update(value.tobytes())
    return digest.hexdigest()


def pair_target(outcomes):
    """A fault or unfinished native branch never receives a terminal label."""
    qualified = all(row['status'] in (native.VICTORY, native.DEATH) for row in outcomes)
    delta = outcomes[0]['return'] - outcomes[1]['return'] if qualified else None
    direction = delta is not None and abs(delta) > TIE_EPSILON
    return {'complete': qualified, 'delta': delta,
            'label': int(delta > 0) if direction else None,
            'weight': min(abs(delta) / .02, 5.) if direction else 0.}


def same_slot(data, slot, expected):
    return all(np.array_equal(data[key][slot], expected[key]) for key in data)


def collect(start, heads, out, *, prefix, attempts, seconds, block=32,
            max_steps=30000, controls=False, device='cpu'):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    models = policies(start, heads, device)
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'prefix': prefix, 'attempts': attempts, 'seconds': seconds, 'block': block,
              'max_steps': max_steps, 'controls': controls, 'device': device, 'precision': 'float32',
              'head_sha256': HEAD_SHA, 'reward_columns': list(REWARD_COLUMNS),
              'runtime_source_sha256': {str(p): sha(p) for p in
                  (Path(__file__), Path('tools/nosl/check_endturn_credit.py'))},
              'tie_epsilon': TIE_EPSILON, 'scope': 'one-world fixed-continuation whole-run comparisons'}
    write(out / 'config.json', config)
    (out / 'branches.jsonl').touch()
    began = time.monotonic();deadline = began + seconds
    root_data, complete, roots, attempts_rows = [], [], [], []
    control_checks = {'fork_initial_equal': 0, 'parent_unchanged_ticks': 0, 'same_action_sibling_ticks': 0}
    for first in range(0, attempts, block):
        seeds = [f'{prefix}{i:06d}' for i in range(first, min(first + block, attempts))]
        for seed in seeds:check_split('train', seed)
        if time.monotonic() >= deadline:
            attempts_rows.extend({'seed': seed, 'root': False, 'end': 'not_started'} for seed in seeds)
            continue
        n = len(seeds);copies = 4 if controls else 2;total = n * (1 + copies)
        batch = native.Batch(total, 256, 256, max_steps);batch.set_filter(1);batch.set_active(n)
        data = buffers(total, 256, 256)
        source = {i: 1 if (first + i) % 2 == 0 else 3 for i in range(n)}
        history = [[] for _ in range(n)]
        for i, seed in enumerate(seeds):batch.reset(i, seed, 10)
        batch.step(np.full(total, -1, np.int32), *data.values())
        waiting, found = set(range(n)), {}
        while waiting and time.monotonic() < deadline:
            live = [i for i in sorted(waiting) if data['status'][i] == native.RUNNING]
            for i in sorted(waiting - set(live)):
                status = int(data['status'][i])
                row = {'seed': seeds[i], 'root': False, 'status': status, 'end': native.STATUS_NAMES[status],
                       **batch.run_info(i), 'action_rows': history[i]}
                if status not in (native.DEATH, native.VICTORY):row['error'] = batch.last_error(i)
                attempts_rows.append(row);waiting.remove(i)
            if not live:break
            picked = infer(models, data, live, source, device)
            choices = np.full(total, -1, np.int32)
            for i in live:
                index = first + i;minimum = 4 if (index // 4) % 2 == 0 else 10
                stop = (index // 2) % 2 == 0;current = picked[i]
                selected_stop = int(data['actions'][i, current['choice'], 0]) == 3
                if current['active'] and data['global'][i, native.G_TOTAL_FLOOR] >= minimum and stop == selected_stop:
                    found[i] = {'seed': seeds[i], 'index': index, 'source': source[i], 'held': split(seeds[i]),
                                'prefix': history[i].copy(), 'floor': int(data['global'][i, native.G_TOTAL_FLOOR]),
                                'desired_stop': stop, 'minimum_floor': minimum, **current,
                                'public': public_row(data, i),
                                'frozen_slot': {k: v[i].copy() for k, v in data.items()}}
                    waiting.remove(i)
                else:
                    choices[i] = current['choice'];history[i].append(current['choice'])
            batch.step(choices, *data.values())
        for i in sorted(waiting):
            status = int(data['status'][i])
            attempts_rows.append({'seed': seeds[i], 'root': False, 'status': status,
                                  'end': 'collection_time_limit' if status == native.RUNNING else native.STATUS_NAMES[status],
                                  **batch.run_info(i), 'action_rows': history[i]})
        batch.set_active(total)
        branches, traces = {}, {}
        for i, root in found.items():
            for copy in range(copies):
                slot = n + copies * i + copy
                assert batch.fork(i, slot) == native.RUNNING
                branches[slot] = (i, copy % 2);source[slot] = root['source']
                traces[slot] = {'actions': [], 'rewards': []}
        batch.step(np.full(total, -1, np.int32), *data.values())
        for slot, (i, _) in branches.items():
            assert same_slot(data, slot, found[i]['frozen_slot']), 'initial fork differs'
            control_checks['fork_initial_equal'] += 1
        rewarder = RunRewards(total, gamma=1, boss_damage=.3)
        initial = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
        for slot in branches:
            if initial.inside_boss[slot]:
                reference = max(float(initial.enemy_max_hp[slot]), float(initial.enemy_hp[slot]), 1.)
                rewarder.boss_ref[slot] = reference
                rewarder.boss_paid[slot] = np.clip(1 - initial.enemy_hp[slot] / reference, 0, 1)
        first_action = True
        while any(data['status'][slot] == native.RUNNING for slot in branches) and time.monotonic() < deadline:
            live = [slot for slot in sorted(branches) if data['status'][slot] == native.RUNNING]
            if first_action:
                choices = np.full(total, -1, np.int32)
                for slot in live:
                    i, branch = branches[slot]
                    choices[slot] = found[i]['end_action' if branch == 0 else 'continue_action']
            else:
                picked = infer(models, data, live, source, device)
                choices = np.full(total, -1, np.int32)
                for slot in live:choices[slot] = picked[slot]['choice']
            before = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
            before_hp = data['global'][:, native.G_HP].copy()
            for slot in live:traces[slot]['actions'].append(int(choices[slot]))
            batch.step(choices, *data.values())
            after = reward_state(data['global'], data['tokens'], data['counts'], .2, 70)
            finished = [{'slot': slot, 'seed': seeds[branches[slot][0]], 'status': int(data['status'][slot]),
                         **batch.run_info(slot)} for slot in live if data['status'][slot] != native.RUNNING]
            slot_seeds = [''] * total
            for slot, (i, _) in branches.items():slot_seeds[slot] = seeds[i]
            rewards, _ = rewarder.step(before, after, finished, slot_seeds)
            for slot in live:
                traces[slot]['rewards'].append([int(before.floor[slot]), int(before_hp[slot]),
                    int(before.inside_boss[slot]), int(before.enemy_hp[slot]), int(before.enemy_max_hp[slot]),
                    int(after.floor[slot]), int(data['global'][slot, native.G_HP]),
                    int(after.inside_boss[slot]), int(after.enemy_hp[slot]), int(after.enemy_max_hp[slot]),
                    int(data['status'][slot]), float(rewards[slot])])
            if first_action:
                for slot in branches:traces[slot]['first_after'] = {k: v[slot].copy() for k, v in data.items()}
            for i, root in found.items():
                assert same_slot(data, i, root['frozen_slot']), 'fork mutated a parked parent'
                control_checks['parent_unchanged_ticks'] += 1
                if controls:
                    left = n + copies * i
                    for branch in (0, 1):
                        expected = {k: v[left + branch].copy() for k, v in data.items()}
                        assert same_slot(data, left + branch + 2, expected), 'same-action sibling differs'
                        assert traces[left + branch]['actions'] == traces[left + branch + 2]['actions']
                        assert traces[left + branch]['rewards'] == traces[left + branch + 2]['rewards']
                        control_checks['same_action_sibling_ticks'] += 1
            first_action = False
        for i, root in found.items():
            outcomes = []
            for branch in (0, 1):
                slot = n + copies * i + branch;status = int(data['status'][slot])
                good = status in (native.VICTORY, native.DEATH)
                row = {'seed': root['seed'], 'branch': 'end' if branch == 0 else 'continue',
                       'ascension': 10, 'actor_filter': 1, 'status': status, 'end': native.STATUS_NAMES[status],
                       **batch.run_info(slot), 'mc_qualified': good,
                       'action_rows': root['prefix'] + traces[slot]['actions'],
                       'branch_actions': traces[slot]['actions'], 'reward_steps': traces[slot]['rewards'],
                       'return': sum(r[-1] for r in traces[slot]['rewards']) if good else None}
                if not good:row['error'] = 'collection deadline' if status == native.RUNNING else batch.last_error(slot)
                outcomes.append(row)
                with (out / 'branches.jsonl').open('a') as stream:stream.write(json.dumps(row) + '\n')
            target = pair_target(outcomes)
            qualified = target['complete']
            record = {k: v for k, v in root.items() if k not in ('public', 'frozen_slot')}
            record.update(**target, public_sha256=public_digest(root['public']),
                          outcome_returns=[r['return'] for r in outcomes])
            roots.append(record);root_data.append(root['public']);complete.append(qualified)
            attempts_rows.append({'seed': root['seed'], 'root': True, 'complete': qualified})
            if controls:
                np.savez_compressed(out / (root['seed'] + '-fork.npz'), **{
                    **{'root_' + k: v for k, v in root['frozen_slot'].items()},
                    **{label + '_' + k: v for label, slot in (('end', n + copies * i), ('continue', n + copies * i + 1))
                       for k, v in traces[slot]['first_after'].items()}})
        for name, records in (('roots.jsonl', roots), ('attempts.jsonl', attempts_rows)):
            with (out / name).open('w') as stream:
                for row in records:stream.write(json.dumps(row) + '\n')
        summary = {'attempts': len(attempts_rows), 'roots': len(roots), 'complete_roots': sum(complete),
                   'incomplete_roots': len(roots) - sum(complete), 'seconds': time.monotonic() - began,
                   'control_checks': control_checks}
        write(out / 'summary.json', summary);print(json.dumps(summary), flush=True)
        del batch
    # Persist every scheduled seed even when a block never started.
    for name, records in (('roots.jsonl', roots), ('attempts.jsonl', attempts_rows)):
        with (out / name).open('w') as stream:
            for row in records:stream.write(json.dumps(row) + '\n')
    data = packed(root_data) if root_data else {k: v for k, v in buffers(0, 1, 1).items() if k in KEYS}
    data.update(complete=np.asarray(complete, bool), delta=np.asarray([r['delta'] if r['delta'] is not None else np.nan for r in roots], np.float64),
                held=np.asarray([r['held'] for r in roots], bool), source=np.asarray([r['source'] for r in roots], np.int32),
                index=np.asarray([r['index'] for r in roots], np.int32))
    np.savez_compressed(out / 'public-roots.npz', **data)
    summary = {'attempts': len(attempts_rows), 'roots': len(roots), 'complete_roots': sum(complete),
               'incomplete_roots': len(roots) - sum(complete), 'seconds': time.monotonic() - began,
               'control_checks': control_checks, 'packing_completed': True,
               'public_roots_sha256': sha(out / 'public-roots.npz')}
    write(out / 'summary.json', summary)
    return summary


def prepare(start, heads, directory, audit_path, out, deadline):
    """Cache only features derived by the frozen actor from four public tables."""
    directory, out = Path(directory), Path(out)
    audit = json.loads(Path(audit_path).read_text())
    if not audit['independent_verification'] or not audit['data_gate_passed']:
        raise ValueError('actual-terminal dataset has not passed the fixed data gate')
    out.mkdir(parents=True, exist_ok=False)
    model = policies(start, heads, 'cpu')[1]
    frozen = digest(model.base)
    roots = [json.loads(s) for s in (directory / 'roots.jsonl').read_text().splitlines()]
    with np.load(directory / 'public-roots.npz', allow_pickle=False) as source:
        tables = {key: source[key] for key in KEYS}
        assert len(roots) == len(tables['counts']) == audit['roots']
        for i, root in enumerate(roots):
            assert public_digest({key: tables[key][i] for key in KEYS}) == root['public_sha256']
        features = []
        for first in range(0, len(roots), MINIBATCH):
            if time.monotonic() >= deadline:raise TimeoutError('registered cache/training deadline')
            ids = np.arange(first, min(first + MINIBATCH, len(roots)))
            batch = tuple(t.long() for t in inputs(tables, ids, 'cpu'))
            with torch.no_grad():
                run, _ = model.base.encode(batch[0], batch[1], batch[3])
                feature = gate_features(run, *batch)
                assert torch.isfinite(feature).all() and scope(batch[0], batch[2], batch[3])[0].all()
            features.append(feature.numpy())
    cache = {'features': np.concatenate(features),
             'complete': np.asarray([r['complete'] for r in roots], bool),
             'held': np.asarray([r['held'] for r in roots], bool),
             'direction': np.asarray([r['label'] is not None for r in roots], bool),
             'target': np.asarray([r['label'] if r['label'] is not None else 0 for r in roots], np.float32),
             'weight': np.asarray([r['weight'] for r in roots], np.float32)}
    np.savez_compressed(out / 'cache.npz', **cache)
    metadata = {'dataset_sha256': sha(directory / 'public-roots.npz'),
                'raw_roots_sha256': sha(directory / 'roots.jsonl'), 'audit_sha256': sha(audit_path),
                'cache_sha256': sha(out / 'cache.npz'), 'baseline_sha256': START_SHA,
                'frozen_parameter_sha256': frozen, 'feature_names': list(FEATURES),
                'feature_width': 146, 'actor_input_keys': list(KEYS),
                'rows': len(roots), 'direction': audit['direction']}
    write(out / 'cache.json', metadata)
    return metadata


def anchor_kl(score, initial):
    probability = initial.sigmoid()
    return (probability * (F.logsigmoid(initial) - F.logsigmoid(score)) +
            (1 - probability) * (F.logsigmoid(-initial) - F.logsigmoid(-score))).mean()


def train(start, heads, cache, out, seed, deadline):
    cache, out = Path(cache), Path(out);out.mkdir(parents=True, exist_ok=False)
    metadata = json.loads((cache / 'cache.json').read_text())
    if sha(cache / 'cache.npz') != metadata['cache_sha256']:raise ValueError('credit cache differs')
    with np.load(cache / 'cache.npz', allow_pickle=False) as raw:
        data = {key: raw[key] for key in raw.files}
    torch.manual_seed(seed)
    model = policies(start, heads, 'cpu')[seed]
    frozen = digest(model.base)
    assert frozen == metadata['frozen_parameter_sha256']
    feature = torch.from_numpy(data['features'])
    target, weight = torch.from_numpy(data['target']), torch.from_numpy(data['weight'])
    anchor = np.flatnonzero(data['complete'] & ~data['held'])
    rows = np.flatnonzero(data['complete'] & data['direction'] & ~data['held'])
    held = np.flatnonzero(data['complete'] & data['direction'] & data['held'])
    with torch.no_grad():initial = model.head(feature).squeeze(-1).detach().clone()
    optimizer = torch.optim.Adam(model.head.parameters(), lr=.0003)
    config = {'seed': seed, 'epochs': EPOCHS, 'minibatch': MINIBATCH, 'lr': .0003,
              'grad_norm': 1., 'kl_coefficient': .1,
              'loss': 'mean_margin_weighted_direction_BCE_plus_all_complete_train_initial_Bernoulli_KL',
              'anchor_rows': len(anchor), 'direction_train_rows': len(rows), 'direction_held_rows': len(held),
              'initial_head_sha256': HEAD_SHA[seed], 'baseline_sha256': START_SHA,
              'dataset_sha256': metadata['dataset_sha256'], 'cache_sha256': metadata['cache_sha256'],
              'device': 'cpu', 'precision': 'float32'}
    write(out / 'config.json', config)
    rng = np.random.default_rng(seed);began = time.monotonic();updates = 0
    try:
        for epoch in range(EPOCHS + 1):
            if epoch:
                order = rng.permutation(rows)
                for first in range(0, len(order), MINIBATCH):
                    if time.monotonic() >= deadline:raise TimeoutError('registered cache/training deadline')
                    chosen = order[first:first + MINIBATCH]
                    score = model.head(feature[chosen]).squeeze(-1)
                    bce = (F.binary_cross_entropy_with_logits(score, target[chosen], reduction='none') * weight[chosen]).mean()
                    kl = anchor_kl(model.head(feature[anchor]).squeeze(-1), initial[anchor])
                    loss = bce + .1 * kl
                    if not torch.isfinite(loss):raise ValueError('nonfinite credit learning loss')
                    optimizer.zero_grad(set_to_none=True);loss.backward()
                    torch.nn.utils.clip_grad_norm_(model.head.parameters(), 1., error_if_nonfinite=True)
                    optimizer.step();updates += 1
            with torch.no_grad():
                score = model.head(feature).squeeze(-1)
                record = {'epoch': epoch, 'updates': updates, 'seconds': time.monotonic() - began,
                          'train': binary_metrics(score[rows], target[rows]),
                          'held': binary_metrics(score[held], target[held]),
                          'all_train_anchor_kl': float(anchor_kl(score[anchor], initial[anchor])),
                          'train_weighted_bce': float((F.binary_cross_entropy_with_logits(score[rows], target[rows], reduction='none') * weight[rows]).mean())}
            with (out / 'training.jsonl').open('a') as stream:stream.write(json.dumps(record) + '\n')
            if epoch in (0, EPOCHS):print(json.dumps({'arm': f'C{seed}', **record}), flush=True)
    except Exception as error:
        write(out / 'error.json', {'error': type(error).__name__ + ': ' + str(error), 'updates': updates})
        torch.save({'head': model.head.state_dict(), 'optimizer': optimizer.state_dict()}, out / 'failed-state.pt')
        raise
    assert digest(model.base) == frozen
    assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    checkpoint = {'format': FORMAT, 'schema': 1, 'actor_filter': 1, 'features': list(FEATURES),
                  'baseline_checkpoint_sha256': START_SHA, 'frozen_parameter_sha256': frozen,
                  'head_state': model.head.state_dict(), 'training': config, 'final_epoch': EPOCHS}
    torch.save(checkpoint, out / 'checkpoint.pt');torch.save(optimizer.state_dict(), out / 'optimizer.pt')
    loaded, _ = load_gate(out / 'checkpoint.pt', start)
    assert digest(loaded.head) == digest(model.head)
    metrics = record['train']
    result = {'completed': True, 'frozen_parameters_unchanged': True,
              'frozen_parameter_sha256': frozen, 'updates': updates, 'final': record,
              'seconds': time.monotonic() - began, 'checkpoint_sha256': sha(out / 'checkpoint.pt'),
              'optimizer_sha256': sha(out / 'optimizer.pt'),
              'fitting_gate_passed': bool(metrics['balanced_accuracy'] >= .8 and metrics['recall'] >= .7 and metrics['specificity'] >= .7)}
    write(out / 'training-result.json', result)
    return result
