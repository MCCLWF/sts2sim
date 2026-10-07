#!/usr/bin/env python3
"""Check public power owner/amount pooling on real Silent A10 observations.

Checks zero-growth equivalence, independently reconstructs the pools, and
poisons padding/reorders rows. Writes nonzero diagnostic weights so the
existing native forward checker exercises the new layer rather than zero.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import torch

from nosl import native
from nosl.model import N0, TOKEN_ENEMY, TOKEN_POWER, grow, load, save


def independent(model, rows):
    width = model.config['width']
    expected = np.zeros((len(rows) + 1, 2 * width), np.float32)
    present = np.zeros(len(rows) + 1, bool)
    table = model.embed['power'].weight.detach().numpy()
    for t in rows:
        if t[0] != TOKEN_POWER or t[5] <= 0:
            continue
        owner, amount = int(t[N0 + 1]), np.float32(t[N0])
        if not 0 <= owner <= len(rows):
            continue
        e = table[np.clip(t[5], 0, len(table) - 1)]
        expected[owner, :width] += e * (np.sign(amount) * np.log1p(abs(amount)) / np.float32(3))
        expected[owner, width:] += e * (np.clip(amount, -20, 20) / np.float32(10))
        present[owner] = True
    return expected, present


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint', required=True)
    p.add_argument('--out', required=True)
    args = p.parse_args()
    torch.set_num_threads(1)
    model, blob = load(args.checkpoint, 'cpu')
    if model.power_context is not None:
        raise ValueError('diagnostic needs an initial policy without power_context')
    grown = grow(model, model.features, '', model.value_net is not None, True)
    model.eval()
    grown.eval()
    for key, before in model.state_dict().items():
        if not torch.equal(before, grown.state_dict()[key]):
            raise AssertionError('growth changed an existing tensor: ' + key)
    n, T, A = 4, 192, 160
    batch = native.Batch(n, T, A, 30000)
    batch.set_filter(int(blob.get('actor_filter', 0)))
    for j in range(n):
        if batch.reset(j, f'NQ4PCHK{j:04d}', 10) != native.RUNNING:
            raise AssertionError('real-observation reset failed')
    g = np.zeros((n, native.GLOBAL_WIDTH), np.int32)
    tokens = np.zeros((n, T, native.TOKEN_WIDTH), np.int32)
    actions = np.zeros((n, A, native.ACTION_WIDTH), np.int32)
    counts = np.zeros((n, 2), np.int32)
    status = np.zeros(n, np.int32)
    steps = np.zeros(n, np.int32)
    choice = np.full(n, -1, np.int32)
    batch.step(choice, g, tokens, actions, counts, status, steps)
    observations = powers = player_pools = enemy_pools = 0
    torch.manual_seed(2)
    for _ in range(128):
        live = np.flatnonzero(status == native.RUNNING)
        if not len(live):
            break
        used = int(counts[live, 0].max())
        batch_args = (torch.from_numpy(g[live]), torch.from_numpy(tokens[live, :used]),
                      torch.from_numpy(actions[live]), torch.from_numpy(counts[live]))
        with torch.no_grad():
            before = model(*batch_args)
            after = grown(*batch_args)
            if not all(torch.equal(a, b) for a, b in zip(before, after)):
                raise AssertionError('zero-growth logits/value changed')
            for slot in live:
                rows = tokens[slot, :counts[slot, 0]].copy()
                expected, present = independent(grown, rows)
                # Validate the real observation's public relation before
                # checking the consumer. Every enemy-owned power has exactly
                # one shown enemy with the matching ordinal.
                enemy_owners = [int(t[N0 + 6]) + 1 for t in rows if t[0] == TOKEN_ENEMY]
                if len(enemy_owners) != len(set(enemy_owners)):
                    raise AssertionError('duplicate visible enemy ordinal')
                for t in rows:
                    if t[0] == TOKEN_POWER:
                        owner = int(t[N0 + 1])
                        if owner != 0 and owner not in enemy_owners:
                            raise AssertionError('power owner has no visible enemy')
                        powers += 1
                real = torch.from_numpy(rows).unsqueeze(0)
                one_count = torch.from_numpy(counts[slot:slot + 1])
                actual, actual_present = grown.power_summary(real, one_count)
                torch.testing.assert_close(actual[0], torch.from_numpy(expected), rtol=1e-6, atol=2e-6)
                if not np.array_equal(actual_present[0].numpy(), present):
                    raise AssertionError('empty-owner mask differs')
                shuffled = real[:, torch.randperm(real.shape[1])]
                other, mask = grown.power_summary(shuffled, one_count)
                torch.testing.assert_close(other, actual, rtol=1e-6, atol=2e-6)
                if not torch.equal(mask, actual_present):
                    raise AssertionError('token order changed owner presence')
                # A valid-looking power in unused padding must not enter
                # the player pool or create a spurious owner.
                poison = np.zeros((1, native.TOKEN_WIDTH), np.int32)
                poison[0, 0], poison[0, 5], poison[0, N0] = TOKEN_POWER, 1, 99
                padded = torch.cat([real, torch.from_numpy(poison).unsqueeze(0)], dim=1)
                other, mask = grown.power_summary(padded, one_count)
                torch.testing.assert_close(other[:, :actual.shape[1]], actual, rtol=0, atol=0)
                if not torch.equal(mask[:, :actual.shape[1]], actual_present) or mask[:, -1].any():
                    raise AssertionError('padding changed owner presence')
                player_pools += int(present[0])
                enemy_pools += sum(bool(present[o]) for o in enemy_owners)
            choice.fill(-1)
            choice[live] = before[0].argmax(1).numpy().astype(np.int32)
        observations += len(live)
        batch.step(choice, g, tokens, actions, counts, status, steps)
    if not powers or not player_pools or not enemy_pools:
        raise AssertionError('real diagnostic did not exercise both player and enemy powers')
    torch.manual_seed(4)
    torch.nn.init.normal_(grown.power_context.weight, std=.003)
    torch.nn.init.normal_(grown.power_context.bias, std=.003)
    # Ensure the new path changes outputs; a no-op could pass native parity.
    with torch.no_grad():
        if all(torch.equal(a, b) for a, b in zip(model(*batch_args), grown(*batch_args))):
            raise AssertionError('nonzero context layer did not change outputs')
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    save(grown, args.out, actor_filter=int(blob.get('actor_filter', 0)), diagnostic_only=True)
    print(json.dumps({'passed': True, 'observations': observations, 'power_rows': powers,
                      'player_pools': player_pools, 'enemy_pools': enemy_pools,
                      'existing_tensors_preserved': True, 'zero_growth_outputs_identical': True,
                      'independent_pools': True, 'reorder_and_padding': True,
                      'nonzero_checkpoint': args.out}))


if __name__ == '__main__':
    main()
