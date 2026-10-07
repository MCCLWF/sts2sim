#!/usr/bin/env python3
"""Run/check the preregistered Q4-r Silent A10 collector-shape replication.

No rule, reward or actor changes. Two new learner seeds each compare 64/480
against 128/240 at identical decisions per update and total update count.
Only final checkpoints are evaluated; failures never restart an arm.
"""
import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import time
from pathlib import Path

import torch

from nosl.model import load

START_SHA = '6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703'
START_DECISIONS = 50_073_600
ADDED_DECISIONS = 5_007_360
LEARNER_SEEDS = (1, 3)
SHAPES = {'C': (64, 80), 'R': (128, 40)}
COMMON = dict(decisions=5_000_000, time_limit_s=1500, workers=6, features=3,
              reset_optimizer=True, seed_prefix='NQ4R', actor_filter=1, gamma=1,
              lam=.95, hp_potential=.2, hp_potential_denominator=70,
              boss_damage=.3, target_kl=.01, lr=.000133, entropy=.01,
              entropy_out=.05, save_every=20)
EVAL_PREFIX = 'NQ4REVAL'
EVAL_RUNS = 2000
ROOT = Path(__file__).resolve().parents[2]


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def json_rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def preflight(start):
    sha = hashlib.sha256(start.read_bytes()).hexdigest()
    assert sha == START_SHA, 'the registered starting checkpoint differs'
    model, blob = load(start, 'cpu')
    assert blob['decisions'] == START_DECISIONS and blob['actor_filter'] == 1
    assert model.features == 3 and model.power_context is None
    assert all(torch.isfinite(t).all() for t in model.state_dict().values() if t.is_floating_point())
    assert all(length * slots * COMMON['workers'] == 30720 for length, slots in SHAPES.values())
    return {'start_sha256': sha, 'start_decisions': blob['decisions'],
            'start_iteration': blob['iteration'], 'features': model.features,
            'actor_filter': blob['actor_filter'], 'learner_seeds': list(LEARNER_SEEDS),
            'common_config': COMMON, 'shapes': SHAPES,
            'eval_prefix': EVAL_PREFIX, 'eval_runs': EVAL_RUNS}


def verify(out, start):
    expected = {f'{EVAL_PREFIX}{i:06d}' for i in range(EVAL_RUNS)}
    summary = {'training': {}, 'evaluation': {}, 'pairs': {}, 'native': {}}
    configs = {}
    for label in ('X1', 'C1', 'R1', 'C3', 'R3'):
        rows = json_rows(out / 'rows' / (label + '.jsonl'))
        assert len(rows) == EVAL_RUNS and {r['seed'] for r in rows} == expected, label
        done = [r for r in rows if r['status'] in (1, 2)]
        summary['evaluation'][label] = {'attempts': len(rows), 'terminals': len(done),
            'faults': len(rows) - len(done), 'victories': sum(r['status'] == 1 for r in rows),
            'mean_floor': sum(r['floor'] for r in done) / len(done) if done else None,
            'act2': sum(r['act'] >= 1 for r in done) / len(done) if done else None,
            'act3': sum(r['act'] >= 2 for r in done) / len(done) if done else None}
        native = json.loads((out / (label + '-native.json')).read_text())
        assert native['passed'], label
        summary['native'][label] = native
        if label == 'X1':
            continue
        shape, seed = label[0], int(label[1:])
        config = json.loads((out / label / 'config.json').read_text())
        for key, value in COMMON.items():
            assert config[key] == value, (label, key, config[key], value)
        assert config['seed'] == seed and config['resume'] == str(start)
        assert not config['power_context'] and config['critic'] == 'shared'
        assert (config['rollout'], config['runs_per_worker']) == SHAPES[shape]
        configs[label] = config
        logs = json_rows(out / label / 'train.jsonl')
        assert len(logs) == 163 and logs[-1]['decisions'] - START_DECISIONS == ADDED_DECISIONS, label
        assert all(not r['failures'] and r['hangs'] == 0 for r in logs), label
        failures = out / label / 'failures.jsonl'
        assert not failures.exists() or not failures.read_text().strip(), label
        model, blob = load(out / label / 'checkpoint.pt', 'cpu')
        assert blob['decisions'] == START_DECISIONS + ADDED_DECISIONS and blob['actor_filter'] == 1
        assert model.features == 3 and model.power_context is None
        assert all(torch.isfinite(t).all() for t in model.state_dict().values() if t.is_floating_point())
        summary['training'][label] = {'new_decisions': ADDED_DECISIONS, 'updates': len(logs),
            'seconds': logs[-1]['wall_s'], 'decisions_per_second': ADDED_DECISIONS / logs[-1]['wall_s'],
            'episodes': sum(r['episodes'] for r in logs),
            'checkpoint_sha256': hashlib.sha256((out / label / 'checkpoint.pt').read_bytes()).hexdigest()}
    summary['config_differences'] = {}
    for seed in LEARNER_SEEDS:
        c, r = configs[f'C{seed}'], configs[f'R{seed}']
        diff = {k: [c.get(k), r.get(k)] for k in c.keys() | r.keys() if c.get(k) != r.get(k)}
        assert set(diff) == {'out', 'rollout', 'runs_per_worker'}, diff
        summary['config_differences'][str(seed)] = diff
        for candidate in (f'C{seed}', f'R{seed}'):
            controls = ('X1',) if candidate.startswith('C') else (f'C{seed}', 'X1')
            for control in controls:
                key = candidate + '-' + control
                pair = json.loads((out / ('paired-' + key + '.json')).read_text())
                assert pair['attempts'] == EVAL_RUNS
                summary['pairs'][key] = pair
    required = [summary['pairs'][f'R{s}-{c}'] for s in LEARNER_SEEDS for c in (f'C{s}', 'X1')]
    passed = all(p['difference'] >= .5 and p['difference_ci95'][0] > 0 and p['excluded_pairs'] == 0 for p in required)
    summary['quality_gate'] = {'passed': passed,
        'criteria': 'Each of learner seeds 1 and 3: R gains >=.5 floors over C and X1, paired CI95 lower>0, no faults',
        'interpretation': 'Single final checkpoint per arm; a pass only opens independent rule/deployment qualification'}
    summary['weights_finite'] = True
    write(out / 'verified-results.json', summary)
    return summary


def run(out, start):
    assert not (out / 'events.jsonl').exists(), 'refuse to restart a formal experiment'
    for label in ('C1', 'R1', 'C3', 'R3'):
        assert not (out / label).exists(), 'refuse to overwrite an existing learning arm'
    spec = preflight(start)
    spec['source_commit'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    spec['python'] = platform.python_version()
    spec['torch'] = torch.__version__
    spec['cuda'] = torch.version.cuda
    spec['gpu'] = torch.cuda.get_device_name(0)
    spec['native_sha256'] = hashlib.sha256((ROOT / 'build/libsts2nosl_c.so').read_bytes()).hexdigest()
    write(out / 'platform.json', spec)
    began = time.monotonic()

    def execute(name, command, timeout):
        with (out / 'events.jsonl').open('a') as events:
            events.write(json.dumps({'stage': name, 'command': command, 'began_seconds': time.monotonic() - began}) + '\n')
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
        with (out / 'events.jsonl').open('a') as events:
            events.write(json.dumps({'stage': name, 'exit': result.returncode, 'ended_seconds': time.monotonic() - began}) + '\n')
        if result.returncode:
            raise RuntimeError(f'{name} failed with exit {result.returncode}; no restart')

    def evaluate(label, checkpoint):
        execute(label + '-eval', [sys.executable, 'tools/nosl/greedy_eval.py', f'{label}={checkpoint}',
            '--runs', str(EVAL_RUNS), '--seed-prefix', EVAL_PREFIX, '--seed-width', '6', '--workers', '6',
            '--out', str(out / 'greedy.json'), '--rows-dir', str(out / 'rows')], 300)
        execute(label + '-native', [sys.executable, 'tools/nosl/check_policy_net.py',
            '--checkpoint', str(checkpoint), '--runs', '4'], 180)
        native = json.loads((out / (label + '-native.log')).read_text())
        assert native['passed']
        write(out / (label + '-native.json'), native)

    evaluate('X1', start)
    for seed in LEARNER_SEEDS:
        for shape, (length, slots) in SHAPES.items():
            label = shape + str(seed)
            command = ['timeout', '--signal=INT', '--kill-after=30s', '1560s', sys.executable,
                       '-m', 'nosl.ppo', '--out', str(out / label), '--resume', str(start),
                       '--seed', str(seed), '--rollout', str(length), '--runs-per-worker', str(slots)]
            for key, value in COMMON.items():
                flag = '--' + key.replace('_', '-')
                command.extend([flag] if value is True else [flag, str(value)])
            execute(label + '-train', command, 1620)
            evaluate(label, out / label / 'checkpoint.pt')
            (out / (label + '-frozen')).write_text('training, evaluation and native check complete\n')
    for seed in LEARNER_SEEDS:
        for candidate in (f'C{seed}', f'R{seed}'):
            for control in (('X1',) if candidate.startswith('C') else (f'C{seed}', 'X1')):
                key = candidate + '-' + control
                execute('compare-' + key, [sys.executable, 'tools/nosl/compare_greedy.py',
                    '--control', str(out / 'rows' / (control + '.jsonl')),
                    '--candidate', str(out / 'rows' / (candidate + '.jsonl')),
                    '--out', str(out / ('paired-' + key + '.json'))], 60)
    verify(out, start)
    (out / 'complete').write_text('complete\n')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--start', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--mode', choices=('preflight', 'run', 'verify'), default='preflight')
    args = p.parse_args()
    torch.set_num_threads(1)
    os.chdir(ROOT)
    args.out.mkdir(parents=True, exist_ok=True)
    if args.mode == 'preflight':
        print(json.dumps(preflight(args.start)))
    elif args.mode == 'verify':
        print(json.dumps(verify(args.out, args.start)))
    else:
        try:
            run(args.out, args.start)
        except Exception as error:
            write(args.out / 'failed.json', {'error': type(error).__name__, 'detail': str(error)})
            raise


if __name__ == '__main__':
    main()
