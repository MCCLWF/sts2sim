#!/usr/bin/env python3
"""Fixed CPU-only whole-run CEM learning of public tactical rule weights."""
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
from nosl.tactical import PRIOR, TacticalPolicy, load, save
from nosl.teacher_distill import sha
from endturn_experiment import collect, select, summarize
from check_endturn_credit import replay

START = ROOT / 'artifacts/nosl-q4r/start.pt'


def lines(path):return [json.loads(s) for s in Path(path).read_text().splitlines() if s]
def source():return subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
def objective(rows):return (sum(r['floor'] for r in rows) + 100 * sum(r['end'] == 'victory' for r in rows)) / (50 * len(rows))


def require_check(path):
    checked = json.loads(Path(path).read_text());assert checked['exit_code'] == 0
    changed = subprocess.check_output(['git', 'diff', '--name-only', checked['source_commit'], 'HEAD'], text=True).splitlines()
    assert all(p in ('README.md', 'docs/roadmap.md') or p.startswith('docs/') for p in changed)
    subprocess.run(['git', 'diff', '--exit-code', '--', 'python/nosl', 'tools/nosl'], check=True, stdout=subprocess.DEVNULL)


def train(args):
    out = Path(args.out);out.mkdir(parents=True, exist_ok=False);require_check(args.check_result)
    pilot = json.loads(Path(args.normal_qualification).read_text())
    assert pilot['runs'] == pilot['whole_run_qualified'] == 8 and not pilot['failures']
    assert pilot['oracle_test_mode'] is False and pilot['verify_snapshots']
    assert json.loads(Path(args.contract).read_text())['passed']
    base = baseline(START);began = time.monotonic();results = {}
    config = {'source_commit': source(), 'device': 'cpu', 'cloud_used': False, 'torch_version': torch.__version__,
        'seeds': [1, 3], 'generations': 4, 'population': 8, 'runs_per_candidate': 32,
        'seconds_per_learning_seed': 1200, 'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'),
        'objective': 'mean_complete_terminal_floor_div_50_plus_2_times_victory', 'initial_weights': PRIOR.tolist()}
    write(out / 'config.json', config)
    for seed in (1, 3):
        directory = out / f'H{seed}';directory.mkdir();deadline = time.monotonic() + 1200
        rng = np.random.default_rng(seed);initial = np.log(PRIOR);mean = initial.copy();sigma = np.full(16, .35)
        prefix = f'NQ4AH{seed}TRAIN';arm_started = time.monotonic()
        for generation in range(4):
            proposals = np.vstack([mean, np.clip(rng.normal(mean, sigma, (7, 16)), initial - 1.5, initial + 1.5)])
            seeds = [f'{prefix}{generation * 32 + i:06d}' for i in range(32)]
            write(directory / f'g{generation}-proposals.json', {'generation': generation, 'seed_bank': seeds,
                'old_mean': mean.tolist(), 'old_sigma': sigma.tolist(), 'proposals': proposals.tolist(),
                'rng_state_after_proposals': rng.bit_generator.state})
            scored = []
            for candidate, parameters in enumerate(proposals):
                if time.monotonic() >= deadline:raise TimeoutError('registered CEM learning deadline')
                policy = TacticalPolicy(base, np.exp(parameters))
                rows, timing = collect(policy, seeds, 'cpu', deadline)
                file = directory / f'g{generation}-c{candidate}.jsonl'
                file.write_text(''.join(json.dumps(r) + '\n' for r in rows))
                assert all(r['mc_qualified'] for r in rows), 'CEM candidate contains nonterminal/fault'
                scored.append({'candidate': candidate, 'log_weights': parameters.tolist(),
                    'objective': objective(rows), 'statistics': summarize(rows), 'resources': timing})
                print(json.dumps({'learning': f'H{seed}', 'generation': generation, 'candidate': candidate,
                    'objective': scored[-1]['objective'], 'statistics': scored[-1]['statistics']}), flush=True)
            elite = sorted(range(8), key=lambda i: (-scored[i]['objective'], i))[:2]
            next_mean = .5 * mean + .5 * proposals[elite].mean(0)
            next_sigma = np.maximum(.1, .5 * sigma + .5 * proposals[elite].std(0))
            record = {'generation': generation, 'seed_bank': seeds, 'old_mean': mean.tolist(), 'old_sigma': sigma.tolist(),
                'candidates': scored, 'elite': elite, 'next_mean': next_mean.tolist(), 'next_sigma': next_sigma.tolist()}
            with (directory / 'generations.jsonl').open('a') as stream:stream.write(json.dumps(record) + '\n')
            mean, sigma = next_mean, next_sigma
        assert not np.array_equal(mean, initial)
        metadata = dict(config, learning_seed=seed, final_generation=4)
        save(directory / 'checkpoint.json', np.exp(mean), metadata)
        write(directory / 'optimizer-final.json', {'mean': mean.tolist(), 'sigma': sigma.tolist(), 'rng_state': rng.bit_generator.state})
        results[f'H{seed}'] = {'completed': True, 'material_parameter_change': True,
            'policy_runs': 1024, 'unique_seeds': 128, 'seconds': time.monotonic() - arm_started,
            'checkpoint_sha256': sha(directory / 'checkpoint.json')}
    result = {'completed': True, 'config': config, 'training': results, 'seconds': time.monotonic() - began}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True);return 0


def normal(args):
    experiment, out = Path(args.experiment), Path(args.out);out.mkdir(parents=True, exist_ok=False)
    config = json.loads((experiment / 'experiment.json').read_text());rows = []
    arms = ('X1', 'R') if config['mode'] == 'pilot' else ('R', 'H1', 'H3')
    for arm in arms:
        attempts = lines(experiment / 'evaluation' / (arm + '.jsonl'))
        assert [r['seed'] for r in attempts] == [f'{config["prefix"]}{i:06d}' for i in range(config['runs'])]
        chosen = select(attempts, config['mode'] == 'evaluate')
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


def comparisons(rows):
    result = {}
    for arm, against in [('R', 'X1'), ('H1', 'R'), ('H3', 'R'), ('H1', 'X1'), ('H3', 'X1')]:
        delta = np.array([a['floor'] - b['floor'] for a, b in zip(rows[arm], rows[against])])
        assert len(delta) == 1024
        rng = np.random.default_rng(20261006);values = []
        for _ in range(100):values.extend(delta[rng.integers(0, 1024, (100, 1024))].mean(1).tolist())
        interval = np.quantile(values, [.025, .975]);threshold = 1. if against == 'R' else 2.
        result[arm + '-' + against] = {'floor_gain': float(delta.mean()), 'ci95': interval.tolist(),
            'threshold': threshold, 'gate': bool(delta.mean() >= threshold and interval[0] > 0)}
    return result


def experiment(args):
    out = Path(args.out);out.mkdir(parents=True, exist_ok=False);final = args.mode == 'evaluate'
    if final:
        require_check(args.check_result)
        assert json.loads(Path(args.learning_audit).read_text())['independent_verification']
        assert json.loads(Path(args.contract).read_text())['passed']
    base = baseline(START);policies = {'X1': base, 'R': TacticalPolicy(base)}
    if final:
        summary = json.loads((Path(args.learning) / 'summary.json').read_text());assert summary['completed']
        for arm in ('H1', 'H3'):
            path = Path(args.learning) / arm / 'checkpoint.json'
            assert sha(path) == summary['training'][arm]['checkpoint_sha256']
            model, blob = load(path, START);assert blob['training']['final_generation'] == 4
            policies[arm] = model
    config = {'source_commit': source(), 'mode': args.mode, 'prefix': 'NQ4AHEVAL' if final else 'NQ4AHPILOT',
        'runs': 1024 if final else 4, 'seconds': 1800 if final else 120, 'device': 'cpu', 'baseline_precision': 'float32',
        'tactical_score_precision': 'float64_sequential_quantized_1e-8', 'greedy_batch': 64,
        'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'), 'cloud_used': False,
        'weights': {arm: policy.weights.tolist() for arm, policy in policies.items() if arm != 'X1'}}
    write(out / 'experiment.json', config);evaluation = out / 'evaluation';evaluation.mkdir()
    seeds = [f'{config["prefix"]}{i:06d}' for i in range(config['runs'])]
    rows, resources = {}, {};deadline = time.monotonic() + 1800 if final else None
    for arm, policy in policies.items():
        if not final:deadline = time.monotonic() + 120
        attempts, timings = [], []
        with (evaluation / (arm + '.jsonl')).open('w') as stream:
            for first in range(0, len(seeds), 64):
                group = seeds[first:first + 64]
                if time.monotonic() >= deadline:
                    batch = [{'seed': seed, 'status': 0, 'end': 'not_started', 'ascension': 10, 'actor_filter': 1,
                        'mc_qualified': False, 'action_rows': [], 'error': 'registered evaluation deadline'} for seed in group]
                else:batch, timing = collect(policy, group, 'cpu', deadline);timings.append(timing)
                for row in batch:stream.write(json.dumps(row) + '\n')
                stream.flush();attempts.extend(batch)
                print(json.dumps({'arm': arm, 'attempts': len(attempts), 'terminals': sum(r['mc_qualified'] for r in attempts)}), flush=True)
        rows[arm] = attempts;resources[arm] = timings
        chosen = select(attempts, final)
        (out / (arm + '-normal-inputs.jsonl')).write_text(''.join(json.dumps(r) + '\n' for r in chosen))
    complete = all(r['mc_qualified'] for attempts in rows.values() for r in attempts)
    stats = {arm: summarize(attempts) for arm, attempts in rows.items()}
    paired = comparisons(rows) if final and complete else {}
    gate = complete and (not final or all(paired[f'{h}-{b}']['gate'] for h in ('H1', 'H3') for b in ('R', 'X1')) and
        all(stats[h][key] >= max(stats[b][key] for b in ('X1', 'R')) for h in ('H1', 'H3') for key in ('act3', 'victories')))
    result = {'experiment': config, 'policies': stats, 'resources': resources, 'paired': paired,
        'all_evaluation_runs_terminal': complete, 'gate_passed': bool(gate)}
    write(out / 'results.json', result);print(json.dumps(result), flush=True);return 0 if gate else 1


def main():
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('mode', choices=('pilot', 'train', 'evaluate', 'normal'))
    for name in ('learning', 'learning-audit', 'normal-qualification', 'check-result', 'contract', 'experiment'):
        p.add_argument('--' + name)
    p.add_argument('--out', required=True);a = p.parse_args();torch.set_num_threads(1)
    if a.mode == 'train':return train(a)
    if a.mode == 'normal':return normal(a)
    return experiment(a)


if __name__ == '__main__':sys.exit(main())
