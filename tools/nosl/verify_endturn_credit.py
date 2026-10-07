#!/usr/bin/env python3
"""Independently audit actual-terminal labels, public cache and fixed final heads."""
import argparse
import json
from pathlib import Path
import sys

import numpy as np
import torch
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import decode, gate_features, load_gate, scope, write
from nosl.endturn_credit import HEAD_SHA, policies, public_digest, split
from nosl.episodic_strategy import START_SHA, digest
from nosl.teacher_distill import sha


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def same(a, b):
    if isinstance(a, torch.Tensor):assert torch.equal(a, b)
    elif isinstance(a, dict):
        assert a.keys() == b.keys()
        for key in a:same(a[key], b[key])
    elif isinstance(a, (tuple, list)):
        assert len(a) == len(b)
        for left, right in zip(a, b):same(left, right)
    else:assert a == b


def metrics(score, target):
    positive = target.bool();prediction = score > 0
    tp, tn = int((prediction & positive).sum()), int((~prediction & ~positive).sum())
    p, n = int(positive.sum()), int((~positive).sum())
    recall, specificity = tp / p, tn / n
    return {'rows': len(target), 'positive': p, 'true_positive': tp, 'true_negative': tn,
            'false_positive': n - tn, 'false_negative': p - tp,
            'recall': recall, 'specificity': specificity,
            'balanced_accuracy': (recall + specificity) / 2,
            'bce': float(F.binary_cross_entropy_with_logits(score, target.float()))}


def learning(start, heads, directory, trained, out):
    directory, trained = Path(directory), Path(trained)
    audit = json.loads((directory.parent / 'data-audit.json').read_text())
    summary = json.loads((trained / 'summary.json').read_text())
    metadata = json.loads((trained / 'cache/cache.json').read_text())
    roots = lines(directory / 'roots.jsonl');branches = lines(directory / 'branches.jsonl')
    assert audit['independent_verification'] and audit['data_gate_passed']
    assert sha(start) == START_SHA == metadata['baseline_sha256']
    assert sha(directory / 'roots.jsonl') == metadata['raw_roots_sha256']
    assert sha(directory / 'public-roots.npz') == metadata['dataset_sha256']
    assert sha(directory.parent / 'data-audit.json') == metadata['audit_sha256']
    assert sha(trained / 'cache/cache.npz') == metadata['cache_sha256']
    assert summary['registered_seconds'] == 180 and summary['seconds'] <= 180
    pair = {(r['seed'], r['branch']): r for r in branches}
    complete, held, direction, targets, weights = [], [], [], [], []
    for root in roots:
        end, continuing = pair[(root['seed'], 'end')], pair[(root['seed'], 'continue')]
        qualified = all(r['end'] in ('victory', 'death') for r in (end, continuing))
        delta = sum(r[-1] for r in end['reward_steps']) - sum(r[-1] for r in continuing['reward_steps']) if qualified else None
        directional = qualified and abs(delta) > 1e-5
        target = int(delta > 0) if directional else 0
        weight = min(abs(delta) / .02, 5.) if directional else 0.
        assert root['complete'] == qualified and root['delta'] == delta
        assert root['label'] == (target if directional else None) and root['weight'] == weight
        complete.append(qualified);held.append(split(root['seed']))
        direction.append(directional);targets.append(target);weights.append(weight)
    expected = {'complete': np.asarray(complete, bool), 'held': np.asarray(held, bool),
                'direction': np.asarray(direction, bool), 'target': np.asarray(targets, np.float32),
                'weight': np.asarray(weights, np.float32)}
    with np.load(trained / 'cache/cache.npz', allow_pickle=False) as cache, \
            np.load(directory / 'public-roots.npz', allow_pickle=False) as raw:
        assert set(cache.files) == {'features', *expected}
        for key, value in expected.items():assert np.array_equal(cache[key], value), key
        tables = {key: raw[key] for key in KEYS}
        for i, root in enumerate(roots):assert public_digest({key: tables[key][i] for key in KEYS}) == root['public_sha256']
        feature = torch.from_numpy(cache['features'])
        target, weight = torch.from_numpy(expected['target']), torch.from_numpy(expected['weight'])
        anchor = np.flatnonzero(expected['complete'] & ~expected['held'])
        train = np.flatnonzero(expected['complete'] & expected['direction'] & ~expected['held'])
        valid = np.flatnonzero(expected['complete'] & expected['direction'] & expected['held'])
        initial = policies(start, heads, 'cpu');results = {};forward_checks = 0
        for seed, source in initial.items():
            label = f'C{seed}';path = trained / label
            policy, blob = load_gate(path / 'checkpoint.pt', start)
            config = json.loads((path / 'config.json').read_text());logs = lines(path / 'training.jsonl')
            result = json.loads((path / 'training-result.json').read_text())
            assert config == blob['training'] and config['seed'] == seed
            assert config['epochs'] == 128 and config['minibatch'] == 128 and config['lr'] == .0003
            assert config['grad_norm'] == 1. and config['kl_coefficient'] == .1
            assert config['device'] == 'cpu' and config['precision'] == 'float32'
            assert config['initial_head_sha256'] == HEAD_SHA[seed]
            assert config['cache_sha256'] == metadata['cache_sha256'] and config['dataset_sha256'] == metadata['dataset_sha256']
            assert config['loss'] == 'mean_margin_weighted_direction_BCE_plus_all_complete_train_initial_Bernoulli_KL'
            assert (config['anchor_rows'], config['direction_train_rows'], config['direction_held_rows']) == (len(anchor), len(train), len(valid))
            updates = 128 * ((len(train) + 127) // 128)
            assert len(logs) == 129 and [r['epoch'] for r in logs] == list(range(129))
            assert all(r['updates'] == r['epoch'] * (updates // 128) for r in logs)
            assert blob['final_epoch'] == 128 and result['updates'] == updates
            assert digest(policy.base) == digest(source.base) == metadata['frozen_parameter_sha256']
            for name, tensor in source.base.state_dict().items():assert torch.equal(tensor, policy.base.state_dict()[name])
            assert not any(p.requires_grad or p.grad is not None for p in policy.base.parameters())
            with torch.no_grad():
                before = source.head(feature).squeeze(-1);after = policy.head(feature).squeeze(-1)
                for scores, record in ((before, logs[0]), (after, logs[-1])):
                    assert metrics(scores[train], target[train]) == record['train']
                    assert metrics(scores[valid], target[valid]) == record['held']
                probability = before[anchor].sigmoid()
                kl = (probability * (F.logsigmoid(before[anchor]) - F.logsigmoid(after[anchor])) +
                      (1 - probability) * (F.logsigmoid(-before[anchor]) - F.logsigmoid(-after[anchor]))).mean()
                assert float(kl) == logs[-1]['all_train_anchor_kl']
                weighted = (F.binary_cross_entropy_with_logits(after[train], target[train], reduction='none') * weight[train]).mean()
                assert float(weighted) == logs[-1]['train_weighted_bce']
                for first in range(0, len(roots), 128):
                    ids = np.arange(first, min(first + 128, len(roots)))
                    batch = tuple(t.long() for t in inputs(tables, ids, 'cpu'))
                    run, _ = source.base.encode(batch[0], batch[1], batch[3])
                    assert torch.equal(gate_features(run, *batch), feature[ids])
                    ordinary, old_value = source.base(*batch)
                    now, value = policy(*batch)
                    active, end, other = scope(batch[0], batch[2], batch[3])
                    assert active.all() and torch.equal(value, old_value)
                    score = policy.head(feature[ids]).squeeze(-1)
                    assert torch.equal(now, decode(ordinary, score, batch[0], batch[2], batch[3]))
                    assert torch.isneginf(now[torch.isneginf(ordinary)]).all()
                    assert torch.equal(now.masked_fill(~other, float('-inf')).argmax(1), ordinary.masked_fill(~other, float('-inf')).argmax(1))
                    forward_checks += len(ids)
            assert sha(path / 'checkpoint.pt') == result['checkpoint_sha256']
            assert sha(path / 'optimizer.pt') == result['optimizer_sha256']
            optimizer = torch.load(path / 'optimizer.pt', map_location='cpu', weights_only=False)
            loaded = torch.optim.Adam(policy.head.parameters(), lr=.0003);loaded.load_state_dict(optimizer)
            same(optimizer, loaded.state_dict())
            assert len(optimizer['param_groups']) == 1 and optimizer['param_groups'][0]['lr'] == .0003
            assert len(optimizer['state']) == 4
            for state in optimizer['state'].values():
                assert int(state['step']) == updates and all(torch.isfinite(t).all() for t in state.values())
                assert (state['exp_avg_sq'] >= 0).all()
            final = logs[-1]['train']
            gate = final['balanced_accuracy'] >= .8 and final['recall'] >= .7 and final['specificity'] >= .7
            assert result['fitting_gate_passed'] == gate and result['final'] == logs[-1]
            assert result == summary['training'][label]
            results[label] = {'updates': updates, 'train': final, 'held': logs[-1]['held'],
                             'fitting_gate_passed': gate, 'checkpoint_sha256': result['checkpoint_sha256'],
                             'optimizer_sha256': result['optimizer_sha256']}
    gate = all(r['fitting_gate_passed'] for r in results.values())
    assert summary['fitting_gate_passed'] == gate
    result = {'independent_verification': True,
              'scope': 'raw reward-derived labels/splits/weights, public cache and ordinary forward, fixed final budget/metrics, full Adam roundtrip; no full optimizer replay',
              'public_cached_forward_checks': forward_checks, 'actor_input_keys': list(KEYS),
              'frozen_parameter_sha256': metadata['frozen_parameter_sha256'],
              'anchor_roots': len(anchor), 'direction_train_roots': len(train), 'direction_held_roots': len(valid),
              'arms': results, 'fitting_gate_passed': gate}
    write(out, result);print(json.dumps(result), flush=True);return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--start', required=True);parser.add_argument('--heads', required=True)
    parser.add_argument('--data', required=True);parser.add_argument('--trained', required=True)
    parser.add_argument('--out', required=True);args = parser.parse_args();torch.set_num_threads(1)
    learning(args.start, args.heads, args.data, args.trained, args.out)
    return 0


if __name__ == '__main__':sys.exit(main())
