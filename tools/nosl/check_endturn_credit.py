#!/usr/bin/env python3
"""Qualify actual Combat forks, full normal paths, and raw reward arithmetic."""
import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl import native
from nosl.branch_credit import KEYS, buffers, inputs
from nosl.endturn import scope, write
from nosl.endturn_credit import (collect, pair_target, policies, prepare, public_digest,
                                split, train, TIE_EPSILON)
from nosl.teacher_distill import oracle_module
from oracle_replay import one


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def audit(directory, out, start, heads):
    """Independent scalar reconstruction of the actual reward implementation."""
    directory = Path(directory);config = json.loads((directory / 'config.json').read_text())
    roots = lines(directory / 'roots.jsonl');branches = lines(directory / 'branches.jsonl')
    attempts = lines(directory / 'attempts.jsonl');returns = {};reward_steps = 0
    assert [r['seed'] for r in sorted(attempts, key=lambda r: r['seed'])] == [f'{config["prefix"]}{i:06d}' for i in range(config['attempts'])]
    assert len(branches) == 2 * len(roots)
    assert len({(r['seed'], r['branch']) for r in branches}) == len(branches)
    models = policies(start, heads, 'cpu')
    public_checks = 0
    with np.load(directory / 'public-roots.npz', allow_pickle=False) as packed:
        assert set(packed.files) == set(KEYS) | {'complete', 'delta', 'held', 'source', 'index'}
        assert len(packed['counts']) == len(roots)
        for index, root in enumerate(roots):
            row = {key: packed[key][index] for key in KEYS}
            assert public_digest(row) == root['public_sha256']
            for key in ('complete', 'held', 'source', 'index'):
                assert packed[key][index] == root[key]
            assert (np.isnan(packed['delta'][index]) if root['delta'] is None else packed['delta'][index] == root['delta'])
            assert all(packed[key].dtype == np.int32 for key in KEYS)
        # Recompute the source greedy and the original X1 non-EndTurn choice
        # from the public tables, independently of the collector's infer().
        tables = {key: packed[key] for key in KEYS}
        for source, model in models.items():
            selected = np.asarray([i for i, root in enumerate(roots) if root['source'] == source])
            for first in range(0, len(selected), 64):
                ids = selected[first:first + 64]
                batch = inputs(tables, ids, 'cpu')
                with torch.no_grad():
                    logits, _ = model(*batch)
                    original, _ = model.base(*batch)
                    active, end, other = scope(batch[0], batch[2], batch[3])
                assert active.all()
                choices = logits.argmax(1).numpy()
                continuing = original.masked_fill(~other, float('-inf')).argmax(1).numpy()
                ending = end.long().argmax(1).numpy()
                for j, index in enumerate(ids):
                    root = roots[index]
                    assert root['active'] and root['choice'] == int(choices[j])
                    assert root['end_action'] == int(ending[j]) and root['continue_action'] == int(continuing[j])
                    assert root['desired_stop'] == (root['choice'] == root['end_action'])
                    assert root['floor'] == int(tables['global'][index, native.G_TOTAL_FLOOR])
                    public_checks += 1
    root_index = {r['seed']: r for r in roots}
    for row in branches:
        assert row['end'] == native.STATUS_NAMES[row['status']]
        assert row['mc_qualified'] == (row['status'] in (native.DEATH, native.VICTORY))
        root = root_index[row['seed']]
        assert row['action_rows'] == root['prefix'] + row['branch_actions']
        assert len(row['branch_actions']) == len(row['reward_steps'])
        if row['branch_actions']:
            assert row['branch_actions'][0] == root['end_action' if row['branch'] == 'end' else 'continue_action']
        else:assert not row['mc_qualified']
        reference = paid = 0.
        if row['reward_steps'] and row['reward_steps'][0][2]:
            first = row['reward_steps'][0]
            reference = max(first[3], first[4], 1)
            paid = np.clip(1 - first[3] / reference, 0, 1)
        total = 0.
        previous = None
        for facts in row['reward_steps']:
            f0, hp0, boss0, enemy0, max0, f1, hp1, boss1, enemy1, max1, status, reward = facts
            assert all(isinstance(x, int) for x in facts[:-1])
            assert isinstance(reward, (int, float)) and np.isfinite(reward)
            if previous is not None:assert facts[:5] == previous[5:10]
            assert previous is None or previous[10] == native.RUNNING
            if boss0 and reference == 0:
                reference, paid = max(enemy0, max0, 1), 0.
            ended = status != native.RUNNING
            fighting = reference > 0 and not ended
            won = fighting and not boss1
            share = 1 - paid if won else min(max(enemy0 - enemy1, 0) / max(reference, 1), 1 - paid)
            if not fighting:share = 0.
            base = np.float32((f1 - f0) / 50 + .2 * hp1 / 70 - .2 * hp0 / 70)
            expected = np.float32(float(base) + .3 * share)
            paid += share
            if won:reference = 0.
            if ended:
                bonus = .3 * (1 - paid) if reference > 0 and status == native.VICTORY else 0.
                reference = 0.
                if status in (native.VICTORY, native.DEATH):
                    expected = np.float32((f1 - f0) / 50 + (status == native.VICTORY) - .2 * hp0 / 70 + bonus)
                else:expected = np.float32(0.)
            assert float(expected) == reward, (row['seed'], row['branch'], reward_steps, expected, reward)
            total += reward;reward_steps += 1;previous = facts
        if row['mc_qualified']:
            assert total == row['return'] and previous[5] == row['floor'] and previous[6] == row['hp']
        else:assert row['return'] is None
        returns[(row['seed'], row['branch'])] = row['return']
    direction = {'train': {'rows': 0, 'positive': 0, 'negative': 0}, 'held': {'rows': 0, 'positive': 0, 'negative': 0}}
    for root in roots:
        pair = [returns[(root['seed'], b)] for b in ('end', 'continue')]
        assert root['complete'] == all(r is not None for r in pair)
        assert root['held'] == split(root['seed'])
        assert root['source'] == (1 if root['index'] % 2 == 0 else 3)
        assert root['desired_stop'] == ((root['index'] // 2) % 2 == 0)
        assert root['minimum_floor'] == (4 if (root['index'] // 4) % 2 == 0 else 10)
        assert root['floor'] >= root['minimum_floor']
        assert root['outcome_returns'] == pair
        if root['complete']:
            delta = pair[0] - pair[1];assert delta == root['delta']
            if abs(delta) > TIE_EPSILON:
                assert root['label'] == int(delta > 0) and root['weight'] == min(abs(delta) / .02, 5.)
                selected = direction['held' if root['held'] else 'train']
                selected['rows'] += 1;selected['positive' if delta > 0 else 'negative'] += 1
            else:assert root['label'] is None and root['weight'] == 0
        else:assert root['delta'] is None and root['label'] is None and root['weight'] == 0
    summary = {'independent_verification': True, 'roots': len(roots),
               'public_root_and_source_choice_checks': public_checks,
               'complete_roots': sum(r['complete'] for r in roots),
               'branches': len(branches), 'reward_steps': reward_steps, 'direction': direction,
               'branch_failures': [r['seed'] + ':' + r['branch'] for r in branches if not r['mc_qualified']],
               'attempt_failures': [r['seed'] for r in attempts if not r['root'] and r.get('status') not in (native.VICTORY, native.DEATH)]}
    if config['prefix'] == 'NQ4ADTRAIN':
        assert config['attempts'] == 512 and config['seconds'] == 600 and config['max_steps'] == 30000
        requirements = {'complete_roots': summary['complete_roots'] >= 384,
                        'no_branch_fault_or_truncation': not summary['branch_failures'],
                        'directional_train_roots': direction['train']['rows'] >= 128,
                        'train_positive_and_negative': min(direction['train']['positive'], direction['train']['negative']) >= 16,
                        'directional_held_roots': direction['held']['rows'] >= 24,
                        'held_positive_and_negative': min(direction['held']['positive'], direction['held']['negative']) >= 4}
        summary.update(data_gate_requirements=requirements, data_gate_passed=all(requirements.values()))
    write(out, summary);print(json.dumps(summary), flush=True);return summary


def replay(trajectory, directory):
    previous = one((trajectory, Path(directory), 300, 'off'))
    result = {'seed': trajectory['seed'], 'branch': trajectory.get('branch'),
              'pipeline': previous, 'whole_run_qualified': False}
    if previous['status'] == 'identical' and previous.get('script') == 'complete':
        try:
            l4 = oracle_module();trace = Path(directory) / trajectory['seed'] / 'oracle.jsonl'
            run = l4.parse_runs(trace.read_text().splitlines())[0];begin = run['begin']
            assert begin['oracle_test_mode'] is False and begin['character'] == 'SILENT' and begin['ascension'] == 10
            assert begin['hp'] is None and begin['deck'] is None and not begin['relics']
            assert run['end']['run']['total_floor'] == trajectory['floor']
            decisions, end, error = l4.replay(str(ROOT / 'build/sts2sim_cli'), run, verify_snapshots=True,
                timeout=300, artifacts=str(trace.parent / 'strict-snapshots-native'))
            comparison = l4.compare_run(run, decisions, end, error)
            diff = l4.first_diff(run['end']['run'], (end or {}).get('run'))
            result.update(comparison=comparison, end_run_first_diff=diff, native_error=error,
                          whole_run_qualified=bool(comparison['status'] == 'identical' and
                              comparison['matched'] == comparison['total'] and error is None and diff is None))
        except Exception as error:result['error'] = type(error).__name__ + ': ' + str(error)
    return result


def direct_forks(directory, out):
    directory, out = Path(directory), Path(out);out.mkdir(parents=True, exist_ok=False)
    roots = lines(directory / 'roots.jsonl');checks = []
    for root in roots:
        with np.load(directory / (root['seed'] + '-fork.npz'), allow_pickle=False) as frozen:
            for branch in ('end', 'continue'):
                batch = native.Batch(1, 256, 256, 30000);batch.set_filter(1)
                assert batch.reset(0, root['seed'], 10) == native.RUNNING
                data = buffers(1, 256, 256);batch.step(np.asarray([-1], np.int32), *data.values())
                for action in root['prefix']:batch.step(np.asarray([action], np.int32), *data.values())
                assert all(np.array_equal(v[0], frozen['root_' + k]) for k, v in data.items())
                action = root['end_action' if branch == 'end' else 'continue_action']
                batch.step(np.asarray([action], np.int32), *data.values())
                assert all(np.array_equal(v[0], frozen[branch + '_' + k]) for k, v in data.items())
                checks.append({'seed': root['seed'], 'branch': branch, 'steps': int(data['steps'][0])})
    # Use the first fixed real root with its real prefix and a one-action
    # remaining budget: a genuine native nonterminal result, no fake status.
    first = roots[0]
    with np.load(directory / (first['seed'] + '-fork.npz'), allow_pickle=False) as frozen:
        limit = int(frozen['root_steps']) + 1
    batch = native.Batch(3, 256, 256, limit);batch.set_filter(1);batch.set_active(1)
    assert batch.reset(0, first['seed'], 10) == native.RUNNING
    data = buffers(3, 256, 256);batch.step(np.full(3, -1, np.int32), *data.values())
    for action in first['prefix']:batch.step(np.asarray([action, -1, -1], np.int32), *data.values())
    assert batch.fork(0, 1) == native.RUNNING and batch.fork(0, 2) == native.RUNNING
    batch.set_active(3);batch.step(np.asarray([-1, first['end_action'], first['continue_action']], np.int32), *data.values())
    actual = [int(data['status'][i]) for i in (1, 2)]
    assert native.STEP_LIMIT in actual and not all(s in (native.VICTORY, native.DEATH) for s in actual)
    target = pair_target([{'status': status, 'return': None} for status in actual])
    assert target == {'complete': False, 'delta': None, 'label': None, 'weight': 0.}
    result = {'passed': True, 'independent_fresh_prefix_first_action_checks': checks,
              'native_nonterminal_statuses': actual, 'native_limit': limit,
              'nonterminal_pair_has_no_learning_label': True, 'actual_label_result': target}
    write(out / 'result.json', result);return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('pilot', 'collect', 'audit', 'normal', 'train'))
    parser.add_argument('--start');parser.add_argument('--heads');parser.add_argument('--out', required=True)
    parser.add_argument('--data');args = parser.parse_args();torch.set_num_threads(1)
    out = Path(args.out)
    if args.mode in ('pilot', 'collect'):
        pilot = args.mode == 'pilot'
        result = collect(args.start, args.heads, out,
            prefix='NQ4ADPILOT' if pilot else 'NQ4ADTRAIN', attempts=16 if pilot else 512,
            seconds=180 if pilot else 600, block=16 if pilot else 32, controls=pilot)
        print(json.dumps(result), flush=True);return 0
    if args.mode == 'audit':
        if not args.start or not args.heads:raise ValueError('audit requires the fixed source policies')
        audit(args.data, out, args.start, args.heads);return 0
    if args.mode == 'train':
        qualification = Path(args.data).parent
        normal = json.loads((qualification / 'qualification-normal/summary.json').read_text())
        pilot = json.loads((qualification / 'qualification-policy-audit.json').read_text())
        assert normal['oracle_test_mode'] is False and normal['verify_snapshots'] is True
        assert normal['fork_contract_passed'] and not normal['failures']
        assert pilot['complete_roots'] >= 8 and pilot['branches'] == normal['runs'] == normal['whole_run_qualified']
        assert not pilot['branch_failures'] and not pilot['attempt_failures']
        out.mkdir(parents=True, exist_ok=False)
        began = time.monotonic();deadline = began + 180
        prepare(args.start, args.heads, args.data, qualification / 'data-audit.json', out / 'cache', deadline)
        result = {f'C{seed}': train(args.start, args.heads, out / 'cache', out / f'C{seed}', seed, deadline) for seed in (1, 3)}
        write(out / 'summary.json', {'training': result, 'seconds': time.monotonic() - began,
              'registered_seconds': 180, 'fitting_gate_passed': all(row['fitting_gate_passed'] for row in result.values()),
              'source_commit': __import__('subprocess').check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()})
        print((out / 'summary.json').read_text(), flush=True)
        return 0
    out.mkdir(parents=True, exist_ok=False)
    fork = direct_forks(args.data, out / 'fork-contract')
    rows = []
    for trajectory in lines(Path(args.data) / 'branches.jsonl'):
        result = replay(trajectory, out / trajectory['branch']);rows.append(result)
        with (out / 'results.jsonl').open('a') as stream:stream.write(json.dumps(result) + '\n')
        print(json.dumps(result), flush=True)
    summary = {'runs': len(rows), 'whole_run_qualified': sum(r['whole_run_qualified'] for r in rows),
               'decisions_matched': sum(r.get('comparison', {}).get('matched', 0) for r in rows),
               'oracle_test_mode': False, 'verify_snapshots': True, 'fork_contract_passed': fork['passed'],
               'failures': [r for r in rows if not r['whole_run_qualified']]}
    write(out / 'summary.json', summary);print(json.dumps(summary), flush=True)
    return 0 if summary['runs'] >= 16 and summary['runs'] == summary['whole_run_qualified'] else 1


if __name__ == '__main__':sys.exit(main())
