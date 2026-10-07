#!/usr/bin/env python3
"""Audit public action caches, fixed final learning, and whole-run statistics."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import torch
import torch.nn.functional as F

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import gate_features, scope, write
from nosl.endturn_action import DATA_SHA, MANIFEST_SHA, initial, load
from nosl.episodic_strategy import digest
from nosl.teacher_distill import sha
from verify_endturn_credit import metrics, same
from endturn_action_experiment import ARMS, paired
from endturn_experiment import select, summarize


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def learning(start, heads, directory, trained, out):
    directory, trained = Path(directory), Path(trained)
    assert sha(directory / 'manifest.json') == MANIFEST_SHA
    assert sha(directory / 'data/public-roots.npz') == DATA_SHA
    roots = lines(directory / 'data/roots.jsonl');branches = lines(directory / 'data/branches.jsonl')
    pairs = {(r['seed'], r['branch']): r for r in branches}
    target, direction, weights, held = [], [], [], []
    for root in roots:
        a, b = pairs[(root['seed'], 'end')], pairs[(root['seed'], 'continue')]
        assert a['end'] == b['end'] == 'death' and a['mc_qualified'] and b['mc_qualified']
        delta = sum(r[-1] for r in a['reward_steps']) - sum(r[-1] for r in b['reward_steps'])
        assert delta == root['delta'] and root['complete']
        used = abs(delta) > 1e-5
        target.append(int(delta > 0) if used else 0);direction.append(used)
        weights.append(min(abs(delta) / .02, 5.) if used else 0.)
        held.append(int.from_bytes(hashlib.sha256(root['seed'].encode()).digest()[:8], 'little') % 5 == 0)
    expected = {'complete': np.ones(len(roots), bool), 'held': np.asarray(held, bool),
                'target': np.asarray(target, np.float32), 'direction': np.asarray(direction, bool),
                'weight': np.asarray(weights, np.float32)}
    meta = json.loads((trained / 'cache/cache.json').read_text());summary = json.loads((trained / 'summary.json').read_text())
    assert meta['data_sha256'] == DATA_SHA and meta['sealed_manifest_sha256'] == MANIFEST_SHA
    assert sha(trained / 'cache/cache.npz') == meta['cache_sha256']
    assert summary['initial_contract_passed'] and summary['registered_seconds'] == 180 and summary['seconds'] <= 180
    anchor = np.flatnonzero(~expected['held']);rows = np.flatnonzero(~expected['held'] & expected['direction'])
    valid = np.flatnonzero(expected['held'] & expected['direction'])
    assert (len(anchor), len(rows), len(valid)) == (352, 177, 49)
    result = {};forward_checks = 0
    with np.load(trained / 'cache/cache.npz', allow_pickle=False) as cached, \
            np.load(directory / 'data/public-roots.npz', allow_pickle=False) as raw:
        assert set(cached.files) == {'gate', 'action', *expected}
        for key, value in expected.items():assert np.array_equal(cached[key], value)
        tables = {key: raw[key] for key in KEYS}
        gate, action, target, weight = (torch.from_numpy(cached[key]) for key in ('gate', 'action', 'target', 'weight'))
        for arm in ARMS:
            seed, context = int(arm[1]), arm[0] == 'C'
            model, blob = load(trained / arm / 'checkpoint.pt', start, heads)
            original = initial(start, heads, seed, context)
            config = json.loads((trained / arm / 'config.json').read_text())
            logs = lines(trained / arm / 'training.jsonl');published = json.loads((trained / arm / 'training-result.json').read_text())
            assert config == blob['training'] and config['seed'] == seed and config['action_context'] == context
            assert config['epochs'] == 128 and config['minibatch'] == 128 and config['lr'] == .0003
            assert config['grad_norm'] == 1. and config['kl_coefficient'] == .1
            assert config['device'] == 'cpu' and config['precision'] == 'float32'
            assert config['loss'] == 'mean_margin_weighted_direction_BCE_plus_all_complete_train_initial_Bernoulli_KL'
            assert config['cache_sha256'] == meta['cache_sha256'] and config['data_sha256'] == DATA_SHA
            assert config['initial_head_digest'] == digest(original.head) and config['initial_residual_digest'] == digest(original.residual)
            assert config['learned_parameters'] == sum(p.numel() for p in model.learned_parameters())
            assert len(logs) == 129 and [r['epoch'] for r in logs] == list(range(129))
            assert all(r['updates'] == r['epoch'] * 2 for r in logs)
            assert published['updates'] == 256 and blob['final_epoch'] == 128
            assert digest(model.base) == digest(original.base) == meta['frozen_parameter_sha256']
            assert all(torch.equal(v, model.base.state_dict()[k]) for k, v in original.base.state_dict().items())
            assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
            with torch.no_grad():
                feature = torch.cat((gate, action if context else torch.zeros_like(action)), 1)
                before = (original.head(gate) + original.residual(feature)).squeeze(-1)
                after = (model.head(gate) + model.residual(feature)).squeeze(-1)
                for score, record in ((before, logs[0]), (after, logs[-1])):
                    assert metrics(score[rows], target[rows]) == record['train']
                    assert metrics(score[valid], target[valid]) == record['held']
                probability = before[anchor].sigmoid()
                kl = (probability * (F.logsigmoid(before[anchor]) - F.logsigmoid(after[anchor])) +
                      (1 - probability) * (F.logsigmoid(-before[anchor]) - F.logsigmoid(-after[anchor]))).mean()
                assert float(kl) == logs[-1]['all_train_anchor_kl']
                weighted = (F.binary_cross_entropy_with_logits(after[rows], target[rows], reduction='none') * weight[rows]).mean()
                assert float(weighted) == logs[-1]['weighted_bce']
                for first in range(0, len(roots), 128):
                    ids = np.arange(first, min(first + 128, len(roots)))
                    batch = tuple(t.long() for t in inputs(tables, ids, 'cpu'))
                    run, tokens = original.base.encode(batch[0], batch[1], batch[3])
                    encoded = original.base.action_features(run, tokens, batch[2])
                    logits = original.base.score_actions(encoded, batch[3])
                    active, end, other = scope(batch[0], batch[2], batch[3])
                    best = logits.masked_fill(~other, float('-inf')).argmax(1)
                    assert active.all() and best.tolist() == [roots[i]['continue_action'] for i in ids]
                    expected_action = encoded[torch.arange(len(ids)), best, 128:]
                    assert torch.equal(expected_action, action[ids])
                    assert torch.equal(gate_features(run, *batch), gate[ids]);forward_checks += len(ids)
            optimizer = torch.load(trained / arm / 'optimizer.pt', map_location='cpu', weights_only=False)
            loaded = torch.optim.Adam(model.learned_parameters(), lr=.0003);loaded.load_state_dict(optimizer)
            same(optimizer, loaded.state_dict())
            assert len(optimizer['state']) == 8 and len(optimizer['param_groups']) == 1
            assert optimizer['param_groups'][0]['lr'] == .0003 and optimizer['param_groups'][0]['weight_decay'] == 0
            for state in optimizer['state'].values():
                assert int(state['step']) == 256 and all(torch.isfinite(t).all() for t in state.values())
                assert (state['exp_avg_sq'] >= 0).all()
            if not context:
                assert torch.equal(model.residual[0].weight[:, 146:], original.residual[0].weight[:, 146:])
            assert sha(trained / arm / 'checkpoint.pt') == published['checkpoint_sha256']
            assert sha(trained / arm / 'optimizer.pt') == published['optimizer_sha256']
            assert published['final'] == logs[-1] and published == summary['training'][arm]
            metric = logs[-1]['train'];fit = metric['balanced_accuracy'] >= .8 and metric['recall'] >= .7 and metric['specificity'] >= .7
            assert fit == published['fitting_gate_passed']
            result[arm] = {'updates': 256, 'metrics': {'train': metric, 'held': logs[-1]['held']},
                           'fitting_gate_passed': fit, 'checkpoint_sha256': published['checkpoint_sha256'],
                           'optimizer_sha256': published['optimizer_sha256'], 'learned_parameters': config['learned_parameters']}
    for seed in (1, 3):
        a = json.loads((trained / f'B{seed}/config.json').read_text());b = json.loads((trained / f'C{seed}/config.json').read_text())
        for key in ('initial_head_digest', 'initial_residual_digest', 'learned_parameters'):
            assert a[key] == b[key]
    fit = all(result[f'C{seed}']['fitting_gate_passed'] for seed in (1, 3))
    assert summary['fitting_gate_passed'] == fit
    audited = {'independent_verification': True, 'scope': 'raw labels/splits/weights, independent public action/cache reconstruction, fixed final metrics/full Adam roundtrip; no full optimizer replay',
               'public_action_cache_checks': forward_checks, 'capacity_and_initial_tensors_matched': True,
               'arms': result, 'fitting_gate_passed': fit}
    write(out, audited);print(json.dumps(audited), flush=True);return audited


def evaluation(experiment, out):
    experiment = Path(experiment);config = json.loads((experiment / 'experiment.json').read_text())
    assert config['mode'] == 'evaluate' and config['evaluation_runs'] == 1024
    assert config['evaluation_prefix'] == 'NQ4AEEVAL' and config['evaluation_seconds'] == 1500
    assert config['device'] == 'cpu' and config['precision'] == 'float32'
    expected = [f'NQ4AEEVAL{i:06d}' for i in range(1024)];rows = {}
    for arm in ('X1', *ARMS):
        attempts = lines(experiment / 'evaluation' / (arm + '.jsonl'));rows[arm] = attempts
        assert [r['seed'] for r in attempts] == expected
        for row in attempts:
            assert row['mc_qualified'] == (row['end'] in ('death', 'victory'))
            assert row['ascension'] == 10 and row['actor_filter'] == 1
            if row['end'] != 'not_started':
                assert sum(row['action_kinds'].values()) == len(row['action_rows']) == sum(row['routing'].values())
                assert row['endturn_decisions'] == row['action_kinds'].get('3', 0)
            if row['mc_qualified']:assert 'error' not in row
        if arm != 'X1':assert select(attempts, True) == lines(experiment / (arm + '-normal-inputs.jsonl'))
    complete = all(r['mc_qualified'] for arm in rows.values() for r in arm)
    statistics = {arm: summarize(attempts) for arm, attempts in rows.items()}
    comparison = {}
    if complete:
        for seed in (1, 3):
            for against in ('X1', f'B{seed}'):
                delta = np.asarray([row['floor'] - control['floor'] for row, control in zip(rows[f'C{seed}'], rows[against])], np.float64)
                rng = np.random.default_rng(20261006);means = np.empty(10000)
                for first in range(0, 10000, 250):
                    draw = rng.integers(0, 1024, (min(250, 10000 - first), 1024))
                    means[first:first + len(draw)] = delta[draw].mean(1)
                low, high = np.quantile(means, [.025, .975])
                comparison[f'C{seed}-{against}'] = {'floor_gain': float(delta.mean()), 'ci95': [float(low), float(high)],
                                                   'gate': bool(delta.mean() >= .5 and low > 0)}
    gate = bool(complete and all(r['gate'] for r in comparison.values()) and
                max(statistics[arm]['act3'] for arm in ('C1', 'C3')) >= statistics['X1']['act3'])
    published = json.loads((experiment / 'results.json').read_text())
    assert published['policies'] == statistics and published['paired'] == comparison
    assert published['all_evaluation_runs_terminal'] == complete and published['gate_passed'] == gate
    win_intervals = {}
    z = 1.959963984540054
    for arm, stats in statistics.items():
        n, victories = stats['terminals'], stats['victories']
        if not n:win_intervals[arm] = None;continue
        fraction = victories / n;denominator = 1 + z*z/n
        centre = (fraction + z*z/(2*n))/denominator
        half = z * np.sqrt(fraction*(1 - fraction)/n + z*z/(4*n*n))/denominator
        win_intervals[arm] = {'victories': victories, 'runs': n, 'wilson95': [max(0., float(centre-half)), min(1., float(centre+half))]}
    result = {'independent_verification': True, 'registered_seeds_and_selection_verified': True,
              'policies': statistics, 'paired': comparison, 'win_intervals': win_intervals,
              'all_evaluation_runs_terminal': complete, 'gate_passed': gate}
    write(out, result);print(json.dumps(result), flush=True);return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('learning', 'evaluation'))
    parser.add_argument('--start');parser.add_argument('--heads');parser.add_argument('--data')
    parser.add_argument('--trained');parser.add_argument('--experiment');parser.add_argument('--out', required=True)
    args = parser.parse_args();torch.set_num_threads(1)
    if args.mode == 'learning':learning(args.start, args.heads, args.data, args.trained, args.out)
    else:evaluation(args.experiment, args.out)
    return 0


if __name__ == '__main__':sys.exit(main())
