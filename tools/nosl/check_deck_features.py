#!/usr/bin/env python3
"""Validate a features-3 warm start, deck bag multiplicity and the padding boundary.

Uses real Silent A10 observations and the checkpoint's printed-card table.
Writes a features-4 checkpoint with a deliberately nonzero new layer for the
existing PyTorch/native forward comparison, rather than testing a zero layer
that could hide a missing native implementation.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import torch

from nosl import native
from nosl.model import CARD_NUMBERS, N0, TOKEN_DECK, grow, load, save


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint', required=True)
    p.add_argument('--out', required=True)
    args = p.parse_args()
    torch.set_num_threads(1)
    model, blob = load(args.checkpoint, 'cpu')
    if model.features != 3:
        raise ValueError('this diagnostic expects the registered features-3 starting policy')
    grown = grow(model, 4, '', False)
    model.eval()
    grown.eval()
    for key, before in model.state_dict().items():
        if not torch.equal(before, grown.state_dict()[key]):
            raise AssertionError('growth changed an existing tensor: ' + key)

    n, T, A = 4, 192, 160
    batch = native.Batch(n, T, A, 30000)
    batch.set_filter(int(blob.get('actor_filter', 0)))
    for j in range(n):
        if batch.reset(j, f'NQ4LCHK{j:04d}', 10) != native.RUNNING:
            raise AssertionError('registered real-observation seed failed to reset')
    g = np.zeros((n, native.GLOBAL_WIDTH), np.int32)
    tokens = np.zeros((n, T, native.TOKEN_WIDTH), np.int32)
    actions = np.zeros((n, A, native.ACTION_WIDTH), np.int32)
    counts = np.zeros((n, 2), np.int32)
    status = np.zeros(n, np.int32)
    steps = np.zeros(n, np.int32)
    choice = np.full(n, -1, np.int32)
    batch.step(choice, g, tokens, actions, counts, status, steps)
    observations = bag_checks = combat_checks = 0
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
            for a, b in zip(before, after):
                if not torch.equal(a, b):
                    raise AssertionError('features growth changed starting logits/value')
            summary = grown.deck_summary(batch_args[1], batch_args[3])
            for row, slot in enumerate(live):
                deck = [t for t in tokens[slot, :counts[slot, 0]] if t[0] == TOKEN_DECK and t[1] > 0]
                if deck:
                    # Expand each bag row into copies before taking means. This
                    # catches accidentally counting distinct rows as cards.
                    cards = [(int(t[1]), int(np.clip(t[N0], 0, 1)))
                             for t in deck for _ in range(max(0, int(t[N0 + 1])))]
                    numbers = torch.stack([model.card_numbers[c, level] for c, level in cards])
                    flags = torch.stack([model.card_flags[c, level] for c, level in cards])
                    expected = torch.cat([numbers.sum(0), numbers.mean(0), flags.sum(0),
                                          torch.tensor([len(cards), sum(level > 0 for _, level in cards)])])
                    torch.testing.assert_close(summary[row], expected, rtol=0, atol=1e-6)
                    # Even a duplicate valid-looking Deck row in unused padding
                    # must not leak into the summary.
                    real = torch.from_numpy(tokens[slot:slot + 1, :counts[slot, 0]].copy())
                    poisoned = torch.cat([real, torch.from_numpy(np.array(deck[:1], np.int32)).unsqueeze(0)], dim=1)
                    one_count = torch.from_numpy(counts[slot:slot + 1])
                    torch.testing.assert_close(grown.deck_summary(poisoned, one_count)[0], expected, rtol=0, atol=1e-6)
                    shuffled = real[:, torch.randperm(real.shape[1])]
                    torch.testing.assert_close(grown.deck_summary(shuffled, one_count)[0], expected, rtol=0, atol=1e-6)
                    bag_checks += 1
                else:
                    if torch.count_nonzero(summary[row]):
                        raise AssertionError('offers or combat piles were treated as the permanent deck')
                    if g[slot, 11]:
                        combat_checks += 1
            choice.fill(-1)
            choice[live] = before[0].argmax(1).numpy().astype(np.int32)
        observations += len(live)
        batch.step(choice, g, tokens, actions, counts, status, steps)
    if not bag_checks or not combat_checks:
        raise AssertionError('the diagnostic did not reach both deck and combat observations')
    # Exercise new nonzero weights in the independent existing native checker.
    torch.manual_seed(4)
    torch.nn.init.normal_(grown.deck_features.weight, std=0.003)
    torch.nn.init.normal_(grown.deck_features.bias, std=0.003)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    save(grown, args.out, actor_filter=int(blob.get('actor_filter', 0)), diagnostic_only=True)
    print(json.dumps({'passed': True, 'observations': observations, 'bag_checks': bag_checks,
                      'combat_checks': combat_checks, 'existing_tensors_preserved': True,
                      'zero_growth_outputs_identical': True, 'nonzero_checkpoint': args.out}))


if __name__ == '__main__':
    main()
