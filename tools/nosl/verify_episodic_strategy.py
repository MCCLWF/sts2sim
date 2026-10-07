#!/usr/bin/env python3
"""Read-only independent budget, split, terminal and pairing verification."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
import torch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'python'))
from nosl.episodic_strategy import START_SHA,digest,models
from nosl.model import load


def read(path):
    return json.loads(path.read_text())


def lines(path):
    return [json.loads(s) for s in path.read_text().splitlines() if s]


def verify(start,out):
    assert hashlib.sha256(start.read_bytes()).hexdigest()==START_SHA
    combat,_,_=models(start,'cpu');combat_hash=digest(combat)
    expected=read(out/'results.json');experiment=read(out/'experiment.json')
    assert experiment['checkpoint_sha256']==START_SHA and experiment['seeds']==[1,3]
    assert experiment['updates']==128 and experiment['episodes_per_update']==32
    assert experiment['precision']=='float32' and experiment['evaluation_runs']==1024
    training={};hashes={};configs=[]
    for seed in (1,3):
        label=f'S{seed}';d=out/label;c=read(d/'config.json');s=read(d/'summary.json')
        assert c['source_commit']==experiment['source_commit']
        assert c['seed']==seed and c['checkpoint_sha256']==START_SHA and c['prefix']=='NQ4YTRAIN'
        assert c['gamma']==1 and c['clip']==.2 and c['epochs']==2 and c['minibatch']==256
        assert c['lr']==.000133 and c['entropy']==.05 and c['value_coef']==.5
        assert c['updates']==128 and c['episodes_per_update']==32 and c['seconds_limit']==1800
        configs.append({k:v for k,v in c.items() if k!='seed'})
        episodes=lines(d/'episodes.jsonl');updates=lines(d/'train.jsonl')
        seeds=[f'NQ4YTRAIN{seed}{i:04d}{j:02d}' for i in range(128) for j in range(32)]
        assert [r['seed'] for r in episodes]==seeds[:len(episodes)] and len(episodes)%32==0
        for r in episodes:
            assert r['ascension']==10 and r['actor_filter']==1
            assert r['mc_qualified']==(r['status'] in (1,2))
            assert sum(r['routing'].values())==len(r['action_rows'])
            if r['mc_qualified']:assert np.isfinite(r['reward_sum'])
            else:assert r['reward_sum'] is None
        assert s['attempts']==len(episodes) and s['faults']==sum(not r['mc_qualified'] for r in episodes)
        assert s['decisions']==sum(len(r['action_rows']) for r in episodes)
        assert [r['iteration'] for r in updates]==list(range(1,len(updates)+1))
        assert s['updates']==len(updates) and s['strategic_rows']==sum(r['rows'] for r in updates)
        assert s['optimizer_steps']==sum(r['optimizer_steps'] for r in updates)
        assert all(r['attempts']==r['iteration']*32 for r in updates)
        assert all(np.isfinite(v) for r in updates for k,v in r.items() if isinstance(v,(int,float)))
        assert s['combat_parameters_unchanged'] and s['combat_parameter_sha256']==combat_hash==c['combat_parameter_sha256']
        model,blob=load(d/'checkpoint.pt','cpu')
        assert blob['actor_filter']==1 and blob['combat_checkpoint_sha256']==START_SHA
        assert blob['iteration']==s['updates'] and blob['decisions']==s['decisions']
        assert model.config==combat.config and digest(model)==s['strategic_parameter_sha256']
        assert all(torch.isfinite(t).all() for t in model.state_dict().values())
        if s['status']=='complete':assert len(episodes)==4096 and len(updates)==128
        else:assert len(episodes)<=4096 and len(updates)<128
        training[label]=s;hashes[label]=hashlib.sha256((d/'checkpoint.pt').read_bytes()).hexdigest()
    assert configs[0]==configs[1]
    seeds=[f'NQ4YEVAL{i:06d}' for i in range(1024)]
    rows={label:lines(out/'evaluation'/f'{label}.jsonl') for label in ('X1','S1','S3')}
    policies={};paired={}
    for label,attempts in rows.items():
        assert [r['seed'] for r in attempts]==seeds
        for r in attempts:
            assert r['mc_qualified']==(r['status'] in (1,2))
            if 'routing' in r:assert sum(r['routing'].values())==len(r['action_rows'])
        good=[r for r in attempts if r['mc_qualified']]
        policies[label]={'attempts':len(attempts),'qualified':len(good),
            'ends':dict(collections.Counter(r['end'] for r in attempts)),
            'floor_mean':float(np.mean([r['floor'] for r in good])) if good else None,
            'act2':sum(r['act']>=1 for r in good),'act3':sum(r['act']>=2 for r in good),
            'victories':sum(r['end']=='victory' for r in good)}
    complete=all(r['mc_qualified'] for arm in rows.values() for r in arm)
    if complete:
        for label in ('S1','S3'):
            delta=np.asarray([r['floor'] for r in rows[label]])-np.asarray([r['floor'] for r in rows['X1']])
            generator=np.random.default_rng(20261005)
            indices=generator.integers(0,1024,(10000,1024))
            ci=np.quantile(delta[indices].mean(axis=1),[.025,.975])
            paired[label]={'floor_gain':float(delta.mean()),'ci95':ci.tolist(),
                'gate':bool(delta.mean()>=.5 and ci[0]>0 and training[label]['status']=='complete' and training[label]['faults']==0)}
    gate=bool(complete and all(paired[label]['gate'] for label in ('S1','S3'))
        and max(policies[label]['act3'] for label in ('S1','S3'))>=policies['X1']['act3'])
    assert policies==expected['policies'] and paired==expected['paired']
    assert training==expected['trained'] and complete==expected['all_evaluation_runs_qualified'] and gate==expected['gate_passed']
    assert expected['combat_parameters_sha256']==combat_hash and expected['baseline_checkpoint_sha256']==START_SHA
    return {'independent_verification':True,'training':training,'checkpoint_sha256':hashes,
            'evaluation':policies,'paired':paired,'gate_passed':gate,'combat_parameter_sha256':combat_hash}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint',required=True);p.add_argument('--experiment',required=True);p.add_argument('--out',required=True)
    args=p.parse_args();torch.set_num_threads(1)
    result=verify(Path(args.checkpoint),Path(args.experiment))
    Path(args.out).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
