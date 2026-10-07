#!/usr/bin/env python3
"""Independently audit frozen budgets, raw credit transitions and paired results."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from check_encounter_credit import independent
from nosl.encounter_credit import combat_models, HP_SCALE, TURN_COST
from nosl.episodic_strategy import START_SHA, digest
from nosl.model import load


def read(path):return json.loads(path.read_text())
def lines(path):return [json.loads(s) for s in path.read_text().splitlines() if s]


def verify(checkpoint, out):
    outside, _, _, _ = combat_models(checkpoint, 'cpu');original = digest(outside)
    experiment = read(out/'experiment.json');expected = read(out/'results.json')
    assert experiment['baseline_checkpoint_sha256'] == START_SHA
    assert experiment['seeds'] == [1,3] and experiment['credits'] == ['C','R']
    assert experiment['updates'] == 64 and experiment['episodes_per_update'] == 32
    assert experiment['training_seconds_per_arm'] == 900 and experiment['evaluation_seconds'] == 1200
    assert experiment['evaluation_runs'] == 1024 and experiment['precision'] == 'float32'
    training = {};hashes = {};configs = [];first_batches = {}
    for seed in (1,3):
        for credit in ('C','R'):
            label = f'{credit}{seed}';d = out/label;c = read(d/'config.json');s = read(d/'summary.json')
            assert c['seed'] == seed and c['credit'] == credit and c['prefix'] == 'NQ4ZTRAIN'
            assert c['checkpoint_sha256'] == START_SHA and c['source_commit'] == experiment['source_commit']
            assert c['updates'] == 64 and c['episodes_per_update'] == 32 and c['seconds_limit'] == 900
            assert c['max_steps'] == 30000 and c['precision'] == 'float32' and c['actor_filter'] == 1
            assert c['gamma'] == 1 and c['hp_scale'] == HP_SCALE and c['end_turn_cost'] == TURN_COST
            assert c['survived_bonus'] == 1 and c['death_bonus'] == -1
            assert c['lr'] == .000133 and c['clip'] == .2 and c['epochs'] == 2 and c['minibatch'] == 256
            assert c['entropy'] == .05 and c['value_coef'] == .5 and c['grad_norm'] == .5
            configs.append({k:v for k,v in c.items() if k not in ('seed','credit')})
            episodes = lines(d/'episodes.jsonl');updates = lines(d/'train.jsonl')
            first_batches[label] = [{k:v for k,v in r.items() if k != 'learning_returns'} for r in episodes[:32]]
            seeds = [f'NQ4ZTRAIN{seed}{i:04d}{slot:02d}' for i in range(64) for slot in range(32)]
            assert [r['seed'] for r in episodes] == seeds[:len(episodes)] and len(episodes)%32 == 0
            for row in episodes:
                assert row['ascension'] == 10 and row['actor_filter'] == 1
                assert row['mc_qualified'] == (row['status'] in (1,2))
                assert sum(row['routing'].values()) == len(row['action_rows']) == len(row['credit_steps']) == len(row['whole_rewards'])
                assert row['routing'].get('combat',0) == sum(t[0] for t in row['credit_steps'])
                assert row['learning_times'] == sorted(set(row['learning_times']))
                assert row['combat_rows'] == len(row['learning_times'])
                assert all(row['credit_steps'][i][0] == 1 for i in row['learning_times'])
                if row['mc_qualified']:assert len(row['learning_returns']) == len(row['learning_times'])
                independent(row, credit)
            assert s['attempts'] == len(episodes) and s['decisions'] == sum(len(r['action_rows']) for r in episodes)
            assert s['faults'] == sum(not r['mc_qualified'] for r in episodes)
            assert [r['iteration'] for r in updates] == list(range(1,len(updates)+1))
            assert s['updates'] == len(updates) and all(r['attempts'] == 32*r['iteration'] for r in updates)
            assert s['combat_rows'] == sum(r['rows'] for r in updates)
            assert s['combat_rows'] == sum(r['combat_rows'] for r in episodes[:32*s['updates']] if r['mc_qualified'])
            assert s['optimizer_steps'] == sum(r['optimizer_steps'] for r in updates)
            assert all(np.isfinite(v) for r in updates for v in r.values() if isinstance(v,(int,float)))
            assert s['outside_parameters_unchanged'] and s['outside_parameter_sha256'] == c['outside_parameter_sha256'] == original
            model, blob = load(d/'checkpoint.pt', 'cpu')
            assert model.config == outside.config and digest(model) == s['combat_parameter_sha256']
            assert blob['outside_checkpoint_sha256'] == START_SHA and blob['actor_filter'] == 1 and blob['credit'] == credit
            assert blob['learning_scope'] == 'combat only; must deploy with frozen outside X1'
            assert blob['iteration'] == s['updates'] and blob['decisions'] == s['decisions'] and blob['combat_rows'] == s['combat_rows']
            assert all(torch.isfinite(t).all() for t in model.state_dict().values())
            if s['status'] == 'complete':assert len(episodes) == 2048 and len(updates) == 64
            else:assert len(episodes) <= 2048 and len(updates) < 64
            training[label] = s;hashes[label] = hashlib.sha256((d/'checkpoint.pt').read_bytes()).hexdigest()
    assert all(c == configs[0] for c in configs)
    for seed in (1,3):
        assert len(first_batches[f'C{seed}']) == len(first_batches[f'R{seed}']) == 32
        assert first_batches[f'C{seed}'] == first_batches[f'R{seed}']
    seeds = [f'NQ4ZEVAL{i:06d}' for i in range(1024)]
    rows = {label:lines(out/'evaluation'/(label+'.jsonl')) for label in ('X1','C1','R1','C3','R3')}
    policies = {};paired = {}
    for label, attempts in rows.items():
        assert [r['seed'] for r in attempts] == seeds
        for r in attempts:
            assert r['mc_qualified'] == (r['status'] in (1,2))
            if 'routing' in r:assert sum(r['routing'].values()) == len(r['action_rows'])
        good = [r for r in attempts if r['mc_qualified']]
        policies[label] = {'attempts': len(attempts), 'terminals': len(good),
            'ends': dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean': float(np.mean([r['floor'] for r in good])) if good else None,
            'act2': sum(r['act'] >= 1 for r in good), 'act3': sum(r['act'] >= 2 for r in good),
            'victories': sum(r['end'] == 'victory' for r in good)}
    complete = all(r['mc_qualified'] for arm in rows.values() for r in arm)
    if complete:
        for seed in (1,3):
            for reference in ('X1',f'C{seed}'):
                label = f'R{seed}';delta = np.asarray([r['floor'] for r in rows[label]])-np.asarray([r['floor'] for r in rows[reference]])
                rng = np.random.default_rng(20261005);indices = rng.integers(0,1024,(10000,1024))
                ci = np.quantile(delta[indices].mean(1),[.025,.975])
                paired[label+'-'+reference] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                    'gate': bool(delta.mean() >= .5 and ci[0] > 0 and training[label]['status'] == 'complete'
                        and training[label]['faults'] == 0 and training[f'C{seed}']['status'] == 'complete'
                        and training[f'C{seed}']['faults'] == 0)}
    gate = bool(complete and all(r['gate'] for r in paired.values())
        and max(policies[f'R{seed}']['act3'] for seed in (1,3)) >= policies['X1']['act3'])
    assert expected['baseline_checkpoint_sha256'] == START_SHA and expected['outside_parameter_sha256'] == original
    assert expected['trained'] == training and expected['policies'] == policies and expected['paired'] == paired
    assert expected['all_evaluation_runs_terminal'] == complete and expected['gate_passed'] == gate
    return {'independent_verification': True, 'raw_credit_formulas_verified': True,
        'matched_formal_first_batches': True,
        'training': training, 'checkpoint_sha256': hashes, 'evaluation': policies, 'paired': paired,
        'all_evaluation_runs_terminal': complete, 'gate_passed': gate, 'outside_parameter_sha256': original}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', required=True);parser.add_argument('--experiment', required=True)
    parser.add_argument('--out', required=True);args = parser.parse_args();torch.set_num_threads(1)
    result = verify(Path(args.checkpoint), Path(args.experiment))
    Path(args.out).write_text(json.dumps(result, indent=2)+'\n');print(json.dumps(result))


if __name__ == '__main__':main()
