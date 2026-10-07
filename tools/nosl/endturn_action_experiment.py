#!/usr/bin/env python3
"""Fixed local normal pilots and capacity-matched whole-run action evaluation."""
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
from nosl.endturn import START_SHA, baseline, write
from nosl.endturn_action import load
from nosl.teacher_distill import sha
from endturn_experiment import collect, select, summarize
from check_endturn_credit import replay

ARMS = ('B1', 'C1', 'B3', 'C3')


def paired(rows):
    result = {}
    for seed in (1, 3):
        label = f'C{seed}'
        for against in ('X1', f'B{seed}'):
            delta = np.asarray([r['floor'] - b['floor'] for r, b in zip(rows[label], rows[against])])
            rng = np.random.default_rng(20261006)
            values = []
            for _ in range(100):
                values.extend(delta[rng.integers(0, 1024, (100, 1024))].mean(1).tolist())
            ci = np.quantile(values, [.025, .975])
            result[label + '-' + against] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                                            'gate': bool(delta.mean() >= .5 and ci[0] > 0)}
    return result


def normal(experiment, out):
    experiment, out = Path(experiment), Path(out);out.mkdir(parents=True, exist_ok=False)
    config = json.loads((experiment / 'experiment.json').read_text());final = config['mode'] == 'evaluate'
    expected = [f'{"NQ4AEEVAL" if final else "NQ4AEPILOT"}{i:06d}' for i in range(1024 if final else 4)]
    rows = []
    for arm in ARMS:
        attempts = [json.loads(s) for s in (experiment / 'evaluation' / (arm + '.jsonl')).read_text().splitlines()]
        assert [r['seed'] for r in attempts] == expected
        chosen = select(attempts, final)
        assert chosen == [json.loads(s) for s in (experiment / (arm + '-normal-inputs.jsonl')).read_text().splitlines()]
        for trajectory in chosen:
            result = replay(trajectory, out / arm);result['arm'] = arm;rows.append(result)
            with (out / 'results.jsonl').open('a') as stream:stream.write(json.dumps(result) + '\n')
            print(json.dumps(result), flush=True)
    result = {'registered_selection_verified': True, 'runs': len(rows),
              'whole_run_qualified': sum(r['whole_run_qualified'] for r in rows),
              'decisions_matched': sum(r.get('comparison', {}).get('matched', 0) for r in rows),
              'oracle_test_mode': False, 'verify_snapshots': True,
              'failures': [r for r in rows if not r['whole_run_qualified']]}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True)
    return 0 if result['runs'] == result['whole_run_qualified'] and not result['failures'] else 1


def experiment(args):
    learning, out = Path(args.learning), Path(args.out)
    summary = json.loads((learning / 'summary.json').read_text())
    contract = json.loads(Path(args.contract).read_text())
    assert summary['initial_contract_passed'] and summary['fitting_gate_passed'] and contract['passed']
    assert all(summary['training'][arm]['completed'] for arm in ARMS)
    final = args.mode == 'evaluate'
    if final:
        normal = json.loads(Path(args.normal_qualification).read_text())
        assert normal['registered_selection_verified'] and normal['runs'] == normal['whole_run_qualified'] == 16
        assert normal['oracle_test_mode'] is False and normal['verify_snapshots'] is True and not normal['failures']
        if not args.learning_audit or not json.loads(Path(args.learning_audit).read_text())['independent_verification']:
            raise ValueError('formal evaluation requires the independent learning audit')
        checked = json.loads(Path(args.check_result).read_text()) if args.check_result else None
        if not checked or checked['exit_code'] != 0:
            raise ValueError('formal evaluation requires the complete source gate')
        changed = subprocess.check_output(['git', 'diff', '--name-only', checked['source_commit'], 'HEAD'], text=True).splitlines()
        if any(name not in ('README.md', 'docs/roadmap.md') and not name.startswith('docs/') for name in changed):
            raise ValueError('runtime source changed after the complete gate')
        subprocess.run(['git', 'diff', '--exit-code', '--', 'python/nosl', 'tools/nosl'], check=True,
                       stdout=subprocess.DEVNULL)
    out.mkdir(parents=True, exist_ok=False)
    policies = {'X1': baseline(args.start)} if final else {}
    for arm in ARMS:
        assert sha(learning / arm / 'checkpoint.pt') == summary['training'][arm]['checkpoint_sha256']
        model, blob = load(learning / arm / 'checkpoint.pt', args.start, args.heads)
        assert blob['final_epoch'] == 128 and blob['initial_seed'] == int(arm[1]) and blob['action_context'] == (arm[0] == 'C')
        policies[arm] = model
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'mode': args.mode, 'evaluation_prefix': 'NQ4AEEVAL' if final else 'NQ4AEPILOT',
              'evaluation_runs': 1024 if final else 4, 'evaluation_seconds': 1500 if final else 120,
              'device': 'cpu', 'precision': 'float32', 'baseline_sha256': START_SHA,
              'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'),
              'checkpoints': {arm: summary['training'][arm]['checkpoint_sha256'] for arm in ARMS}}
    write(out / 'experiment.json', config);evaluation = out / 'evaluation';evaluation.mkdir()
    seeds = [f'{config["evaluation_prefix"]}{i:06d}' for i in range(config['evaluation_runs'])]
    deadline = time.monotonic() + 1500 if final else None
    rows, resources = {}, {}
    for arm, model in policies.items():
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
                    batch, timing = collect(model, group, 'cpu', deadline);timings.append(timing)
                for row in batch:stream.write(json.dumps(row) + '\n')
                stream.flush();attempts.extend(batch)
                print(json.dumps({'arm': arm, 'attempts': len(attempts), 'terminals': sum(r['mc_qualified'] for r in attempts)}), flush=True)
        rows[arm] = attempts;resources[arm] = timings
        if arm != 'X1':
            with (out / (arm + '-normal-inputs.jsonl')).open('w') as stream:
                for row in select(attempts, final):stream.write(json.dumps(row) + '\n')
    complete = all(r['mc_qualified'] for batch in rows.values() for r in batch)
    stats = {arm: summarize(batch) for arm, batch in rows.items()}
    comparison = paired(rows) if final and complete else {}
    gate = bool(complete and (not final or (all(r['gate'] for r in comparison.values()) and
                max(stats[arm]['act3'] for arm in ('C1', 'C3')) >= stats['X1']['act3'])))
    result = {'experiment': config, 'policies': stats, 'resources': resources, 'paired': comparison,
              'all_evaluation_runs_terminal': complete, 'gate_passed': gate}
    write(out / 'results.json', result);print(json.dumps(result), flush=True);return 0 if gate else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('pilot', 'evaluate', 'normal'))
    parser.add_argument('--start');parser.add_argument('--heads');parser.add_argument('--learning')
    parser.add_argument('--contract');parser.add_argument('--normal-qualification');parser.add_argument('--learning-audit')
    parser.add_argument('--check-result');parser.add_argument('--experiment');parser.add_argument('--out', required=True)
    args = parser.parse_args();torch.set_num_threads(1)
    return normal(args.experiment, args.out) if args.mode == 'normal' else experiment(args)


if __name__ == '__main__':sys.exit(main())
