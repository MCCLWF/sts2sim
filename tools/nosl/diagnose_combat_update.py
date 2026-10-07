#!/usr/bin/env python3
"""Frozen first-batch loss interventions; no resampling or strength evaluation."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from check_encounter_credit import independent
from nosl import native
from nosl.branch_credit import buffers, inputs
from nosl.episodic_strategy import START_SHA, digest, pack, public_row, update as reference_update
from nosl.model import load, save

SOURCE_MANIFEST_SHA = 'e0c4989f5a9bd1a6af2ad3e9f158629f7a9678ea81864a05b5443d7c255b28f2'
NATIVE_SHA = '6296b12846d035ba598cb3beea10d2eaa649c241c468136cf8d51e67a099e231'
MODES = ('P', 'PV', 'PH', 'PVH')


def write(path, obj):path.write_text(json.dumps(obj, indent=2)+'\n')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def expired(deadline):
    if time.monotonic() >= deadline:raise TimeoutError('registered diagnostic deadline')


def reconstruct(source, seed, model, out, deadline, records):
    batches = {}
    for credit in ('C', 'R'):
        rel = f'cloud/experiment/{credit}{seed}/episodes.jsonl'
        file = source/rel
        assert sha(file) == records[rel]['sha256']
        with file.open() as stream:batches[credit] = [json.loads(next(stream)) for _ in range(32)]
        for row in batches[credit]:independent(row, credit)
    rows = batches['C']
    assert all(r['mc_qualified'] for r in rows)
    assert [{k:v for k,v in r.items() if k!='learning_returns'} for r in rows] == [
        {k:v for k,v in r.items() if k!='learning_returns'} for r in batches['R']]
    env = native.Batch(32, 256, 256, 30000);env.set_filter(1);obs = buffers(32, 256, 256)
    for slot,row in enumerate(rows):assert env.reset(slot,row['seed'],10)==native.RUNNING
    env.step(np.full(32,-1,np.int32),*obs.values())
    times = np.zeros(32,np.int32);public = [[] for _ in rows];actions = [[] for _ in rows]
    while np.any(obs['status']==native.RUNNING):
        expired(deadline);live = np.flatnonzero(obs['status']==native.RUNNING)
        choices = np.full(32,-1,np.int32)
        for slot in live:
            at=int(times[slot]);row=rows[slot];step=row['credit_steps'][at]
            choices[slot]=row['action_rows'][at]
            assert int(obs['global'][slot,11]>0)==step[0]
            assert int(obs['actions'][slot,choices[slot],0])==step[1]
            assert int(obs['global'][slot,native.G_HP])==step[2]
            if step[0] and obs['counts'][slot,1]>1:
                assert at==row['learning_times'][len(public[slot])]
                public[slot].append(public_row(obs,slot));actions[slot].append(int(choices[slot]))
        env.step(choices,*obs.values())
        for slot in live:
            hp=env.run_info(int(slot))['hp'] if obs['status'][slot]!=native.RUNNING else obs['global'][slot,native.G_HP]
            assert int(hp)==rows[slot]['credit_steps'][times[slot]][3];times[slot]+=1
    for slot,row in enumerate(rows):
        assert times[slot]==len(row['action_rows']) and int(obs['status'][slot])==row['status']
        info=env.run_info(slot);assert all(info[k]==row[k] for k in ('act','floor','hp'))
    public=sum(public,[]);actions=np.asarray(sum(actions,[]),np.int64);n=len(public)
    nt=max(int(r['counts'][0]) for r in public);na=max(int(r['counts'][1]) for r in public)
    arrays={'global':np.stack([r['global'] for r in public]),'counts':np.stack([r['counts'] for r in public]),
        'tokens':np.zeros((n,nt,native.TOKEN_WIDTH),np.int32),'actions':np.zeros((n,na,native.ACTION_WIDTH),np.int32),
        'action':actions,'value':np.zeros(n,np.float32),'logp':np.zeros(n,np.float32),
        'logits':np.full((n,na),-np.inf,np.float32)}
    for i,row in enumerate(public):
        t,a=map(int,row['counts']);arrays['tokens'][i,:t]=row['tokens'];arrays['actions'][i,:a]=row['actions']
    for credit in ('C','R'):
        arrays[credit+'_return']=np.asarray(sum((r['learning_returns'] for r in batches[credit]),[]),np.float32)
        assert len(arrays[credit+'_return'])==n
    for start in range(0,n,256):
        expired(deadline);ix=np.arange(start,min(start+256,n))
        with torch.no_grad():
            logits,value=model(*pack(public,ix,'cpu'));dist=torch.distributions.Categorical(logits=logits.float())
            arrays['logits'][ix,:logits.shape[1]]=logits.numpy()
            arrays['value'][ix]=value.numpy();arrays['logp'][ix]=dist.log_prob(torch.from_numpy(actions[ix])).numpy()
    cache=out/f'public-{seed}.npz';np.savez_compressed(cache,**arrays)
    meta={'seed':seed,'runs':32,'decisions':int(times.sum()),'combat_rows':n,'cache_sha256':sha(cache),
        'matched_C_R_histories':True,'raw_credit_formulas_verified':True,'native_replay_verified':True,
        'public_int32':all(arrays[k].dtype==np.int32 for k in ('global','tokens','actions','counts')),
        'floating_point_scope':'CPU float32 recomputation on frozen actions; not bitwise original CUDA logp/value'}
    write(out/f'public-{seed}.json',meta);print(json.dumps({'reconstructed':meta}),flush=True)


def dataset(cache, credit):
    with np.load(cache,allow_pickle=False) as z:arr={k:z[k] for k in z.files}
    rows=[]
    for i,(t,a) in enumerate(arr['counts']):
        rows.append({'global':arr['global'][i], 'tokens':arr['tokens'][i,:t],
            'actions':arr['actions'][i,:a], 'counts':arr['counts'][i]})
    return {'public':rows,'action':arr['action'],'logp':arr['logp'],'value':arr['value'],
        'return':arr[credit+'_return'],'logits':arr['logits']}


def losses(model,data,ix,advantage):
    logits,values=model(*pack(data['public'],ix,'cpu'));dist=torch.distributions.Categorical(logits=logits.float())
    chosen=torch.as_tensor(data['action'][ix]);old=torch.as_tensor(data['logp'][ix]);new=dist.log_prob(chosen)
    ratio=torch.exp(new-old);adv=torch.as_tensor(advantage[ix])
    policy=-torch.minimum(ratio*adv,ratio.clamp(.8,1.2)*adv).mean()
    value=.5*(values.float()-torch.as_tensor(data['return'][ix])).square().mean()
    entropy=dist.entropy().mean()
    return policy,value,entropy,old,new


def advantage(data):
    a=data['return']-data['value'];return (a-a.mean())/(a.std()+1e-8)


def gradient_stats(model,data,deadline):
    params=list(model.named_parameters());n=len(data['public']);a=advantage(data)
    length=sum(p.numel() for _,p in params);grad={k:torch.zeros(length,dtype=torch.float64) for k in ('P','V','H')}
    groups={k:[] for k in ('shared','policy_head','value_head')};offset=0
    for name,p in params:
        group='value_head' if name.startswith('value.') else 'policy_head' if name.startswith(('policy.','action_')) else 'shared'
        groups[group].append((offset,offset+p.numel()));offset+=p.numel()
    for start in range(0,n,256):
        expired(deadline);ix=np.arange(start,min(start+256,n));pl,vl,ent,_,_=losses(model,data,ix,a)
        for j,(key,loss) in enumerate((('P',pl),('V',.5*vl),('H',-.05*ent))):
            gradients=torch.autograd.grad(loss,[p for _,p in params],retain_graph=j<2,allow_unused=True)
            offset=0
            for (_,p),g in zip(params,gradients):
                if g is not None:grad[key][offset:offset+p.numel()]+=g.detach().reshape(-1).double()*(len(ix)/n)
                offset+=p.numel()
    assert all(p.grad is None for _,p in params) and all(torch.isfinite(g).all() for g in grad.values())
    norms={};cosines={}
    for group,parts in {'all':[(0,length)],**groups}.items():
        selected={k:torch.cat([v[b:e] for b,e in parts]) for k,v in grad.items()}
        norms[group]={k:float(v.norm()) for k,v in selected.items()}
        norms[group]['PVH']=float(sum(selected.values()).norm())
        cosines[group]={left+right:float(torch.dot(selected[left],selected[right])/
            (selected[left].norm()*selected[right].norm()).clamp(min=1e-30)) for left,right in (('P','V'),('P','H'),('V','H'))}
    return {'rows':n,'gradient_accumulation':'sample-weighted float64 accumulation of float32 gradients',
        'norms':norms,'cosines':cosines}


def measure(model,data,deadline):
    n=len(data['public']);results={k:[] for k in ('kl','entropy','argmax_changed','clipped','logp_delta','value_squared_error')}
    for start in range(0,n,256):
        expired(deadline);ix=np.arange(start,min(start+256,n));batch=pack(data['public'],ix,'cpu')
        with torch.no_grad():
            logits,values=model(*batch);old_logits=torch.from_numpy(data['logits'][ix,:logits.shape[1]])
            mask=torch.arange(logits.shape[1])[None,:]<batch[3][:,1:2]
            assert torch.isfinite(logits[mask]).all() and torch.isneginf(logits[~mask]).all()
            old_log=old_logits.log_softmax(-1);new_log=logits.log_softmax(-1)
            kl=(old_log.exp()*torch.where(mask,old_log-new_log,torch.zeros_like(old_log))).sum(-1)
            dist=torch.distributions.Categorical(logits=logits.float())
            delta=dist.log_prob(torch.from_numpy(data['action'][ix]))-torch.from_numpy(data['logp'][ix])
            values_={'kl':kl,'entropy':dist.entropy(),'argmax_changed':logits.argmax(-1)!=old_logits.argmax(-1),
                'clipped':(delta.exp()<.8)|(delta.exp()>1.2),'logp_delta':delta,
                'value_squared_error':(values-torch.from_numpy(data['return'][ix])).square()}
        for key,value in values_.items():results[key].extend(value.float().numpy().tolist())
    assert all(np.isfinite(v).all() for v in results.values())
    return {'rows':n,'exact_kl_mean':float(np.mean(results['kl'])),'exact_kl_p95':float(np.quantile(results['kl'],.95)),
        'exact_kl_max':float(np.max(results['kl'])),'entropy_mean':float(np.mean(results['entropy'])),
        'argmax_changed_fraction':float(np.mean(results['argmax_changed'])),
        'clip_fraction':float(np.mean(results['clipped'])),'sampled_action_logp_delta_mean':float(np.mean(results['logp_delta'])),
        'value_rmse':float(np.sqrt(np.mean(results['value_squared_error'])))}


def intervene(model,data,seed,mode,deadline):
    optimizer=torch.optim.Adam(model.parameters(),lr=.000133);rng=np.random.default_rng(seed);a=advantage(data);steps=0
    for _ in range(2):
        order=rng.permutation(len(data['public']))
        for start in range(0,len(order),256):
            expired(deadline);ix=order[start:start+256];pl,vl,ent,_,_=losses(model,data,ix,a)
            loss=pl
            if 'V' in mode:loss=loss+.5*vl
            if 'H' in mode:loss=loss-.05*ent
            assert torch.isfinite(loss);optimizer.zero_grad(set_to_none=True);loss.backward()
            norm=torch.nn.utils.clip_grad_norm_(model.parameters(),.5);assert torch.isfinite(norm)
            optimizer.step();steps+=1
    assert all(torch.isfinite(p).all() for p in model.parameters())
    return steps


def worker(args):
    out=Path(args.out);out.mkdir(parents=True,exist_ok=False);deadline=time.monotonic()+240
    seed=int(args.worker[1]);credit=args.worker[0];data=dataset(Path(args.cache),credit)
    initial,_=load(args.checkpoint,'cpu');initial.eval();initial_hash=digest(initial)
    baseline=measure(initial,data,deadline)
    assert abs(baseline['exact_kl_mean'])<1e-7 and baseline['argmax_changed_fraction']==0
    write(out/'initial.json',baseline)
    gradients=gradient_stats(initial,data,deadline);write(out/'gradients.json',gradients)
    results={}
    for mode in MODES:
        expired(deadline);model=copy.deepcopy(initial);began=time.monotonic()
        steps=intervene(model,data,seed,mode,deadline);metrics=measure(model,data,deadline)
        result={'mode':mode,'optimizer_steps':steps,'parameter_sha256':digest(model),
            'metrics':metrics,'seconds':time.monotonic()-began};results[mode]=result
        assert result['parameter_sha256']!=initial_hash
        save(model,out/(mode+'.pt'),actor_filter=1,outside_checkpoint_sha256=START_SHA,
            learning_scope='diagnostic first update only; combat with frozen outside X1; no strength qualification')
        write(out/(mode+'.json'),result);print(json.dumps(result),flush=True)
    expired(deadline);ref=copy.deepcopy(initial);optimizer=torch.optim.Adam(ref.parameters(),lr=.000133)
    stats=reference_update(ref,optimizer,data,'cpu',np.random.default_rng(seed))
    reference={'parameter_sha256':digest(ref),'stats':stats,'matched_PVH':digest(ref)==results['PVH']['parameter_sha256']}
    write(out/'reference.json',reference);assert reference['matched_PVH']
    result={'status':'complete','label':args.worker,'initial':baseline,'gradient':gradients,
        'modes':results,'unmodified_reference':reference};write(out/'results.json',result)
    return 0


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--out',required=True)
    parser.add_argument('--checkpoint',default=str(ROOT/'artifacts/nosl-q4r/start.pt'))
    parser.add_argument('--source',default=str(ROOT/'artifacts/nosl-q4z'))
    parser.add_argument('--worker',choices=('C1','R1','C3','R3'));parser.add_argument('--cache')
    args=parser.parse_args();torch.set_num_threads(1)
    assert sha(Path(args.checkpoint))==START_SHA
    if args.worker:return worker(args)
    out=Path(args.out);out.mkdir(parents=True,exist_ok=False);began=time.monotonic();deadline=began+1200
    source=Path(args.source);assert sha(source/'manifest.json')==SOURCE_MANIFEST_SHA
    assert sha(ROOT/'build/libsts2nosl_c.so')==NATIVE_SHA
    manifest=json.loads((source/'manifest.json').read_text());records={r['path']:r for r in manifest['files']}
    write(out/'config.json',{'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
        'baseline_checkpoint_sha256':START_SHA,'native_sha256':NATIVE_SHA,'frozen_source_manifest_sha256':SOURCE_MANIFEST_SHA,
        'seeds':[1,3],'credits':['C','R'],'modes':list(MODES),'episodes_per_seed':32,
        'seconds_limit':1200,'queue_seconds':240,'device':'cpu','precision':'float32',
        'lr':.000133,'epochs':2,'minibatch':256,'clip':.2,'value_coef':.5,'entropy':.05,'grad_norm':.5})
    model,_=load(args.checkpoint,'cpu');model.eval()
    for seed in (1,3):reconstruct(source,seed,model,out,deadline,records)
    queues={}
    for label in ('C1','R1','C3','R3'):
        if time.monotonic()>=deadline:
            queues[label]={'status':'not_started'};continue
        print(json.dumps({'worker_start':label}),flush=True)
        with (out/(label+'.log')).open('w') as log:
            try:
                result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--worker',label,
                    '--checkpoint',args.checkpoint,'--cache',str(out/f'public-{label[1]}.npz'),'--out',str(out/label)],
                    stdout=log,stderr=subprocess.STDOUT,timeout=min(240,deadline-time.monotonic()))
                file=out/label/'results.json'
                queues[label]=json.loads(file.read_text()) if result.returncode==0 and file.exists() else {
                    'status':'failed','returncode':result.returncode}
            except subprocess.TimeoutExpired:queues[label]={'status':'timeout'}
        write(out/'partial.json',queues);print(json.dumps({'worker_end':label,'status':queues[label]['status']}),flush=True)
    complete=all(r['status']=='complete' for r in queues.values());signals={}
    if complete:
        for component,pairs in {'V':(('P','PV'),('PH','PVH')),'H':(('P','PH'),('PV','PVH'))}.items():
            details={}
            for credit in ('C','R'):
                effects=[]
                for seed in (1,3):
                    for left,right in pairs:
                        modes=queues[f'{credit}{seed}']['modes'];low=modes[left]['metrics']['exact_kl_mean'];high=modes[right]['metrics']['exact_kl_mean']
                        effects.append({'label':f'{credit}{seed}','pair':right+'-'+left,'kl_delta':high-low,
                            'kl_ratio':high/max(low,1e-12),'signal':bool(high-low>=.005 and high>=1.5*low)})
                details[credit]={'replicated_signal':all(e['signal'] for e in effects),'effects':effects}
            signals[component]={'credits':details,'common_signal':all(r['replicated_signal'] for r in details.values())}
    final={'complete':complete,'queues':queues,'signals':signals,'wall_seconds':time.monotonic()-began,
        'scope':'frozen first-update CPU loss interventions; no whole-run strength evaluation or promotion'}
    write(out/'results.json',final);print(json.dumps({'complete':complete,'signals':signals}),flush=True)
    return 0 if complete else 1


if __name__=='__main__':sys.exit(main())
