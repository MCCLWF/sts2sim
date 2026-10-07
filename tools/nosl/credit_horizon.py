#!/usr/bin/env python3
"""Measure whole-run credit on actual sampled X1 Silent A10 trajectories.

Full successors are offline measurement data only; actions use the ordinary
public ActorCritic. Also compares reward/GAE with the frozen pre-refactor PPO.
"""
import argparse
import collections
import hashlib
import json
import subprocess
import textwrap
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import torch

from nosl import native
from nosl.model import load
from nosl.returns import FLOOR_SCALE, G_IN_COMBAT, RunRewards, gae, reward_state, store_observation
from nosl.vec import STATUS_NAMES, VecSampler

REFERENCE = '4f9529ecd9202f110673e36ca41af94993a2083f'


def legacy_functions():
    """Execute the unchanged mathematical blocks of the fixed prior source."""
    source = subprocess.check_output(['git', 'show', REFERENCE + ':python/nosl/ppo.py'], text=True)
    opening = source[source.index('                inside_before, hp_before, max_hp_before = boss_state()'):
                     source.index('                batch = observe(t)')]
    reward = source[source.index('                floor_now = A["global"]'):
                    source.index('            _, values = policy(observe())')]
    function = '''def legacy_reward(self, before, after, results, seeds_before):
    N, t = len(seeds_before), 0
    reward = np.zeros((1,N),np.float32)
    done = np.zeros_like(reward)
    valid = np.ones_like(reward,dtype=bool)
    episode_start = np.zeros(N,np.int64)
    finished, recent, boss_fights = [], [], []
    failures = collections.Counter()
    args = SimpleNamespace(gamma=self.gamma,boss_damage=self.boss_damage)
    boss_ref,boss_paid = self.boss_ref,self.boss_paid
    floor_before,phi_before = before.floor,before.potential
    A = {"global":np.zeros((N,native.GLOBAL_WIDTH),np.int32)}
    A["global"][:,native.G_TOTAL_FLOOR] = after.floor
    potential = lambda: after.potential
    states = iter((before,after))
    def boss_state():
        s=next(states)
        return s.inside_boss,s.enemy_hp,s.enemy_max_hp
'''
    function += textwrap.indent(textwrap.dedent(opening + reward), '    ')
    function += '    return reward[0],boss_fights\n'
    advantage = source[source.index('            advantage = np.zeros((L, N), np.float32)'):
                       source.index('            returns = advantage + value[:L]')]
    old_gae = 'def legacy_gae(reward,value,done,gamma,lam):\n    L,N=reward.shape\n    args=SimpleNamespace(gamma=gamma,lam=lam)\n'
    old_gae += textwrap.indent(textwrap.dedent(advantage), '    ') + '    return advantage\n'
    scope = dict(np=np, native=native, collections=collections, SimpleNamespace=SimpleNamespace,
                 FLOOR_SCALE=FLOOR_SCALE, STATUS_NAMES=STATUS_NAMES)
    exec(function + old_gae, scope)
    return scope['legacy_reward'], scope['legacy_gae'], hashlib.sha256(source.encode()).hexdigest()


def math_checks(old_gae):
    rng = np.random.default_rng(41006)
    cases = 0
    for length in (8,64,128):
        reward = rng.normal(0,.03,(length,7)).astype(np.float32)
        value = rng.normal(.2,.1,(length+1,7)).astype(np.float32)
        done = (rng.random((length,7))<.08).astype(np.float32)
        for gamma in (1.,.99):
            for lam in (0.,.95,.995,1.):
                actual=gae(reward,value,done,gamma,lam)
                if not np.array_equal(actual,old_gae(reward,value,done,gamma,lam)):
                    raise AssertionError('iterative GAE differs from fixed prior source')
                # Independent finite weighted residual sum, stopping at a
                # real terminal, tests the bootstrap and boundary semantics.
                expected=np.zeros_like(actual,dtype=np.float64)
                for t in range(length):
                    for slot in range(7):
                        for j in range(t,length):
                            expected[t,slot]+=(gamma*lam)**(j-t)*(float(reward[j,slot])+gamma*float(value[j+1,slot])*(1-float(done[j,slot]))-float(value[j,slot]))
                            if done[j,slot]:break
                np.testing.assert_allclose(actual,expected,rtol=1e-5,atol=2e-6)
                cases+=1
    original=np.array([[np.iinfo(np.int32).min,-32769,32768,np.iinfo(np.int32).max]],np.int32)
    target=np.ones((1,8),np.int32);store_observation(target,original)
    assert np.array_equal(target[:,:4],original) and not target[:,4:].any()
    return {'gae_cases':cases,'old_iterative_gae_exact':True,'independent_residual_sum':True,'int32_extremes_preserved':True}


def analyze(episodes):
    groups=collections.defaultdict(list)
    spans=[]
    terminal_count=0
    for episode in episodes:
        if episode['status'] not in (native.DEATH,native.VICTORY):continue
        terminal_count+=1
        rows=episode['rows'];n=len(rows)
        r=np.array([x['reward'] for x in rows],np.float32)
        v=np.array([x['value'] for x in rows]+[0.],np.float32)
        done=np.zeros((n,1),np.float32);done[-1]=1
        mc=np.cumsum(r[::-1],dtype=np.float64)[::-1]
        estimates={}
        for label,length,lam in (('short',64,.95),('short128',128,.95),('long128',128,.995),('long256',256,.995)):
            a=np.zeros(n,np.float32);begin=0
            while begin<n:
                end=min(n,begin+length-(rows[begin]['tick']%length))
                a[begin:end]=gae(r[begin:end,None],v[begin:end+1,None],done[begin:end],1.,lam)[:,0]
                begin=end
            estimates[label]=a
        full=gae(r[:,None],v[:,None],done,1.,1.)[:,0]
        np.testing.assert_allclose(full,mc-v[:-1],atol=3e-6,rtol=1e-5)
        future_end=None
        for t in reversed(range(n)):
            row=rows[t]
            if row['combat_end']:future_end=t
            outside=not row['in_combat'] and row['actions']>1
            if outside and future_end is not None:spans.append(future_end-t+1)
            record={'value':v[t],'return':mc[t],'mc_adv':mc[t]-v[t],**{key:float(x[t]) for key,x in estimates.items()}}
            groups['all'].append(record);groups['outside' if outside else 'combat'].append(record)
            groups['kind_'+str(row['kind'])].append(record)
    summary={}
    for group,rows in groups.items():
        value=np.array([x['value'] for x in rows]);ret=np.array([x['return'] for x in rows]);mc=ret-value
        substantial=np.abs(mc)>1e-6
        item={'rows':len(rows),'critic_bias':float((value-ret).mean()),'critic_rmse':float(np.mean((value-ret)**2)**.5),
              'value_return_correlation':float(np.corrcoef(value,ret)[0,1]) if np.std(value)>1e-8 and np.std(ret)>1e-8 else None}
        for key in ('short','short128','long128','long256'):
            a=np.array([x[key] for x in rows]);item[key]={'mc_sign_agreement':float(np.mean(np.sign(a[substantial])==np.sign(mc[substantial]))) if substantial.any() else None,
             'mc_adv_rmse':float(np.mean((a-mc)**2)**.5),'adv_sd':float(a.std())}
        summary[group]=item
    span={'choices_with_next_combat_end':len(spans),'median_steps':float(np.median(spans)) if spans else None,
          'fraction_over_20':float(np.mean(np.array(spans)>20)) if spans else 0.,
          'fraction_over_64':float(np.mean(np.array(spans)>64)) if spans else 0.}
    passed=terminal_count>=96 and span['fraction_over_20']>=.25 and summary.get('outside',{}).get('short',{}).get('mc_sign_agreement',1)<.75
    return {'normal_terminals':terminal_count,'spans':span,'groups':summary,'diagnostic_gate':passed,
            'interpretation':'MC is one sampled successor, not the true action advantage or a counterfactual action label'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--checkpoint',required=True);p.add_argument('--out',required=True)
    p.add_argument('--runs',type=int,default=128);p.add_argument('--seed-prefix',default='NQ4QDIAG');p.add_argument('--workers',type=int,default=1)
    p.add_argument('--runs-per-worker',type=int,default=16);p.add_argument('--policy-seed',type=int,default=2)
    p.add_argument('--device',default='cpu');p.add_argument('--time-limit-s',type=int,default=300);args=p.parse_args()
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    old_reward,old_gae,source_sha=legacy_functions();checks=math_checks(old_gae)
    torch.set_num_threads(1);device=torch.device(args.device);model,blob=load(args.checkpoint,device);model.eval();torch.manual_seed(args.policy_seed)
    seeds=[f'{args.seed_prefix}{i:06d}' for i in range(args.runs)]
    collected={};records=collections.defaultdict(list);began=time.monotonic();tick=reward_checks=table_checks=clipped_cells=0;peak_abs=0
    clipping=[]
    with VecSampler(args.workers,args.runs_per_worker,split='train',seeds=seeds,actor_filter=int(blob.get('actor_filter',0)),failure_log=out/'faults.jsonl') as vec:
        a=vec.arrays;initial=vec.reset();tracker=RunRewards(vec.runs);old_tracker=RunRewards(vec.runs)
        for x in initial:collected[x['seed']]={**x,'rows':[]}
        while len(collected)<len(seeds):
            if time.monotonic()-began>args.time_limit_s:raise TimeoutError('fixed diagnostic time limit, do not relabel unfinished runs as deaths')
            active=vec.running();used=max(1,int(a['counts'][:,0].max()));actions=max(1,int(a['counts'][:,1].max()))
            tables=(a['global'],a['tokens'][:,:used],a['actions'][:,:actions],a['counts'])
            for x in tables:
                target=np.zeros_like(x);store_observation(target,x);assert np.array_equal(target,x)
                clipped_cells+=int(((x<-32768)|(x>32767))[active].sum());peak_abs=max(peak_abs,int(np.abs(x.astype(np.int64)).max()))
                table_checks+=1
            with torch.no_grad(),torch.autocast('cuda',dtype=torch.bfloat16,enabled=device.type=='cuda'):
                logits,values=model(*(torch.from_numpy(np.ascontiguousarray(x)).to(device) for x in tables))
            logits=logits.float();logits=torch.where(torch.isinf(logits).all(1,keepdim=True),torch.zeros_like(logits),logits)
            choice=torch.distributions.Categorical(logits=logits).sample().cpu().numpy().astype(np.int32)
            choice[~active]=-1;before=reward_state(a['global'],a['tokens'],a['counts']);old_seeds=list(vec.seed_of)
            if len(clipping)<32 and any((((x<-32768)|(x>32767))[active]).any() for x in tables):
                with torch.no_grad(),torch.autocast('cuda',dtype=torch.bfloat16,enabled=device.type=='cuda'):
                    clipped_logits,_=model(*(torch.from_numpy(np.clip(x,-32768,32767).astype(np.int32)).to(device) for x in tables))
                for table,x in zip(('global','tokens','actions','counts'),tables):
                    for index in np.argwhere((x<-32768)|(x>32767)):
                        slot=int(index[0])
                        if not active[slot] or len(clipping)>=32:continue
                        count=int(a['counts'][slot,1]);now=logits[slot,:count];old=clipped_logits[slot,:count].float()
                        pick=int(choice[slot]);where=tuple(map(int,index))
                        clipping.append({'seed':old_seeds[slot],'tick':tick,'table':table,'index':list(where),
                         'value':int(x[where]),'entity':x[slot,int(index[1])].tolist() if x.ndim==3 else None,
                         'max_logit_delta':float((now-old).abs().max().cpu()),
                         'sampled_logp_delta':float((torch.log_softmax(old,0)[pick]-torch.log_softmax(now,0)[pick]).cpu()),
                         'greedy_differs':int(now.argmax())!=int(old.argmax())})
            meta=[]
            for slot in range(vec.runs):
                g=a['global'][slot]
                meta.append({'tick':tick,'kind':int(g[1]),'in_combat':bool(g[11]),'act':int(g[4]),'floor':int(g[6]),
                 'hp':int(g[7]),'max_hp':int(g[8]),'room_type':int(g[26]),'inside_boss':bool(before.inside_boss[slot]),
                 'enemy_hp':int(before.enemy_hp[slot]),'enemy_max_hp':int(before.enemy_max_hp[slot]),
                 'actions':int(a['counts'][slot,1]),'action_kind':int(a['actions'][slot,max(0,choice[slot]),0]),'value':float(values[slot].float().cpu())})
            results=vec.step(choice);after=reward_state(a['global'],a['tokens'],a['counts'])
            reward,fights=tracker.step(before,after,results,old_seeds);legacy,old_fights=old_reward(old_tracker,before,after,results,old_seeds)
            assert np.array_equal(reward,legacy) and fights==old_fights
            assert np.array_equal(tracker.boss_ref,old_tracker.boss_ref) and np.array_equal(tracker.boss_paid,old_tracker.boss_paid)
            reward_checks+=int(active.sum());ended={x['seed'] for x in results}
            for slot in np.flatnonzero(active):
                row=meta[slot];row['reward']=float(reward[slot]);row['combat_end']=row['in_combat'] and (old_seeds[slot] in ended or not a['global'][slot,11])
                records[old_seeds[slot]].append(row)
            for x in results:collected[x['seed']]={**x,'rows':records.pop(x['seed'],[])}
            tick+=1
    episodes=[collected[s] for s in seeds]
    with (out/'episodes.jsonl').open('w') as f:
        for episode in episodes:f.write(json.dumps(episode)+'\n')
    (out/'clipping-examples.json').write_text(json.dumps(clipping,indent=2)+'\n')
    result=analyze(episodes)|{'checks':checks,'attempts':len(seeds),'ends':dict(collections.Counter(STATUS_NAMES[x['status']] for x in episodes)),
     'real_reward_checks':reward_checks,'public_table_checks':table_checks,'old_int16_clipped_cells':clipped_cells,'public_integer_peak_abs':peak_abs,
     'reference_commit':REFERENCE,'reference_source_sha256':source_sha,'checkpoint_sha256':hashlib.sha256(Path(args.checkpoint).read_bytes()).hexdigest(),
     'policy_seed':args.policy_seed,'wall_seconds':time.monotonic()-began,'history_sha256':hashlib.sha256((out/'episodes.jsonl').read_bytes()).hexdigest()}
    (out/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


if __name__=='__main__':main()
