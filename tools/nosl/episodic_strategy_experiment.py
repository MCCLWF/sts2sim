#!/usr/bin/env python3
"""Fixed Q4-y two-seed training and paired complete-run evaluation."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'python'))
from nosl import native
from nosl.episodic_strategy import START_SHA,collect,digest,models,RoutedPolicy,train
from nosl.model import load


def write(path,value):
    path.write_text(json.dumps(value,indent=2)+'\n')


def evaluate(checkpoint,out,device):
    combat,start,blob=models(checkpoint,device)
    policies={'X1':RoutedPolicy(combat,start)}
    trained={}
    for seed in (1,3):
        d=out/f'S{seed}';trained[f'S{seed}']=json.loads((d/'summary.json').read_text())
        candidate,meta=load(d/'checkpoint.pt',device);candidate.eval()
        assert meta['actor_filter']==1 and meta['combat_checkpoint_sha256']==START_SHA
        policies[f'S{seed}']=RoutedPolicy(combat,candidate)
    seeds=[f'NQ4YEVAL{i:06d}' for i in range(1024)]
    rows={};deadline=time.monotonic()+1200
    evaluation=out/'evaluation';evaluation.mkdir(exist_ok=False)
    for label,policy in policies.items():
        attempts=[]
        with (evaluation/f'{label}.jsonl').open('w') as stream:
            for begin in range(0,len(seeds),64):
                selected=seeds[begin:begin+64]
                if time.monotonic()>=deadline:
                    block=[{'seed':s,'status':native.RUNNING,'end':'not_started','mc_qualified':False,
                            'error':'registered evaluation deadline','action_rows':[]} for s in selected]
                else:
                    block=collect(policy,selected,device,sample=False,deadline=deadline,keep_public=False)['outcomes']
                for row in block:stream.write(json.dumps(row)+'\n')
                stream.flush();attempts.extend(block)
                print(json.dumps({'evaluation':label,'attempts':len(attempts),'qualified':sum(r['mc_qualified'] for r in attempts)}),flush=True)
        rows[label]=attempts
    result={'trained':trained,'attempts_per_policy':1024,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
            'combat_parameters_sha256':digest(combat),'baseline_checkpoint_sha256':START_SHA,'policies':{},'paired':{}}
    for label,attempts in rows.items():
        good=[r for r in attempts if r['mc_qualified']]
        result['policies'][label]={'attempts':len(attempts),'qualified':len(good),
            'ends':dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean':float(np.mean([r['floor'] for r in good])) if good else None,
            'act2':sum(r['act']>=1 for r in good),'act3':sum(r['act']>=2 for r in good),
            'victories':sum(r['end']=='victory' for r in good)}
    all_qualified=all(r['mc_qualified'] for attempts in rows.values() for r in attempts)
    if all_qualified:
        for label in ('S1','S3'):
            delta=np.asarray([b['floor']-a['floor'] for a,b in zip(rows['X1'],rows[label])],np.float64)
            rng=np.random.default_rng(20261005)
            ci=np.quantile(delta[rng.integers(0,len(delta),(10000,len(delta)))].mean(1),[.025,.975])
            result['paired'][label]={'floor_gain':float(delta.mean()),'ci95':ci.tolist(),
                'gate':bool(delta.mean()>=.5 and ci[0]>0 and trained[label]['status']=='complete' and trained[label]['faults']==0)}
    result['all_evaluation_runs_qualified']=all_qualified
    result['gate_passed']=bool(all_qualified and all(result['paired'][label]['gate'] for label in ('S1','S3'))
        and max(result['policies'][label]['act3'] for label in ('S1','S3'))>=result['policies']['X1']['act3'])
    write(out/'results.json',result)
    # Preregistered normal-rule follow-up inputs; retain all other trajectories.
    for label in ('S1','S3'):
        selected=rows[label][:2]
        good=[r for r in rows[label] if r['mc_qualified']]
        if good:
            deepest=max(good,key=lambda r:r['floor'])
            if deepest['seed'] not in {r['seed'] for r in selected}:selected=selected+[deepest]
        with (out/f'{label}-normal-inputs.jsonl').open('w') as stream:
            for row in selected:stream.write(json.dumps(row)+'\n')
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode',choices=('run','evaluate'))
    p.add_argument('--checkpoint',required=True)
    p.add_argument('--out',required=True)
    p.add_argument('--device',default='cuda')
    args=p.parse_args();out=Path(args.out)
    torch.set_num_threads(1)
    if args.mode=='run':
        out.mkdir(parents=True,exist_ok=False)
        write(out/'experiment.json',{'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
            'checkpoint_sha256':START_SHA,'native_sha256':hashlib.sha256((ROOT/'build/libsts2nosl_c.so').read_bytes()).hexdigest(),
            'seeds':[1,3],'updates':128,'episodes_per_update':32,'training_seconds_per_arm':1800,
            'evaluation_prefix':'NQ4YEVAL','evaluation_runs':1024,'device':args.device,'precision':'float32'})
        for seed in (1,3):
            print(json.dumps({'training_seed':seed,'result':train(args.checkpoint,out/f'S{seed}',args.device,seed)}),flush=True)
    result=evaluate(args.checkpoint,out,args.device)
    print(json.dumps(result),flush=True)
    return 0 if result['gate_passed'] else 1


if __name__=='__main__':sys.exit(main())
