#!/usr/bin/env python3
"""Fixed public-gate pilots and complete normal A10 greedy evaluation."""
import argparse
import collections
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl import native
from nosl.branch_credit import buffers, inputs
from nosl.endturn import START_SHA, baseline, load_gate, write
from nosl.teacher_distill import sha
from nosl.vec import check_split


def collect(policy, seeds, device, deadline):
    """Actual normal runs, never interpreting a deadline/fault as a death."""
    for seed in seeds:check_split('train', seed)
    with_history = [[] for _ in seeds]
    kinds = [collections.Counter() for _ in seeds]
    routes = [collections.Counter() for _ in seeds]
    batch = native.Batch(len(seeds), 256, 256, 30000);batch.set_filter(1)
    tables = buffers(len(seeds), 256, 256)
    for slot, seed in enumerate(seeds):
        if batch.reset(slot, seed, 10) != native.RUNNING:
            raise RuntimeError('normal Silent A10 reset failed: ' + seed)
    batch.step(np.full(len(seeds), -1, np.int32), *tables.values())
    began, latency = time.monotonic(), []
    while np.any(tables['status'] == native.RUNNING) and time.monotonic() < deadline:
        live = np.flatnonzero(tables['status'] == native.RUNNING)
        started = time.monotonic()
        with torch.no_grad():
            logits, _ = policy(*inputs(tables, live, device))
        if not torch.isfinite(logits).any(1).all():
            raise ValueError('live public observation has no finite legal score')
        choice = logits.argmax(1).cpu().numpy()
        latency.append((time.monotonic() - started) * 1000)
        selected = np.full(len(seeds), -1, np.int32)
        for j, slot in enumerate(live):
            action = int(choice[j])
            if not 0 <= action < tables['counts'][slot, 1]:
                raise ValueError('gate selected outside the public legal actions')
            selected[slot] = action;with_history[slot].append(action)
            kinds[slot][int(tables['actions'][slot, action, 0])] += 1
            routes[slot]['combat' if tables['global'][slot, 11] else 'outside'] += 1
        batch.step(selected, *tables.values())
    rows = []
    for slot, seed in enumerate(seeds):
        status = int(tables['status'][slot])
        row = {'seed': seed, 'status': status, 'end': native.STATUS_NAMES[status],
               **batch.run_info(slot), 'ascension': 10, 'actor_filter': 1,
               'action_rows': with_history[slot], 'action_kinds': dict(kinds[slot]),
               'endturn_decisions': kinds[slot][3], 'routing': dict(routes[slot]),
               'mc_qualified': status in (native.DEATH, native.VICTORY)}
        if not row['mc_qualified']:
            row['error'] = 'registered sampling deadline' if status == native.RUNNING else batch.last_error(slot)
        rows.append(row)
    return rows, {'seconds': time.monotonic() - began, 'policy_batches': len(latency),
                  'batch_inference_ms_median': float(np.median(latency)) if latency else None,
                  'batch_inference_ms_p95': float(np.quantile(latency, .95)) if latency else None}


def select(attempts, final):
    if not final:return attempts
    selected = attempts[:2]
    good = [row for row in attempts if row['mc_qualified']]
    if good:
        deepest = max(good, key=lambda row: row['floor'])
        if deepest['seed'] not in {r['seed'] for r in selected}:selected.append(deepest)
    for row in good:
        if row['end'] == 'victory' and row['seed'] not in {r['seed'] for r in selected}:
            selected.append(row)
    return selected


def summarize(rows):
    good = [r for r in rows if r['mc_qualified']]
    return {'attempts': len(rows), 'terminals': len(good),
            'ends': dict(collections.Counter(r['end'] for r in rows)),
            'floor_mean': float(np.mean([r['floor'] for r in good])) if good else None,
            'act2': sum(r['act'] >= 1 for r in good), 'act3': sum(r['act'] >= 2 for r in good),
            'victories': sum(r['end'] == 'victory' for r in good),
            'endturn_decisions': sum(r.get('endturn_decisions', 0) for r in rows),
            'policy_decisions': sum(len(r['action_rows']) for r in rows)}


def paired(rows):
    result = {}
    for label in ('E1', 'E3'):
        delta = np.asarray([r['floor'] - c['floor'] for r, c in zip(rows[label], rows['X1'])])
        rng = np.random.default_rng(20261006)
        ci = np.quantile(delta[rng.integers(0, 1024, (10000, 1024))].mean(1), [.025, .975])
        result[label + '-X1'] = {'floor_gain': float(delta.mean()), 'ci95': ci.tolist(),
                                'gate': bool(delta.mean() >= .5 and ci[0] > 0)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('pilot', 'evaluate'))
    parser.add_argument('--start', required=True);parser.add_argument('--qualification', required=True)
    parser.add_argument('--normal-qualification');parser.add_argument('--out', required=True)
    parser.add_argument('--device', default='cpu');args = parser.parse_args()
    torch.set_num_threads(1);out = Path(args.out);out.mkdir(parents=True, exist_ok=False)
    qualification = Path(args.qualification)
    summary = json.loads((qualification / 'summary.json').read_text())
    assert summary['contract_passed'] and summary['fitting_gate_passed']
    for label in ('E1', 'E3'):
        assert summary['training'][label]['completed'] and summary['training'][label]['fitting_gate_passed']
        assert sha(qualification / label / 'checkpoint.pt') == summary['training'][label]['checkpoint_sha256']
    if args.mode == 'evaluate':
        if not args.normal_qualification:raise ValueError('formal evaluation needs the normal pilot audit')
        normal = json.loads(Path(args.normal_qualification).read_text())
        assert normal['registered_selection_verified'] and normal['runs'] == normal['whole_run_qualified'] == 8
        assert normal['oracle_test_mode'] is False and normal['verify_snapshots'] is True and not normal['failures']
    final = args.mode == 'evaluate'
    policies = {'X1': baseline(args.start, args.device)} if final else {}
    for seed in (1, 3):
        policy, meta = load_gate(qualification / f'E{seed}' / 'checkpoint.pt', args.start, args.device)
        assert meta['final_epoch'] == 128 and meta['training']['seed'] == seed
        policies[f'E{seed}'] = policy
    config = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'mode': args.mode, 'baseline_checkpoint_sha256': START_SHA,
              'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'), 'seeds': [1, 3],
              'evaluation_runs': 1024 if final else 4,
              'evaluation_prefix': 'NQ4ACEVAL' if final else 'NQ4ACPILOT',
              'evaluation_seconds': 900 if final else 120,
              'device': args.device, 'precision': 'float32',
              'head_checkpoints': {label: summary['training'][label]['checkpoint_sha256'] for label in ('E1', 'E3')}}
    write(out / 'experiment.json', config)
    evaluation = out / 'evaluation';evaluation.mkdir()
    seeds = [f'{config["evaluation_prefix"]}{i:06d}' for i in range(config['evaluation_runs'])]
    deadline = time.monotonic() + 900 if final else None
    rows, resources = {}, {}
    for label, policy in policies.items():
        if not final:deadline = time.monotonic() + 120
        attempts, timing = [], []
        with (evaluation / (label + '.jsonl')).open('w') as stream:
            for first in range(0, len(seeds), 64):
                selected = seeds[first:first + 64]
                if time.monotonic() >= deadline:
                    block = [{'seed': s, 'status': 0, 'end': 'not_started', 'ascension': 10,
                              'actor_filter': 1, 'mc_qualified': False, 'action_rows': [],
                              'error': 'registered evaluation deadline'} for s in selected]
                else:
                    block, resource = collect(policy, selected, args.device, deadline);timing.append(resource)
                for row in block:stream.write(json.dumps(row) + '\n')
                stream.flush();attempts.extend(block)
                print(json.dumps({'evaluation': label, 'attempts': len(attempts),
                                  'terminals': sum(r['mc_qualified'] for r in attempts)}), flush=True)
        rows[label] = attempts;resources[label] = timing
        if label != 'X1':
            with (out / (label + '-normal-inputs.jsonl')).open('w') as stream:
                for row in select(attempts, final):stream.write(json.dumps(row) + '\n')
    complete = all(r['mc_qualified'] for arm in rows.values() for r in arm)
    statistics = {label: summarize(attempts) for label, attempts in rows.items()}
    comparison = paired(rows) if complete and final else {}
    gate = bool(complete and (not final or (all(r['gate'] for r in comparison.values()) and
                max(statistics[s]['act3'] for s in ('E1', 'E3')) >= statistics['X1']['act3'])))
    result = {'experiment': config, 'policies': statistics, 'paired': comparison,
              'resources': resources, 'all_evaluation_runs_terminal': complete, 'gate_passed': gate}
    write(out / 'results.json', result);print(json.dumps(result), flush=True)
    return 0 if gate else 1


if __name__ == '__main__':sys.exit(main())
