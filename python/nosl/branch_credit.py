"""Offline whole-run counterfactual labels for public strategic decisions.

collect forks all legal root actions and continues a frozen NoSL policy to
real terminals. train learns their return ordering with policy KL anchors.
Privileged Env copies stay in the collector; only public tables reach the model.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
import torch.nn.functional as F

from . import native
from .model import load, save
from .vec import check_split

KEYS = ('global', 'tokens', 'actions', 'counts')
G_IN_COMBAT, G_ROOM_TYPE, G_PROMPT = 11, 26, 32


def bucket(text):
    h = 2166136261
    for byte in text.encode():
        h = ((h ^ byte) * 16777619) & 0xffffffff
    return 256 + h % 1024


def buffers(n, tokens=192, actions=160):
    return {'global': np.zeros((n, native.GLOBAL_WIDTH), np.int32),
            'tokens': np.zeros((n, tokens, native.TOKEN_WIDTH), np.int32),
            'actions': np.zeros((n, actions, native.ACTION_WIDTH), np.int32),
            'counts': np.zeros((n, 2), np.int32), 'status': np.full(n, native.ERROR, np.int32),
            'steps': np.zeros(n, np.int32)}


def inputs(data, rows, device):
    counts = data['counts'][rows]
    t, a = max(1, int(counts[:, 0].max())), max(1, int(counts[:, 1].max()))
    values = (data['global'][rows], data['tokens'][rows, :t], data['actions'][rows, :a], counts)
    return [torch.from_numpy(np.ascontiguousarray(x)).to(device) for x in values]


def choose(model, data, rows, device):
    with torch.no_grad():
        logits, _ = model(*inputs(data, rows, device))
    if torch.isinf(logits).all(dim=1).any():
        raise RuntimeError('running root has no finite policy action')
    return logits.argmax(dim=1).cpu().numpy().astype(np.int32)


def stack(observations):
    return {key: np.stack([observation[key] for observation in observations]) for key in KEYS}


def public_row(data, i):
    return {key: data[key][i].copy() for key in KEYS}


def held(seed):
    return int.from_bytes(hashlib.sha256(seed.encode()).digest()[:8], 'little') % 10 == 0


def eligible(data, i, index, maximum):
    g = data['global'][i]
    # Four fixed strata, chosen before a trajectory is played. No root cherry picking.
    rest, minimum_floor = index % 2 == 1, (4 if index % 4 < 2 else 10)
    if g[G_IN_COMBAT] or g[native.G_TOTAL_FLOOR] < minimum_floor:
        return False
    if not 2 <= data['counts'][i, 1] <= maximum:
        return False
    return bool(g[native.G_DECISION] == 4 if rest else
                g[native.G_DECISION] == 7 and g[G_PROMPT] == bucket('CardReward'))


def collect(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'config.json').exists():
        raise ValueError('use a fresh collection directory')
    check_split('train', args.seed_prefix + '000000')
    model, blob = load(args.checkpoint, args.device)
    model.eval()
    model_sha = hashlib.sha256(Path(args.checkpoint).read_bytes()).hexdigest()
    config = vars(args) | {'checkpoint_sha256': model_sha, 'actor_filter': int(blob.get('actor_filter', 0)),
                           'schema': native.SCHEMA_VERSION,
                           'score': 'future floor delta + 50*victory + 15*future boss damage share'}
    (out / 'config.json').write_text(json.dumps(config, indent=2) + '\n')
    began, deadline = time.monotonic(), time.monotonic() + args.time_limit_s
    summary = collections.Counter()
    gain_floors, gains, roots_seen, decisions = [], [], 0, 0
    roots_log = (out / 'roots.jsonl').open('w')
    attempts_log = (out / 'attempts.jsonl').open('w')
    traces_log = (out / 'trajectories.jsonl').open('w')
    for begin in range(args.first, args.first + args.attempts, args.block):
        if roots_seen >= args.roots or time.monotonic() >= deadline:
            break
        n = min(args.block, args.first + args.attempts - begin, args.roots - roots_seen)
        total = n * (1 + args.max_branches)
        batch = native.Batch(total, 192, 160, args.max_steps)
        batch.set_filter(config['actor_filter'])
        batch.set_active(n)
        data = buffers(total)
        seeds = [f'{args.seed_prefix}{begin+i:06d}' for i in range(n)]
        histories = [[] for _ in range(n)]
        roots, anchors = {}, {}
        for i, seed in enumerate(seeds):
            batch.reset(i, seed, 10)
        batch.step(np.full(total, -1, np.int32), *data.values())
        waiting = set(range(n))
        while waiting and time.monotonic() < deadline:
            running = np.array([i for i in sorted(waiting) if data['status'][i] == native.RUNNING], dtype=int)
            for i in sorted(waiting - set(running)):
                row = {'seed': seeds[i], 'status': native.STATUS_NAMES[int(data['status'][i])],
                       'root': False, **batch.run_info(i)}
                if data['status'][i] not in (native.VICTORY, native.DEATH):
                    row['error'] = batch.last_error(i)
                attempts_log.write(json.dumps(row) + '\n');summary['no_root_'+row['status']] += 1
                waiting.remove(i)
            if not len(running):
                break
            choices = np.full(total, -1, np.int32)
            selected = choose(model, data, running, args.device)
            for i, action in zip(running, selected):
                if data['global'][i, G_IN_COMBAT] and i not in anchors:
                    anchors[i] = public_row(data, i)
                if eligible(data, i, begin+i, args.max_branches):
                    roots[i] = {'data': public_row(data, i), 'baseline': int(action), 'seed': seeds[i],
                                'prefix': list(histories[i]), 'floor': int(data['global'][i, native.G_TOTAL_FLOOR]),
                                'index': int(begin+i), 'held': held(seeds[i])}
                    waiting.remove(i)
                else:
                    choices[i] = action;histories[i].append(int(action));decisions += 1
            batch.step(choices, *data.values())
        for i in sorted(waiting):
            attempts_log.write(json.dumps({'seed': seeds[i], 'root': False, 'status': 'collection_time_limit'})+'\n')
            summary['no_root_collection_time_limit'] += 1
        branch_map, first_choices, branch_history = {}, np.full(total, -1, np.int32), {}
        batch.set_active(total)
        for i, root in roots.items():
            for action in range(int(root['data']['counts'][1])):
                slot = n + i * args.max_branches + action
                status = batch.fork(i, slot)
                branch_map[slot] = (i, action)
                if status != native.RUNNING:
                    continue
                first_choices[slot] = action;decisions += 1
                if roots_seen + list(roots).index(i) < args.trace_roots:
                    branch_history[slot] = root['prefix'] + [action]
        batch.step(first_choices, *data.values())
        boss_ref, boss_paid, boss_sum = np.zeros(total), np.zeros(total), np.zeros(total)

        def boss_state():
            position = np.arange(data['tokens'].shape[1])[None, :]
            enemy = (data['tokens'][:, :, 0] == 6) & (data['tokens'][:, :, 15] > 0) & (position < data['counts'][:, :1])
            hp = (data['tokens'][:, :, 8] * enemy).sum(axis=1)
            max_hp = (data['tokens'][:, :, 9] * enemy).sum(axis=1)
            inside = (data['global'][:, G_IN_COMBAT] > 0) & (data['global'][:, G_ROOM_TYPE] == 3)
            return inside, hp, max_hp

        running = np.array([i for i in branch_map if data['status'][i] == native.RUNNING], dtype=int)
        while len(running) and time.monotonic() < deadline:
            inside, hp_before, max_hp = boss_state()
            opened = inside & (boss_ref == 0)
            boss_ref[opened] = np.maximum(np.maximum(hp_before, max_hp), 1)[opened]
            boss_paid[opened] = 0
            choices = np.full(total, -1, np.int32)
            choices[running] = choose(model, data, running, args.device)
            for slot in running:
                if slot in branch_history:
                    branch_history[slot].append(int(choices[slot]))
            decisions += len(running)
            batch.step(choices, *data.values())
            now, hp_now, _ = boss_state()
            terminal = data['status'] != native.RUNNING
            fought = boss_ref > 0
            victory = fought & ((~now & ~terminal) | (data['status'] == native.VICTORY))
            share = np.where(victory, 1-boss_paid,
                             np.minimum(np.maximum(hp_before-hp_now, 0) / np.maximum(boss_ref, 1), 1-boss_paid))
            # Terminal buffers have no enemy rows. Their zero HP is not damage:
            # deaths retain only previously observed credit; cleared boss wins
            # have an explicit successful boundary and receive the remainder.
            share = np.where(fought & ((data['status'] == native.RUNNING) | victory), share, 0)
            boss_paid += share;boss_sum += share
            boss_ref[victory | terminal] = 0
            running = np.array([i for i in branch_map if data['status'][i] == native.RUNNING], dtype=int)
        observations, scores, split, anchors_kept, anchor_split = [], [], [], [], []
        for i, root in roots.items():
            outcomes = []
            for action in range(int(root['data']['counts'][1])):
                slot = n+i*args.max_branches+action
                status = int(data['status'][slot])
                facts = batch.run_info(slot)
                row = {'action': action, 'status': ('collection_time_limit' if status == native.RUNNING else native.STATUS_NAMES[status]),
                       **facts, 'boss_share': float(boss_sum[slot])}
                if status in (native.VICTORY, native.DEATH):
                    row['return'] = facts['floor']-root['floor']+50*(status == native.VICTORY)+15*boss_sum[slot]
                else:
                    row['error'] = batch.last_error(slot)
                outcomes.append(row);summary['branch_'+row['status']] += 1
            complete = all('return' in row for row in outcomes)
            meta = {k:v for k,v in root.items() if k not in ('data','prefix')}
            meta.update(complete=complete, outcomes=outcomes)
            roots_log.write(json.dumps(meta)+'\n');roots_log.flush()
            attempts_log.write(json.dumps({'seed': root['seed'], 'root': True, 'complete': complete})+'\n')
            roots_seen += 1;summary['roots'] += 1
            if complete:
                summary['complete_roots'] += 1
                q = np.full(160, np.nan, np.float32)
                q[:len(outcomes)] = [r['return'] for r in outcomes]
                observations.append(root['data']);scores.append(q);split.append(root['held'])
                baseline = outcomes[root['baseline']]
                best_floor = max(r['floor']+50*(r['status']=='victory') for r in outcomes)
                gain = best_floor-baseline['floor']-50*(baseline['status']=='victory')
                gain_floors.append(gain);gains.append(float(np.nanmax(q)-q[root['baseline']]))
                if gain >= .5:summary['root_floor_opportunity'] += 1
                if roots_seen <= args.trace_roots:
                    best = int(np.nanargmax(q));slot = n+i*args.max_branches+best
                    if slot in branch_history:
                        traces_log.write(json.dumps({'seed':root['seed'],'ascension':10,'actor_filter':config['actor_filter'],
                            'end':outcomes[best]['status'],'floor':outcomes[best]['floor'],'action_rows':branch_history[slot]})+'\n');traces_log.flush()
            else:
                summary['incomplete_roots'] += 1
        for i, observation in anchors.items():
            anchors_kept.append(observation);anchor_split.append(held(seeds[i]))
        if observations:
            payload = stack(observations) | {'returns':np.stack(scores),'held':np.asarray(split)}
            if anchors_kept:
                payload.update({'anchor_'+k:v for k,v in stack(anchors_kept).items()})
                payload['anchor_held'] = np.asarray(anchor_split)
            np.savez_compressed(out/f'block-{begin:06d}.npz', **payload)
        attempts_log.flush()
        stats = dict(summary) | {'wall_s':round(time.monotonic()-began,2),'policy_decisions':int(decisions),
            'mean_oracle_floor_gain':float(np.mean(gain_floors)) if gain_floors else None,
            'mean_oracle_return_gain':float(np.mean(gains)) if gains else None,
            'opportunity_fraction':summary['root_floor_opportunity']/max(summary['complete_roots'],1)}
        (out/'summary.json').write_text(json.dumps(stats,indent=2)+'\n')
        print(json.dumps(stats),flush=True)
        del batch
    for log in (roots_log, attempts_log, traces_log):log.close()
    if not summary['complete_roots']:
        raise RuntimeError('no complete root labels; see attempts/roots logs')


def load_data(directory):
    shards = [dict(np.load(p)) for p in sorted(Path(directory).rglob('block-*.npz'))]
    if not shards:raise ValueError('no branch-credit shards')
    keys = (*KEYS,'returns','held')
    data = {k:np.concatenate([s[k] for s in shards]) for k in keys}
    anchors = {k:np.concatenate([s['anchor_'+k] for s in shards if 'anchor_'+k in s]) for k in (*KEYS,'held')}
    return data,anchors


def kl(logits, baseline):
    logp = logits.log_softmax(-1)
    old = baseline.log_softmax(-1)
    mask = torch.isfinite(old)
    return torch.where(mask, old.exp()*(torch.where(mask,old,0)-torch.where(mask,logp,0)),0).sum(-1).mean()


def order_loss(logits, returns):
    # Equal outcomes are unknown preferences, not artificial labels.
    difference = returns.unsqueeze(2)-returns.unsqueeze(1)
    weight = torch.where(torch.isfinite(difference)&(difference>.5),difference.clamp(0,5),0)
    logit = torch.where(torch.isfinite(logits),logits,0)
    loss = -F.logsigmoid(logit.unsqueeze(2)-logit.unsqueeze(1))
    per_root = (loss*weight).sum((1,2))/weight.sum((1,2)).clamp(min=1)
    used = weight.sum((1,2))>0
    return per_root.sum()/used.sum().clamp(min=1),used.sum()


def train(args):
    out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
    if (out/'config.json').exists():raise ValueError('use a fresh training directory')
    data,anchors=load_data(args.data)
    model,blob=load(args.checkpoint,args.device);baseline,_=load(args.checkpoint,args.device)
    model.train();baseline.eval()
    for p in baseline.parameters():p.requires_grad_(False)
    for p in model.value.parameters():p.requires_grad_(False)
    if model.value_net is not None:
        for p in model.value_net.parameters():p.requires_grad_(False)
    rng=np.random.default_rng(args.seed);torch.manual_seed(args.seed)
    optimizer=torch.optim.Adam([p for p in model.parameters() if p.requires_grad],lr=args.lr)
    rows=np.flatnonzero(~data['held']);valid=np.flatnonzero(data['held']);anchor_rows=np.flatnonzero(~anchors['held'])
    if not len(rows) or not len(valid) or not len(anchor_rows):raise ValueError('need independent train/held roots and combat anchors')
    config=vars(args)|{'train_roots':len(rows),'held_roots':len(valid),'combat_anchors':len(anchor_rows),
                      'checkpoint_sha256':hashlib.sha256(Path(args.checkpoint).read_bytes()).hexdigest()}
    (out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
    began=time.monotonic()
    for epoch in range(args.epochs):
        losses,root_kls,anchor_kls=[],[],[]
        for offset in range(0,len(rows),args.minibatch):
            if time.monotonic()-began>args.time_limit_s:raise RuntimeError('supervised time budget reached')
            if offset==0:order=rng.permutation(rows)
            subset=order[offset:offset+args.minibatch]
            batch=inputs(data,subset,args.device)
            logits,_=model(*batch)
            with torch.no_grad():old,_=baseline(*batch)
            q=torch.from_numpy(data['returns'][subset,:logits.shape[1]]).to(args.device)
            loss,_=order_loss(logits,q);root_kl=kl(logits,old)
            anchor=rng.choice(anchor_rows,size=min(args.minibatch,len(anchor_rows)),replace=False)
            anchor_batch=inputs(anchors,anchor,args.device)
            now,_=model(*anchor_batch)
            with torch.no_grad():before,_=baseline(*anchor_batch)
            anchor_kl=kl(now,before)
            objective=loss+args.kl_coef*(root_kl+anchor_kl)
            optimizer.zero_grad(set_to_none=True);objective.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(),.5);optimizer.step()
            losses.append(float(loss.detach()));root_kls.append(float(root_kl.detach()));anchor_kls.append(float(anchor_kl.detach()))
        matches,comparisons=0,0
        with torch.no_grad():
            for offset in range(0,len(valid),args.minibatch):
                subset=valid[offset:offset+args.minibatch];logits,_=model(*inputs(data,subset,args.device))
                q=torch.from_numpy(data['returns'][subset,:logits.shape[1]]).to(args.device)
                gap=q.unsqueeze(2)-q.unsqueeze(1);use=torch.isfinite(gap)&(gap>.5)
                delta=logits.unsqueeze(2)-logits.unsqueeze(1)
                matches+=int(((delta>0)&use).sum());comparisons+=int(use.sum())
        metrics={'epoch':epoch+1,'wall_s':round(time.monotonic()-began,2),'order_loss':float(np.mean(losses)),
                 'root_kl':float(np.mean(root_kls)),'combat_anchor_kl':float(np.mean(anchor_kls)),
                 'held_pairs':comparisons,'held_order_accuracy':matches/comparisons if comparisons else None}
        print(json.dumps(metrics),flush=True)
        extra={k:blob[k] for k in ('iteration','decisions','actor_filter','reward') if k in blob}
        save(model,out/'checkpoint.pt',**extra,branch_credit=config,branch_epoch=epoch+1)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='mode',required=True)
    c=sub.add_parser('collect');t=sub.add_parser('train')
    for parser in (c,t):
        parser.add_argument('--checkpoint',required=True);parser.add_argument('--out',required=True)
        parser.add_argument('--device',default='cuda' if torch.cuda.is_available() else 'cpu')
        parser.add_argument('--time-limit-s',type=float,default=900)
    c.add_argument('--roots',type=int,default=128);c.add_argument('--attempts',type=int,default=256)
    c.add_argument('--first',type=int,default=0);c.add_argument('--block',type=int,default=32)
    c.add_argument('--seed-prefix',default='NQ4M');c.add_argument('--max-branches',type=int,default=8)
    c.add_argument('--max-steps',type=int,default=30000);c.add_argument('--trace-roots',type=int,default=10)
    t.add_argument('--data',required=True);t.add_argument('--epochs',type=int,default=8)
    t.add_argument('--lr',type=float,default=1e-5);t.add_argument('--minibatch',type=int,default=128)
    t.add_argument('--kl-coef',type=float,default=.5);t.add_argument('--seed',type=int,default=2)
    args=p.parse_args()
    if args.time_limit_s<=0:raise ValueError('positive time budget required')
    if args.mode=='collect' and not (args.roots>0 and args.attempts>0 and args.first>=0 and args.block>0
                                    and 2<=args.max_branches<=160 and args.max_steps>0 and args.trace_roots>=0):
        raise ValueError('positive collection counts, nonnegative first/trace counts, and 2..160 branches required')
    if args.mode=='train' and not (args.epochs>0 and args.minibatch>0 and args.lr>0 and args.kl_coef>=0):
        raise ValueError('positive epochs/minibatch/lr and nonnegative KL coefficient required')
    torch.set_num_threads(1)
    torch.backends.cuda.matmul.allow_tf32=True
    collect(args) if args.mode=='collect' else train(args)


if __name__=='__main__':main()
