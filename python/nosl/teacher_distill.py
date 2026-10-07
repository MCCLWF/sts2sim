"""Public-observation labels and bounded KL distillation of actual student roots.

Privileged route/quality metadata is used only by the offline exporter. Model
inputs are explicitly the same four public tables as PPO and deployment.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch
import torch.nn.functional as F

from . import native
from .branch_credit import KEYS, buffers, held, inputs, kl
from .model import load, save
from .vec import check_split

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def oracle_module():
    spec = importlib.util.spec_from_file_location('teacher_l4', ROOT / 'tests/l4/l4_run.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def export_trace(directory, begin, out, filter_version, l4, *, return_continuation=False):
    trace = directory / 'oracle.jsonl'
    run = l4.parse_runs(trace.read_text().splitlines())[0]
    if run['begin'].get('oracle_test_mode') is not False or run['begin'].get('hp') is not None or run['begin'].get('deck'):
        raise ValueError('expected a normal-rule actual student trace')
    raw = '\n'.join(run['actions']) + '\n'
    result = subprocess.run([str(ROOT / 'build/nosl_teacher_export'), '--seed', run['begin']['seed'],
                             '--out', str(out), '--begin-script', str(begin), '--filter', str(filter_version)],
                            input=raw, capture_output=True, text=True, timeout=90)
    out.with_suffix('.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise ValueError('public label export failed: ' + result.stderr[:400])
    summary = json.loads(result.stdout)
    rows = [json.loads(line) for line in out.read_text().splitlines()]
    if not rows:
        raise ValueError('empty public label export')
    # Independent sampler replay: auto-forced actions and filter/history are
    # reconstructed through the same ABI used by training, with no oracle
    # helper and no imported privileged snapshot. Verify every public table.
    tokens = max(192, max(row['counts'][0] for row in rows))
    actions = max(160, max(row['counts'][1] for row in rows))
    batch = native.Batch(1, tokens, actions, 30000)
    batch.set_filter(filter_version)
    batch.reset(0, run['begin']['seed'], 10)
    data = buffers(1, tokens, actions)
    batch.step(np.array([-1], np.int32), *data.values())
    observations = []
    for index, row in enumerate(rows):
        t, a = row['counts']
        expected = {'global': np.array(row['global'], np.int32),
                    'tokens': np.array(row['tokens'], np.int32).reshape(t, native.TOKEN_WIDTH),
                    'actions': np.array(row['actions'], np.int32).reshape(a, native.ACTION_WIDTH),
                    'counts': np.array(row['counts'], np.int32)}
        actual = {'global': data['global'][0], 'tokens': data['tokens'][0, :t],
                  'actions': data['actions'][0, :a], 'counts': data['counts'][0]}
        for key in KEYS:
            if not np.array_equal(expected[key], actual[key]):
                raise ValueError(f'sampler/table mismatch row {index} {key}')
        if data['status'][0] != native.RUNNING or not 0 <= row['label'] < a:
            raise ValueError('label does not bind to a running policy action')
        observations.append(expected)
        batch.step(np.array([row['label']], np.int32), *data.values())
    info = batch.run_info(0)
    if info['hp'] != summary['hp'] or info['floor'] != summary['floor']:
        raise ValueError('sampler label replay terminal HP/floor differs')
    # Global/tokens/actions are dynamic; pad without clipping any integer.
    maximum_t, maximum_a = max(x['counts'][0] for x in observations), max(x['counts'][1] for x in observations)
    for obs in observations:
        obs['tokens'] = np.pad(obs['tokens'], ((0, maximum_t - len(obs['tokens'])), (0, 0)))
        obs['actions'] = np.pad(obs['actions'], ((0, maximum_a - len(obs['actions'])), (0, 0)))
    summary.update(sampler_rows_verified=len(rows), trace_sha256=sha(trace),
                   tables_sha256=sha(out), scope='public_label_replay',
                   end=run['end']['reason'])
    out.with_suffix('.contract.json').write_text(json.dumps(summary, indent=2) + '\n')
    if return_continuation:
        t, a = map(int, data['counts'][0])
        continuation = {'global': data['global'][0].copy(), 'tokens': data['tokens'][0, :t].copy(),
                        'actions': data['actions'][0, :a].copy(), 'counts': data['counts'][0].copy(),
                        'status': int(data['status'][0]), 'run_info': info}
        return observations, rows, summary, continuation
    return observations, rows, summary


def padded_stack(observations):
    t = max(int(x['counts'][0]) for x in observations)
    a = max(int(x['counts'][1]) for x in observations)
    return {'global': np.stack([x['global'] for x in observations]),
            'tokens': np.stack([np.pad(x['tokens'][:x['counts'][0]], ((0, t-x['counts'][0]), (0, 0))) for x in observations]),
            'actions': np.stack([np.pad(x['actions'][:x['counts'][1]], ((0, a-x['counts'][1]), (0, 0))) for x in observations]),
            'counts': np.stack([x['counts'] for x in observations])}


def export(args):
    source, out = Path(args.batch).resolve(), Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'dataset.json').exists():
        raise ValueError('use a fresh export directory')
    source_rows = source / 'audited-results.jsonl'
    if not source_rows.exists(): source_rows = source / 'results.jsonl'
    rows = [json.loads(line) for line in source_rows.read_text().splitlines()]
    labels, label_index, splits, groups, anchors, anchor_splits = [], [], [], [], [], []
    results = []
    l4 = oracle_module()
    for group, row in enumerate(rows):
        check_split('train', row['seed'])
        result = {'seed': row['seed'], 'quality_gain': row.get('terminal_hp_gain'), 'status': 'excluded'}
        if row['seed'].startswith('NQ4OEVAL'):
            raise ValueError('distillation refuses its registered evaluation seeds')
        if row['status'] == 'route_qualified':
            directory = source / row['seed']
            if not all(json.loads((directory / phase / 'env-contract.json').read_text())['qualified'] for phase in ('search', 'native')):
                raise ValueError('batch claims a route without its actual contracts')
            evidence = json.loads((directory / 'search/teacher.json').read_text())
            work = out / row['seed'];work.mkdir()
            obs, exported, contract = export_trace(directory / 'native', evidence['decision'], work / 'teacher.jsonl', args.filter, l4)
            root_rows = [i for i, r in enumerate(exported) if r['teacher']]
            result.update(status='contract_verified', public_rows=len(root_rows),
                          forced=contract['forced'], hidden_checks=contract['hidden_checks'])
            if row.get('terminal_hp_gain', 0) >= 3:
                for i in root_rows:
                    labels.append(obs[i]);label_index.append(exported[i]['label'])
                    splits.append(held(row['seed']));groups.append(group)
                result['status'] = 'teacher_included'
            # Predeclared every-16th public policy row, at most 16 per kind
            # per seed. Held roots never become training anchors.
            student, _, anchor_contract = export_trace(directory / 'search', 0, work / 'student.jsonl', args.filter, l4)
            per_kind = collections.Counter()
            for i, observation in enumerate(student):
                kind = int(observation['global'][native.G_DECISION])
                if i % 16 or per_kind[kind] >= 16: continue
                anchors.append(observation);anchor_splits.append(held(row['seed']));per_kind[kind] += 1
            result['anchor_rows'] = sum(per_kind.values())
        results.append(result)
        print(json.dumps(result), flush=True)
    if not labels or not anchors:
        raise ValueError('no qualified teacher labels or student anchors')
    data = padded_stack(labels) | {'label':np.asarray(label_index, np.int64), 'held':np.asarray(splits), 'group':np.asarray(groups, np.int32)}
    data.update({'anchor_'+k:v for k,v in padded_stack(anchors).items()})
    data['anchor_held'] = np.asarray(anchor_splits)
    np.savez_compressed(out / 'public-tables.npz', **data)
    qualified = sum(r['status'] in ('contract_verified', 'teacher_included') for r in results)
    quality_fraction = len(set(groups))/len(rows)
    manifest = {'schema':'sts2.nosl.real-root-labels.v1', 'source_batch':str(source), 'source_results_sha256':sha(source_rows),
                'checkpoint_sha256':json.loads((source/'batch.json').read_text())['checkpoint_sha256'],
                'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'exporter_sha256':sha(ROOT / 'build/nosl_teacher_export'), 'sampler_sha256':sha(ROOT / 'build/libsts2nosl_c.so'),
                'actor_filter':args.filter,'input_schema':native.SCHEMA_VERSION,'integer_type':'int32',
                'teacher_rows':len(labels),'teacher_roots':len(set(groups)),'train_roots':len(set(data['group'][~data['held']])),
                'held_roots':len(set(data['group'][data['held']])),'anchors':len(anchors),'results':results,
                'qualified_roots':qualified, 'quality_fraction_all_attempts':quality_fraction,
                'min_qualified':args.min_qualified, 'min_quality_fraction':args.min_quality_fraction,
                'export_gate_passed':qualified >= args.min_qualified and quality_fraction >= args.min_quality_fraction,
                'public_tables_sha256':sha(out / 'public-tables.npz')}
    (out / 'dataset.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({k:v for k,v in manifest.items() if k != 'results'}))
    if not manifest['export_gate_passed']:
        raise ValueError('public label collection did not pass the registered root/quality gate')


def train(args):
    out = Path(args.out);out.mkdir(parents=True, exist_ok=True)
    if (out / 'config.json').exists():raise ValueError('use a fresh training directory')
    directory = Path(args.data)
    manifest = json.loads((directory / 'dataset.json').read_text())
    if manifest.get('export_gate_passed') is not True or manifest['checkpoint_sha256'] != sha(args.checkpoint):
        raise ValueError('unqualified label dataset or different frozen student')
    if manifest['public_tables_sha256'] != sha(directory / 'public-tables.npz'):
        raise ValueError('public table SHA differs')
    with np.load(directory / 'public-tables.npz', allow_pickle=False) as blob:
        data = {k:blob[k] for k in (*KEYS, 'label', 'held', 'group')}
        anchors = {k:blob['anchor_'+k] for k in (*KEYS, 'held')}
    model, checkpoint = load(args.checkpoint, args.device)
    baseline, _ = load(args.checkpoint, args.device)
    baseline.eval()
    if checkpoint.get('actor_filter',0) != manifest['actor_filter']:
        raise ValueError('label/student actor filters differ')
    for parameter in baseline.parameters():parameter.requires_grad_(False)
    for parameter in model.value.parameters():parameter.requires_grad_(False)
    if model.value_net is not None:
        for parameter in model.value_net.parameters():parameter.requires_grad_(False)
    before_value = {key:value.detach().clone() for key,value in model.value.state_dict().items()}
    rng = np.random.default_rng(args.seed);torch.manual_seed(args.seed)
    optimizer = torch.optim.Adam([p for p in model.parameters() if p.requires_grad], lr=args.lr)
    rows, valid = np.flatnonzero(~data['held']), np.flatnonzero(data['held'])
    anchor_rows, anchor_valid = np.flatnonzero(~anchors['held']), np.flatnonzero(anchors['held'])
    if not len(rows) or not len(valid) or not len(anchor_rows):raise ValueError('need independent train/held roots and anchors')
    group_counts = collections.Counter(int(data['group'][i]) for i in rows)
    # Mean weight 1 over training rows: each whole root has equal total loss.
    weights = np.asarray([len(rows)/(len(group_counts)*group_counts[int(data['group'][i])]) for i in rows],np.float32)
    row_weights = np.zeros(len(data['label']),np.float32);row_weights[rows]=weights
    config = {key:value for key,value in vars(args).items() if key != 'func'} | {'checkpoint_sha256':sha(args.checkpoint),'dataset_sha256':manifest['public_tables_sha256'],
                          'train_rows':len(rows),'held_rows':len(valid),'anchors':len(anchor_rows),'train_roots':len(group_counts)}
    (out / 'config.json').write_text(json.dumps(config,indent=2)+'\n')
    began = time.monotonic()
    def metrics(subset, tables, labels=False):
        ce=hits=divergence=seen=0
        kinds=collections.defaultdict(lambda: {'rows':0, 'kl_sum':0., 'same_action':0})
        with torch.no_grad():
            for start in range(0,len(subset),args.minibatch):
                chunk=subset[start:start+args.minibatch]
                batch=inputs(tables,chunk,args.device);logits,_=model(*batch);old,_=baseline(*batch)
                divergence+=float(kl(logits,old))*len(chunk);seen+=len(chunk)
                for kind in np.unique(tables['global'][chunk,native.G_DECISION]):
                    selected=tables['global'][chunk,native.G_DECISION]==kind
                    now_kind,old_kind=logits[selected],old[selected]
                    count=int(selected.sum());entry=kinds[int(kind)];entry['rows']+=count
                    entry['kl_sum']+=float(kl(now_kind,old_kind))*count
                    entry['same_action']+=int((now_kind.argmax(-1)==old_kind.argmax(-1)).sum())
                if labels:
                    target=torch.from_numpy(data['label'][chunk]).to(args.device)
                    ce+=float(F.cross_entropy(logits,target))*len(chunk);hits+=int((logits.argmax(-1)==target).sum())
        return {'ce':ce/seen if labels and seen else None,'agreement':hits/seen if labels and seen else None,
                'kl':divergence/seen if seen else None,'rows':seen,
                'decision_kinds':{kind:{'rows':r['rows'],'kl':r['kl_sum']/r['rows'],
                                       'baseline_action_agreement':r['same_action']/r['rows']} for kind,r in kinds.items()}}
    log=[]
    model.eval()
    initial={'epoch':0,'train':metrics(rows,data,True),'held':metrics(valid,data,True),'anchors':metrics(anchor_rows,anchors)}
    log.append(initial);print(json.dumps(initial),flush=True)
    for epoch in range(1,args.epochs+1):
        model.train()
        order=rng.permutation(rows)
        for start in range(0,len(order),args.minibatch):
            if time.monotonic()-began>args.time_limit_s:raise TimeoutError('distillation time budget reached')
            chunk=order[start:start+args.minibatch]
            batch=inputs(data,chunk,args.device);logits,_=model(*batch)
            with torch.no_grad():old,_=baseline(*batch)
            target=torch.from_numpy(data['label'][chunk]).to(args.device)
            weight=torch.from_numpy(row_weights[chunk]).to(args.device)
            ce=(F.cross_entropy(logits,target,reduction='none')*weight).mean()
            root_kl=kl(logits,old)
            sampled=rng.choice(anchor_rows,min(args.minibatch,len(anchor_rows)),replace=False)
            anchor_batch=inputs(anchors,sampled,args.device);now,_=model(*anchor_batch)
            with torch.no_grad():before,_=baseline(*anchor_batch)
            objective=ce+args.kl_coef*(root_kl+kl(now,before))
            if not torch.isfinite(objective):raise ValueError('nonfinite distillation objective')
            optimizer.zero_grad(set_to_none=True);objective.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(),1.0,error_if_nonfinite=True);optimizer.step()
        model.eval()
        result={'epoch':epoch,'wall_seconds':round(time.monotonic()-began,3),'train':metrics(rows,data,True),
                'held':metrics(valid,data,True),'anchors':metrics(anchor_rows,anchors),
                'held_anchors':metrics(anchor_valid,anchors)}
        log.append(result);print(json.dumps(result),flush=True)
    if any(not torch.equal(before_value[key],value) for key,value in model.value.state_dict().items()):
        raise ValueError('frozen value head changed')
    if any(not torch.isfinite(parameter).all() for parameter in model.parameters()):raise ValueError('nonfinite parameters')
    extra={key:checkpoint[key] for key in ('iteration','decisions','actor_filter','reward') if key in checkpoint}
    save(model,out/'checkpoint.pt',**extra,teacher_distillation=config,teacher_epoch=args.epochs)
    (out/'training.json').write_text(json.dumps({'config':config,'epochs':log,'value_head_unchanged':True,
                                               'checkpoint_sha256':sha(out/'checkpoint.pt')},indent=2)+'\n')


def compare(args):
    """Registered seed-paired bootstrap; faults remain explicit failed attempts."""
    def read(path):
        values=[json.loads(line) for line in Path(path).read_text().splitlines() if line]
        by_seed={r['seed']:r for r in values}
        if len(values)!=len(by_seed):raise ValueError('duplicate evaluation seeds')
        return by_seed
    control,candidate=read(args.control),read(args.candidate)
    expected={f'NQ4OEVAL{i:06d}' for i in range(args.runs)}
    if control.keys()!=expected or candidate.keys()!=expected:
        raise ValueError('evaluation must include every registered seed exactly once')
    faults=lambda rows:{s for s,r in rows.items() if r['status'] not in (native.DEATH,native.VICTORY)}
    baseline_faults,candidate_faults=faults(control),faults(candidate)
    matched=sorted(expected-baseline_faults-candidate_faults)
    if len(matched)<2:raise ValueError('too few completed pairs')
    delta=np.asarray([candidate[s]['floor']-control[s]['floor'] for s in matched],np.float64)
    rng=np.random.default_rng(41005)
    boot=np.concatenate([delta[rng.integers(0,len(delta),(min(256,10000-i),len(delta)))].mean(1)
                         for i in range(0,10000,256)])
    ci=np.quantile(boot,[.025,.975]).tolist();mean=float(delta.mean())
    result={'attempts':len(expected),'paired_terminals':len(matched),
            'control_faults':sorted(baseline_faults),'candidate_faults':sorted(candidate_faults),
            'new_faults':sorted(candidate_faults-baseline_faults),
            'control_floor':float(np.mean([control[s]['floor'] for s in matched])),
            'candidate_floor':float(np.mean([candidate[s]['floor'] for s in matched])),
            'difference':mean,'difference_bootstrap_ci95':ci,'bootstrap_replicates':len(boot),'bootstrap_seed':41005,
            'control_ends':dict(collections.Counter(r['status'] for r in control.values())),
            'candidate_ends':dict(collections.Counter(r['status'] for r in candidate.values())),
            'passed':bool(mean>=.5 and ci[0]>0 and not candidate_faults-baseline_faults),
            'control_rows_sha256':sha(args.control),'candidate_rows_sha256':sha(args.candidate)}
    Path(args.out).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))


def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
    e=sub.add_parser('export');e.add_argument('--batch',required=True);e.add_argument('--out',required=True)
    e.add_argument('--filter',type=int,default=1);e.set_defaults(func=export)
    e.add_argument('--min-qualified',type=int,default=1);e.add_argument('--min-quality-fraction',type=float,default=0.)
    t=sub.add_parser('train');t.add_argument('--data',required=True);t.add_argument('--checkpoint',required=True)
    t.add_argument('--out',required=True);t.add_argument('--seed',type=int,default=1);t.add_argument('--epochs',type=int,default=8)
    t.add_argument('--lr',type=float,default=1e-5);t.add_argument('--minibatch',type=int,default=128)
    t.add_argument('--kl-coef',type=float,default=.5);t.add_argument('--time-limit-s',type=float,default=300)
    t.add_argument('--device',default='cuda' if torch.cuda.is_available() else 'cpu');t.set_defaults(func=train)
    c=sub.add_parser('compare');c.add_argument('--control',required=True);c.add_argument('--candidate',required=True)
    c.add_argument('--runs',type=int,default=512);c.add_argument('--out',required=True);c.set_defaults(func=compare)
    args=p.parse_args();torch.set_num_threads(1);torch.backends.cuda.matmul.allow_tf32=True
    args.func(args)


if __name__=='__main__':main()
