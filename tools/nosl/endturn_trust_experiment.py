#!/usr/bin/env python3
"""Fixed local normal pilots and paired whole-run bounded-head evaluation."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.endturn import baseline, write
from nosl.endturn_action import load, source
from nosl.teacher_distill import sha
from endturn_experiment import collect, select, summarize
from check_endturn_credit import replay

START = ROOT / 'artifacts/nosl-q4r/start.pt'
HEADS = ROOT / 'artifacts/nosl-q4ac/qualification'
ARMS = ('E1', 'U1', 'T1', 'E3', 'U3', 'T3')


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]


def paired(rows):
    result = {}
    for seed in (1, 3):
        label = f'T{seed}'
        for against in (f'U{seed}', f'E{seed}', 'X1'):
            delta = np.asarray([r['floor'] - b['floor'] for r, b in zip(rows[label], rows[against])])
            assert len(delta) == 1024
            rng = np.random.default_rng(20261006);values = []
            for _ in range(100):values.extend(delta[rng.integers(0, 1024, (100, 1024))].mean(1).tolist())
            ci = np.quantile(values, [.025, .975])
            threshold = 2. if against.startswith('U') else .5
            result[label + '-' + against] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                'threshold': threshold, 'gate': bool(delta.mean() >= threshold and ci[0] > 0)}
    return result


def normal(experiment, out):
    experiment, out = Path(experiment), Path(out);out.mkdir(parents=True, exist_ok=False)
    config = json.loads((experiment / 'experiment.json').read_text());final = config['mode'] == 'evaluate'
    prefix = 'NQ4AGEVAL' if final else 'NQ4AGPILOT'
    expected = [f'{prefix}{i:06d}' for i in range(1024 if final else 4)]
    rows = []
    for arm in ARMS:
        attempts = lines(experiment / 'evaluation' / (arm + '.jsonl'))
        assert [r['seed'] for r in attempts] == expected
        chosen = select(attempts, final)
        assert chosen == lines(experiment / (arm + '-normal-inputs.jsonl'))
        for trajectory in chosen:
            result = replay(trajectory, out / arm);result['arm'] = arm;rows.append(result)
            with (out / 'results.jsonl').open('a') as stream:stream.write(json.dumps(result) + '\n')
            print(json.dumps(result), flush=True)
    result = {'runs': len(rows), 'whole_run_qualified': sum(r['whole_run_qualified'] for r in rows),
        'registered_selection_verified': True, 'oracle_test_mode': False, 'verify_snapshots': True,
        'decisions_matched': sum(r.get('comparison', {}).get('matched', 0) for r in rows),
        'failures': [r for r in rows if not r['whole_run_qualified']]}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True)
    return 0 if rows and not result['failures'] else 1


def experiment(args):
    learning, out = Path(args.learning), Path(args.out)
    summary = json.loads((learning / 'summary.json').read_text())
    contract = json.loads((learning / 'contract/result.json').read_text())
    assert summary['evaluation_gate'] and summary['contract_passed'] and contract['passed']
    final = args.mode == 'evaluate'
    if final:
        normal_qualification = json.loads(Path(args.normal_qualification).read_text())
        assert normal_qualification['runs'] == normal_qualification['whole_run_qualified'] == 24
        assert normal_qualification['registered_selection_verified'] and not normal_qualification['failures']
        assert normal_qualification['oracle_test_mode'] is False and normal_qualification['verify_snapshots'] is True
        assert json.loads(Path(args.learning_audit).read_text())['independent_verification']
        checked = json.loads(Path(args.check_result).read_text());assert checked['exit_code'] == 0
        changed = subprocess.check_output(['git', 'diff', '--name-only', checked['source_commit'], 'HEAD'], text=True).splitlines()
        if any(p not in ('README.md', 'docs/roadmap.md') and not p.startswith('docs/') for p in changed):
            raise ValueError('runtime source changed after complete gate')
        subprocess.run(['git', 'diff', '--exit-code', '--', 'python/nosl', 'tools/nosl'], check=True, stdout=subprocess.DEVNULL)
    out.mkdir(parents=True, exist_ok=False)
    policies = {'X1': baseline(START)} if final else {}
    for arm in ARMS:
        if arm[0] == 'E':policies[arm] = source(START, HEADS, int(arm[1]))
        else:
            assert sha(learning / arm / 'checkpoint.pt') == summary['training'][arm]['checkpoint_sha256']
            model, blob = load(learning / arm / 'checkpoint.pt', START, HEADS)
            assert blob['final_epoch'] == 128 and blob['action_context'] and blob['initial_seed'] == int(arm[1])
            policies[arm] = model
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'mode': args.mode, 'prefix': 'NQ4AGEVAL' if final else 'NQ4AGPILOT',
        'runs': 1024 if final else 4, 'seconds': 1800 if final else 120,
        'device': 'cpu', 'precision': 'float32', 'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'),
        'checkpoints': {arm: sha(learning / arm / 'checkpoint.pt') for arm in ('U1', 'T1', 'U3', 'T3')},
        'greedy_batch': 64, 'scope': 'normal_Silent_A10_development_comparison_not_final_product_acceptance'}
    write(out / 'experiment.json', config);evaluation = out / 'evaluation';evaluation.mkdir()
    seeds = [f'{config["prefix"]}{i:06d}' for i in range(config['runs'])]
    deadline = time.monotonic() + 1800 if final else None;rows, resources = {}, {}
    for arm, policy in policies.items():
        if not final:deadline = time.monotonic() + 120
        attempts, timings = [], []
        with (evaluation / (arm + '.jsonl')).open('w') as stream:
            for first in range(0, len(seeds), 64):
                group = seeds[first:first + 64]
                if time.monotonic() >= deadline:
                    batch = [{'seed': seed, 'status': 0, 'end': 'not_started', 'ascension': 10,
                        'actor_filter': 1, 'mc_qualified': False, 'action_rows': [],
                        'error': 'registered local evaluation deadline'} for seed in group]
                else:
                    batch, timing = collect(policy, group, 'cpu', deadline);timings.append(timing)
                for row in batch:stream.write(json.dumps(row) + '\n')
                stream.flush();attempts.extend(batch)
                print(json.dumps({'arm': arm, 'attempts': len(attempts), 'terminals': sum(r['mc_qualified'] for r in attempts)}), flush=True)
        rows[arm] = attempts;resources[arm] = timings
        if arm != 'X1':
            (out / (arm + '-normal-inputs.jsonl')).write_text(''.join(json.dumps(r) + '\n' for r in select(attempts, final)))
    complete = all(r['mc_qualified'] for attempts in rows.values() for r in attempts)
    stats = {arm: summarize(attempts) for arm, attempts in rows.items()}
    comparison = paired(rows) if final and complete else {}
    mechanism = final and complete and all(comparison[f'T{s}-U{s}']['gate'] for s in (1, 3))
    net = final and complete and all(comparison[f'T{s}-{b}']['gate'] for s in (1, 3) for b in (f'E{s}', 'X1'))
    if net:net = all(stats[f'T{s}']['act3'] >= stats['X1']['act3'] for s in (1, 3))
    gate = complete and (not final or mechanism and net)
    result = {'experiment': config, 'policies': stats, 'resources': resources, 'paired': comparison,
        'all_evaluation_runs_terminal': complete, 'mechanism_gate': bool(mechanism), 'net_quality_gate': bool(net),
        'gate_passed': bool(gate)}
    write(out / 'results.json', result);print(json.dumps(result), flush=True);return 0 if gate else 1


def main():
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('mode', choices=('pilot', 'evaluate', 'normal'))
    p.add_argument('--learning');p.add_argument('--normal-qualification');p.add_argument('--learning-audit')
    p.add_argument('--check-result');p.add_argument('--experiment');p.add_argument('--out', required=True)
    a = p.parse_args();torch.set_num_threads(1)
    return normal(a.experiment, a.out) if a.mode == 'normal' else experiment(a)


if __name__ == '__main__':sys.exit(main())
