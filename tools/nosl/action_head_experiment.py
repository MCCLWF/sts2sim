#!/usr/bin/env python3
"""Fixed four-arm action-head learning and paired whole-run strength audit."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

import numpy as np
import torch

from nosl.action_head import START_SHA, DATA_SHA, frozen_digest
from nosl.model import load
from nosl.teacher_distill import sha

ROOT = Path(__file__).resolve().parents[2]
ARMS = ('C1', 'P1', 'C3', 'P3')
RUNS = 512
PREFIX = 'NQ4UEVAL'


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def json_rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def verify(out, start):
    model, _ = load(start, 'cpu')
    assert sha(start) == START_SHA
    frozen = frozen_digest(model)
    platform_info = read(out / 'platform.json')
    assert platform_info['start_sha256'] == START_SHA and platform_info['data_sha256'] == DATA_SHA
    assert tuple(platform_info['arms']) == ARMS and platform_info['eval_prefix'] == PREFIX and platform_info['runs'] == RUNS
    cache = read(out / 'cache/cache.json')
    assert cache['start_sha256'] == START_SHA and cache['data_sha256'] == DATA_SHA
    assert cache['cache_sha256'] == sha(out / 'cache/cache.npz')
    assert cache['cache_forward_max_difference'] == 0 and cache['frozen_parameter_sha256'] == frozen
    assert cache['teacher_rows'] == 1248 and cache['anchor_rows'] == 1511
    expected = {f'{PREFIX}{i:06d}' for i in range(RUNS)}
    summary = {'training': {}, 'evaluation': {}, 'pairs': {}, 'native': {}}
    records = {}
    for label in ('X1', *ARMS):
        rows = json_rows(out / 'rows' / (label + '.jsonl'))
        assert len(rows) == RUNS and {r['seed'] for r in rows} == expected
        records[label] = {r['seed']: r for r in rows}
        done = [r for r in rows if r['status'] in (1, 2)]
        summary['evaluation'][label] = {'attempts': len(rows), 'terminals': len(done), 'faults': len(rows) - len(done),
            'victories': sum(r['status'] == 1 for r in rows),
            'mean_floor': float(np.mean([r['floor'] for r in done])) if done else None,
            'act2': sum(r['act'] >= 1 for r in done), 'act3': sum(r['act'] >= 2 for r in done)}
        summary['native'][label] = read(out / (label + '-native.json'))
        assert summary['native'][label]['passed'], label
        if label == 'X1':
            continue
        config = read(out / label / 'config.json')
        assert config['start_sha256'] == START_SHA and config['data_sha256'] == DATA_SHA
        assert config['seed'] == int(label[1]) and config['action_interaction'] == label.startswith('P')
        assert config['epochs'] == 64 and config['lr'] == 3e-4 and config['minibatch'] == 128
        assert config['kl_coef'] == .5 and config['grad_norm'] == 1.
        assert config['cache_sha256'] == cache['cache_sha256'] and config['frozen_parameter_sha256'] == frozen
        assert config['train_rows'] == 1216 and config['held_rows'] == 32 and config['train_roots'] == 55
        expected_trainable = ['policy.0.weight', 'policy.0.bias', 'policy.2.weight', 'policy.2.bias']
        if label.startswith('P'):
            expected_trainable.insert(0, 'action_interaction.weight')
        assert config['trainable_parameters'] == expected_trainable
        logs = json_rows(out / label / 'training.jsonl')
        assert [r['epoch'] for r in logs] == list(range(65))
        result = read(out / label / 'training-result.json')
        assert result['updates'] == 640 and result['frozen_parameters_unchanged']
        learned, blob = load(out / label / 'checkpoint.pt', 'cpu')
        assert blob['teacher_epoch'] == 64 and blob['actor_filter'] == 1 and learned.features == 3
        assert (learned.action_interaction is not None) == label.startswith('P')
        assert learned.power_context is None and frozen_digest(learned) == frozen
        assert result['checkpoint_sha256'] == sha(out / label / 'checkpoint.pt')
        assert all(torch.isfinite(value).all() for value in learned.state_dict().values())
        summary['training'][label] = result
    for seed in (1, 3):
        for candidate, control in ((f'C{seed}', 'X1'), (f'P{seed}', 'X1'), (f'P{seed}', f'C{seed}')):
            valid = sorted(s for s in expected if records[candidate][s]['status'] in (1, 2)
                           and records[control][s]['status'] in (1, 2))
            delta = np.asarray([records[candidate][s]['floor'] - records[control][s]['floor'] for s in valid], np.float64)
            assert len(delta) >= 2
            mean = float(delta.mean())
            standard_error = float(delta.std(ddof=1) / np.sqrt(len(delta)))
            interval = [mean - 1.96 * standard_error, mean + 1.96 * standard_error]
            pair = {'attempts': RUNS, 'paired_terminals': len(valid), 'excluded_pairs': RUNS - len(valid),
                    'difference': mean, 'standard_error': standard_error, 'difference_ci95': interval,
                    'passed': len(valid) == RUNS and mean >= .5 and interval[0] > 0}
            summary['pairs'][candidate + '-' + control] = pair
    required = [summary['pairs'][f'P{s}-{c}']['passed'] for s in (1, 3) for c in (f'C{s}', 'X1')]
    controls = [summary['pairs'][f'C{s}-X1']['passed'] for s in (1, 3)]
    summary['quality_gate'] = {'interaction_passed': all(required), 'head_only_passed': all(controls),
        'criteria': 'Both learner seeds: >=.5 floor increase, paired CI95 lower>0, no faults; P versus C and X1, C versus X1',
        'scope': 'Only opens independent subsequent learning/normal-rule/deployment qualification, not high-level success'}
    summary['frozen_non_scoring_parameters_sha256'] = frozen
    write(out / 'verified-results.json', summary)
    return summary


def run(out, start, data):
    assert not (out / 'events.jsonl').exists(), 'refuse to restart formal experiment'
    assert sha(start) == START_SHA and sha(data / 'public-tables.npz') == DATA_SHA
    assert torch.cuda.is_available(), 'formal driver requires the authorized cloud GPU'
    spec = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
            'start_sha256': START_SHA, 'data_sha256': DATA_SHA, 'arms': ARMS, 'eval_prefix': PREFIX, 'runs': RUNS,
            'python': platform.python_version(), 'torch': torch.__version__, 'cuda': torch.version.cuda,
            'gpu': torch.cuda.get_device_name(0), 'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so')}
    write(out / 'platform.json', spec)
    began = time.monotonic()

    def execute(stage, command, timeout):
        with (out / 'events.jsonl').open('a') as stream:
            stream.write(json.dumps({'stage': stage, 'command': command, 'began_seconds': time.monotonic() - began}) + '\n')
        with (out / (stage + '.log')).open('w') as log:
            completed = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=timeout, check=False)
        with (out / 'events.jsonl').open('a') as stream:
            stream.write(json.dumps({'stage': stage, 'exit': completed.returncode, 'ended_seconds': time.monotonic() - began}) + '\n')
        assert completed.returncode == 0, (stage, completed.returncode)

    execute('cache', [sys.executable, '-m', 'nosl.action_head', 'cache', '--start', str(start),
                     '--data', str(data), '--out', str(out / 'cache'), '--device', 'cuda'], 180)
    checkpoints = {'X1': start}
    for label in ARMS:
        command = [sys.executable, '-m', 'nosl.action_head', 'train', '--start', str(start),
                   '--cache', str(out / 'cache'), '--out', str(out / label), '--seed', label[1], '--device', 'cuda']
        if label.startswith('P'):
            command += ['--interaction']
        execute(label + '-train', command, 180)
        checkpoints[label] = out / label / 'checkpoint.pt'
    for label, checkpoint in checkpoints.items():
        execute(label + '-eval', [sys.executable, 'tools/nosl/greedy_eval.py', f'{label}={checkpoint}',
            '--runs', str(RUNS), '--seed-prefix', PREFIX, '--seed-width', '6', '--workers', '6',
            '--out', str(out / 'greedy.json'), '--rows-dir', str(out / 'rows')], 300)
        execute(label + '-native', [sys.executable, 'tools/nosl/check_policy_net.py', '--checkpoint', str(checkpoint), '--runs', '4'], 180)
        native = read(out / (label + '-native.log'))
        assert native['passed']
        write(out / (label + '-native.json'), native)
    summary = verify(out, start)
    (out / 'complete').write_text('complete\n')
    print(json.dumps(summary))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('run', 'verify'), default='verify')
    parser.add_argument('--start', required=True, type=Path)
    parser.add_argument('--data', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    os.chdir(ROOT)
    args.out.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    if args.mode == 'run':
        try:
            run(args.out, args.start, args.data)
        except Exception as error:
            write(args.out / 'failed.json', {'error': type(error).__name__, 'detail': str(error)})
            raise
    else:
        print(json.dumps(verify(args.out, args.start)))


if __name__ == '__main__':
    main()
