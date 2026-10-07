#!/usr/bin/env python3
"""Diagnose frozen X1 act-one boss failures with the pinned offline teacher.

32 fixed training seeds, one search/route attempt each, no student update.
The actor remains public-observation-only; oracle roots and teacher routes are
offline evidence. This gate only decides whether to plan later boss learning.
"""
import argparse
import collections
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import student_roots

ROOT = Path(__file__).resolve().parents[2]
START_SHA = '6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703'
TEACHER_COMMIT = '9a4489d840a3772aef451d5c6dc42dd79719f380'
SEEDS = [f'NQ4S{i:06d}' for i in range(32)]


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def preflight(checkpoint):
    import torch
    from nosl.model import load
    assert student_roots.sha(checkpoint) == START_SHA
    model, blob = load(checkpoint, 'cpu')
    assert blob['actor_filter'] == 1 and model.features == 3 and model.power_context is None
    assert all(torch.isfinite(t).all() for t in model.state_dict().values() if t.is_floating_point())
    lock = read(ROOT / 'tools/combatsolver/upstream.json')
    assert lock['commit'] == TEACHER_COMMIT
    source = ROOT / lock['checkout']
    assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip() == TEACHER_COMMIT
    assert not subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=source, text=True)
    return {'checkpoint_sha256': START_SHA, 'teacher_commit': TEACHER_COMMIT,
            'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
            'native_sha256': student_roots.sha(ROOT / 'build/libsts2nosl_c.so'),
            'solver_sha256': student_roots.sha(source / '.godot/mono/temp/bin/Release/CombatSolver.dll'),
            'harness_sha256': student_roots.sha(ROOT / 'tools/NoSLTeacherHarness/bin/Release/net9.0/OfflineSearchHarness.dll'),
            'seeds': SEEDS, 'min_floor': 17, 'beam': 30, 'nodes': 10000,
            'budget_ms': 10000, 'host_timeout_seconds': 90, 'student_update': False}


def verify(out):
    collection = read(out / 'student/collection.json')
    assert collection['completed'] and collection['seeds'] == SEEDS
    assert collection['checkpoint_sha256'] == START_SHA and collection['actor_filter'] == 1
    batch = read(out / 'batch/batch.json')
    assert batch['completed'] and batch['seeds'] == SEEDS and batch['min_floor'] == 17
    source_rows = rows(out / 'batch/results.jsonl')
    assert len(source_rows) == len(SEEDS) and [r['seed'] for r in source_rows] == SEEDS
    report_rows = []
    for source in source_rows:
        row = dict(source)
        directory = out / 'batch' / source['seed']
        search = directory / 'search'
        evidence = read(search / 'teacher.json') if (search / 'teacher.json').exists() else {}
        root_path = search / 'root-before.json'
        row['student_normal_qualified'] = bool(source.get('search_qualified'))
        row['rescued_student_death'] = False
        row['boss_route_qualified'] = False
        if evidence.get('status') == 'no_root':
            # A student dying before the boss is still one of the 32 attempts.
            # Independently verify its ordinary whole trace rather than treating
            # a missing root as an engine error or dropping it from the gate.
            student_roots.verify(argparse.Namespace(out=search, timeout=90))
            row['student_normal_qualified'] = read(search / 'env-contract.json')['qualified']
            row['no_boss_root'] = True
            row['student_end'] = evidence.get('end')
        if root_path.exists():
            root = read(root_path)
            row['boss_root'] = root['run']['total_floor'] == 17 and root['run']['room'] == 'Boss'
            row['root_context'] = {k: root['run'][k] for k in ('act', 'total_floor', 'room', 'hp', 'max_hp', 'deck', 'relics', 'potions')}
            row['root_context']['enemies'] = root['combat']['enemies']
        if row.get('boss_root') and row['student_normal_qualified']:
            assert evidence['oracle_test_mode'] is False and evidence['oracle_test_mode_after'] is False
            assert evidence['root_unchanged'] and evidence['root_stamps_unchanged']
            assert read(search / 'env-contract.json')['qualified']
            decisions = [e for e in rows(search / 'oracle.jsonl') if e['event'] == 'decision']
            later = [e for e in decisions[evidence['decision'] + 1:]
                     if e['kind'] in {'rewards', 'map', 'event', 'rest', 'shop', 'relic_select'}]
            terminal = later[0]['run'] if later else evidence['student_end']['run']
            assert terminal['total_floor'] == evidence['floor']
            died = evidence['student_end']['reason'] == 'death' and terminal['hp'] <= 0
            row.update(student_died=died, student_terminal_hp=terminal['hp'],
                       student_terminal_potions=terminal['potions'])
        if source['status'] == 'route_qualified':
            native_dir = directory / 'native'
            native = read(native_dir / 'teacher.json')
            assert native['status'] == 'native_route_replayed' and native['predicted_hp_matched']
            assert read(native_dir / 'env-contract.json')['qualified']
            row.update(boss_route_qualified=bool(row.get('boss_root')),
                       teacher_terminal_hp=native['terminal_hp'],
                       terminal_hp_gain=native['terminal_hp'] - row['student_terminal_hp'],
                       teacher_terminal_potions=native['student_end']['run']['potions'],
                       rescued_student_death=bool(row.get('boss_root') and row['student_died']))
        report_rows.append(row)
    (out / 'boss-results.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in report_rows))
    qualified = [r for r in report_rows if r['boss_route_qualified']]
    rescued = sum(r['rescued_student_death'] for r in report_rows)
    summary = {'attempts': len(report_rows), 'boss_roots': sum(bool(r.get('boss_root')) for r in report_rows),
               'no_root': sum(bool(r.get('no_boss_root')) for r in report_rows),
               'student_normal_qualified': sum(r['student_normal_qualified'] for r in report_rows),
               'student_boss_deaths': sum(r.get('student_died') is True for r in report_rows),
               'student_boss_wins': sum(r.get('student_died') is False for r in report_rows),
               'qualified_boss_routes': len(qualified), 'rescued_student_deaths': rescued,
               'qualified_student_boss_deaths': sum(r['student_died'] for r in qualified),
               'qualified_student_boss_wins': sum(not r['student_died'] for r in qualified),
               'qualified_hp_gains_at_least_3': sum(r['terminal_hp_gain'] >= 3 for r in qualified),
               'statuses': dict(collections.Counter(r['status'] for r in report_rows)),
               'search_statuses': dict(collections.Counter(r.get('search_status', 'missing') for r in report_rows)),
               'gate': {'passed': len(qualified) >= 16 and rescued >= 8,
                        'criteria': 'at least 16 qualified boss routes and 8/32 actual rescues of student boss deaths',
                        'scope': 'only opens a separate boss-learning plan, no model or high-level qualification'},
               'source_manifest': read(out / 'invocation.json')}
    write(out / 'boss-summary.json', summary)
    return summary


def run(args):
    out = args.out
    assert not (out / 'invocation.json').exists() and not (out / 'student').exists() and not (out / 'batch').exists(), 'refuse to restart formal diagnosis'
    write(out / 'invocation.json', preflight(args.checkpoint))
    began = time.monotonic()
    commands = [('collect', [sys.executable, 'tools/nosl/student_roots.py', 'collect',
                 '--checkpoint', str(args.checkpoint), '--seed-prefix', 'NQ4S', '--count', '32',
                 '--max-steps', '30000', '--timeout', '300', '--out', str(out / 'student')]),
                ('batch', [sys.executable, 'tools/nosl/student_roots.py', 'batch',
                 '--collection', str(out / 'student'), '--dotnet', str(args.dotnet), '--min-floor', '17',
                 '--min-qualified', '16', '--out', str(out / 'batch')])]
    for label, command in commands:
        with (out / 'events.jsonl').open('a') as f:
            f.write(json.dumps({'stage': label, 'command': command, 'began_seconds': time.monotonic() - began}) + '\n')
        with (out / (label + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
        with (out / 'events.jsonl').open('a') as f:
            f.write(json.dumps({'stage': label, 'returncode': result.returncode,
                                'ended_seconds': time.monotonic() - began}) + '\n')
        assert result.returncode == 0, (label, result.returncode)
    summary = verify(out)
    (out / 'complete').write_text('complete\n')
    print(json.dumps(summary))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--checkpoint', type=Path)
    p.add_argument('--dotnet', type=Path, default=Path.home() / '.dotnet/dotnet')
    p.add_argument('--mode', choices=('run', 'verify'), default='verify')
    args = p.parse_args()
    os.chdir(ROOT)
    args.out.mkdir(parents=True, exist_ok=True)
    if args.mode == 'run':
        assert args.checkpoint is not None
        try:
            run(args)
        except Exception as error:
            write(args.out / 'failed.json', {'error': type(error).__name__, 'detail': str(error)})
            raise
    else:
        print(json.dumps(verify(args.out)))


if __name__ == '__main__':
    main()
