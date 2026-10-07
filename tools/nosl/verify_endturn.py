#!/usr/bin/env python3
"""Audit fixed gate learning, saved results, and selected full normal traces."""
import argparse
import json
from pathlib import Path
import sys

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.endturn import (DATA_SHA, EPOCHS, MINIBATCH, START_SHA, binary_metrics,
                          load_gate, write)
from nosl.episodic_strategy import digest
from nosl.teacher_distill import oracle_module, sha
from endturn_experiment import paired, select, summarize
from oracle_replay import one


def lines(path):
    return [json.loads(line) for line in Path(path).read_text().splitlines() if line]


def same(left, right):
    if isinstance(left, torch.Tensor):assert torch.equal(left, right)
    elif isinstance(left, dict):
        assert left.keys() == right.keys()
        for key in left:same(left[key], right[key])
    elif isinstance(left, (tuple, list)):
        assert len(left) == len(right)
        for a, b in zip(left, right):same(a, b)
    else:assert left == right


def learning(start, directory, qualification, out):
    directory, qualification = Path(directory), Path(qualification)
    cache_meta = json.loads((qualification / 'cache/cache.json').read_text())
    assert sha(start) == START_SHA and sha(directory / 'public-tables.npz') == DATA_SHA
    assert cache_meta['cache_sha256'] == sha(qualification / 'cache/cache.npz')
    with np.load(directory / 'public-tables.npz', allow_pickle=False) as raw, \
            np.load(qualification / 'cache/cache.npz', allow_pickle=False) as cached:
        legal = np.arange(raw['actions'].shape[1])[None] < raw['counts'][:, 1:2]
        end = legal & (raw['actions'][..., 0] == 3)
        active = (raw['global'][:, 11] != 0) & (end.sum(1) == 1) & (legal & ~end).any(1)
        target = raw['actions'][np.arange(len(raw['label'])), raw['label'], 0] == 3
        for key, expected in (('eligible', active), ('target', target), ('held', raw['held']), ('group', raw['group'])):
            assert np.array_equal(cached[key], expected), key
        rows, held = np.flatnonzero(active & ~raw['held']), np.flatnonzero(active & raw['held'])
        assert len(rows) == 1088 and target[rows].sum() == 89
        assert len(held) == 28 and target[held].sum() == 2
        assert len(set(raw['group'][rows])) == 55 and len(set(raw['group'][held])) == 2
        feature = torch.from_numpy(cached['features'])
        expected = json.loads((qualification / 'summary.json').read_text())
        results = {}
        for seed in (1, 3):
            label = f'E{seed}';path = qualification / label
            logs = lines(path / 'training.jsonl');published = json.loads((path / 'training-result.json').read_text())
            policy, blob = load_gate(path / 'checkpoint.pt', start)
            config = json.loads((path / 'config.json').read_text())
            assert config == blob['training'] and config['seed'] == seed
            assert config['epochs'] == EPOCHS and config['minibatch'] == MINIBATCH and config['lr'] == .001
            assert config['loss'] == 'unweighted_binary_cross_entropy' and config['device'] == 'cpu'
            assert len(logs) == EPOCHS + 1 and [r['epoch'] for r in logs] == list(range(EPOCHS + 1))
            steps = (len(rows) + MINIBATCH - 1) // MINIBATCH
            assert all(row['updates'] == row['epoch'] * steps for row in logs)
            assert blob['final_epoch'] == EPOCHS and published['updates'] == EPOCHS * steps == 1152
            assert digest(policy.base) == cache_meta['frozen_parameter_sha256'] == published['frozen_parameter_sha256']
            assert sha(path / 'checkpoint.pt') == published['checkpoint_sha256']
            assert sha(path / 'optimizer.pt') == published['optimizer_sha256']
            optimizer = torch.load(path / 'optimizer.pt', map_location='cpu', weights_only=False)
            loaded = torch.optim.Adam(policy.head.parameters(), lr=.001);loaded.load_state_dict(optimizer)
            same(optimizer, loaded.state_dict())
            assert len(optimizer['state']) == len(list(policy.head.parameters())) == 4
            assert len(optimizer['param_groups']) == 1 and optimizer['param_groups'][0]['lr'] == .001
            for state in optimizer['state'].values():
                assert int(state['step']) == 1152
                assert all(torch.isfinite(t).all() for t in state.values())
                assert (state['exp_avg_sq'] >= 0).all()
            with torch.no_grad():score = policy.head(feature).squeeze(-1)
            metrics = {'train': binary_metrics(score[rows], torch.from_numpy(target[rows])),
                       'held': binary_metrics(score[held], torch.from_numpy(target[held]))}
            assert all(metrics[k] == logs[-1][k] for k in metrics)
            assert logs[-1] == published['final']
            assert published == expected['training'][label]
            fitting = metrics['train']['balanced_accuracy'] >= .8 and metrics['train']['recall'] >= .7
            assert published['fitting_gate_passed'] == fitting
            results[label] = {'updates': 1152, 'metrics': metrics, 'fitting_gate_passed': fitting,
                              'checkpoint_sha256': published['checkpoint_sha256'],
                              'optimizer_sha256': published['optimizer_sha256']}
    result = {'independent_verification': True,
              'scope': 'raw cached labels/splits, fixed budget, final fitting and saved full Adam roundtrip; not full optimizer replay',
              'frozen_parameter_sha256': cache_meta['frozen_parameter_sha256'], 'arms': results,
              'fitting_gate_passed': all(r['fitting_gate_passed'] for r in results.values())}
    write(out, result);return result


def evaluation(experiment, out):
    experiment = Path(experiment);config = json.loads((experiment / 'experiment.json').read_text())
    assert config['mode'] == 'evaluate' and config['evaluation_runs'] == 1024
    assert config['evaluation_prefix'] == 'NQ4ACEVAL' and config['evaluation_seconds'] == 900
    assert config['baseline_checkpoint_sha256'] == START_SHA and config['precision'] == 'float32'
    expected = [f'NQ4ACEVAL{i:06d}' for i in range(1024)]
    rows = {}
    for label in ('X1', 'E1', 'E3'):
        rows[label] = lines(experiment / 'evaluation' / (label + '.jsonl'))
        assert [row['seed'] for row in rows[label]] == expected
        for row in rows[label]:
            assert row['mc_qualified'] == (row['end'] in ('death', 'victory'))
            assert row['ascension'] == 10 and row['actor_filter'] == 1
            if row['end'] != 'not_started':
                assert sum(row['action_kinds'].values()) == len(row['action_rows']) == sum(row['routing'].values())
                assert row['endturn_decisions'] == row['action_kinds'].get('3', 0)
            if row['mc_qualified']:assert 'error' not in row
        if label != 'X1':assert select(rows[label], True) == lines(experiment / (label + '-normal-inputs.jsonl'))
    complete = all(row['mc_qualified'] for arm in rows.values() for row in arm)
    statistics = {label: summarize(arm) for label, arm in rows.items()}
    comparison = paired(rows) if complete else {}
    gate = bool(complete and all(r['gate'] for r in comparison.values()) and
                max(statistics[s]['act3'] for s in ('E1', 'E3')) >= statistics['X1']['act3'])
    published = json.loads((experiment / 'results.json').read_text())
    assert published['policies'] == statistics and published['paired'] == comparison
    assert published['gate_passed'] == gate and published['all_evaluation_runs_terminal'] == complete
    result = {'independent_verification': True, 'registered_seeds_and_selection_verified': True,
              'policies': statistics, 'paired': comparison, 'all_evaluation_runs_terminal': complete,
              'gate_passed': gate}
    write(out, result);return result


def normal(experiment, out):
    experiment, out = Path(experiment), Path(out);out.mkdir(parents=True, exist_ok=False)
    config = json.loads((experiment / 'experiment.json').read_text())
    final = config['mode'] == 'evaluate'
    expected_seeds = [f'{"NQ4ACEVAL" if final else "NQ4ACPILOT"}{i:06d}'
                      for i in range(1024 if final else 4)]
    l4 = oracle_module();results = []
    for label in ('E1', 'E3'):
        attempts = lines(experiment / 'evaluation' / (label + '.jsonl'))
        assert [r['seed'] for r in attempts] == expected_seeds
        chosen = select(attempts, final)
        assert chosen == lines(experiment / (label + '-normal-inputs.jsonl'))
        for trajectory in chosen:
            previous = one((trajectory, out / label, 300, 'off'))
            result = {'arm': label, 'seed': trajectory['seed'], 'floor': trajectory.get('floor'),
                      'pipeline': previous, 'whole_run_qualified': False}
            if previous['status'] == 'identical' and previous.get('script') == 'complete':
                try:
                    trace = out / label / trajectory['seed'] / 'oracle.jsonl'
                    run = l4.parse_runs(trace.read_text().splitlines())[0];begin = run['begin']
                    assert begin['oracle_test_mode'] is False and begin['seed'] == trajectory['seed']
                    assert begin['character'] == 'SILENT' and begin['ascension'] == 10
                    assert begin['hp'] is None and begin['deck'] is None and not begin['relics']
                    assert run['end']['run']['total_floor'] == trajectory['floor']
                    decisions, end, error = l4.replay(str(ROOT / 'build/sts2sim_cli'), run,
                        verify_snapshots=True, timeout=300, artifacts=str(trace.parent / 'strict-snapshots-native'))
                    comparison = l4.compare_run(run, decisions, end, error)
                    diff = l4.first_diff(run['end']['run'], (end or {}).get('run'))
                    result.update(comparison=comparison, end_run_first_diff=diff, native_error=error,
                        whole_run_qualified=bool(comparison['status'] == 'identical' and
                            comparison['matched'] == comparison['total'] and error is None and diff is None))
                except Exception as error:
                    result['verification_error'] = type(error).__name__ + ': ' + str(error)
            results.append(result)
            with (out / 'results.jsonl').open('a') as stream:stream.write(json.dumps(result) + '\n')
            print(json.dumps(result), flush=True)
    summary = {'registered_selection_verified': True, 'runs': len(results),
               'whole_run_qualified': sum(r['whole_run_qualified'] for r in results),
               'decisions_matched': sum(r.get('comparison', {}).get('matched', 0) for r in results),
               'oracle_test_mode': False, 'verify_snapshots': True,
               'failures': [r for r in results if not r['whole_run_qualified']]}
    write(out / 'summary.json', summary);return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('learning', 'evaluation', 'normal'))
    parser.add_argument('--start');parser.add_argument('--data');parser.add_argument('--qualification')
    parser.add_argument('--experiment');parser.add_argument('--out', required=True)
    args = parser.parse_args();torch.set_num_threads(1)
    if args.mode == 'learning':
        result = learning(args.start, args.data, args.qualification, args.out)
    elif args.mode == 'evaluation':result = evaluation(args.experiment, args.out)
    else:result = normal(args.experiment, args.out)
    print(json.dumps(result), flush=True)
    return 0 if args.mode != 'normal' or result['whole_run_qualified'] == result['runs'] else 1


if __name__ == '__main__':sys.exit(main())
