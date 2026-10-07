#!/usr/bin/env python3
"""Verify public action interactions, neutral growth and explicit native format rejection."""
import argparse
import ctypes
import json
from pathlib import Path

import numpy as np
import torch

from nosl import native
from nosl.branch_credit import inputs
from nosl.export import export
from nosl.model import grow, load, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--start', required=True)
    parser.add_argument('--data', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    assert not (out / 'result.json').exists(), 'refuse to overwrite contract evidence'
    model, blob = load(args.start)
    model.eval()
    grown = grow(model, model.features, '', False, action_interaction=True).eval()
    assert grown is not model
    assert all(torch.equal(v, grown.state_dict()[k]) for k, v in model.state_dict().items())
    with np.load(Path(args.data) / 'public-tables.npz', allow_pickle=False) as data:
        tables = {k: data[k] for k in ('global', 'tokens', 'actions', 'counts')}
    chosen = np.arange(min(128, len(tables['global'])))
    batch = inputs(tables, chosen, 'cpu')
    with torch.no_grad():
        before, after = model(*batch), grown(*batch)
        assert all(torch.equal(a, b) for a, b in zip(before, after)), 'neutral growth changed outputs'
        run, token = model.encode(batch[0], batch[1], batch[3])
        cached = model.action_features(run, token, batch[2], True)
        assert torch.equal(model.score_actions(cached, batch[3]), before[0])
        width = model.config['width']
        missing = (batch[2][..., 1] < 0) | (batch[2][..., 2] < 0)
        assert missing.any() and (cached[..., 2 * width:][missing] == 0).all()
        assert (cached[..., 2 * width:] != 0).any(), 'no real source-target pair exercised'
        # Reordering token rows and remapping the action pointers must preserve
        # the product. Ordinal owner metadata stays with its public entity.
        reverse = torch.arange(token.shape[1] - 1, -1, -1)
        remapped = batch[2].clone()
        for column in (1, 2):
            remapped[..., column] = torch.where(remapped[..., column] >= 0,
                token.shape[1] - 1 - remapped[..., column], remapped[..., column])
        assert torch.equal(cached, model.action_features(run, token[:, reverse], remapped, True))
        torch.manual_seed(4)
        torch.nn.init.normal_(grown.action_interaction.weight, std=.01)
        nonzero = grown(*batch)
        assert not torch.equal(before[0], nonzero[0]) and torch.equal(before[1], nonzero[1])
        assert torch.equal(grown.score_actions(cached, batch[3]), nonzero[0])
        padded = torch.arange(nonzero[0].shape[1])[None] >= batch[3][:, 1:2]
        assert torch.isneginf(nonzero[0][padded]).all()
        reordered = batch[1].clone()
        actions = batch[2].clone()
        for row, count in enumerate(batch[3][:, 0]):
            count = int(count)
            reordered[row, :count] = batch[1][row, :count].flip(0)
            for column in (1, 2):
                actions[row, :, column] = torch.where(actions[row, :, column] >= 0,
                    count - 1 - actions[row, :, column], actions[row, :, column])
        changed_order = grown(batch[0], reordered, actions, batch[3])
        for old, new in zip(nonzero, changed_order):
            torch.testing.assert_close(old, new, atol=2e-5, rtol=1e-5)
        extra_tokens = torch.cat([batch[1], batch[1].new_zeros(len(chosen), 7, batch[1].shape[-1])], dim=1)
        extra_actions = torch.cat([batch[2], batch[2].new_zeros(len(chosen), 5, batch[2].shape[-1])], dim=1)
        extra_padding = grown(batch[0], extra_tokens, extra_actions, batch[3])
        torch.testing.assert_close(nonzero[0], extra_padding[0][:, :nonzero[0].shape[1]], atol=2e-5, rtol=1e-5)
        torch.testing.assert_close(nonzero[1], extra_padding[1], atol=2e-5, rtol=1e-5)
        assert torch.isneginf(extra_padding[0][:, nonzero[0].shape[1]:]).all()
        separate = grow(grown, grown.features, '', True).eval()
        assert separate.value_net is not None and separate.value_net.action_interaction is None
        assert torch.equal(separate.action_interaction.weight, grown.action_interaction.weight)
        for old, new in zip(nonzero, separate(*batch)):
            assert torch.equal(old, new), 'separate critic growth changed the policy or value'
    save(grown, out / 'nonzero.pt', actor_filter=blob['actor_filter'], diagnostic_only=True)
    export(str(out / 'nonzero.pt'), str(out / 'nonzero.bin'))
    library = native._LIB
    library.nosl_policy_load.argtypes = [ctypes.c_char_p]
    library.nosl_policy_load.restype = ctypes.c_void_p
    library.nosl_policy_last_error.restype = ctypes.c_char_p
    library.nosl_policy_destroy.argtypes = [ctypes.c_void_p]
    handle = library.nosl_policy_load(str(out / 'nonzero.bin').encode())
    assert handle, (library.nosl_policy_last_error() or b'').decode()
    library.nosl_policy_destroy(handle)
    original = (out / 'nonzero.bin').read_bytes()
    rejected = []
    for name, offset, value, message in [('interaction', 10, 2, 'action-interaction'),
                                        ('power', 9, 2, 'power-context'),
                                        ('features', 8, 0, 'features')]:
        damaged = bytearray(original)
        damaged[4 * offset:4 * offset + 4] = np.int32(value).tobytes()
        path = out / (name + '-invalid.bin')
        path.write_bytes(damaged)
        handle = library.nosl_policy_load(str(path).encode())
        assert not handle and message in (library.nosl_policy_last_error() or b'').decode(), name
        rejected.append(name)
    result = {'passed': True, 'observations': len(chosen), 'old_tensors_identical': True,
              'neutral_growth_identical': True, 'nonzero_changes_logits': True, 'value_identical': True,
              'cache_forward_identical': True, 'pair_reorder_identical': True,
              'missing_entity_zero': True, 'padding_masked': True, 'encoder_reorder_close': True,
              'extra_padding_close': True, 'invalid_flags_rejected': rejected}
    result['separate_critic_growth_identical'] = True
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
