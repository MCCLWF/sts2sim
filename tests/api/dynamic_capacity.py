#!/usr/bin/env python3
"""Large-state public ABI/SDK test consumer; no gameplay oracle qualification."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'python'))
from sts2sim_env import Env, _LIB, OperationCapacityExceeded

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output-dir',type=Path,required=True)
    args=parser.parse_args()
    args.output_dir.mkdir(parents=True,exist_ok=False)
    fixture=ctypes.CDLL(str(ROOT/'build/libenv_api_capacity_fixture.so'))
    fixture.sts2_test_capacity_snapshot.argtypes=[ctypes.c_int32,ctypes.c_int32]
    fixture.sts2_test_capacity_snapshot.restype=ctypes.c_char_p
    records=[]
    for pending in (0,1):
        payload=fixture.sts2_test_capacity_snapshot(6500,pending)
        assert payload and len(payload)>4*1024*1024
        path=args.output_dir/f'large-{pending}.portable.json'
        path.write_bytes(payload)
        env=Env.restore_snapshot(json.loads(payload))
        source_hash=env.state_hash
        actions=env.legal_actions(); mask=env.action_mask(); observation=env.observe()
        assert _LIB.sts2_env_legal_actions(env._handle,None,0)==len(actions)
        assert _LIB.sts2_env_action_mask(env._handle,None,0)==len(mask)
        assert _LIB.sts2_env_observe(env._handle,None,0)==len(observation)
        assert sum(mask)==len(actions) and observation[0]==2 and env.state_hash==source_hash
        if pending:
            assert len(actions)==6500 and len(mask)==6500 and len(observation)>2048
            # An undersized buffer is rejected without corrupting its sentinel.
            sentinel=(ctypes.c_uint8*2)(123,124)
            assert _LIB.sts2_env_action_mask(env._handle,sentinel,2)==-1 and list(sentinel)==[123,124]
        restored=Env.restore_snapshot(env.export_snapshot())
        assert restored.state_hash==source_hash and restored.legal_actions()==actions
        a=env.predict(actions[0]); b=restored.predict(actions[0])
        assert a.state_hash==b.state_hash and env.state_hash==source_hash
        answers=0
        if pending:
            while a.decision==7:
                a=a.predict(a.legal_actions()[0]); answers+=1
            assert answers==2 and a.decision==0
            assert len(a.export_run_queries()['player']['deck'])==6500
        bad=env.export_snapshot()
        bad['state']['cards']['capacity']=2147483647
        try:
            Env.restore_snapshot(bad)
            raise AssertionError('huge metadata allocation was accepted')
        except OperationCapacityExceeded as error:
            assert 'resource budget' in str(error)
        assert env.state_hash==source_hash
        records.append({'pending':bool(pending),'snapshot_bytes':len(payload),'actions':len(actions),
                        'mask':len(mask),'observation':len(observation),'remaining_answers':answers,
                        'snapshot_sha256':hashlib.sha256(payload).hexdigest()})
    (args.output_dir/'summary.json').write_text(json.dumps({'qualified':True,'scope':'injected capacity contract only','cases':records},indent=2)+'\n')
    print(json.dumps(records))

if __name__=='__main__': main()
