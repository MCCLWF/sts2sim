#!/usr/bin/env python3
"""Independent public-root, raw reward, fixed-selection and intervention audit."""
import argparse
import collections
import json
from pathlib import Path
import sys

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl import native
from nosl.branch_credit import KEYS, buffers, inputs
from nosl.endturn import write
from nosl.endturn_action import load, source
from nosl.endturn_credit import public_digest
from nosl.teacher_distill import sha

ARMS = ('B1', 'C1', 'B3', 'C3')
START = ROOT / 'artifacts/nosl-q4r/start.pt'
HEADS = ROOT / 'artifacts/nosl-q4ac/qualification'
LEARNING = ROOT / 'artifacts/nosl-q4ae/learning'


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def reward_verify(row):
    reference = paid = 0.;previous = None;total = 0.
    facts = row['reward_steps'];assert len(facts) == len(row['branch_actions'])
    if facts and facts[0][2]:
        reference = max(facts[0][3], facts[0][4], 1)
        paid = np.clip(1 - facts[0][3] / reference, 0, 1)
    for record in facts:
        f0, hp0, boss0, enemy0, max0, f1, hp1, boss1, enemy1, max1, status, reward = record
        assert all(type(x) is int for x in record[:-1]) and np.isfinite(reward)
        if previous is not None:assert record[:5] == previous[5:10] and previous[10] == native.RUNNING
        if boss0 and reference == 0:reference, paid = max(enemy0, max0, 1), 0.
        ended = status != native.RUNNING
        fighting = reference > 0 and not ended;won = fighting and not boss1
        share = 1 - paid if won else min(max(enemy0 - enemy1, 0) / max(reference, 1), 1 - paid)
        if not fighting:share = 0.
        base = np.float32((f1 - f0) / 50 + .2 * hp1 / 70 - .2 * hp0 / 70)
        expected = np.float32(float(base) + .3 * share);paid += share
        if won:reference = 0.
        if ended:
            bonus = .3 * (1 - paid) if reference > 0 and status == native.VICTORY else 0.
            reference = 0.
            expected = np.float32((f1 - f0) / 50 + (status == native.VICTORY) - .2 * hp0 / 70 + bonus) if status in (native.VICTORY, native.DEATH) else np.float32(0.)
        assert float(expected) == reward, (row['arm'], row['seed'], row['branch'], expected, reward)
        total += reward;previous = record
    qualified = row['status'] in (native.DEATH, native.VICTORY)
    assert row['mc_qualified'] == qualified
    if qualified:
        assert previous is not None and previous[10] == row['status']
        assert previous[5:7] == [row['floor'], row['hp']] and total == row['return_from_root']
    else:assert row['return_from_root'] is None
    return len(facts)


def interval(values):
    values = np.asarray(values, np.float64)
    if not len(values):return {'mean': None, 'ci95': None}
    rng = np.random.default_rng(20261006);means = []
    for _ in range(40):means.extend(values[rng.integers(0, len(values), (250, len(values)))].mean(1))
    return {'mean': float(values.mean()), 'ci95': np.quantile(means, [.025, .975]).tolist(),
            'positive': int((values > 1e-5).sum()), 'negative': int((values < -1e-5).sum()),
            'ties': int((np.abs(values) <= 1e-5).sum())}


def audit(data, out):
    data = Path(data)
    config = json.loads((data / 'config.json').read_text());summary = json.loads((data / 'summary.json').read_text())
    assert config['block'] == 64 and config['continuation_batch'] == 1 and config['device'] == 'cpu' and not config['cloud_used']
    assert config['seconds'] == 900 and summary['seconds'] <= 900
    assert sha(data / 'public-roots.npz') == summary['public_roots_sha256']
    assert sha(ROOT / 'build/libsts2nosl_c.so') == config['native_sha256']
    assert sha(ROOT / 'tools/nosl/endturn_visit_credit.py') == config['source_sha256']
    visits, roots, attempts, outcomes = (lines(data / name) for name in ('visits.jsonl', 'roots.jsonl', 'attempts.jsonl', 'outcomes.jsonl'))
    expected_seeds = [f'NQ4AEEVAL{i:06d}' for i in range(128)]
    assert config['seeds'] == expected_seeds and config['arms'] == list(ARMS)
    pair = {(r['arm'], r['seed'], r['branch']): r for r in outcomes};assert len(pair) == len(outcomes)
    first, states = {}, collections.defaultdict(collections.Counter)
    expected_original_decisions = 0
    for arm in ARMS:
        prior_path = ROOT / 'artifacts/nosl-q4ae/experiment/evaluation' / (arm + '.jsonl')
        assert sha(prior_path) == config['prior_sha256'][arm]
        assert sha(LEARNING / arm / 'checkpoint.pt') == config['head_sha256'][arm]
        prior = lines(prior_path)[:128]
        assert [r['seed'] for r in prior] == expected_seeds
        selected = [r for r in attempts if r['arm'] == arm]
        assert [r['seed'] for r in selected] == expected_seeds
        for a, original in zip(selected, prior):
            assert a['reproduced'] and a['mc_qualified']
            for key in ('status', 'end', *native.RUN_INFO_FIELDS):assert a[key] == original[key]
        expected_original_decisions += sum(len(r['action_rows']) for r in prior)
        last = {}
        for row in [r for r in visits if r['arm'] == arm]:
            seed = row['seed'];index = expected_seeds.index(seed)
            assert row['decision'] > last.get(seed, -1);last[seed] = row['decision']
            assert row['choice'] == prior[index]['action_rows'][row['decision']]
            assert np.isfinite(row['stop']) and np.isfinite(row['initial_stop'])
            assert (row['choice_kind'] == 3) == (row['stop'] > 0)
            assert (row['initial_kind'] == 3) == (row['initial_stop'] > 0)
            changed = row['choice'] != row['initial_choice']
            assert changed == ((row['choice_kind'] == 3) != (row['initial_kind'] == 3))
            group = '1-3' if row['floor'] < 4 else '4-9' if row['floor'] < 10 else '10+'
            c = states[(arm, group)];c['states'] += 1;c['changed'] += changed
            c['updated_stop'] += row['choice_kind'] == 3;c['initial_stop'] += row['initial_kind'] == 3
            c['stop_instead_of_continue'] += changed and row['choice_kind'] == 3
            c['continue_instead_of_stop'] += changed and row['initial_kind'] == 3
            if changed and (arm, seed) not in first:first[(arm, seed)] = row
        selected_roots = [r for r in roots if r['arm'] == arm]
        assert len({r['seed'] for r in selected_roots}) == len(selected_roots)
        assert {r['seed'] for r in selected_roots} == {s for s in expected_seeds if (arm, s) in first}
        for i, root in enumerate(selected_roots):
            assert root['audit_root'] == (i < 4)
            for key, value in first[(arm, root['seed'])].items():assert root[key] == value
            assert root['prefix'] == prior[expected_seeds.index(root['seed'])]['action_rows'][:root['decision']]
            actual = root['original_terminal'];original = prior[expected_seeds.index(root['seed'])]
            for key in ('status', 'end', *native.RUN_INFO_FIELDS, 'action_rows'):assert actual[key] == original[key]
            a, b = (pair[(arm, root['seed'], branch)] for branch in ('updated', 'initial_once'))
            for row, action in ((a, root['choice']), (b, root['initial_choice'])):
                assert row['action_rows'] == root['prefix'] + row['branch_actions']
                assert row['branch_actions'] and row['branch_actions'][0] == action
            complete = a['mc_qualified'] and b['mc_qualified'];assert root['complete'] == complete
            assert root['floor_delta'] == (b['floor'] - a['floor'] if complete else None)
            assert root['return_delta'] == (b['return_from_root'] - a['return_from_root'] if complete else None)
            assert root['canonical_updated_matches_prior'] == (a['action_rows'] == original['action_rows'])
    assert len(attempts) == summary['attempts'] == summary['reproduced'] == 512
    assert summary['controls']['matched_original_actions'] == expected_original_decisions
    assert len(roots) == summary['roots'] == len(first)
    assert summary['controls']['frozen_fork_after_original_terminal'] == 2 * len(roots)
    assert len(outcomes) == summary['outcomes'] == 2 * len(roots)
    reward_steps = sum(reward_verify(r) for r in outcomes)
    public_checks = 0
    with np.load(data / 'public-roots.npz', allow_pickle=False) as raw:
        assert set(raw.files) == set(KEYS)
        tables = {k: raw[k] for k in KEYS};assert len(tables['counts']) == len(roots)
        for i, root in enumerate(roots):
            row = {key: tables[key][i] for key in KEYS}
            assert public_digest(row) == root['public_sha256']
            assert int(row['global'][6]) == root['floor'] and int(row['global'][4]) == root['act']
            end = np.flatnonzero(row['actions'][:row['counts'][1], 0] == 3)
            assert row['global'][11] and len(end) == 1 and row['counts'][1] >= 2
            assert row['actions'][root['choice'], 0] == root['choice_kind']
            assert row['actions'][root['initial_choice'], 0] == root['initial_kind']
        for arm in ARMS:
            model = load(LEARNING / arm / 'checkpoint.pt', START, HEADS)[0]
            reference = source(START, HEADS, int(arm[1]))
            ids = [i for i, r in enumerate(roots) if r['arm'] == arm]
            for first_index in range(0, len(ids), 64):
                chosen = np.asarray(ids[first_index:first_index + 64]);args = inputs(tables, chosen, 'cpu')
                with torch.no_grad():
                    updated, _ = model(*args);initial, _ = reference(*args)
                for j, index in enumerate(chosen):
                    root = roots[index];actions = tables['actions'][index]
                    # Score numeric drift across padding/batches is tolerated;
                    # the meaningful EndTurn-vs-continue decision is exact.
                    assert int(actions[int(updated[j].argmax()), 0] == 3) == (root['choice_kind'] == 3)
                    assert int(actions[int(initial[j].argmax()), 0] == 3) == (root['initial_kind'] == 3)
                    public_checks += 1
    result = {'independent_verification': True, 'roots': len(roots), 'original_runs': len(attempts),
        'original_policy_decisions': expected_original_decisions, 'active_visits': len(visits),
        'public_root_forward_checks': public_checks, 'reward_steps_verified_float32_exact': reward_steps,
        'states': {arm: {g: dict(states[(arm, g)]) for g in ('1-3', '4-9', '10+')} for arm in ARMS},
        'arms': {}}
    prior_roots = lines(ROOT / 'artifacts/nosl-q4ad/data/roots.jsonl')
    training = [r for r in prior_roots if r['complete'] and not r['held']]
    assert len(prior_roots) == 450 and len(training) == 352 and min(r['floor'] for r in training) >= 4
    result['training_root_coverage'] = {'all_roots': len(prior_roots), 'training_roots': len(training),
        'floor_1_3': sum(r['floor'] < 4 for r in training),
        'floor_4_9': sum(4 <= r['floor'] < 10 for r in training),
        'floor_10_plus': sum(r['floor'] >= 10 for r in training),
        'floor_counts': dict(collections.Counter(r['floor'] for r in training))}
    early_arms = 0
    for arm in ARMS:
        selected = [r for r in roots if r['arm'] == arm];complete = [r for r in selected if r['complete']]
        early = sum(r['floor'] < 4 for r in selected);early_arms += bool(selected and early > len(selected) / 2)
        a = [pair[(arm, r['seed'], 'updated')] for r in complete]
        b = [pair[(arm, r['seed'], 'initial_once')] for r in complete]
        result['arms'][arm] = {'roots': len(selected), 'complete_roots': len(complete),
            'incomplete_pairs': [r['seed'] for r in selected if not r['complete']],
            'first_difference_floor_1_3': early, 'first_difference_floor_counts': dict(collections.Counter(r['floor'] for r in selected)),
            'canonical_updated_matches_prior': sum(r['canonical_updated_matches_prior'] for r in selected),
            'floor_delta': interval([r['floor_delta'] for r in complete]),
            'return_delta': interval([r['return_delta'] for r in complete]),
            'updated': {'floor_mean': float(np.mean([r['floor'] for r in a])) if a else None,
                'victories': sum(r['end'] == 'victory' for r in a), 'act2': sum(r['act'] >= 1 for r in a), 'act3': sum(r['act'] >= 2 for r in a)},
            'initial_once': {'floor_mean': float(np.mean([r['floor'] for r in b])) if b else None,
                'victories': sum(r['end'] == 'victory' for r in b), 'act2': sum(r['act'] >= 1 for r in b), 'act3': sum(r['act'] >= 2 for r in b)}}
        # Descriptive follow-up only; this does not change the registered gate.
        visited = [r for r in visits if r['arm'] == arm]
        changed = np.asarray([r['stop'] for r in visited]);initial = np.asarray([r['initial_stop'] for r in visited])
        probability = np.exp(-np.logaddexp(0, -initial))
        divergence = probability * (-np.logaddexp(0, -initial) + np.logaddexp(0, -changed)) + (
            1 - probability) * (-np.logaddexp(0, initial) + np.logaddexp(0, changed))
        training_log = lines(LEARNING / arm / 'training.jsonl')[-1]
        result['arms'][arm]['descriptive_followup'] = {
            'visited_initial_to_updated_Bernoulli_kl': float(divergence.mean()),
            'train_root_initial_to_updated_Bernoulli_kl': training_log['all_train_anchor_kl'],
            'initial_continue_updated_end_roots': sum(r['choice_kind'] == 3 and r['initial_kind'] != 3 for r in selected),
            'updated_end_with_positive_energy_roots': sum(r['choice_kind'] == 3 and r['energy'] > 0 for r in selected)}
    result['early_coverage_priority_gate'] = summary['all_prior_reproduced'] and summary['all_root_terminal'] and early_arms >= 3 and all(
        result['arms'][arm]['floor_delta']['mean'] is not None and result['arms'][arm]['floor_delta']['mean'] > 0 and
        result['arms'][arm]['floor_delta']['ci95'][0] > 0 for arm in ('C1', 'C3'))
    write(out, result);print(json.dumps(result), flush=True);return 0


def direct(data, out):
    data, out = Path(data), Path(out);out.mkdir(parents=True, exist_ok=False)
    roots = [r for r in lines(data / 'roots.jsonl') if r['audit_root']]
    assert len(roots) == 16 and all(sum(r['arm'] == arm for r in roots) == 4 for arm in ARMS)
    checks = []
    for root in roots:
        path = data / f'{root["arm"]}-{root["seed"]}-fork.npz'
        with np.load(path, allow_pickle=False) as frozen:
            for label, action in (('updated', root['choice']), ('initial', root['initial_choice'])):
                batch = native.Batch(1, 256, 256, 30000);batch.set_filter(1)
                assert batch.reset(0, root['seed'], 10) == native.RUNNING
                tables = buffers(1, 256, 256)
                batch.step(np.asarray([-1], np.int32), *tables.values())
                for previous in root['prefix']:batch.step(np.asarray([previous], np.int32), *tables.values())
                for key in tables:assert np.array_equal(tables[key][0], frozen['root_' + key]), (root['arm'], root['seed'], key)
                batch.step(np.asarray([action], np.int32), *tables.values())
                for key in tables:assert np.array_equal(tables[key][0], frozen[label + '_' + key]), (root['arm'], root['seed'], label, key)
                checks.append({'arm': root['arm'], 'seed': root['seed'], 'branch': label, 'root_and_first_after_equal': True})
    result = {'independent_verification': True, 'roots': len(roots), 'checks': checks,
              'fresh_prefix_and_first_action_matches': len(checks)}
    write(out / 'result.json', result);print(json.dumps(result), flush=True);return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('audit', 'direct'));parser.add_argument('--data', required=True)
    parser.add_argument('--out', required=True);args = parser.parse_args();torch.set_num_threads(1)
    return audit(args.data, args.out) if args.mode == 'audit' else direct(args.data, args.out)


if __name__ == '__main__':sys.exit(main())
