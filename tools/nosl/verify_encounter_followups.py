#!/usr/bin/env python3
"""Replay exactly the preregistered frozen candidates, including all failures."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from nosl.teacher_distill import oracle_module
from oracle_replay import one


def lines(path):return [json.loads(s) for s in path.read_text().splitlines() if s]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--experiment', required=True);parser.add_argument('--out', required=True)
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args();experiment = Path(args.experiment);out = Path(args.out)
    out.mkdir(parents=True, exist_ok=False);l4 = oracle_module();results = []
    for label in ('C1','R1','C3','R3'):
        attempts = lines(experiment/'evaluation'/(label+'.jsonl'))
        selected = attempts[:2];good = [r for r in attempts if r['mc_qualified']]
        if good:
            deepest = max(good, key=lambda r: r['floor'])
            if deepest['seed'] not in {r['seed'] for r in selected}:selected = selected+[deepest]
        assert selected == lines(experiment/(label+'-normal-inputs.jsonl'))
        for trajectory in selected:
            arm = out/label
            previous = one((trajectory, arm, args.timeout, 'off'))
            result = {'arm': label, 'seed': trajectory['seed'], 'floor': trajectory.get('floor'),
                'act': trajectory.get('act'), 'pipeline': previous, 'whole_run_qualified': False}
            trace = arm/trajectory['seed']/'oracle.jsonl'
            if previous['status'] == 'identical' and previous.get('script') == 'complete':
                try:
                    run = l4.parse_runs(trace.read_text().splitlines())[0];begin = run['begin']
                    assert begin['oracle_test_mode'] is False and begin['seed'] == trajectory['seed']
                    assert begin['character'] == 'SILENT' and begin['ascension'] == 10
                    assert begin['hp'] is None and begin['deck'] is None and not begin['relics']
                    assert run['end']['run']['total_floor'] == trajectory['floor']
                    decisions, end, error = l4.replay(str(ROOT/'build/sts2sim_cli'), run,
                        verify_snapshots=True, timeout=args.timeout,
                        artifacts=str(trace.parent/'strict-snapshots-native'))
                    comparison = l4.compare_run(run, decisions, end, error)
                    diff = l4.first_diff(run['end']['run'], (end or {}).get('run'))
                    result.update(comparison=comparison, end_run_first_diff=diff, native_error=error,
                        whole_run_qualified=bool(comparison['status']=='identical'
                            and comparison['matched']==comparison['total'] and error is None and diff is None))
                except Exception as error:
                    result['verification_error'] = type(error).__name__+': '+str(error)
            results.append(result)
            with (out/'results.jsonl').open('a') as stream:stream.write(json.dumps(result)+'\n')
            print(json.dumps(result), flush=True)
    summary = {'registered_selection_verified': True, 'runs': len(results),
        'whole_run_qualified': sum(r['whole_run_qualified'] for r in results),
        'decisions_matched': sum(r.get('comparison',{}).get('matched',0) for r in results),
        'oracle_test_mode': False, 'verify_snapshots': True,
        'failures': [r for r in results if not r['whole_run_qualified']]}
    (out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n');print(json.dumps(summary), flush=True)
    return 0 if summary['whole_run_qualified']==summary['runs'] else 1


if __name__ == '__main__':sys.exit(main())
