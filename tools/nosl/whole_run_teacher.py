#!/usr/bin/env python3
"""Bounded offline whole-run teacher reference; never a NoSL deployment policy.

Only four public tables enter the frozen student. Privileged original-game
search, prefix qualification and routes stay in this offline collector.
"""
import argparse
import collections
import gzip
import json
from pathlib import Path
import subprocess
import sys
import time
import traceback

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import oracle_replay
import student_roots
from nosl import native
from nosl.branch_credit import KEYS, buffers, choose
from nosl.model import load
from nosl.teacher_distill import export_trace, oracle_module, sha
from nosl.vec import check_split

X1_SHA = '6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703'


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def timeout(deadline, maximum=90):
    left = deadline - time.monotonic()
    if left <= 0:
        raise TimeoutError('registered whole-run teacher deadline')
    return min(maximum, left)


def compress(path):
    if path.exists():
        target = path.with_suffix(path.suffix + '.gz')
        assert not target.exists(), target
        with path.open('rb') as source, gzip.open(target, 'wb', compresslevel=6) as dest:
            while block := source.read(1024 * 1024):
                dest.write(block)
        path.unlink()  # Only this invocation's raw file, now losslessly stored.


def script(seed, history, directory, filter_version, deadline):
    directory.mkdir(parents=True, exist_ok=True)
    rows = directory / 'rows.txt'
    rows.write_text(' '.join(map(str, history)))
    path = directory / 'script.jsonl'
    command = [str(ROOT / 'build/nosl_script'), '--seed', seed, '--rows-file', str(rows),
               '--out', str(path), '--filter', str(filter_version), '--ascension', '10']
    result = subprocess.run(command, capture_output=True, text=True, timeout=timeout(deadline))
    (directory / 'converter.log').write_text(result.stdout + result.stderr)
    info = json.loads(result.stdout)
    write(directory / 'converter.json', info)
    if result.returncode or info['status'] != 'complete':
        raise ValueError('cannot express actual teacher/student prefix: ' + json.dumps(info))
    return path, info


def qualify_prefix(directory, l4, deadline):
    runs = l4.parse_runs((directory / 'oracle.jsonl').read_text().splitlines())
    if len(runs) != 1 or runs[0]['begin'].get('oracle_test_mode') is not False:
        raise ValueError('expected one actual normal-rule original-game prefix')
    run = runs[0]
    decisions, end, error = l4.replay(str(ROOT / 'build/sts2sim_cli'), run,
                                     verify_snapshots=True, timeout=timeout(deadline))
    comparison = l4.compare_run(run, decisions, end, error)
    terminal_diff = l4.first_diff(run['end']['run'], (end or {}).get('run'))
    qualified = bool(comparison['status'] in ('identical', 'truncated') and error is None
                     and comparison.get('matched') == comparison.get('total') and terminal_diff is None)
    result = {'scope': 'normal_combat_prefix_only', 'qualified': qualified, 'l4': comparison,
              'oracle_test_mode': False, 'terminal_diff': terminal_diff,
              'trace_sha256': sha(directory / 'oracle.jsonl')}
    write(directory / 'env-contract.json', result)
    if not qualified:
        raise ValueError('original-game prefix/Env differs: ' + json.dumps(result)[:1200])
    return result


def match_row(data, row):
    t, a = map(int, row['counts'])
    expected = {'global': np.asarray(row['global'], np.int32),
                'tokens': np.asarray(row['tokens'], np.int32).reshape(t, native.TOKEN_WIDTH),
                'actions': np.asarray(row['actions'], np.int32).reshape(a, native.ACTION_WIDTH),
                'counts': np.asarray(row['counts'], np.int32)}
    for key in KEYS:
        actual = data[key][0]
        if key == 'tokens': actual = actual[:t]
        if key == 'actions': actual = actual[:a]
        if not np.array_equal(actual, expected[key]):
            raise ValueError('continuing sampler public table differs: ' + key)


def match_continuation(batch, data, expected):
    match_row(data, expected)
    if int(data['status'][0]) != expected['status'] or batch.run_info(0) != expected['run_info']:
        raise ValueError('continuing sampler endpoint differs from independent label replay')


def generate(args, model, blob, l4, global_deadline=None, *, use_teacher=True):
    seed, out = args.seed, Path(args.out).resolve()
    check_split('train', seed)
    if 'EVAL' in seed:
        raise ValueError('teacher curriculum refuses registered evaluation seeds')
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'invocation.json').exists():
        raise ValueError('use a fresh whole-run teacher directory')
    deadline = min(time.monotonic() + args.run_timeout, global_deadline or float('inf'))
    filter_version = int(blob.get('actor_filter', 0))
    write(out / 'invocation.json', {'seed': seed, 'checkpoint_sha256': sha(args.checkpoint),
          'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
          'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so'), 'actor_filter': filter_version,
          'device': args.device, 'precision': 'float32', 'use_teacher': use_teacher,
          'search': {'beam': 30, 'nodes': 10000, 'budget_ms': 10000, 'dop': 1},
          'run_timeout': args.run_timeout, 'max_roots': 64, 'max_policy_decisions': 10000})
    batch = native.Batch(1, 512, 512, 30000)
    batch.set_filter(filter_version)
    if batch.reset(0, seed, 10) != native.RUNNING:
        raise ValueError('cannot start normal A10 teacher run')
    data = buffers(1, 512, 512)
    batch.step(np.array([-1], np.int32), *data.values())
    history, roots = [], []
    combat_seen = False
    began = time.monotonic()
    result = {'seed': seed, 'status': 'started', 'scope': 'offline_teacher_reference' if use_teacher else 'public_student_baseline',
              'whole_run_qualified': False}
    try:
        while data['status'][0] == native.RUNNING:
            timeout(deadline)
            if len(history) >= 10000:
                raise TimeoutError('registered 10000 policy-decision limit')
            if not data['global'][0, 11]:
                combat_seen = False
            combat = data['global'][0, 11] and data['global'][0, native.G_DECISION] == 1
            if use_teacher and combat and not combat_seen:
                combat_seen = True
                if len(roots) >= 64:
                    raise TimeoutError('registered 64 combat-root limit')
                directory = out / f'root-{len(roots):03d}'
                directory.mkdir()
                root = {'root': len(roots), **batch.run_info(0), 'policy_rows_before': len(history), 'status': 'started'}
                roots.append(root)
                try:
                    prefix_script, prefix = script(seed, history, directory / 'prefix', filter_version, deadline)
                    decision = prefix['script_actions']
                    # The carrier only lets the original run loop reach the
                    # observer. Capture-only stops before executing it. Never
                    # step a dummy through a pending subset/turn-start choice.
                    carrier = directory / 'search-input'
                    carrier.mkdir()
                    search_script = carrier / 'script.jsonl'
                    search_script.write_text(prefix_script.read_text() + json.dumps(
                        {'event': 'action', 'kind': 'combat', 'type': 'end_turn'}) + '\n')
                    options = argparse.Namespace(dotnet=oracle_replay.DOTNET, seed=seed, script=search_script,
                        out=directory / 'search', min_floor=0, root_decision=decision,
                        beam=30, nodes=10000, budget_ms=10000, timeout=timeout(deadline), route=None, capture_only=True)
                    if student_roots.teacher(options):
                        raise ValueError('normal-rule original search failed')
                    evidence = json.loads((options.out / 'teacher.json').read_text())
                    if (evidence.get('status') != 'searched' or evidence.get('decision') != decision
                            or evidence.get('oracle_test_mode') is not False or evidence.get('oracle_test_mode_after') is not False
                            or evidence.get('root_unchanged') is not True or evidence.get('root_stamps_unchanged') is not True):
                        raise ValueError('teacher root/normal-rule qualification failed')
                    root.update(decision=decision, wall_seconds=evidence['wall_seconds'],
                                metrics=evidence['metrics'], predicted_complete=bool(evidence['all_enemies_dead'] and not evidence['player_dead']))
                    qualify_prefix(options.out, l4, deadline)
                    if not root['predicted_complete']:
                        root['status'] = 'incomplete_route_public_student_fallback'
                    else:
                        options.route = options.out / 'teacher.json'
                        options.capture_only = False
                        options.out = directory / 'native'
                        options.timeout = timeout(deadline)
                        if student_roots.teacher(options):
                            raise ValueError('frozen route original playback failed')
                        actual = json.loads((options.out / 'teacher.json').read_text())
                        if actual.get('status') != 'native_route_replayed' or actual.get('predicted_hp_matched') is not True:
                            raise ValueError('frozen route original endpoint failed')
                        qualify_prefix(options.out, l4, deadline)
                        _, public, contract, continuation = export_trace(options.out, decision, directory / 'public.jsonl',
                            filter_version, l4, return_continuation=True)
                        before = len(history)
                        if [r['label'] for r in public[:before]] != history or len(public) <= before:
                            raise ValueError('teacher public prefix differs from actual history')
                        if any(r['teacher'] for r in public[:before]) or not all(r['teacher'] for r in public[before:]):
                            raise ValueError('teacher/strategic public label boundary drift')
                        for row in public[before:]:
                            match_row(data, row)
                            if data['status'][0] != native.RUNNING:
                                raise ValueError('route extends past actual sampler endpoint')
                            label = int(row['label'])
                            history.append(label)
                            batch.step(np.array([label], np.int32), *data.values())
                        match_continuation(batch, data, continuation)
                        root.update(status='qualified_teacher_route', teacher_rows=len(history) - before,
                                    policy_rows_after=len(history), terminal_hp=actual['terminal_hp'], public_contract=contract)
                        combat_seen = False  # Completed teacher combat, including immediate next event fights.
                except Exception:
                    root['status'] = 'qualification_error'
                    root['error'] = traceback.format_exc()
                    raise
                finally:
                    write(directory / 'result.json', root)
                    for name in ('search/oracle.jsonl', 'native/oracle.jsonl', 'public.jsonl'):
                        compress(directory / name)
                    with (out / 'roots.jsonl').open('a') as stream:
                        stream.write(json.dumps(root) + '\n')
                print(json.dumps({'seed': seed, 'root': root['root'], 'floor': root['floor'],
                                  'status': root['status'], 'teacher_rows': root.get('teacher_rows')}), flush=True)
                if root['status'] == 'qualified_teacher_route':
                    continue
            label = int(choose(model, data, np.array([0]), args.device)[0])
            history.append(label)
            batch.step(np.array([label], np.int32), *data.values())
        info = batch.run_info(0)
        result.update(**info, end=native.STATUS_NAMES[int(data['status'][0])])
        if data['status'][0] not in (native.VICTORY, native.DEATH):
            raise ValueError('teacher sampler fault: ' + batch.last_error(0))
        trajectory = {'seed': seed, 'ascension': 10, 'actor_filter': filter_version,
                      'end': result['end'], **info, 'action_rows': history}
        write(out / 'trajectory.json', trajectory)
        final = oracle_replay.one((trajectory, out / 'final-normal', timeout(deadline, 180), 'off'))
        write(out / 'final-contract.json', final)
        if final['status'] != 'identical' or final.get('oracle_test_mode') is not False or final['oracle_end'] != result['end']:
            raise ValueError('teacher whole-run normal qualification failed: ' + json.dumps(final))
        result.update(status='whole_run_qualified', whole_run_qualified=True, matched=final['matched'])
        compress(out / 'final-normal' / seed / 'oracle.jsonl')
    except Exception as error:
        result.update(status='truncation' if isinstance(error, TimeoutError) else 'fault',
                      error=traceback.format_exc(), **batch.run_info(0))
    result.update(policy_decisions=len(history), roots=len(roots),
                  qualified_routes=sum(r['status'] == 'qualified_teacher_route' for r in roots),
                  teacher_rows=sum(r.get('teacher_rows', 0) for r in roots),
                  wall_seconds=time.monotonic() - began)
    write(out / 'result.json', result)
    return result


def experiment(args, model, blob, l4):
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'experiment.json').exists():
        raise ValueError('use a fresh registered experiment directory')
    seeds = [f'NQ4X{i:06d}' for i in range(8)]
    write(out / 'experiment.json', {'seeds': seeds, 'checkpoint_sha256': sha(args.checkpoint),
          'device': args.device, 'precision': 'float32', 'total_timeout': 3000,
          'gates': {'all_16_whole_run_qualified': True, 'route_fraction_at_least': .75,
                    'act3_at_least': 4, 'or_victories_at_least': 1, 'paired_floor_gain_at_least': 5, 'ci95_lower_above': 0}})
    deadline = time.monotonic() + 3000
    baselines = []
    # Identical singleton float32 inference for both arms; batching/device
    # changes must not be confused with the offline teacher intervention.
    for seed in seeds:
        options = argparse.Namespace(**vars(args))
        options.seed, options.out = seed, out / 'baseline' / seed
        baselines.append(generate(options, model, blob, l4, deadline, use_teacher=False))
    by_seed = {row['seed']: row for row in baselines}
    write(out / 'baseline-results.json', baselines)
    results = []
    for seed in seeds:
        options = argparse.Namespace(**vars(args))
        options.seed, options.out = seed, out / 'teacher' / seed
        if time.monotonic() >= deadline:
            result = {'seed': seed, 'status': 'truncation', 'whole_run_qualified': False, 'error': 'registered total deadline',
                      'roots': 0, 'qualified_routes': 0, 'teacher_rows': 0}
        else:
            result = generate(options, model, blob, l4, deadline)
        results.append(result)
        with (out / 'results.jsonl').open('a') as stream: stream.write(json.dumps(result) + '\n')
        print(json.dumps(result), flush=True)
    qualified = all(r.get('whole_run_qualified') for r in results) and all(q.get('whole_run_qualified') for q in baselines)
    roots = sum(r['roots'] for r in results)
    routes = sum(r['qualified_routes'] for r in results)
    # RUN_INFO reports the original zero-based CurrentActIndex.
    act3 = sum(r.get('whole_run_qualified') and r.get('act', 0) >= 2 for r in results)
    wins = sum(r.get('whole_run_qualified') and r.get('end') == 'victory' for r in results)
    summary = {'attempts': 8, 'baseline_qualified': sum(bool(q.get('whole_run_qualified')) for q in baselines),
               'teacher_qualified': sum(bool(r.get('whole_run_qualified')) for r in results),
               'root_attempts': roots, 'qualified_routes': routes, 'route_fraction': routes / max(1, roots),
               'act3': act3, 'victories': wins, 'statuses': dict(collections.Counter(r['status'] for r in results)),
               'teacher_rows': sum(r['teacher_rows'] for r in results), 'whole_run_gates_evaluable': qualified}
    if qualified:
        delta = np.array([r['floor'] - by_seed[r['seed']]['floor'] for r in results], dtype=np.float64)
        rng = np.random.default_rng(20261005)
        ci = np.quantile(delta[rng.integers(0, 8, (10000, 8))].mean(1), [.025, .975])
        summary.update(paired_floor_gain=float(delta.mean()), ci95=list(map(float, ci)),
                       gate_passed=routes / max(1, roots) >= .75 and (act3 >= 4 or wins >= 1) and delta.mean() >= 5 and ci[0] > 0)
    else:
        summary['gate_passed'] = False
    write(out / 'summary.json', summary)
    print(json.dumps(summary), flush=True)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('run', 'experiment'))
    parser.add_argument('--seed', default='NQ4XPILOT000000')
    parser.add_argument('--checkpoint', default=str(ROOT / 'artifacts/nosl-q4r/start.pt'))
    parser.add_argument('--out', required=True)
    parser.add_argument('--device', default='cpu')
    parser.add_argument('--run-timeout', type=int, default=900)
    args = parser.parse_args()
    torch.set_num_threads(1)
    if sha(args.checkpoint) != X1_SHA:
        raise ValueError('registered whole-run pilot requires frozen official X1')
    model, blob = load(args.checkpoint, args.device)
    model.eval()
    l4 = oracle_module()
    result = generate(args, model, blob, l4) if args.command == 'run' else experiment(args, model, blob, l4)
    if args.command == 'run': print(json.dumps(result), flush=True)
    return 0 if (result.get('whole_run_qualified') if args.command == 'run' else result['gate_passed']) else 1


if __name__ == '__main__':
    sys.exit(main())
