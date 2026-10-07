#!/usr/bin/env python3
"""Actual normal runs, independent suffix sums and frozen-combat update proof."""
import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'python'))
from nosl.episodic_strategy import START_SHA,collect,digest,models,mc_returns,pack,RoutedPolicy,update
from nosl.model import save


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint',default=str(ROOT/'artifacts/nosl-q4r/start.pt'))
    p.add_argument('--out',required=True)
    p.add_argument('--device',default='cpu')
    args=p.parse_args()
    out=Path(args.out);out.mkdir(parents=True,exist_ok=False)
    torch.set_num_threads(1);torch.manual_seed(7)
    combat,strategic,_=models(args.checkpoint,args.device)
    policy=RoutedPolicy(combat,strategic)
    before=digest(combat);initial=digest(strategic)
    seeds=[f'NQ4YPILOT{i:06d}' for i in range(8)]
    data=collect(policy,seeds,args.device,sample=True,deadline=time.monotonic()+240)
    assert all(r['mc_qualified'] for r in data['outcomes'])
    independent=[]
    for rewards,samples in zip(data['rewards'],data['samples']):
        actual=mc_returns(rewards)
        for i in range(len(rewards)):
            assert abs(actual[i]-sum(rewards[i:]))<1e-10
        for entry in samples:
            independent.append(sum(rewards[entry['time']:]))
            assert entry['public']['global'][11]==0
            assert all(entry['public'][k].dtype==np.int32 for k in ('global','tokens','actions','counts'))
    np.testing.assert_allclose(data['return'],independent,rtol=1e-6,atol=1e-7)
    batch=pack(data['public'],np.arange(min(64,len(data['public']))),args.device)
    with torch.no_grad():
        a,av=combat(*batch);b,bv=policy(*batch)
    torch.testing.assert_close(a,b,rtol=1e-4,atol=2e-4)
    torch.testing.assert_close(av,bv,rtol=1e-4,atol=2e-4)
    optimizer=torch.optim.Adam(strategic.parameters(),lr=.000133)
    stats=update(strategic,optimizer,data,args.device,np.random.default_rng(7))
    assert digest(combat)==before and digest(strategic)!=initial
    # Real combat observations from normal seeds, not synthetic in-combat flags.
    from nosl import native
    from nosl.branch_credit import buffers,inputs
    env=native.Batch(8,256,256,30000);env.set_filter(1)
    observations=buffers(8,256,256)
    for slot,seed in enumerate(seeds):assert env.reset(slot,seed,10)==native.RUNNING
    env.step(np.full(8,-1,np.int32),*observations.values())
    checked=choices=outside=0
    for step in range(400):
        live=np.flatnonzero(observations['status']==native.RUNNING)
        if not len(live):break
        batch=inputs(observations,live,args.device)
        with torch.no_grad():
            base,value=combat(*batch);routed,rv=policy(*batch)
        active=batch[0][:,11]>0
        assert torch.equal(base[active],routed[active]) and torch.equal(value[active],rv[active])
        assert torch.isneginf(routed[torch.arange(routed.shape[1],device=routed.device)[None,:]>=batch[3][:,1:2]]).all()
        checked+=int(active.sum());choices+=int((active&(batch[0][:,1]==7)).sum());outside+=int((~active).sum())
        chosen=np.full(8,-1,np.int32);chosen[live]=routed.argmax(-1).cpu().numpy()
        env.step(chosen,*observations.values())
    assert checked>0 and choices>0 and outside>0
    # The actual updated routed actor, independently regenerated normal runs.
    greedy=collect(policy,seeds[:2],args.device,sample=False,deadline=time.monotonic()+180,keep_public=False)
    assert all(r['mc_qualified'] for r in greedy['outcomes'])
    with (out/'updated-trajectories.jsonl').open('w') as f:
        for row in greedy['outcomes']:f.write(json.dumps(row)+'\n')
    with (out/'pilot-trajectories.jsonl').open('w') as f:
        for row in data['outcomes']:f.write(json.dumps(row)+'\n')
    save(strategic,out/'smoke.pt',actor_filter=1,learning_scope='qualification only; routed outside',combat_checkpoint_sha256=START_SHA)
    result={'qualified':True,'pilot_runs':8,'pilot_decisions':data['decisions'],'strategic_rows':len(data['public']),
        'independent_actual_suffix_returns':True,'initial_routing_unchanged':True,
        'combat_parameters_unchanged':True,'combat_parameter_sha256':before,'updated_combat_rows':checked,
        'updated_combat_choice_rows':choices,'updated_outside_rows':outside,'gradient':stats,
        'pilot_seconds':data['seconds'],'updated_real_runs':2}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
