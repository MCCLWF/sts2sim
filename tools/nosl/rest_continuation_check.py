#!/usr/bin/env python3
"""Qualify the frozen Q4-v rest continuation without new evaluation or learning."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import oracle_replay


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def write(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n')


def verify_old(old):
    manifest = json.loads((old / 'manifest.json').read_text())
    for record in manifest['files']:
        data = (old / record['path']).read_bytes()
        assert len(data) == record['bytes'] and hashlib.sha256(data).hexdigest() == record['sha256'], record['path']
    return len(manifest['files'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    old, out = args.old.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    assert not (out / 'invocation.json').exists(), 'refuse to restart the fixed qualification'
    write(out / 'invocation.json', {'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                                  'old_files_verified': verify_old(old), 'oracle_test_mode': False})
    began = time.monotonic()
    cases = []
    for arm in ('C1', 'P1', 'C3', 'P3'):
        for mode in ('combat', 'outside'):
            label = arm + '-' + mode
            for row in rows(old / 'cloud/experiment/trajectories' / (label + '.jsonl')):
                cases.append((label, row, old / 'oracle-normal' / label / row['seed']))
    for label in ('P1-combat', 'P3-full'):
        for row in rows(old / (label + '-deep.jsonl')):
            cases.append((label + '-deep', row, old / 'oracle-normal-deep' / label / row['seed']))
    assert len(cases) == 18
    results = []
    for label, trajectory, baseline in cases:
        work = out / 'batch' / label
        work.mkdir(parents=True, exist_ok=True)
        result = oracle_replay.one((trajectory, work, 180, 'off'))
        current = work / trajectory['seed']
        result['label'] = label
        assert result['status'] == 'identical' and result['oracle_test_mode'] is False, result
        if label != 'P3-full-deep':
            for filename in ('script.jsonl', 'oracle.jsonl'):
                assert (baseline / filename).read_bytes() == (current / filename).read_bytes(), (label, filename)
            result['old_success_bytes_unchanged'] = True
        else:
            previous = [e for e in rows(baseline / 'oracle.jsonl') if e['event'] != 'end']
            assert rows(current / 'oracle.jsonl')[:len(previous)] == previous, 'frozen 301-decision prefix changed'
            result['old_prefix_unchanged'] = True
        results.append(result)
        print(json.dumps(result), flush=True)
    write(out / 'results.json', results)

    deep = out / 'batch/P3-full-deep/NQ4VEVAL000330'
    snapshot_command = [sys.executable, str(ROOT / 'tests/l4/l4_run.py'), '--trace', str(deep / 'oracle.jsonl'),
                        '--verify-snapshots', '--result-json', str(out / 'snapshots.json')]
    with (out / 'snapshots.log').open('w') as log:
        completed = subprocess.run(snapshot_command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    snapshot = json.loads((out / 'snapshots.json').read_text())
    assert completed.returncode == 0 and snapshot['results'][0]['status'] == 'identical'
    assert snapshot['results'][0]['identity']['oracle_test_mode'] is False

    # Capture the dynamic remaining index from the actual shipped-DLL trace.
    script = rows(deep / 'script.jsonl')
    second = None
    previous_floor = None
    action_index = 0
    current_options = None
    current_floor = None
    for event in rows(deep / 'oracle.jsonl'):
        if event['event'] == 'decision' and event['kind'] == 'rest':
            current_options, current_floor = event['options'], event['run']['total_floor']
        if event['event'] == 'action':
            if event['kind'] == 'rest' and 'index' in event:
                if current_floor == previous_floor:
                    second = (action_index, len(current_options))
                    break
                previous_floor = current_floor
            action_index += 1
    assert second is not None
    index, count = second
    assert script[index]['kind'] == 'rest'
    negatives = {}
    bad = [dict(action) for action in script]
    bad[index]['index'] = count
    negatives['remaining-index-out-of-range'] = ('NQ4VEVAL000330', bad)
    bad = [dict(action) for action in script]
    card = next(i for i in range(index + 1, len(bad)) if bad[i]['kind'] == 'card_select')
    bad[card]['indices'] = [99999]
    negatives['invalid-card-selection'] = ('NQ4VEVAL000330', bad)
    ordinary = out / 'batch/C1-combat/NQ4VEVAL000000'
    bad = rows(ordinary / 'script.jsonl')
    first = next(i for i, action in enumerate(bad) if action['kind'] == 'rest' and 'index' in action)
    following_map = next(i for i in range(first + 1, len(bad)) if bad[i]['kind'] == 'map')
    before = rows(ordinary / 'oracle.jsonl')
    first_rest = next(e for e in before if e['event'] == 'decision' and e['kind'] == 'rest')
    assert not any(r['id'] == 'MINIATURE_TENT' for r in first_rest['run']['relics'])
    bad.insert(following_map, {'event': 'action', 'kind': 'rest', 'index': 0})
    negatives['second-choice-without-tent'] = ('NQ4VEVAL000000', bad)
    negative_results = {}
    for label, (seed, actions) in negatives.items():
        work = out / 'negative' / label
        work.mkdir(parents=True)
        with (work / 'script.jsonl').open('w') as stream:
            for action in actions:
                stream.write(json.dumps(action) + '\n')
        command = [oracle_replay.DOTNET, str(oracle_replay.ORACLE), 'run', '--seed', seed, '--character', 'SILENT',
                   '--ascension', '10', '--script', str(work / 'script.jsonl'), '--stop', 'victory', '--oracle-test-mode', 'off']
        with (work / 'oracle.jsonl').open('w') as log, (work / 'stderr.txt').open('w') as error:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=error, timeout=180)
        events = rows(work / 'oracle.jsonl')
        assert next(e for e in events if e['event'] == 'begin')['oracle_test_mode'] is False
        end = next(e for e in reversed(events) if e['event'] == 'end')
        assert end['reason'] == 'error', (label, end)
        negative_results[label] = {'rejected': True, 'message': end.get('message'), 'index': index if seed.endswith('330') else None}
        print(json.dumps({'negative': label, **negative_results[label]}), flush=True)
    summary = {'trajectories': 18, 'fully_qualified': 18, 'decisions_matched': sum(r['matched'] for r in results),
               'unchanged_successes': 17, 'unchanged_prefix_decisions': 301,
               'repaired_deep_decisions': next(r['matched'] for r in results if r['label'] == 'P3-full-deep'),
               'snapshot_identical': True, 'negative_tests': negative_results, 'old_files_verified_after': verify_old(old),
               'seconds': time.monotonic() - began, 'passed': True}
    write(out / 'summary.json', summary)
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()
