#!/usr/bin/env python3
"""Verify the frozen public gate contract; optionally prepare/train the two arms."""
import argparse
import copy
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import (DATA_SHA, EndTurnPolicy, baseline, decode, gate_features,
                          load_gate, prepare, scope, train, write)
from nosl.episodic_strategy import digest
from nosl.model import load
from nosl.teacher_distill import sha


def verify(start, directory, out, candidates=(), device='cpu', cache=None):
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    assert sha(Path(directory) / 'public-tables.npz') == DATA_SHA
    base = baseline(start, device)
    torch.manual_seed(17)
    initial = EndTurnPolicy(base).eval()
    models = [('initial', initial)] + [(Path(p).parent.name, load_gate(p, start, device)[0]) for p in candidates]
    count, active_count, inactive_count, outside_count = 0, 0, 0, 0
    cached = np.load(Path(cache) / 'cache.npz', allow_pickle=False) if cache else None
    worst_cache_difference = 0.
    with np.load(Path(directory) / 'public-tables.npz', allow_pickle=False) as source:
        for prefix in ('', 'anchor_'):
            tables = {k: source[prefix + k] for k in KEYS}
            for first in range(0, len(tables['counts']), 128):
                ids = np.arange(first, min(first + 128, len(tables['counts'])))
                batch = inputs(tables, ids, device)
                with torch.no_grad():
                    old, old_value = base(*batch)
                    run, _ = base.encode(batch[0].long(), batch[1].long(), batch[3].long())
                    feature = gate_features(run, *batch)
                    if cached is not None:
                        frozen_feature = torch.from_numpy(cached[prefix + 'features'][ids]).to(device)
                        worst_cache_difference = max(worst_cache_difference, float((feature - frozen_feature).abs().max()))
                        if device == 'cpu':assert torch.equal(feature, frozen_feature)
                    active, end, other = scope(batch[0], batch[2], batch[3])
                    other_best = old.masked_fill(~other, float('-inf')).argmax(1)
                    count += len(ids);active_count += int(active.sum());inactive_count += int((~active).sum())
                    outside_count += int((batch[0][:, 11] == 0).sum())
                    for label, model in models:
                        now, value = model(*batch)
                        assert torch.equal(value, old_value), 'gate changed the original value'
                        assert torch.equal(now[~active], old[~active]), 'gate changed an out-of-scope row'
                        assert torch.isneginf(now[torch.isneginf(old)]).all(), 'padding became legal'
                        assert torch.equal(now.masked_fill(~other, float('-inf')).argmax(1)[active], other_best[active])
                        head_score = model.head(feature).squeeze(-1)
                        assert torch.equal(now, decode(old, head_score, batch[0], batch[2], batch[3]))
                        if label == 'initial':
                            assert (head_score < 0).all()
                            assert torch.equal(now.argmax(1)[active], other_best[active])
                    # Ignore even apparently meaningful enemy/action padding.
                    tokens = torch.cat((batch[1], batch[1][:, :1].clone()), dim=1)
                    tokens[:, -1, 0] = 6;tokens[:, -1, 11:13] = 10000;tokens[:, -1, 15] = 10000
                    actions = torch.cat((batch[2], batch[2][:, :1].clone()), dim=1)
                    actions[:, -1, 0] = 3
                    assert torch.equal(gate_features(run, batch[0], tokens, actions, batch[3]), feature)
                    assert all(torch.equal(a, b) for a, b in zip(scope(batch[0], actions, batch[3]),
                        (active, torch.cat((end, torch.zeros_like(end[:, :1])), 1),
                         torch.cat((other, torch.zeros_like(other[:, :1])), 1))))
                    for stop in (-2., 2.):
                        forced = decode(old, old.new_full((len(ids),), stop), batch[0], batch[2], batch[3])
                        expected = end.long().argmax(1) if stop > 0 else other_best
                        assert torch.equal(forced.argmax(1)[active], expected[active])
                        assert torch.equal(forced[~active], old[~active])
    if cached is not None:cached.close()
    base_state = base.state_dict()
    for _, model in models:
        assert all(torch.equal(t, model.base.state_dict()[name]) for name, t in base_state.items())
        assert not any(p.requires_grad for p in model.base.parameters())
    # Explicit terminal/only-EndTurn/duplicated-EndTurn boundaries. These are
    # decoder contract probes, not fabricated game results or training rows.
    g = torch.zeros(4, 40, dtype=torch.int32, device=device);g[:, 11] = 1
    actions = torch.zeros(4, 3, 4, dtype=torch.int32, device=device)
    actions[:, :, 0] = torch.tensor([3, 2, 3], device=device)
    counts = torch.tensor([[0, 0], [1, 1], [1, 2], [1, 3]], device=device)
    old = torch.tensor([[float('-inf')]*3, [1., float('-inf'), float('-inf')],
                         [-30., 4., float('-inf')], [-30., 4., -29.]], device=device)
    value = decode(old, torch.full((4,), 2., device=device), g, actions, counts)
    assert torch.equal(value[[0, 1, 3]], old[[0, 1, 3]])
    assert int(value[2].argmax()) == 0
    rejected = []
    if candidates:
        raw = torch.load(candidates[0], map_location='cpu', weights_only=False)
        for label in ('schema', 'baseline', 'nonfinite'):
            damaged = copy.deepcopy(raw)
            if label == 'schema':damaged['schema'] = 2
            elif label == 'baseline':damaged['baseline_checkpoint_sha256'] = 'invalid'
            else:damaged['head_state']['2.bias'][0] = float('nan')
            file = out / (label + '-invalid.pt');torch.save(damaged, file)
            try:load_gate(file, start, device)
            except ValueError:rejected.append(label)
            else:raise AssertionError('invalid gate accepted: ' + label)
        try:load(candidates[0], device)
        except KeyError:rejected.append('generic_loader')
        else:raise AssertionError('generic loader silently accepted an experimental gate')
    result = {'passed': True, 'device': device, 'precision': 'float32', 'rows': count,
              'active_rows': active_count, 'inactive_rows': inactive_count, 'outside_rows': outside_count,
              'frozen_parameter_sha256': digest(base), 'all_original_tensors_identical': True,
              'values_identical': True, 'out_of_scope_logits_identical': True,
              'other_greedy_action_identical': True, 'padding_ignored_and_masked': True,
              'initial_continues': True, 'positive_negative_decoder_controls': True,
              'only_endturn_duplicate_and_parked_unchanged': True,
              'cache_forward_identical': True if device == 'cpu' and cache else None,
              'cache_max_feature_difference': worst_cache_difference, 'invalid_contracts_rejected': rejected}
    write(out / 'result.json', result)
    print(json.dumps(result), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--start', required=True);parser.add_argument('--data', required=True)
    parser.add_argument('--out', required=True);parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--candidates', nargs='*', default=[]);parser.add_argument('--device', default='cpu')
    args = parser.parse_args();torch.set_num_threads(1)
    out = Path(args.out)
    if args.verify_only:
        verify(args.start, args.data, out, args.candidates, args.device)
        return 0
    if args.device != 'cpu':raise ValueError('registered training is CPU float32 only')
    out.mkdir(parents=True, exist_ok=False)
    deadline = time.monotonic() + 240
    prepare(args.start, args.data, out / 'cache', deadline)
    results = {}
    for seed in (1, 3):
        results[f'E{seed}'] = train(args.start, out / 'cache', out / f'E{seed}', seed, deadline)
    candidates = [str(out / arm / 'checkpoint.pt') for arm in results]
    contract = verify(args.start, args.data, out / 'contract', candidates, cache=out / 'cache')
    summary = {'training': results, 'contract_passed': contract['passed'],
               'fitting_gate_passed': all(r['fitting_gate_passed'] for r in results.values()),
               'cache_and_training_seconds': 240 - (deadline - time.monotonic())}
    write(out / 'summary.json', summary);print(json.dumps(summary), flush=True)
    return 0 if summary['fitting_gate_passed'] else 1


if __name__ == '__main__':sys.exit(main())
