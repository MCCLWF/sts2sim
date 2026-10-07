#!/usr/bin/env python3
"""Frozen public-context counterfactuals; no training or online search."""
import argparse
import collections
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl import native
from nosl.action_head import START_SHA, DATA_SHA, frozen_digest
from nosl.model import load
from nosl.teacher_distill import sha
from nosl.vec import VecSampler

G_IN_COMBAT, G_DECISION = 11, 1
ARMS = ('C1', 'P1', 'C3', 'P3')
MODES = ('full', 'combat', 'outside')
RUNS, PREFIX = 512, 'NQ4VEVAL'
CANDIDATE_SHA = {
    'C1': '5302ae6724e9e6d8e2d0e3fb0903ada2ac52f8373e0c3a196e3e3462bd944ec3',
    'P1': 'e339a20e9630c4eeba928899e379e22fa1684043652f9b024f385f77823d134b',
    'C3': '3c19c8fc2e30291df3a694bc2a41f210f8014e482c4ad4bfd9cc15beaea267d6',
    'P3': 'b70b35fefa9564206585dcaf8e51dcb63e0e4ef678071040f4a2ab96a17a6853',
}
LABELS = ('X1', *(arm + '-' + mode for arm in ARMS for mode in MODES))


def write(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n')


def read(path):
    return json.loads(path.read_text())


def models(start, candidates, device):
    assert sha(start) == START_SHA
    baseline, blob = load(start, device)
    baseline.eval()
    assert blob['actor_filter'] == 1 and baseline.features == 3
    assert baseline.power_context is None and baseline.action_interaction is None
    digest = frozen_digest(baseline)
    learned = {}
    for arm in ARMS:
        path = candidates / arm / 'checkpoint.pt'
        assert sha(path) == CANDIDATE_SHA[arm], arm
        model, metadata = load(path, device)
        model.eval()
        assert frozen_digest(model) == digest and metadata['actor_filter'] == 1
        assert model.features == 3 and model.power_context is None and model.value_net is None
        assert (model.action_interaction is not None) == arm.startswith('P')
        learned[arm] = model
    return baseline, learned, digest


class ContextPolicy(torch.nn.Module):
    """Use identical frozen encodings, route only on the existing public flag."""
    def __init__(self, baseline, candidate, mode):
        super().__init__()
        if mode not in (*MODES, 'baseline'):
            raise ValueError('unknown public routing mode')
        if frozen_digest(baseline) != frozen_digest(candidate):
            raise ValueError('shared computation requires identical non-scoring tensors')
        self.baseline, self.candidate, self.mode = baseline, candidate, mode

    def scores(self, global_, tokens, actions, counts):
        global_, tokens, actions, counts = [x.long() for x in (global_, tokens, actions, counts)]
        run, encoded = self.baseline.encode(global_, tokens, counts)
        features = self.baseline.action_features(run, encoded, actions,
                                                self.candidate.action_interaction is not None)
        base = self.baseline.score_actions(features, counts)
        changed = self.candidate.score_actions(features, counts)
        if self.mode == 'baseline':
            active = torch.zeros_like(global_[:, G_IN_COMBAT], dtype=torch.bool)
        elif self.mode == 'full':
            active = torch.ones_like(global_[:, G_IN_COMBAT], dtype=torch.bool)
        else:
            active = global_[:, G_IN_COMBAT] > 0
            if self.mode == 'outside':
                active = ~active
        logits = torch.where(active[:, None], changed, base)
        values = self.baseline.value(run).squeeze(-1)
        return logits, values, base, changed, active

    def forward(self, *batch):
        logits, values, *_ = self.scores(*batch)
        return logits, values


def contract(start, candidates, data, device):
    assert sha(data) == DATA_SHA
    baseline, learned, digest = models(start, candidates, device)
    result = {'device': str(device), 'frozen_parameter_sha256': digest, 'rows': 0,
              'combat_choice_rows': 0, 'contexts': {}, 'arms': {}, 'autocast': device.type == 'cuda'}
    with np.load(data) as source:
        for arm, candidate in learned.items():
            checked = changed = 0
            policies = {mode: ContextPolicy(baseline, candidate, mode) for mode in (*MODES, 'baseline')}
            for prefix in ('', 'anchor_'):
                length = len(source[prefix + 'counts'])
                for begin in range(0, length, 64):
                    arrays = [source[prefix + k][begin:begin + 64] for k in ('global', 'tokens', 'actions', 'counts')]
                    batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device) for a in arrays]
                    combat = batch[0][:, G_IN_COMBAT] > 0
                    with torch.no_grad(), torch.autocast('cuda', dtype=torch.bfloat16, enabled=device.type == 'cuda'):
                        base, value = baseline(*batch)
                        other, _ = candidate(*batch)
                        for mode in (*MODES, 'baseline'):
                            policy = policies[mode]
                            logits, values = policy(*batch)
                            active = {'full': torch.ones_like(combat), 'combat': combat,
                                      'outside': ~combat, 'baseline': torch.zeros_like(combat)}[mode]
                            assert torch.equal(logits, torch.where(active[:, None], other, base)), (arm, mode)
                            assert torch.equal(value, values)
                            count = batch[3][:, 1]
                            assert ((logits.argmax(-1) >= 0) & (logits.argmax(-1) < count)).all()
                            mask = torch.arange(logits.shape[1], device=device)[None, :] >= count[:, None]
                            assert torch.isneginf(logits[mask]).all()
                    changed += int((base.argmax(-1) != other.argmax(-1)).sum())
                    checked += len(combat)
                    if arm == ARMS[0]:
                        result['rows'] += len(combat)
                        result['combat_choice_rows'] += int((combat & (batch[0][:, G_DECISION] == 7)).sum())
                        result['contexts']['combat'] = result['contexts'].get('combat', 0) + int(combat.sum())
                        result['contexts']['outside'] = result['contexts'].get('outside', 0) + int((~combat).sum())
            result['arms'][arm] = {'rows': checked, 'different_argmax': changed, 'max_logit_difference': 0,
                                   'value_difference': 0, 'modes_identical': True}
    assert result['rows'] == 2759 and result['combat_choice_rows'] > 0
    assert all(result['contexts'].get(k, 0) > 0 for k in ('combat', 'outside'))
    result['passed'] = True
    return result


def play(policy, seeds, device, workers, failure_log):
    rows, per_seed = {}, collections.defaultdict(collections.Counter)
    per_kind = collections.defaultdict(collections.Counter)
    started = time.monotonic()
    with VecSampler(workers, -(-len(seeds) // workers), split='train', seeds=seeds,
                    actor_filter=1, keep_history=True, failure_log=failure_log) as vec:
        A = vec.arrays
        results = vec.reset()
        while True:
            for row in results:
                assert row['seed'] not in rows
                rows[row['seed']] = {**row, 'routing': dict(per_seed[row['seed']])}
            if len(rows) == len(seeds):
                break
            live = vec.running()
            nt, na = max(1, int(A['counts'][:, 0].max())), max(1, int(A['counts'][:, 1].max()))
            batch = [torch.from_numpy(np.ascontiguousarray(a)).to(device) for a in
                     (A['global'], A['tokens'][:, :nt], A['actions'][:, :na], A['counts'])]
            with torch.no_grad(), torch.autocast('cuda', dtype=torch.bfloat16, enabled=device.type == 'cuda'):
                logits, _, base, candidate, active = policy.scores(*batch)
            selected = logits.float().argmax(-1).cpu().numpy()
            base_choice = base.float().argmax(-1).cpu().numpy()
            other_choice = candidate.float().argmax(-1).cpu().numpy()
            enabled = active.cpu().numpy()
            assert torch.isfinite(logits[torch.from_numpy(live).to(device)]).any(-1).all()
            assert ((selected[live] >= 0) & (selected[live] < A['counts'][live, 1])).all()
            for slot in np.flatnonzero(live):
                context = 'combat' if A['global'][slot, G_IN_COMBAT] > 0 else 'outside'
                kind = str(int(A['global'][slot, G_DECISION]))
                counts = {'decisions': 1, 'candidate_active': int(enabled[slot]),
                          'candidate_differs': int(other_choice[slot] != base_choice[slot]),
                          'changed': int(selected[slot] != base_choice[slot])}
                per_kind[context + ':' + kind].update(counts)
                for key, value in counts.items():
                    per_seed[vec.seed_of[slot]][context + ':' + key] += value
            choice = np.where(live, selected, -1).astype(np.int32)
            results = vec.step(choice)
    return rows, {'seconds': time.monotonic() - started,
                  'by_context_decision': {k: dict(v) for k, v in sorted(per_kind.items())}}


def pair(records, candidate, control):
    keys = sorted(set(records[candidate]) & set(records[control]))
    valid = [s for s in keys if records[candidate][s]['status'] in (1, 2) and records[control][s]['status'] in (1, 2)]
    delta = np.asarray([records[candidate][s]['floor'] - records[control][s]['floor'] for s in valid], np.float64)
    assert len(delta) >= 2
    mean, se = float(delta.mean()), float(delta.std(ddof=1) / np.sqrt(len(delta)))
    ci = [mean - 1.96 * se, mean + 1.96 * se]
    return {'attempts': len(keys), 'paired_terminals': len(valid), 'excluded_pairs': len(keys) - len(valid),
            'difference': mean, 'standard_error': se, 'difference_ci95': ci,
            'passed': len(valid) == RUNS and mean >= .5 and ci[0] > 0}


def verify(out, start, candidates):
    baseline, _, digest = models(start, candidates, 'cpu')
    spec = read(out / 'platform.json')
    assert spec['labels'] == list(LABELS) and spec['runs'] == RUNS and spec['prefix'] == PREFIX
    assert spec['start_sha256'] == START_SHA and spec['candidate_sha256'] == CANDIDATE_SHA
    assert spec['frozen_parameter_sha256'] == digest and spec['actor_filter'] == 1 and spec['ascension'] == 10
    contract_info = read(out / 'contract.json')
    assert contract_info['passed'] and contract_info['frozen_parameter_sha256'] == digest
    expected = {f'{PREFIX}{i:06d}' for i in range(RUNS)}
    summary = {'evaluation': {}, 'pairs': {}, 'continuation_gate': {}}
    records = {}
    for label in LABELS:
        path = out / 'rows' / (label + '.jsonl')
        assert sha(path) == read(out / (label + '.json'))['rows_sha256']
        lines = [json.loads(line) for line in path.read_text().splitlines()]
        assert len(lines) == RUNS and {r['seed'] for r in lines} == expected
        records[label] = {r['seed']: r for r in lines}
        done = [r for r in lines if r['status'] in (1, 2)]
        routing = collections.Counter()
        for row in lines:
            routing.update(row['routing'])
            assert isinstance(row.get('action_rows'), list) or row['status'] not in (1, 2)
            for context in ('combat', 'outside'):
                r = row['routing']
                assert 0 <= r.get(context + ':changed', 0) <= r.get(context + ':candidate_active', 0) <= r.get(context + ':decisions', 0)
                assert 0 <= r.get(context + ':candidate_differs', 0) <= r.get(context + ':decisions', 0)
        mode = label.split('-')[-1]
        if mode in ('combat', 'outside', 'X1'):
            excluded = 'outside' if mode == 'combat' else 'combat'
            assert routing[excluded + ':candidate_active'] == 0
            assert routing[excluded + ':changed'] == 0
        if label == 'X1':
            assert routing['outside:changed'] == routing['combat:changed'] == 0
            assert routing['outside:candidate_active'] == routing['combat:candidate_active'] == 0
        elif mode == 'full':
            assert all(routing[k + ':candidate_active'] == routing[k + ':decisions'] for k in ('combat', 'outside'))
        summary['evaluation'][label] = {'attempts': len(lines), 'terminals': len(done), 'faults': len(lines) - len(done),
            'victories': sum(r['status'] == 1 for r in done),
            'mean_floor': float(np.mean([r['floor'] for r in done])),
            'act2': sum(r['act'] >= 1 for r in done), 'act3': sum(r['act'] >= 2 for r in done),
            'routing': dict(routing)}
    for arm in ARMS:
        for mode in MODES:
            label = arm + '-' + mode
            summary['pairs'][label + '-X1'] = pair(records, label, 'X1')
        for mode in ('combat', 'outside'):
            label, full = arm + '-' + mode, arm + '-full'
            summary['pairs'][label + '-' + full] = pair(records, label, full)
    for seed in (1, 3):
        for mode in MODES:
            a, b = f'P{seed}-{mode}', f'C{seed}-{mode}'
            summary['pairs'][a + '-' + b] = pair(records, a, b)
    for kind in ('C', 'P'):
        for mode in ('combat', 'outside'):
            summary['continuation_gate'][kind + '-' + mode] = all(
                summary['pairs'][f'{kind}{seed}-{mode}-X1']['passed'] for seed in (1, 3))
    summary['continuation_gate']['criteria'] = 'both learner seeds >=.5 floor, paired CI95 lower>0, all 512 real terminal pairs'
    summary['continuation_gate']['scope'] = 'exploratory diagnosis only; never a high-level qualification or automatic X1 replacement'
    write(out / 'verified-results.json', summary)
    return summary


def run(out, start, candidates, data, device, workers):
    assert not (out / 'platform.json').exists(), 'refuse to restart formal diagnosis'
    assert device.type == 'cuda' and torch.cuda.is_available(), 'formal diagnosis uses the authorized cloud GPU'
    baseline, learned, digest = models(start, candidates, device)
    contract_result = contract(start, candidates, data, device)
    write(out / 'contract.json', contract_result)
    spec = {'labels': list(LABELS), 'runs': RUNS, 'prefix': PREFIX, 'ascension': 10, 'actor_filter': 1,
            'greedy': True, 'autocast': 'bfloat16', 'workers': workers, 'start_sha256': START_SHA,
            'candidate_sha256': CANDIDATE_SHA, 'frozen_parameter_sha256': digest,
            'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
            'python': platform.python_version(), 'torch': torch.__version__, 'cuda': torch.version.cuda,
            'gpu': torch.cuda.get_device_name(0), 'native_sha256': sha(ROOT / 'build/libsts2nosl_c.so')}
    write(out / 'platform.json', spec)
    (out / 'rows').mkdir()
    (out / 'trajectories').mkdir()
    seeds = [f'{PREFIX}{i:06d}' for i in range(RUNS)]
    for label in LABELS:
        arm, _, mode = label.partition('-')
        policy = ContextPolicy(baseline, baseline if arm == 'X1' else learned[arm], mode or 'baseline')
        rows, timing = play(policy, seeds, device, workers, out / (label + '-failures.jsonl'))
        path = out / 'rows' / (label + '.jsonl')
        with path.open('w') as stream:
            for seed in seeds:
                stream.write(json.dumps(rows[seed]) + '\n')
        with (out / 'trajectories' / (label + '.jsonl')).open('w') as stream:
            for seed in seeds[:2]:
                row = rows[seed]
                stream.write(json.dumps({**row, 'ascension': 10, 'actor_filter': 1,
                    'end': {1: 'victory', 2: 'death'}.get(row['status'], 'error')}) + '\n')
        info = {'label': label, **timing, 'rows_sha256': sha(path)}
        write(out / (label + '.json'), info)
        print(json.dumps(info), flush=True)
    summary = verify(out, start, candidates)
    (out / 'complete').write_text('complete\n')
    print(json.dumps(summary), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('contract', 'run', 'verify', 'pilot'), required=True)
    parser.add_argument('--start', type=Path, required=True)
    parser.add_argument('--candidates', type=Path, required=True)
    parser.add_argument('--data', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--device', choices=('cpu', 'cuda'), default='cpu')
    parser.add_argument('--workers', type=int, default=6)
    args = parser.parse_args()
    os.chdir(ROOT)
    args.out.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    device = torch.device(args.device)
    if args.mode == 'contract':
        result = contract(args.start, args.candidates, args.data, device)
        write(args.out / 'contract.json', result)
        print(json.dumps(result))
    elif args.mode == 'verify':
        print(json.dumps(verify(args.out, args.start, args.candidates)))
    elif args.mode == 'pilot':
        baseline, learned, _ = models(args.start, args.candidates, device)
        seeds = ['NQ4VPILOT0000', 'NQ4VPILOT0001']
        for mode in ('baseline', *MODES):
            policy = ContextPolicy(baseline, learned['P1'], mode)
            rows, result = play(policy, seeds, device, 1, args.out / (mode + '-failures.jsonl'))
            write(args.out / (mode + '.json'), {'results': rows, **result})
            print(json.dumps({'mode': mode, 'seconds': result['seconds'], 'ends': [r['status'] for r in rows.values()]}), flush=True)
    else:
        run(args.out, args.start, args.candidates, args.data, device, args.workers)


if __name__ == '__main__':
    main()
