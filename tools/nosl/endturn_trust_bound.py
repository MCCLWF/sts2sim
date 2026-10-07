#!/usr/bin/env python3
"""Identical-data public head learning with explicit projected-Adam KL limits."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch
from torch import nn
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import FEATURES, START_SHA, binary_metrics, scope, write
from nosl.endturn_action import (DATA_SHA, EPOCHS, FORMAT, MINIBATCH, initial, load,
                                public_features, source, train as unbounded_train)
from nosl.endturn_credit import HEAD_SHA, anchor_kl
from nosl.episodic_strategy import digest
from nosl.model import load as generic_load
from nosl.teacher_distill import sha
from endturn_visit_credit import sealed_inputs
from verify_endturn_credit import same

START = ROOT / 'artifacts/nosl-q4r/start.pt'
HEADS = ROOT / 'artifacts/nosl-q4ac/qualification'
PRIOR = ROOT / 'artifacts/nosl-q4ae/learning'
CACHE = PRIOR / 'cache'
ARMS = ('U1', 'T1', 'U3', 'T3')
LIMITS, WORKING_LIMITS = (.02, .2), (.0199, .1999)


def each_kl(score, reference):
    probability = reference.sigmoid()
    return probability * (F.logsigmoid(reference) - F.logsigmoid(score)) + (
        1 - probability) * (F.logsigmoid(-reference) - F.logsigmoid(-score))


def kl_stats(model, gate, action, reference, anchor):
    with torch.no_grad():
        score = model.stop(gate, action)
        divergence = each_kl(score[anchor], reference[anchor])
        mean, maximum = float(divergence.mean()), float(divergence.max())
    if not torch.isfinite(score).all() or not np.isfinite([mean, maximum]).all():
        raise ValueError('nonfinite projected head')
    return mean, maximum


def bounded_train(out, seed, deadline):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    meta = json.loads((CACHE / 'cache.json').read_text())
    assert sha(CACHE / 'cache.npz') == meta['cache_sha256']
    with np.load(CACHE / 'cache.npz', allow_pickle=False) as raw:data = {k: raw[k] for k in raw.files}
    model = initial(START, HEADS, seed, True);frozen = digest(model.base)
    gate, action, target, weight = (torch.from_numpy(data[k]) for k in ('gate', 'action', 'target', 'weight'))
    anchor = np.flatnonzero(data['complete'] & ~data['held'])
    rows = np.flatnonzero(data['complete'] & data['direction'] & ~data['held'])
    held = np.flatnonzero(data['complete'] & data['direction'] & data['held'])
    assert (len(anchor), len(rows), len(held)) == (352, 177, 49)
    with torch.no_grad():reference = model.stop(gate, action).detach().clone()
    config = {'seed': seed, 'action_context': True, 'epochs': EPOCHS, 'minibatch': MINIBATCH,
        'lr': .0003, 'grad_norm': 1., 'kl_coefficient': .1, 'train_direction_rows': len(rows),
        'held_direction_rows': len(held), 'anchor_rows': len(anchor),
        'loss': 'mean_margin_weighted_direction_BCE_plus_all_complete_train_initial_Bernoulli_KL',
        'cache_sha256': meta['cache_sha256'], 'data_sha256': DATA_SHA,
        'initial_head_sha256': HEAD_SHA[seed], 'device': 'cpu', 'precision': 'float32',
        'learned_parameters': sum(p.numel() for p in model.learned_parameters()),
        'initial_head_digest': digest(model.head), 'initial_residual_digest': digest(model.residual),
        'projection_mean_limit': LIMITS[0], 'projection_max_limit': LIMITS[1],
        'working_mean_limit': WORKING_LIMITS[0], 'working_max_limit': WORKING_LIMITS[1],
        'max_halvings': 20, 'optimizer': 'projected_Adam_moments_kept_after_proposal',
        'constraint_distribution': 'all_352_complete_train_roots_initial_to_updated_Bernoulli'}
    write(out / 'config.json', config)
    parameters = model.learned_parameters();initial_parameters = [p.detach().clone() for p in parameters]
    optimizer = torch.optim.Adam(parameters, lr=.0003);rng = np.random.default_rng(seed)
    began = time.monotonic();updates = projected = rejected = 0;initial_record = None
    try:
        with (out / 'steps.jsonl').open('w') as steps, (out / 'training.jsonl').open('w') as epochs:
            for epoch in range(EPOCHS + 1):
                if epoch:
                    order = rng.permutation(rows)
                    for first in range(0, len(order), MINIBATCH):
                        if time.monotonic() >= deadline:raise TimeoutError('registered head-learning deadline')
                        selected = order[first:first + MINIBATCH]
                        score = model.stop(gate[selected], action[selected])
                        bce = (F.binary_cross_entropy_with_logits(score, target[selected], reduction='none') * weight[selected]).mean()
                        loss = bce + .1 * anchor_kl(model.stop(gate[anchor], action[anchor]), reference[anchor])
                        if not torch.isfinite(loss):raise ValueError('nonfinite projected learning objective')
                        optimizer.zero_grad(set_to_none=True);loss.backward()
                        nn.utils.clip_grad_norm_(parameters, 1., error_if_nonfinite=True)
                        before = [p.detach().clone() for p in parameters]
                        optimizer.step();proposal = [p.detach().clone() for p in parameters]
                        proposed = kl_stats(model, gate, action, reference, anchor)
                        alpha = 1.;accepted = proposed;halvings = 0
                        while accepted[0] > WORKING_LIMITS[0] or accepted[1] > WORKING_LIMITS[1]:
                            halvings += 1;alpha = 2. ** -halvings if halvings <= 20 else 0.
                            with torch.no_grad():
                                for p, old, new in zip(parameters, before, proposal):
                                    p.copy_(old if not alpha else old + alpha * (new - old))
                            accepted = kl_stats(model, gate, action, reference, anchor)
                            if not alpha:break
                        assert accepted[0] <= LIMITS[0] and accepted[1] <= LIMITS[1]
                        updates += 1;projected += alpha < 1;rejected += alpha == 0
                        actual_change = sum(float((p.detach() - old).square().sum()) for p, old in zip(parameters, before))
                        record = {'epoch': epoch, 'update': updates, 'alpha': alpha, 'halvings': halvings,
                            'proposal_mean_kl': proposed[0], 'proposal_max_kl': proposed[1],
                            'accepted_mean_kl': accepted[0], 'accepted_max_kl': accepted[1],
                            'actual_parameter_delta_squared': actual_change, 'minibatch_indices': selected.tolist()}
                        steps.write(json.dumps(record) + '\n')
                with torch.no_grad():
                    score = model.stop(gate, action);divergence = each_kl(score[anchor], reference[anchor])
                    record = {'epoch': epoch, 'updates': updates, 'seconds': time.monotonic() - began,
                        'train': binary_metrics(score[rows], target[rows]), 'held': binary_metrics(score[held], target[held]),
                        'all_train_anchor_kl': float(divergence.mean()), 'max_train_anchor_kl': float(divergence.max()),
                        'weighted_bce': float((F.binary_cross_entropy_with_logits(score[rows], target[rows], reduction='none') * weight[rows]).mean()),
                        'projected_steps': projected, 'rejected_steps': rejected}
                assert record['all_train_anchor_kl'] <= LIMITS[0] and record['max_train_anchor_kl'] <= LIMITS[1]
                epochs.write(json.dumps(record) + '\n')
                if initial_record is None:initial_record = record
                if epoch in (0, EPOCHS):print(json.dumps({'arm': 'T' + str(seed), **record}), flush=True)
    except Exception as error:
        write(out / 'error.json', {'error': type(error).__name__ + ': ' + str(error), 'updates': updates})
        torch.save({'model': model.state_dict(), 'optimizer': optimizer.state_dict()}, out / 'failed-state.pt');raise
    assert digest(model.base) == frozen == meta['frozen_parameter_sha256']
    assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    blob = {'format': FORMAT, 'schema': 1, 'actor_filter': 1, 'baseline_checkpoint_sha256': START_SHA,
        'feature_names': list(FEATURES), 'action_feature_width': 128, 'initial_seed': seed,
        'action_context': True, 'initial_head_sha256': HEAD_SHA[seed], 'frozen_parameter_sha256': frozen,
        'head_state': model.head.state_dict(), 'residual_state': model.residual.state_dict(),
        'training': config, 'final_epoch': EPOCHS}
    torch.save(blob, out / 'checkpoint.pt');torch.save(optimizer.state_dict(), out / 'optimizer.pt')
    reloaded, _ = load(out / 'checkpoint.pt', START, HEADS);assert digest(reloaded) == digest(model)
    changed = any(not torch.equal(p.detach(), old) for p, old in zip(parameters, initial_parameters))
    result = {'completed': True, 'updates': updates, 'seconds': time.monotonic() - began,
        'projected_steps': projected, 'rejected_steps': rejected, 'final': record,
        'checkpoint_sha256': sha(out / 'checkpoint.pt'), 'optimizer_sha256': sha(out / 'optimizer.pt'),
        'material_learning': changed and initial_record['weighted_bce'] - record['weighted_bce'] >= .001,
        'all_accepted_steps_within_limits': True, 'frozen_parameter_sha256': frozen}
    write(out / 'training-result.json', result);return result


def contract(learning, out):
    learning, out = Path(learning), Path(out);out.mkdir(parents=True, exist_ok=False)
    models = {arm: load(learning / arm / 'checkpoint.pt', START, HEADS)[0] for arm in ARMS}
    references = {seed: source(START, HEADS, seed) for seed in (1, 3)}
    frozen = references[1].base.state_dict()
    cases = inactive = outside = 0
    teacher = ROOT / 'artifacts/nosl-q4o/cloud/labels/public-tables.npz'
    assert sha(teacher) == '8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29'
    for path, prefixes in ((ROOT / 'artifacts/nosl-q4ad/data/public-roots.npz', ('',)), (teacher, ('', 'anchor_'))):
        with np.load(path, allow_pickle=False) as raw:
            for prefix in prefixes:
                tables = {key: raw[prefix + key] for key in KEYS}
                for first in range(0, len(tables['counts']), 128):
                    args = inputs(tables, np.arange(first, min(first + 128, len(tables['counts']))), 'cpu')
                    with torch.no_grad():
                        old, value = references[1].base(*args)
                        active, _, other = scope(args[0], args[2], args[3])
                        best = old.masked_fill(~other, float('-inf')).argmax(1)
                        for model in models.values():
                            logits, now_value = model(*args)
                            assert torch.equal(now_value, value)
                            assert torch.equal(logits[~active], old[~active])
                            assert torch.isneginf(logits[torch.isneginf(old)]).all()
                            assert torch.equal(logits.masked_fill(~other, float('-inf')).argmax(1)[active], best[active])
                    cases += len(args[0]);inactive += int((~active).sum());outside += int((args[0][:, 11] == 0).sum())
    for model in models.values():
        assert all(torch.equal(t, model.base.state_dict()[name]) for name, t in frozen.items())
        assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    rejected = [];blob = torch.load(learning / 'T1/checkpoint.pt', map_location='cpu', weights_only=False)
    for reason in ('format', 'schema', 'baseline', 'nonfinite'):
        bad = copy.deepcopy(blob)
        if reason == 'format':bad['format'] = 'old-endturn'
        elif reason == 'schema':bad['schema'] = 2
        elif reason == 'baseline':bad['baseline_checkpoint_sha256'] = 'wrong'
        else:bad['residual_state']['2.bias'][0] = float('nan')
        path = out / (reason + '-invalid.pt');torch.save(bad, path)
        try:load(path, START, HEADS)
        except ValueError:rejected.append(reason)
        else:raise AssertionError('invalid constrained checkpoint accepted')
    try:generic_load(learning / 'T1/checkpoint.pt', 'cpu')
    except KeyError:rejected.append('generic_loader')
    else:raise AssertionError('generic loader dropped constrained head')
    assert (cases, inactive, outside) == (3209, 611, 368)
    result = {'passed': True, 'public_states': cases, 'inactive_states': inactive, 'outside_states': outside,
        'actor_input_keys': list(KEYS), 'base_tensors_values_masks_and_nonend_argmax_unchanged': True,
        'invalid_checkpoints_rejected': rejected, 'frozen_parameter_sha256': digest(references[1].base)}
    write(out / 'result.json', result);print(json.dumps(result), flush=True);return result


def learning(out):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    began = time.monotonic();deadline = began + 180;verified = sealed_inputs()
    previous = ROOT / 'artifacts/nosl-q4af'
    assert sha(previous / 'manifest.json') == '986a1fa05031cc364d5b05a67ab26f47e6985746eea260a0d9d136e7d8fe871b'
    manifest = json.loads((previous / 'manifest.json').read_text())
    for record in manifest['files']:
        file = previous / record['path'];assert file.stat().st_size == record['bytes'] and sha(file) == record['sha256']
    verified['nosl-q4af'] = len(manifest['files'])
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'source_sha256': sha(__file__), 'device': 'cpu', 'precision': 'float32', 'seconds': 180,
        'torch_version': torch.__version__, 'sealed_inputs_verified': verified,
        'cached_input_sha256': sha(CACHE / 'cache.npz'), 'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'),
        'arms': list(ARMS), 'limits': list(LIMITS), 'working_limits': list(WORKING_LIMITS), 'cloud_used': False}
    write(out / 'learning-config.json', config)
    results, reproductions = {}, {}
    for seed in (1, 3):
        arm = 'U' + str(seed)
        results[arm] = unbounded_train(START, HEADS, CACHE, out / arm, seed, True, deadline)
        old_blob = torch.load(PRIOR / f'C{seed}/checkpoint.pt', map_location='cpu', weights_only=False)
        new_blob = torch.load(out / arm / 'checkpoint.pt', map_location='cpu', weights_only=False)
        same(old_blob, new_blob)
        same(torch.load(PRIOR / f'C{seed}/optimizer.pt', map_location='cpu', weights_only=False),
             torch.load(out / arm / 'optimizer.pt', map_location='cpu', weights_only=False))
        reproductions[arm] = {'all_checkpoint_tensors_and_metadata_identical': True,
            'all_optimizer_tensors_and_steps_identical': True, 'prior_checkpoint_sha256': sha(PRIOR / f'C{seed}/checkpoint.pt')}
        results['T' + str(seed)] = bounded_train(out / ('T' + str(seed)), seed, deadline)
    qualification = contract(out, out / 'contract')
    assert time.monotonic() < deadline
    result = {'training': results, 'control_reproductions': reproductions, 'contract_passed': qualification['passed'],
        'registered_seconds': 180, 'seconds': time.monotonic() - began,
        'evaluation_gate': all(results[arm]['completed'] and results[arm]['material_learning'] and
            results[arm]['all_accepted_steps_within_limits'] for arm in ('T1', 'T3'))}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True)
    return 0 if result['evaluation_gate'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__);parser.add_argument('--out', required=True)
    args = parser.parse_args();torch.set_num_threads(1);return learning(args.out)


if __name__ == '__main__':sys.exit(main())
