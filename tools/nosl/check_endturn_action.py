#!/usr/bin/env python3
"""Qualify capacity-matched action gates and run their fixed local learning."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import torch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'python'))
from nosl.branch_credit import KEYS, inputs
from nosl.endturn import DATA_SHA as TEACHER_SHA, decode, gate_features, scope, write
from nosl.endturn_action import initial, load, prepare, public_features, source, train
from nosl.episodic_strategy import digest
from nosl.model import load as generic_load
from nosl.teacher_distill import sha


def contract(start, heads, directory, cache, out, trained=None):
    directory, cache, out = Path(directory), Path(cache), Path(out)
    out.mkdir(parents=True, exist_ok=False)
    initial_models = {f'{kind}{seed}': initial(start, heads, seed, kind == 'C')
                      for seed in (1, 3) for kind in ('B', 'C')}
    sources = {seed: source(start, heads, seed) for seed in (1, 3)}
    for seed in (1, 3):
        left, right = initial_models[f'B{seed}'], initial_models[f'C{seed}']
        assert digest(left.head) == digest(right.head) == digest(sources[seed].head)
        assert digest(left.residual) == digest(right.residual)
        assert sum(p.numel() for p in left.learned_parameters()) == sum(p.numel() for p in right.learned_parameters())
    candidates = {label: load(Path(trained) / label / 'checkpoint.pt', start, heads)[0]
                  for label in initial_models} if trained else {}
    all_models = {**{'initial-' + k: v for k, v in initial_models.items()}, **candidates}
    with np.load(cache / 'cache.npz', allow_pickle=False) as cached:
        cache_gate, cache_action = cached['gate'], cached['action']
    old_data = ROOT / 'artifacts/nosl-q4o/cloud/labels/public-tables.npz'
    assert sha(old_data) == TEACHER_SHA
    cases, cache_checks, inactive, outside = 0, 0, 0, 0
    for file, prefixes in ((directory / 'data/public-roots.npz', ('',)), (old_data, ('', 'anchor_'))):
        with np.load(file, allow_pickle=False) as raw:
            for prefix in prefixes:
                tables = {key: raw[prefix + key] for key in KEYS}
                for first in range(0, len(tables['counts']), 128):
                    ids = np.arange(first, min(first + 128, len(tables['counts'])))
                    batch = inputs(tables, ids, 'cpu')
                    with torch.no_grad():
                        gate, action, old, value = public_features(sources[1].base, *batch)
                        original, ordinary_value = sources[1].base(*batch)
                        assert torch.equal(old, original) and torch.equal(value, ordinary_value)
                        active, end, other = scope(batch[0], batch[2], batch[3])
                        best = old.masked_fill(~other, float('-inf')).argmax(1)
                        if file != old_data:
                            assert torch.equal(gate, torch.from_numpy(cache_gate[ids]))
                            assert torch.equal(action, torch.from_numpy(cache_action[ids]));cache_checks += len(ids)
                        cases += len(ids);inactive += int((~active).sum());outside += int((batch[0][:, 11] == 0).sum())
                        source_results = {seed: policy(*batch) for seed, policy in sources.items()}
                        for label, model in all_models.items():
                            now, now_value = model(*batch)
                            assert torch.equal(now_value, value)
                            assert torch.equal(now[~active], old[~active])
                            assert torch.isneginf(now[torch.isneginf(old)]).all()
                            assert torch.equal(now.masked_fill(~other, float('-inf')).argmax(1)[active], best[active])
                            stop = model.stop(gate, action)
                            assert torch.equal(now, decode(old, stop, batch[0], batch[2], batch[3]))
                            if label.startswith('initial-'):
                                assert torch.equal(now, source_results[model.seed][0])
                                assert torch.equal(now_value, source_results[model.seed][1])
                            if not model.action_context:
                                assert torch.equal(stop, model.stop(gate, action + 1000))
                        pad_tokens = torch.cat((batch[1], torch.zeros_like(batch[1][:, :1])), 1)
                        pad_tokens[:, -1, 0] = 6;pad_tokens[:, -1, 11:13] = 10000;pad_tokens[:, -1, 15] = 10000
                        pad_actions = torch.cat((batch[2], torch.zeros_like(batch[2][:, :1])), 1)
                        pad_actions[:, -1, 0] = 3
                        assert torch.equal(gate_features(gate[:, :128], batch[0], pad_tokens, pad_actions, batch[3]), gate)
                        legal = torch.arange(batch[2].shape[1])[None] < batch[3][:, 1:2]
                        masked = old.clone();masked[~legal] = float('-inf')
                        assert torch.equal(masked.masked_fill(~other, float('-inf')).argmax(1), best)
    baseline = sources[1].base.state_dict()
    for model in all_models.values():
        assert all(torch.equal(t, model.base.state_dict()[name]) for name, t in baseline.items())
        assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    g = torch.zeros(4, 40, dtype=torch.int32);g[:, 11] = 1
    actions = torch.zeros(4, 3, 4, dtype=torch.int32);actions[:, :, 0] = torch.tensor([3, 2, 3])
    counts = torch.tensor([[0, 0], [1, 1], [1, 2], [1, 3]])
    old = torch.tensor([[float('-inf')]*3, [1., float('-inf'), float('-inf')],
                        [-30., 4., float('-inf')], [-30., 4., -29.]])
    now = decode(old, torch.full((4,), 2.), g, actions, counts)
    assert torch.equal(now[[0, 1, 3]], old[[0, 1, 3]]) and int(now[2].argmax()) == 0
    rejected = []
    if trained:
        path = Path(trained) / 'C1/checkpoint.pt';blob = torch.load(path, map_location='cpu', weights_only=False)
        for reason in ('format', 'schema', 'baseline', 'nonfinite'):
            bad = copy.deepcopy(blob)
            if reason == 'format':bad['format'] = 'old-endturn'
            elif reason == 'schema':bad['schema'] = 2
            elif reason == 'baseline':bad['baseline_checkpoint_sha256'] = 'wrong'
            else:bad['residual_state']['2.bias'][0] = float('nan')
            file = out / (reason + '-invalid.pt');torch.save(bad, file)
            try:load(file, start, heads)
            except ValueError:rejected.append(reason)
            else:raise AssertionError('malformed action checkpoint accepted')
        try:generic_load(path, 'cpu')
        except KeyError:rejected.append('generic_loader')
        else:raise AssertionError('generic loader silently dropped action residual')
    result = {'passed': True, 'public_rows': cases, 'cached_public_rows': cache_checks,
              'inactive_rows': inactive, 'outside_rows': outside,
              'initial_B_C_tensors_identical': True, 'capacity_matched': True,
              'initial_E_logits_and_values_bit_identical': True,
              'frozen_parameter_sha256': digest(sources[1].base),
              'base_tensors_values_inactive_and_nonend_greedy_unchanged': True,
              'B_action_context_always_zero': True, 'public_padding_and_decoder_boundaries': True,
              'invalid_checkpoints_rejected': rejected, 'actor_input_keys': list(KEYS)}
    write(out / 'result.json', result);print(json.dumps(result), flush=True);return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('train', 'contract'))
    parser.add_argument('--start', required=True);parser.add_argument('--heads', required=True)
    parser.add_argument('--data', required=True);parser.add_argument('--out', required=True)
    parser.add_argument('--trained');args = parser.parse_args();torch.set_num_threads(1)
    out = Path(args.out)
    if args.mode == 'contract':
        contract(args.start, args.heads, args.data, Path(args.trained) / 'cache', out, args.trained);return 0
    out.mkdir(parents=True, exist_ok=False);began = time.monotonic();deadline = began + 180
    prepare(args.start, args.heads, args.data, out / 'cache', deadline)
    before = contract(args.start, args.heads, args.data, out / 'cache', out / 'initial-contract')
    results = {}
    for seed in (1, 3):
        for context in (False, True):
            label = ('C' if context else 'B') + str(seed)
            results[label] = train(args.start, args.heads, out / 'cache', out / label, seed, context, deadline)
    result = {'training': results, 'initial_contract_passed': before['passed'],
              'seconds': time.monotonic() - began, 'registered_seconds': 180,
              'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'runtime_source_sha256': {str(p): sha(p) for p in (Path(__file__), ROOT / 'python/nosl/endturn_action.py')},
              'fitting_gate_passed': all(results[f'C{seed}']['fitting_gate_passed'] for seed in (1, 3))}
    write(out / 'summary.json', result);print(json.dumps(result), flush=True);return 0


if __name__ == '__main__':sys.exit(main())
