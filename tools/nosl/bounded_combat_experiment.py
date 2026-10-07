#!/usr/bin/env python3
"""Preregistered two-seed matched original versus bounded combat updates."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from nosl.bounded_combat import collect, combat_models, train, KL_CAP
from nosl.episodic_strategy import START_SHA, digest, RoutedPolicy
from nosl.model import load


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def evaluate(checkpoint, out, device):
    outside, start, baseline, _ = combat_models(checkpoint, device)
    policies = {'X1': baseline};trained = {}
    for seed in (1, 3):
        for credit in ('U', 'B'):
            label = f'{credit}{seed}';d = out/label
            trained[label] = json.loads((d/'summary.json').read_text())
            actor, meta = load(d/'checkpoint.pt', device);actor.eval()
            assert meta['actor_filter'] == 1 and meta['outside_checkpoint_sha256'] == START_SHA
            policies[label] = RoutedPolicy(actor, outside)
    evaluation = out/'evaluation';evaluation.mkdir(exist_ok=False)
    seeds = [f'NQ4ABEVAL{i:06d}' for i in range(1024)]
    deadline = time.monotonic()+1200;rows = {}
    for label, policy in policies.items():
        attempts = []
        with (evaluation/(label+'.jsonl')).open('w') as stream:
            for begin in range(0, len(seeds), 64):
                selected = seeds[begin:begin+64]
                if time.monotonic() >= deadline:
                    block = [{'seed': s, 'status': 0, 'end': 'not_started', 'mc_qualified': False,
                              'error': 'registered evaluation deadline', 'action_rows': []} for s in selected]
                else:
                    block = collect(policy, selected, device, sample=False, credit='C',
                        deadline=deadline, keep_public=False)['outcomes']
                for row in block:stream.write(json.dumps(row)+'\n')
                stream.flush();attempts.extend(block)
                print(json.dumps({'evaluation': label, 'attempts': len(attempts),
                    'terminals': sum(r['mc_qualified'] for r in attempts)}), flush=True)
        rows[label] = attempts
    result = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'baseline_checkpoint_sha256': START_SHA, 'outside_parameter_sha256': digest(outside),
        'trained': trained, 'policies': {}, 'paired': {}}
    for label, attempts in rows.items():
        good = [r for r in attempts if r['mc_qualified']]
        result['policies'][label] = {'attempts': len(attempts), 'terminals': len(good),
            'ends': dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean': float(np.mean([r['floor'] for r in good])) if good else None,
            'act2': sum(r['act'] >= 1 for r in good), 'act3': sum(r['act'] >= 2 for r in good),
            'victories': sum(r['end'] == 'victory' for r in good)}
    complete = all(r['mc_qualified'] for arm in rows.values() for r in arm)
    if complete:
        for seed in (1, 3):
            for reference in ('X1', f'U{seed}'):
                label = f'B{seed}';delta = np.asarray([r['floor']-c['floor'] for r,c in zip(rows[label], rows[reference])])
                rng = np.random.default_rng(20261006)
                ci = np.quantile(delta[rng.integers(0, 1024, (10000, 1024))].mean(1), [.025, .975])
                result['paired'][label+'-'+reference] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                    'gate': bool(delta.mean() >= .5 and ci[0] > 0 and trained[label]['status'] == 'complete'
                        and trained[label]['faults'] == 0 and trained[f'U{seed}']['status'] == 'complete'
                        and trained[f'U{seed}']['faults'] == 0 and trained[label]['accepted_updates'] >= 48)}
    acceptance = {}
    for seed in (1, 3):
        label = f'B{seed}'
        logs = [json.loads(line) for line in (out/label/'train.jsonl').read_text().splitlines()]
        acceptance[label] = bool(len(logs) == 64 and trained[label]['accepted_updates'] >= 48
            and all(r['retained']['exact_kl_mean'] <= KL_CAP for r in logs)
            and all(r['accepted'] == (r['candidate']['exact_kl_mean'] <= KL_CAP) for r in logs))
    result['bounded_acceptance_limits'] = acceptance
    result['all_evaluation_runs_terminal'] = complete
    result['gate_passed'] = bool(complete and all(r['gate'] for r in result['paired'].values())
        and all(acceptance.values()) and max(result['policies'][f'B{seed}']['act3'] for seed in (1,3)) >= result['policies']['X1']['act3'])
    write(out/'results.json', result)
    for label in trained:
        selected = rows[label][:2];good = [r for r in rows[label] if r['mc_qualified']]
        if good:
            deepest = max(good, key=lambda r: r['floor'])
            if deepest['seed'] not in {r['seed'] for r in selected}:selected = selected+[deepest]
        with (out/(label+'-normal-inputs.jsonl')).open('w') as stream:
            for row in selected:stream.write(json.dumps(row)+'\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', required=True);parser.add_argument('--out', required=True)
    parser.add_argument('--device', default='cuda');args = parser.parse_args()
    torch.set_num_threads(1);out = Path(args.out);out.mkdir(parents=True, exist_ok=False)
    write(out/'experiment.json', {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'baseline_checkpoint_sha256': START_SHA, 'native_sha256': hashlib.sha256((ROOT/'build/libsts2nosl_c.so').read_bytes()).hexdigest(),
        'seeds': [1,3], 'arms': ['U','B'], 'updates': 64, 'episodes_per_update': 32,
        'training_seconds_per_arm': 900, 'evaluation_seconds': 1200, 'evaluation_runs': 1024,
        'evaluation_prefix': 'NQ4ABEVAL', 'device': args.device, 'precision': 'float32'})
    for seed in (1,3):
        for credit in ('U','B'):
            label = f'{credit}{seed}'
            print(json.dumps({'training_arm': label, 'result': train(args.checkpoint, out/label, args.device, seed, credit)}), flush=True)
    result = evaluate(args.checkpoint, out, args.device);print(json.dumps(result), flush=True)
    return 0 if result['gate_passed'] else 1


if __name__ == '__main__':sys.exit(main())
