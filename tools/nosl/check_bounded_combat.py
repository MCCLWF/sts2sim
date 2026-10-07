#!/usr/bin/env python3
"""Qualify real frozen update transactions and new normal-run public routing."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from check_encounter_credit import independent
from diagnose_combat_update import dataset
from nosl import native
from nosl.bounded_combat import KL_CAP, MAX_LR, adam_digest, checked_update, write
from nosl.encounter_credit import collect, combat_models
from nosl.episodic_strategy import START_SHA, digest, update as reference_update
from nosl.model import load, save


class VerifiedPolicy(torch.nn.Module):
    def __init__(self, policy, outside):
        super().__init__();self.policy = policy;self.outside = outside
        self.outside_rows = self.combat_rows = 0

    def forward(self, *batch):
        logits, values = self.policy(*batch)
        selected = torch.nonzero(batch[0][:, 11] == 0).flatten()
        if len(selected):
            frozen, value = self.outside(*(x[selected] for x in batch))
            assert torch.equal(logits[selected], frozen) and torch.equal(values[selected], value)
        mask = torch.arange(logits.shape[1], device=logits.device)[None, :] < batch[3][:, 1:2]
        assert torch.isfinite(logits[mask]).all() and torch.isneginf(logits[~mask]).all()
        self.outside_rows += len(selected);self.combat_rows += len(batch[0])-len(selected)
        return logits, values


def frozen(checkpoint, out, device):
    deadline = time.monotonic()+480;results = {}
    for seed, sha in ((1,'774046b35bcfabb1d9e163f0c851730490dbfb5f36933d28cc6426f9ac5ac68c'),
                      (3,'51582fd90c39eb2bd77e045a5f92dc7e2053e16789e1473eff2f6015ec609ab8')):
        cache = ROOT/f'artifacts/nosl-q4aa/public-{seed}.npz'
        assert hashlib.sha256(cache.read_bytes()).hexdigest() == sha
        data = dataset(cache, 'C')
        outside, initial, _, _ = combat_models(checkpoint, device)
        ref = copy.deepcopy(initial);wrapped = copy.deepcopy(initial)
        ref_opt = torch.optim.Adam(ref.parameters(), lr=.000133)
        wrapped_opt = torch.optim.Adam(wrapped.parameters(), lr=.000133)
        ref_stats = reference_update(ref, ref_opt, data, device, np.random.default_rng(seed))
        control = checked_update(wrapped, wrapped_opt, data, device, np.random.default_rng(seed), deadline)
        assert digest(wrapped) == digest(ref) and adam_digest(wrapped_opt) == adam_digest(ref_opt)
        bounded = copy.deepcopy(initial);opt = torch.optim.Adam(bounded.parameters(), lr=MAX_LR)
        actual = checked_update(bounded, opt, data, device, np.random.default_rng(seed), deadline, cap=KL_CAP)
        if actual['accepted']:
            assert actual['retained']['exact_kl_mean'] <= KL_CAP and digest(bounded) != digest(initial)
        before = digest(wrapped);before_adam = adam_digest(wrapped_opt, include_lr=False)
        forced = checked_update(wrapped, wrapped_opt, data, device, np.random.default_rng(seed), deadline, cap=1e-12)
        assert not forced['accepted'] and digest(wrapped) == before
        assert adam_digest(wrapped_opt, include_lr=False) == before_adam
        assert digest(outside) == digest(initial)
        results[str(seed)] = {'cache_sha256': sha, 'rows': len(data['public']), 'reference': ref_stats,
                             'control_exact_parameter_and_adam': True, 'control': control,
                             'actual_bound': actual, 'forced_rejection': forced,
                             'rollback_of_populated_adam_verified': True}
        write(out/'frozen-result.json', results)
        print(json.dumps({'frozen_seed': seed, 'actual_bound_accepted': actual['accepted'],
                          'actual_candidate_kl': actual['candidate']['exact_kl_mean'],
                          'forced_rejection_restored': True}), flush=True)
    return results


def normal(checkpoint, out, device):
    pilots = {};updated = {};stats = {};routing = {}
    for arm in ('U','B'):
        torch.manual_seed(7)
        outside, combat, routed, _ = combat_models(checkpoint, device)
        initial = digest(outside);policy = VerifiedPolicy(routed, outside)
        data = collect(policy, [f'NQ4ABPILOT{i:06d}' for i in range(8)], device,
                       sample=True, credit='C', deadline=time.monotonic()+120)
        assert all(r['mc_qualified'] for r in data['outcomes'])
        expected = sum((independent(r, 'C') for r in data['outcomes']), [])
        np.testing.assert_allclose(data['return'], expected, rtol=1e-6, atol=3e-7)
        assert len(data['public']) and all(r['global'][11] > 0 and all(
            r[k].dtype == np.int32 for k in ('global','tokens','actions','counts')) for r in data['public'])
        pilots[arm] = data['outcomes']
        opt = torch.optim.Adam(combat.parameters(), lr=MAX_LR if arm == 'B' else .000133)
        stats[arm] = checked_update(combat, opt, data, device, np.random.default_rng(7),
                                   time.monotonic()+120, cap=KL_CAP if arm == 'B' else None)
        assert digest(outside) == initial
        greedy = collect(policy, ['NQ4ABUPDATED000000','NQ4ABUPDATED000001'], device,
                         sample=False, credit='C', deadline=time.monotonic()+120, keep_public=False)
        assert all(r['mc_qualified'] for r in greedy['outcomes'])
        updated[arm] = greedy['outcomes']
        with (out/(arm+'-updated-trajectories.jsonl')).open('w') as stream:
            for row in greedy['outcomes']:
                stream.write(json.dumps(row)+'\n')
        assert policy.outside_rows > 0 and policy.combat_rows > 0
        routing[arm] = {'outside_rows': policy.outside_rows, 'combat_rows': policy.combat_rows}
        save(combat, out/(arm+'-smoke.pt'), actor_filter=1, outside_checkpoint_sha256=START_SHA,
             outside_parameter_sha256=initial, learning_scope='qualification only; combat with frozen outside X1')
        saved, meta = load(out/(arm+'-smoke.pt'), device)
        assert meta['outside_checkpoint_sha256'] == START_SHA and meta['outside_parameter_sha256'] == initial
        assert digest(saved) == digest(combat)
        if arm == 'B':
            partial = collect(policy, [f'NQ4ABPARTIAL{i:06d}' for i in range(4)], device, sample=True,
                              credit='C', max_steps=80, deadline=time.monotonic()+60)
            assert any(not r['mc_qualified'] and r['combat_rows'] for r in partial['outcomes'])
            assert len(partial['public']) == sum(r['combat_rows'] for r in partial['outcomes'] if r['mc_qualified'])
            for row in partial['outcomes']:
                independent(row, 'C')
            write(out/'partial-outcomes.json', partial['outcomes'])
    assert pilots['U'] == pilots['B']
    with (out/'pilot-trajectories.jsonl').open('w') as stream:
        for row in pilots['U']:
            stream.write(json.dumps(row)+'\n')
    return {'pilot_runs': 8, 'updated_runs_per_arm': 2, 'matched_first_batch': True,
            'C_credit_independent': True, 'public_int32': True, 'partial_excluded': True,
            'outside_actual_outputs_unchanged': True, 'routing_counts': routing, 'updates': stats}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', default=str(ROOT/'artifacts/nosl-q4r/start.pt'))
    parser.add_argument('--out', required=True);parser.add_argument('--device', default='cpu')
    parser.add_argument('--skip-frozen', action='store_true', help='cloud normal qualification after local frozen proof')
    args = parser.parse_args();torch.set_num_threads(1)
    out = Path(args.out);out.mkdir(parents=True, exist_ok=False)
    began = time.monotonic()
    if not args.skip_frozen:
        frozen(args.checkpoint, out, args.device)
    result = {'qualified': True, 'device': args.device, 'precision': 'float32',
              'frozen_checks_run': not args.skip_frozen, 'normal': normal(args.checkpoint, out, args.device),
              'wall_seconds': time.monotonic()-began}
    write(out/'result.json', result);print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
