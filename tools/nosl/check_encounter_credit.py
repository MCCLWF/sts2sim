#!/usr/bin/env python3
"""Actual normal-run credit formula, routing and update qualification."""
import argparse
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'python'))
from nosl import native
from nosl.encounter_credit import collect, combat_models, HP_SCALE, TURN_COST
from nosl.episodic_strategy import START_SHA, digest, pack, update
from nosl.model import load, save


def independent(row, credit):
    if not row['mc_qualified']:
        assert row['learning_returns'] is None and row['reward_sum'] is None
        return []
    if credit == 'C':
        expected = [sum(row['whole_rewards'][i:]) for i in row['learning_times']]
    else:
        expected = []
        for at in row['learning_times']:
            segment = next(s for s in row['encounters'] if s['start'] <= at < s['end'])
            transitions = row['credit_steps'][at:segment['end']]
            assert all(t[0] for t in transitions)
            assert all(a[3] == b[2] for a, b in zip(transitions, transitions[1:]))
            bonus = -1 if segment['kind'] == 'death' else 1
            expected.append(HP_SCALE * (segment['hp_end'] - transitions[0][2])
                - TURN_COST * sum(t[1] == 3 for t in transitions) + bonus)
    np.testing.assert_allclose(row['learning_returns'], expected, rtol=1e-6, atol=3e-7)
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', default=str(ROOT/'artifacts/nosl-q4r/start.pt'))
    parser.add_argument('--out', required=True);parser.add_argument('--device', default='cpu')
    args = parser.parse_args();out = Path(args.out);out.mkdir(parents=True, exist_ok=False)
    torch.set_num_threads(1)
    pilots = {};updated = {};stats = {};timings = {};frozen_counts = {}
    seeds = [f'NQ4ZPILOT{i:06d}' for i in range(8)]
    for credit in ('C', 'R'):
        torch.manual_seed(7)
        outside, combat, policy, _ = combat_models(args.checkpoint, args.device)
        initial = digest(outside);assert digest(combat) == initial
        data = collect(policy, seeds, args.device, sample=True, credit=credit, deadline=time.monotonic()+240)
        assert all(r['mc_qualified'] for r in data['outcomes'])
        assert len(data['public']) > 0
        expected = sum((independent(r, credit) for r in data['outcomes']), [])
        np.testing.assert_allclose(data['return'], expected, rtol=1e-6, atol=3e-7)
        for entry in data['public']:
            assert entry['global'][11] > 0
            assert all(entry[k].dtype == np.int32 for k in ('global', 'tokens', 'actions', 'counts'))
        batch = pack(data['public'], np.arange(min(64, len(data['public']))), args.device)
        with torch.no_grad():
            baseline, bv = outside(*batch);routed, rv = policy(*batch)
        assert torch.equal(baseline, routed) and torch.equal(bv, rv)
        pilots[credit] = data['outcomes']
        optimizer = torch.optim.Adam(combat.parameters(), lr=.000133)
        before = time.monotonic()
        stats[credit] = update(combat, optimizer, data, args.device, np.random.default_rng(7))
        timings[credit] = {'collect_seconds': data['seconds'], 'update_seconds': time.monotonic()-before,
            'decisions': data['decisions'], 'learning_rows': len(data['public'])}
        assert digest(outside) == initial and digest(combat) != initial
        from nosl.branch_credit import buffers, inputs
        env = native.Batch(8, 256, 256, 30000);env.set_filter(1);obs = buffers(8, 256, 256)
        for slot, seed in enumerate(seeds):assert env.reset(slot, seed, 10) == native.RUNNING
        env.step(np.full(8, -1, np.int32), *obs.values())
        checked = combat_count = 0
        for tick in range(500):
            live = np.flatnonzero(obs['status'] == native.RUNNING)
            if not len(live):break
            batch = inputs(obs, live, args.device)
            with torch.no_grad():
                routed, value = policy(*batch)
                indices = torch.nonzero(batch[0][:, 11] == 0).flatten()
                if len(indices):
                    frozen, fv = outside(*[t[indices] for t in batch])
                    assert torch.equal(routed[indices], frozen) and torch.equal(value[indices], fv)
            assert torch.isneginf(routed[torch.arange(routed.shape[1], device=routed.device)[None, :] >= batch[3][:, 1:2]]).all()
            checked += len(indices);combat_count += len(live)-len(indices)
            choices = np.full(8, -1, np.int32);choices[live] = routed.argmax(-1).cpu().numpy()
            env.step(choices, *obs.values())
        assert checked > 0 and combat_count > 0
        frozen_counts[credit] = {'outside_rows': checked, 'combat_rows': combat_count}
        greedy = collect(policy, ['NQ4ZUPDATED000000', 'NQ4ZUPDATED000001'], args.device,
            sample=False, credit=credit, deadline=time.monotonic()+180, keep_public=False)
        assert all(r['mc_qualified'] for r in greedy['outcomes'])
        updated[credit] = greedy['outcomes']
        with (out/(credit+'-updated-trajectories.jsonl')).open('w') as stream:
            for row in greedy['outcomes']:stream.write(json.dumps(row)+'\n')
        save(combat, out/(credit+'-smoke.pt'), actor_filter=1, outside_checkpoint_sha256=START_SHA,
             outside_parameter_sha256=initial,
             learning_scope='qualification only; combat with frozen outside X1')
        checked_model, metadata = load(out/(credit+'-smoke.pt'), args.device)
        assert metadata['outside_checkpoint_sha256'] == START_SHA
        assert metadata['outside_parameter_sha256'] == initial and digest(checked_model) == digest(combat)
        if credit == 'R':
            partial = collect(policy, [f'NQ4ZPARTIAL{i:06d}' for i in range(4)], args.device,
                sample=True, credit=credit, deadline=time.monotonic()+60, max_steps=80)
            assert any(not r['mc_qualified'] and r['combat_rows'] for r in partial['outcomes'])
            assert len(partial['public']) == sum(r['combat_rows'] for r in partial['outcomes'] if r['mc_qualified'])
            for row in partial['outcomes']:independent(row, credit)
            (out/'partial-outcomes.json').write_text(json.dumps(partial['outcomes'], indent=2)+'\n')
    assert [r['action_rows'] for r in pilots['C']] == [r['action_rows'] for r in pilots['R']]
    assert [r['whole_rewards'] for r in pilots['C']] == [r['whole_rewards'] for r in pilots['R']]
    with (out/'pilot-trajectories.jsonl').open('w') as stream:
        for row in pilots['R']:stream.write(json.dumps(row)+'\n')
    result = {'qualified': True, 'matched_first_batch': True, 'initial_routing_outputs_unchanged': True,
        'checkpoint_metadata_verified': True, 'independent_credit_formulas': True,
        'outside_parameters_and_actual_outputs_unchanged': True, 'public_int32': True,
        'gradient': stats, 'timings': timings, 'actual_updated_routing_rows': frozen_counts,
        'partial_rows_excluded': True, 'pilot_runs': 8, 'updated_runs_per_arm': 2}
    (out/'result.json').write_text(json.dumps(result, indent=2)+'\n');print(json.dumps(result))


if __name__ == '__main__':main()
