#!/usr/bin/env python3
"""Normal Silent A10 regressions for inspection UI and map-boundary adapters.

Only observed policy action rows are fixed inputs. All expected gameplay states,
RNG, snapshots and complete endpoints come from the shipped game at TestMode=false.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/nosl'))
from oracle_replay import one

CASES = [('pandora', {'seed': 'NQ4ABPILOT000007', 'ascension': 10, 'actor_filter': 1, 'action_rows': [2, 1, 0, 2, 0, 3, 1, 0, 0, 1, 0, 0, 1, 3, 3, 1, 3, 3, 2, 4, 2, 1, 3, 3, 2, 0, 0, 0, 0, 3, 0, 8, 4, 2, 0, 0, 2, 2, 1, 2, 4, 0, 0, 0, 0, 0, 0, 3, 1, 1, 1, 2, 0, 1, 3, 1, 1, 0, 1, 1, 0, 1, 3, 2, 0, 0, 0, 1, 0, 0, 0, 4, 1, 1, 2, 0, 4, 2, 0, 2, 7, 5, 0, 0, 0, 5, 8, 3, 1, 0, 4, 1, 0, 1, 0, 4, 5, 4, 0, 0, 3, 1, 2, 3, 1, 0, 0, 3, 2, 2, 1, 0, 3, 3, 2, 0, 1, 0, 0, 2, 1, 0, 2, 3, 0, 0, 2, 0, 0, 1, 1, 0, 0, 0, 2, 4, 2, 0, 2, 4, 0, 0, 1, 0, 2, 1, 1, 0, 4, 3, 3, 1, 0, 3, 3, 0, 3, 0, 0, 4, 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 9, 8, 8, 1, 6, 1, 0, 7, 2, 7, 6, 1, 3, 0, 1, 2, 0, 3, 2, 1, 1, 0, 3, 1, 1, 3, 0, 1, 1, 0, 0, 4, 4, 3, 0, 0, 1, 0, 2, 3, 1, 0, 2, 3, 1, 1, 2, 1, 0, 3, 8, 4, 3, 0, 2, 1, 0, 0, 0, 5, 1, 3, 3, 3, 1, 1, 0, 2, 0, 1, 0, 4, 4, 4, 1, 1, 1, 3, 4, 2, 1, 2, 0, 0, 2, 3, 2, 1, 0, 0, 0, 0, 1, 1, 2, 3, 1, 1, 3, 0, 0, 4, 0, 2, 4, 3, 0, 1, 0, 1, 2, 0, 1, 5, 5, 3, 4, 1, 2, 1, 0, 1, 3, 0, 0, 0, 1, 0, 2, 2, 1, 2, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 10, 10, 1, 0, 0, 0, 0, 4, 7, 2, 0, 2, 0, 4, 5, 2, 3, 0, 1, 0, 1, 5, 3, 1, 1, 0, 0, 1, 0, 4, 0, 1, 1, 1, 0, 2, 1, 0, 2, 4, 2, 2, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 4, 0, 3, 4, 6, 1, 0, 0, 0, 1, 0, 3, 0, 1, 0, 2, 3, 4, 3, 2, 0, 0, 0, 0, 2, 1, 0, 0, 1, 0, 5, 0, 0, 1, 1, 1, 0, 2, 6, 5, 3, 2, 0, 1, 1, 1, 0], 'end': 'death', 'floor': 30}), ('winged-boots-boss', {'seed': 'NQ4ABSUPPUPD000000', 'ascension': 10, 'actor_filter': 1, 'action_rows': [1, 4, 3, 1, 2, 3, 3, 1, 0, 3, 3, 0, 4, 2, 1, 0, 3, 1, 0, 0, 1, 0, 0, 3, 0, 6, 2, 2, 3, 2, 3, 2, 2, 4, 2, 2, 2, 5, 6, 4, 2, 0, 0, 0, 0, 0, 3, 1, 0, 1, 0, 1, 0, 0, 3, 1, 1, 1, 3, 0, 3, 0, 3, 1, 3, 0, 4, 1, 4, 3, 3, 2, 0, 0, 3, 1, 1, 0, 3, 0, 0, 0, 11, 5, 9, 2, 7, 4, 1, 8, 5, 2, 1, 6, 3, 0, 3, 4, 4, 4, 0, 3, 2, 4, 4, 4, 4, 4, 0, 0, 2, 2, 0, 1, 1, 0, 2, 1, 2, 0, 1, 0, 3, 2, 1, 1, 2, 0, 2, 0], 'end': 'death', 'floor': 17})]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',required=True);a=p.parse_args()
    out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
    spec=importlib.util.spec_from_file_location('normal_adapter_l4',ROOT/'tests/l4/l4_run.py')
    l4=importlib.util.module_from_spec(spec);sys.modules[spec.name]=l4;spec.loader.exec_module(l4)
    results=[]
    for label,row in CASES:
        prior=one((row,out/label,300,'off'))
        assert prior['status']=='identical' and prior['script']=='complete',prior
        trace=out/label/row['seed']/'oracle.jsonl';run=l4.parse_runs(trace.read_text().splitlines())[0]
        begin=run['begin']
        assert begin['oracle_test_mode'] is False and begin['character']=='SILENT' and begin['ascension']==10
        assert begin['hp'] is None and begin['deck'] is None and not begin['relics']
        decisions,end,error=l4.replay(str(ROOT/'build/sts2sim_cli'),run,verify_snapshots=True,
            timeout=300,artifacts=str(trace.parent/'strict-snapshots-native'))
        comparison=l4.compare_run(run,decisions,end,error)
        diff=l4.first_diff(run['end']['run'],(end or {}).get('run'))
        assert comparison['status']=='identical' and comparison['matched']==comparison['total'] and error is None and diff is None,(comparison,error,diff)
        results.append({'case':label,'seed':row['seed'],'comparison':comparison,'end_run_first_diff':diff})
    result={'runs':len(results),'all_normal_whole_runs_identical':True,'oracle_test_mode':False,'verify_snapshots':True,'results':results}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
