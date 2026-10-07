"""Capacity-matched public EndTurn residuals with an optional action context."""
from __future__ import annotations

import json
from pathlib import Path
import time

import numpy as np
import torch
from torch import nn
import torch.nn.functional as F

from .branch_credit import KEYS, inputs
from .endturn import (FEATURES, START_SHA, binary_metrics, decode, gate_features,
                      load_gate, scope, write)
from .endturn_credit import HEAD_SHA, anchor_kl, public_digest
from .episodic_strategy import digest
from .teacher_distill import sha

FORMAT = 'sts2.nosl.endturn-action.v1'
DATA_SHA = '0cc7ca9267b631dac95997ca1b85a1064e7ced893f0232ca8d6d926986a3c557'
MANIFEST_SHA = '724d51d67ed89761939fd2d90477e20fc1cb431bafb7393361b6fb3ef2878878'
EPOCHS, MINIBATCH = 128, 128


def source(start, heads, seed):
    path = Path(heads) / f'E{seed}/checkpoint.pt'
    if sha(path) != HEAD_SHA[seed]:raise ValueError('wrong frozen initial head')
    return load_gate(path, start, 'cpu')[0]


def public_features(base, global_, tokens, actions, counts):
    global_, tokens, actions, counts = (t.long() for t in (global_, tokens, actions, counts))
    run, entities = base.encode(global_, tokens, counts)
    encoded = base.action_features(run, entities, actions)
    old = base.score_actions(encoded, counts)
    active, end, other = scope(global_, actions, counts)
    best = old.masked_fill(~other, float('-inf')).argmax(1)
    selected = encoded[torch.arange(len(best), device=best.device), best, base.config['width']:]
    selected = torch.where(other.any(1)[:, None], selected, torch.zeros_like(selected))
    return gate_features(run, global_, tokens, actions, counts), selected, old, base.value(run).squeeze(-1)


class ActionEndTurnPolicy(nn.Module):
    def __init__(self, initial, action_context, seed):
        super().__init__()
        self.base, self.head = initial.base, initial.head
        self.action_context, self.seed = bool(action_context), seed
        torch.manual_seed(seed)
        self.residual = nn.Sequential(nn.Linear(274, 64), nn.ReLU(), nn.Linear(64, 1))
        nn.init.zeros_(self.residual[-1].weight);nn.init.zeros_(self.residual[-1].bias)

    def stop(self, gate, action):
        selected = action if self.action_context else torch.zeros_like(action)
        return (self.head(gate) + self.residual(torch.cat((gate, selected), dim=1))).squeeze(-1)

    def forward(self, global_, tokens, actions, counts):
        gate, action, old, value = public_features(self.base, global_, tokens, actions, counts)
        return decode(old, self.stop(gate, action), global_, actions, counts), value

    def learned_parameters(self):return list(self.head.parameters()) + list(self.residual.parameters())


def initial(start, heads, seed, action_context):
    return ActionEndTurnPolicy(source(start, heads, seed), action_context, seed).eval()


def load(path, start, heads):
    blob = torch.load(path, map_location='cpu', weights_only=False)
    if blob.get('format') != FORMAT or blob.get('schema') != 1 or blob.get('actor_filter') != 1:
        raise ValueError('unsupported public action gate format')
    seed, context = blob.get('initial_seed'), blob.get('action_context')
    if seed not in (1, 3) or type(context) is not bool:
        raise ValueError('invalid action gate source or mode')
    if blob.get('baseline_checkpoint_sha256') != START_SHA or blob.get('feature_names') != list(FEATURES):
        raise ValueError('public action gate baseline/features differ')
    if blob.get('initial_head_sha256') != HEAD_SHA[seed] or blob.get('action_feature_width') != 128:
        raise ValueError('public action gate source/action encoding differs')
    model = initial(start, heads, seed, context)
    model.head.load_state_dict(blob['head_state'], strict=True)
    model.residual.load_state_dict(blob['residual_state'], strict=True)
    if not all(torch.isfinite(t).all() for module in (model.head, model.residual) for t in module.state_dict().values()):
        raise ValueError('nonfinite public action gate weights')
    if digest(model.base) != blob.get('frozen_parameter_sha256'):
        raise ValueError('frozen public encoder differs')
    return model.eval(), blob


def prepare(start, heads, directory, out, deadline):
    directory, out = Path(directory), Path(out)
    if sha(directory / 'manifest.json') != MANIFEST_SHA or sha(directory / 'data/public-roots.npz') != DATA_SHA:
        raise ValueError('sealed actual-terminal data differs')
    manifest = json.loads((directory / 'manifest.json').read_text())
    for row in manifest['files']:
        file = directory / row['path']
        if file.stat().st_size != row['bytes'] or sha(file) != row['sha256']:
            raise ValueError('changed sealed input: ' + row['path'])
    audit = json.loads((directory / 'data-audit.json').read_text())
    if not audit['independent_verification'] or not audit['data_gate_passed']:
        raise ValueError('actual-terminal source data is not qualified')
    roots = [json.loads(s) for s in (directory / 'data/roots.jsonl').read_text().splitlines()]
    out.mkdir(parents=True, exist_ok=False)
    base = source(start, heads, 1).base
    gate, action = [], []
    with np.load(directory / 'data/public-roots.npz', allow_pickle=False) as raw:
        tables = {key: raw[key] for key in KEYS}
        assert len(roots) == len(tables['counts']) == 450
        for i, root in enumerate(roots):assert public_digest({k: tables[k][i] for k in KEYS}) == root['public_sha256']
        for first in range(0, len(roots), MINIBATCH):
            if time.monotonic() >= deadline:raise TimeoutError('registered public cache/training deadline')
            ids = np.arange(first, min(first + MINIBATCH, len(roots)))
            with torch.no_grad():
                feature, selected, _, _ = public_features(base, *inputs(tables, ids, 'cpu'))
                assert torch.isfinite(feature).all() and torch.isfinite(selected).all()
            gate.append(feature.numpy());action.append(selected.numpy())
    with np.load(directory / 'learning/cache/cache.npz', allow_pickle=False) as prior:
        data = {k: prior[k] for k in ('complete', 'held', 'direction', 'target', 'weight')}
        assert np.array_equal(np.concatenate(gate), prior['features'])
    data.update(gate=np.concatenate(gate), action=np.concatenate(action))
    np.savez_compressed(out / 'cache.npz', **data)
    result = {'data_sha256': DATA_SHA, 'sealed_manifest_sha256': MANIFEST_SHA,
              'cache_sha256': sha(out / 'cache.npz'), 'actor_input_keys': list(KEYS),
              'rows': len(roots), 'gate_width': 146, 'action_width': 128,
              'frozen_parameter_sha256': digest(base), 'all_sealed_files_verified': len(manifest['files'])}
    write(out / 'cache.json', result);return result


def train(start, heads, cache, out, seed, action_context, deadline):
    cache, out = Path(cache), Path(out);out.mkdir(parents=True, exist_ok=False)
    meta = json.loads((cache / 'cache.json').read_text())
    if sha(cache / 'cache.npz') != meta['cache_sha256']:raise ValueError('public action cache changed')
    with np.load(cache / 'cache.npz', allow_pickle=False) as raw:data = {key: raw[key] for key in raw.files}
    model = initial(start, heads, seed, action_context);frozen = digest(model.base)
    gate, action, target, weight = (torch.from_numpy(data[key]) for key in ('gate', 'action', 'target', 'weight'))
    anchor = np.flatnonzero(data['complete'] & ~data['held'])
    rows = np.flatnonzero(data['complete'] & data['direction'] & ~data['held'])
    held = np.flatnonzero(data['complete'] & data['direction'] & data['held'])
    assert len(anchor) == 352 and len(rows) == 177 and len(held) == 49
    with torch.no_grad():reference = model.stop(gate, action).detach().clone()
    config = {'seed': seed, 'action_context': action_context, 'epochs': EPOCHS, 'minibatch': MINIBATCH,
              'lr': .0003, 'grad_norm': 1., 'kl_coefficient': .1, 'train_direction_rows': len(rows),
              'held_direction_rows': len(held), 'anchor_rows': len(anchor),
              'loss': 'mean_margin_weighted_direction_BCE_plus_all_complete_train_initial_Bernoulli_KL',
              'cache_sha256': meta['cache_sha256'], 'data_sha256': DATA_SHA,
              'initial_head_sha256': HEAD_SHA[seed], 'device': 'cpu', 'precision': 'float32',
              'learned_parameters': sum(p.numel() for p in model.learned_parameters()),
              'initial_head_digest': digest(model.head), 'initial_residual_digest': digest(model.residual)}
    write(out / 'config.json', config)
    optimizer = torch.optim.Adam(model.learned_parameters(), lr=.0003)
    rng = np.random.default_rng(seed);began = time.monotonic();updates = 0
    try:
        for epoch in range(EPOCHS + 1):
            if epoch:
                order = rng.permutation(rows)
                for first in range(0, len(order), MINIBATCH):
                    if time.monotonic() >= deadline:raise TimeoutError('registered public cache/training deadline')
                    selected = order[first:first + MINIBATCH]
                    score = model.stop(gate[selected], action[selected])
                    bce = (F.binary_cross_entropy_with_logits(score, target[selected], reduction='none') * weight[selected]).mean()
                    kl = anchor_kl(model.stop(gate[anchor], action[anchor]), reference[anchor])
                    loss = bce + .1 * kl
                    if not torch.isfinite(loss):raise ValueError('nonfinite public action learning objective')
                    optimizer.zero_grad(set_to_none=True);loss.backward()
                    nn.utils.clip_grad_norm_(model.learned_parameters(), 1., error_if_nonfinite=True)
                    optimizer.step();updates += 1
            with torch.no_grad():
                score = model.stop(gate, action)
                record = {'epoch': epoch, 'updates': updates, 'seconds': time.monotonic() - began,
                          'train': binary_metrics(score[rows], target[rows]), 'held': binary_metrics(score[held], target[held]),
                          'all_train_anchor_kl': float(anchor_kl(score[anchor], reference[anchor])),
                          'weighted_bce': float((F.binary_cross_entropy_with_logits(score[rows], target[rows], reduction='none') * weight[rows]).mean())}
            with (out / 'training.jsonl').open('a') as stream:stream.write(json.dumps(record) + '\n')
            if epoch in (0, EPOCHS):print(json.dumps({'arm': ('C' if action_context else 'B') + str(seed), **record}), flush=True)
    except Exception as error:
        write(out / 'error.json', {'error': type(error).__name__ + ': ' + str(error), 'updates': updates})
        torch.save({'model': model.state_dict(), 'optimizer': optimizer.state_dict()}, out / 'failed-state.pt')
        raise
    assert digest(model.base) == frozen == meta['frozen_parameter_sha256']
    assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    blob = {'format': FORMAT, 'schema': 1, 'actor_filter': 1, 'baseline_checkpoint_sha256': START_SHA,
            'feature_names': list(FEATURES), 'action_feature_width': 128, 'initial_seed': seed,
            'action_context': action_context, 'initial_head_sha256': HEAD_SHA[seed],
            'frozen_parameter_sha256': frozen, 'head_state': model.head.state_dict(),
            'residual_state': model.residual.state_dict(), 'training': config, 'final_epoch': EPOCHS}
    torch.save(blob, out / 'checkpoint.pt');torch.save(optimizer.state_dict(), out / 'optimizer.pt')
    reloaded, _ = load(out / 'checkpoint.pt', start, heads)
    assert digest(reloaded) == digest(model)
    final = record['train']
    result = {'completed': True, 'frozen_parameter_sha256': frozen, 'updates': updates,
              'seconds': time.monotonic() - began, 'final': record,
              'checkpoint_sha256': sha(out / 'checkpoint.pt'), 'optimizer_sha256': sha(out / 'optimizer.pt'),
              'fitting_gate_passed': bool(final['balanced_accuracy'] >= .8 and final['recall'] >= .7 and final['specificity'] >= .7)}
    write(out / 'training-result.json', result);return result
