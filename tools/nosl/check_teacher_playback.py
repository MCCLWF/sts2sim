#!/usr/bin/env python3
"""Replay frozen Q4-s teacher routes and guard automatic selection identities.

No search, collection, student update or strength evaluation. New artifacts
preserve the old diagnosis and include all thirteen predicted winning routes,
32 ordinary student controls and five damaged-plan rejection checks.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

import student_roots

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def run(args):
    out, source = args.out.resolve(), args.source.resolve()
    out.mkdir(parents=True, exist_ok=True)
    assert not (out / 'invocation.json').exists(), 'refuse to overwrite replay evidence'
    original = read(source.parent / 'artifact-manifest.json')
    for item in original['files']:
        data = (source.parent / item['path']).read_bytes()
        assert len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256'], item['path']
    seeds = [f'NQ4S{i:06d}' for i in range(32)]
    source_rows = rows(source / 'batch/results.jsonl')
    assert [r['seed'] for r in source_rows] == seeds
    selected = [r for r in source_rows if r.get('all_enemies_dead') and r.get('search_qualified')]
    assert len(selected) == 13 and sum(r['status'] == 'route_qualified' for r in selected) == 8
    manifest = {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                'frozen_source': str(source), 'frozen_manifest_sha256': student_roots.sha(source.parent / 'artifact-manifest.json'),
                'harness_sha256': student_roots.sha(ROOT / 'tools/NoSLTeacherHarness/bin/Release/net9.0/OfflineSearchHarness.dll'),
                'oracle_sha256': student_roots.sha(ROOT / 'tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll'),
                'routes': [r['seed'] for r in selected], 'student_seeds': seeds,
                'searches': 0, 'student_updates': 0}
    write(out / 'invocation.json', manifest)
    began = time.monotonic()
    route_results = []
    for source_row in selected:
        seed = source_row['seed']
        old = source / 'batch' / seed
        new = out / 'routes' / seed
        options = argparse.Namespace(dotnet=args.dotnet, seed=seed, script=old / 'script.jsonl', out=new,
                    min_floor=17, beam=30, nodes=10000, budget_ms=10000, timeout=90,
                    route=old / 'search/teacher.json')
        code = student_roots.teacher(options)
        result = {'seed': seed, 'old_status': source_row['status'], 'teacher_returncode': code, 'qualified': False}
        if code == 0:
            student_roots.verify(argparse.Namespace(out=new, timeout=90))
            result['qualified'] = read(new / 'env-contract.json')['qualified']
            evidence = read(new / 'teacher.json')
            frozen = read(old / 'search/teacher.json')
            assert evidence['actions_replayed'] == len(frozen['route_actions'])
            assert evidence['terminal_hp'] == frozen['metrics']['finalHp'] and evidence['predicted_hp_matched']
            assert evidence['oracle_test_mode'] is False
            result.update(actions=evidence['actions_replayed'], choices=evidence['choices_replayed'],
                          automatic_choices=evidence['automatic_choices_replayed'], hp=evidence['terminal_hp'],
                          decision_budget=evidence['replacement_decision_budget'],
                          decisions=read(new / 'env-contract.json')['l4']['matched'])
            if source_row['status'] == 'route_qualified':
                result['old_trace_identical'] = (new / 'oracle.jsonl').read_bytes() == (old / 'native/oracle.jsonl').read_bytes()
        route_results.append(result)
        write(out / 'route-results.json', route_results)

    ordinary = []
    oracle = ROOT / 'tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll'
    for seed in seeds:
        directory = out / 'ordinary' / seed
        directory.mkdir(parents=True)
        old = source / 'batch' / seed
        command = [str(args.dotnet), str(oracle), 'run', '--seed', seed, '--character', 'SILENT',
                   '--ascension', '10', '--script', str(old / 'script.jsonl'), '--stop', 'victory', '--oracle-test-mode', 'off']
        with (directory / 'oracle.jsonl').open('wb') as stdout, (directory / 'oracle.log').open('wb') as stderr:
            completed = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=90, check=False)
        result = {'seed': seed, 'returncode': completed.returncode,
                  'trace_identical': (directory / 'oracle.jsonl').read_bytes() == (old / 'search/oracle.jsonl').read_bytes(),
                  'command': command}
        ordinary.append(result)
        write(out / 'ordinary-results.json', ordinary)

    negatives = []
    old = source / 'batch/NQ4S000001'
    original_route = read(old / 'search/teacher.json')
    automatic_action = next(i for i, a in enumerate(original_route['route_actions']) if a.get('CardId') == 'HIDDEN_DAGGERS')
    for case in ('option_occurrence', 'source_occurrence', 'state_key', 'missing_choice', 'order'):
        directory = out / 'negative-fixtures' / case
        directory.mkdir(parents=True)
        damaged = copy.deepcopy(original_route)
        action = damaged['route_actions'][automatic_action]
        if case == 'missing_choice':
            action['Choice'] = None
        elif case == 'order':
            action['Choice']['Cards'].reverse()
        else:
            token = action['Choice']['Cards'][0]
            field = {'option_occurrence': 'OptionOccurrence', 'source_occurrence': 'SourceOccurrence', 'state_key': 'StateKey'}[case]
            token[field] = token[field] + '_INVALID' if case == 'state_key' else 999
        write(directory / 'teacher.json', damaged)
        (directory / 'root-before.json').write_bytes((old / 'search/root-before.json').read_bytes())
        new = out / 'negative-replays' / case
        options = argparse.Namespace(dotnet=args.dotnet, seed='NQ4S000001', script=old / 'script.jsonl', out=new,
                    min_floor=17, beam=30, nodes=10000, budget_ms=10000, timeout=90, route=directory / 'teacher.json')
        code = student_roots.teacher(options)
        log = (new / 'harness.log').read_text()
        expected = {'option_occurrence': 'Teacher selection option identity drift',
                    'source_occurrence': 'Teacher selection source occurrence/full card state drift',
                    'state_key': 'Teacher selection source occurrence/full card state drift',
                    'missing_choice': 'Real automatic hand selection has no matching teacher token',
                    'order': 'Teacher automatic selection order differs from the real result'}[case]
        negatives.append({'case': case, 'returncode': code, 'targeted_rejection': code != 0 and expected in log})
        write(out / 'negative-results.json', negatives)
    summary = {'routes': len(route_results), 'qualified_routes': sum(r['qualified'] for r in route_results),
               'old_failed_routes_repaired': sum(r['qualified'] and r['old_status'] == 'failed' for r in route_results),
               'old_successful_traces_identical': sum(bool(r.get('old_trace_identical')) for r in route_results),
               'ordinary_student_traces_identical': sum(r['returncode'] == 0 and r['trace_identical'] for r in ordinary),
               'targeted_negative_rejections': sum(r['targeted_rejection'] for r in negatives),
               'wall_seconds': time.monotonic() - began, 'searches': 0, 'student_updates': 0,
               'passed': all(r['qualified'] for r in route_results)
                   and all(r.get('old_trace_identical', True) for r in route_results)
                   and all(r['returncode'] == 0 and r['trace_identical'] for r in ordinary)
                   and all(r['targeted_rejection'] for r in negatives)}
    write(out / 'summary.json', summary)
    print(json.dumps(summary))
    return 0 if summary['passed'] else 1


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=ROOT / 'artifacts/nosl-q4s/diagnosis')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--dotnet', type=Path, default=Path.home() / '.dotnet/dotnet')
    args = p.parse_args()
    os.chdir(ROOT)
    return run(args)


if __name__ == '__main__':
    raise SystemExit(main())
