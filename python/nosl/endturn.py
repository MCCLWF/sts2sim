"""A learned public EndTurn gate with the entire original X1 frozen.

This experimental format has an explicit loader: a generic ActorCritic
checkpoint loader must not silently drop the gate. Only greedy evaluation is
defined here; its logits do not give the gate's Bernoulli probabilities.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import time

import numpy as np
import torch
from torch import nn
import torch.nn.functional as F

from .branch_credit import KEYS, inputs
from .episodic_strategy import START_SHA, digest
from .model import load, numeric, N0, TOKEN_ENEMY
from .teacher_distill import sha

DATA_SHA = '8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29'
FORMAT = 'sts2.nosl.endturn.v1'
FEATURES = ('energy', 'stars', 'block', 'shown_damage', 'unblocked',
            'excess_block', 'hand_count', 'legal_play_rows', 'legal_potion_rows')
HEAD_WIDTH, EPOCHS, MINIBATCH = 64, 128, 128
G_IN_COMBAT, END_TURN, PLAY_CARD, USE_POTION = 11, 3, 2, 10


def write(path, data):
    Path(path).write_text(json.dumps(data, indent=2) + '\n')


def baseline(path, device='cpu'):
    if sha(path) != START_SHA:
        raise ValueError('EndTurn experiment requires the exact frozen X1')
    model, blob = load(path, device)
    if model.config != {'width': 128, 'layers': 3, 'heads': 4, 'feedforward': 256,
                        'schema': 1, 'features': 3} or blob.get('actor_filter') != 1:
        raise ValueError('unexpected X1 public contract')
    return model.eval().requires_grad_(False)


def scope(global_, actions, counts):
    legal = torch.arange(actions.shape[1], device=actions.device)[None] < counts[:, 1:2]
    end = legal & (actions[..., 0] == END_TURN)
    other = legal & ~end
    active = (global_[:, G_IN_COMBAT] != 0) & (end.sum(1) == 1) & other.any(1)
    return active, end, other


def gate_features(run, global_, tokens, actions, counts):
    """All additional numbers are arithmetic on the ordinary public tables."""
    valid = torch.arange(tokens.shape[1], device=tokens.device)[None] < counts[:, :1]
    alive = valid & (tokens[..., 0] == TOKEN_ENEMY) & (tokens[..., N0 + 7] > 0)
    shown = (tokens[..., N0 + 3].clamp(min=0) *
             tokens[..., N0 + 4].clamp(min=1) * alive).sum(1)
    block = global_[:, 9]
    legal = torch.arange(actions.shape[1], device=actions.device)[None] < counts[:, 1:2]
    numbers = torch.stack((global_[:, 12], global_[:, 14], block, shown,
                           (shown - block).clamp(min=0), (block - shown).clamp(min=0),
                           global_[:, 17], (legal & (actions[..., 0] == PLAY_CARD)).sum(1),
                           (legal & (actions[..., 0] == USE_POTION)).sum(1)), dim=1)
    return torch.cat((run, numeric(numbers).to(run.dtype)), dim=1)


def decode(old, stop, global_, actions, counts):
    active, end, other = scope(global_, actions, counts)
    highest = old.masked_fill(~other, float('-inf')).max(1).values
    # A parked/only-EndTurn row has no other action; never subtract -inf.
    highest = torch.where(torch.isfinite(highest), highest, torch.zeros_like(highest))
    changed = torch.where(end, stop[:, None], old - highest[:, None])
    return torch.where(active[:, None], changed, old)


class EndTurnPolicy(nn.Module):
    def __init__(self, base, prior=89 / 1088):
        super().__init__()
        if not 0 < prior < 1:
            raise ValueError('gate prior must be between zero and one')
        self.base = base.eval().requires_grad_(False)
        self.head = nn.Sequential(nn.Linear(base.config['width'] + 2 * len(FEATURES), HEAD_WIDTH),
                                  nn.ReLU(), nn.Linear(HEAD_WIDTH, 1))
        self.head.to(next(base.parameters()).device)
        nn.init.zeros_(self.head[-1].weight)
        nn.init.constant_(self.head[-1].bias, math.log(prior / (1 - prior)))

    def forward(self, global_, tokens, actions, counts):
        global_, tokens, actions, counts = (t.long() for t in (global_, tokens, actions, counts))
        run, entities = self.base.encode(global_, tokens, counts)
        old = self.base.score_actions(self.base.action_features(run, entities, actions), counts)
        stop = self.head(gate_features(run, global_, tokens, actions, counts)).squeeze(-1)
        return decode(old, stop, global_, actions, counts), self.base.value(run).squeeze(-1)


def load_gate(path, start, device='cpu'):
    blob = torch.load(path, map_location=device, weights_only=False)
    if blob.get('format') != FORMAT or blob.get('schema') != 1 or blob.get('actor_filter') != 1:
        raise ValueError('unknown EndTurn checkpoint contract')
    if blob.get('baseline_checkpoint_sha256') != START_SHA or blob.get('features') != list(FEATURES):
        raise ValueError('EndTurn checkpoint baseline/features differ')
    model = EndTurnPolicy(baseline(start, device))
    model.head.load_state_dict(blob['head_state'], strict=True)
    if not all(torch.isfinite(t).all() for t in model.head.state_dict().values()):
        raise ValueError('nonfinite EndTurn weights')
    if digest(model.base) != blob.get('frozen_parameter_sha256'):
        raise ValueError('EndTurn frozen parameters differ')
    return model.eval(), blob


def prepare(start, directory, out, deadline):
    directory, out = Path(directory), Path(out)
    out.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((directory / 'dataset.json').read_text())
    if not manifest['export_gate_passed'] or manifest['actor_filter'] != 1 or \
            manifest['checkpoint_sha256'] != START_SHA or manifest['public_tables_sha256'] != DATA_SHA or \
            sha(directory / 'public-tables.npz') != DATA_SHA:
        raise ValueError('frozen teacher data contract failed')
    model = baseline(start)
    with np.load(directory / 'public-tables.npz', allow_pickle=False) as blob:
        teacher = {k: blob[k] for k in (*KEYS, 'label', 'held', 'group')}
        anchors = {k: blob['anchor_' + k] for k in (*KEYS, 'held')}
    result = {}
    began = time.monotonic()
    for prefix, tables in (('', teacher), ('anchor_', anchors)):
        cache, active, old_values = [], [], []
        for first in range(0, len(tables['counts']), MINIBATCH):
            if time.monotonic() >= deadline:
                raise TimeoutError('registered cache/training deadline')
            ids = np.arange(first, min(first + MINIBATCH, len(tables['counts'])))
            batch = tuple(t.long() for t in inputs(tables, ids, 'cpu'))
            with torch.no_grad():
                run, entities = model.encode(batch[0], batch[1], batch[3])
                feature = gate_features(run, *batch)
                logits = model.score_actions(model.action_features(run, entities, batch[2]), batch[3])
                ordinary, values = model(*batch)
                assert torch.equal(logits, ordinary), 'cached original action path differs'
                assert torch.equal(model.value(run).squeeze(-1), values)
                assert torch.isfinite(feature).all()
                active.append(scope(batch[0], batch[2], batch[3])[0].numpy())
                cache.append(feature.numpy());old_values.append(values.numpy())
        result[prefix + 'features'] = np.concatenate(cache)
        result[prefix + 'eligible'] = np.concatenate(active)
        result[prefix + 'value'] = np.concatenate(old_values)
        for key in ('held', 'counts'):
            result[prefix + key] = tables[key]
    label = teacher['label']
    assert np.all((label >= 0) & (label < teacher['counts'][:, 1]))
    result['target'] = (teacher['actions'][np.arange(len(label)), label, 0] == END_TURN)
    result['group'] = teacher['group']
    np.savez_compressed(out / 'cache.npz', **result)
    stats = {}
    for name, chosen in (('train', result['eligible'] & ~result['held']),
                          ('held', result['eligible'] & result['held'])):
        stats[name] = {'rows': int(chosen.sum()), 'positive': int(result['target'][chosen].sum()),
                       'roots': len(set(result['group'][chosen].tolist()))}
    assert stats == {'train': {'rows': 1088, 'positive': 89, 'roots': 55},
                     'held': {'rows': 28, 'positive': 2, 'roots': 2}}
    record = {'start_sha256': START_SHA, 'dataset_sha256': DATA_SHA,
              'cache_sha256': sha(out / 'cache.npz'), 'frozen_parameter_sha256': digest(model),
              'feature_names': list(FEATURES), 'feature_width': 146, 'statistics': stats,
              'cache_forward_identical': True, 'seconds': time.monotonic() - began}
    write(out / 'cache.json', record)
    return record


def binary_metrics(score, target):
    prediction = score > 0
    pos, neg = target.bool(), ~target.bool()
    tp, tn = int((prediction & pos).sum()), int((~prediction & neg).sum())
    positives, negatives = int(pos.sum()), int(neg.sum())
    recall = tp / positives if positives else None
    specificity = tn / negatives if negatives else None
    return {'rows': len(target), 'positive': positives, 'true_positive': tp, 'true_negative': tn,
            'false_positive': int((prediction & neg).sum()), 'false_negative': int((~prediction & pos).sum()),
            'recall': recall, 'specificity': specificity,
            'balanced_accuracy': (recall + specificity) / 2 if recall is not None and specificity is not None else None,
            'bce': float(F.binary_cross_entropy_with_logits(score, target.float()))}


def train(start, cache, out, seed, deadline):
    cache, out = Path(cache), Path(out)
    out.mkdir(parents=True, exist_ok=False)
    metadata = json.loads((cache / 'cache.json').read_text())
    if metadata['start_sha256'] != START_SHA or metadata['dataset_sha256'] != DATA_SHA or \
            metadata['cache_sha256'] != sha(cache / 'cache.npz'):
        raise ValueError('EndTurn cache contract failed')
    with np.load(cache / 'cache.npz', allow_pickle=False) as source:
        tables = {k: source[k] for k in source.files}
    torch.manual_seed(seed)
    model = EndTurnPolicy(baseline(start))
    frozen = digest(model.base)
    assert frozen == metadata['frozen_parameter_sha256']
    optimizer = torch.optim.Adam(model.head.parameters(), lr=.001)
    features = torch.from_numpy(tables['features'])
    targets = torch.from_numpy(tables['target']).float()
    rows = np.flatnonzero(tables['eligible'] & ~tables['held'])
    valid = np.flatnonzero(tables['eligible'] & tables['held'])
    config = {'seed': seed, 'epochs': EPOCHS, 'minibatch': MINIBATCH, 'lr': .001, 'grad_norm': 1.,
              'loss': 'unweighted_binary_cross_entropy', 'initial_prior': 89 / 1088,
              'start_sha256': START_SHA, 'dataset_sha256': DATA_SHA, 'cache_sha256': metadata['cache_sha256'],
              'train_rows': len(rows), 'held_rows': len(valid), 'device': 'cpu', 'precision': 'float32'}
    write(out / 'config.json', config)
    rng = np.random.default_rng(seed)
    began, updates = time.monotonic(), 0
    try:
        for epoch in range(EPOCHS + 1):
            if epoch:
                order = rng.permutation(rows)
                for first in range(0, len(order), MINIBATCH):
                    if time.monotonic() >= deadline:
                        raise TimeoutError('registered cache/training deadline')
                    chosen = order[first:first + MINIBATCH]
                    loss = F.binary_cross_entropy_with_logits(model.head(features[chosen]).squeeze(-1), targets[chosen])
                    if not torch.isfinite(loss):
                        raise ValueError('nonfinite gate loss')
                    optimizer.zero_grad(set_to_none=True);loss.backward()
                    nn.utils.clip_grad_norm_(model.head.parameters(), 1., error_if_nonfinite=True)
                    optimizer.step();updates += 1
            with torch.no_grad():
                score = model.head(features).squeeze(-1)
                record = {'epoch': epoch, 'updates': updates, 'seconds': time.monotonic() - began,
                          'train': binary_metrics(score[rows], targets[rows]),
                          'held': binary_metrics(score[valid], targets[valid])}
            with (out / 'training.jsonl').open('a') as stream:
                stream.write(json.dumps(record) + '\n')
            if epoch in (0, EPOCHS):
                print(json.dumps({'seed': seed, **record}), flush=True)
    except Exception as error:
        write(out / 'error.json', {'error': type(error).__name__ + ': ' + str(error), 'updates': updates})
        torch.save({'head': model.head.state_dict(), 'optimizer': optimizer.state_dict()}, out / 'failed-state.pt')
        raise
    assert digest(model.base) == frozen
    assert not any(p.requires_grad or p.grad is not None for p in model.base.parameters())
    assert all(torch.isfinite(t).all() for t in model.head.state_dict().values())
    checkpoint = {'format': FORMAT, 'schema': 1, 'actor_filter': 1, 'features': list(FEATURES),
                  'baseline_checkpoint_sha256': START_SHA, 'frozen_parameter_sha256': frozen,
                  'head_state': model.head.state_dict(), 'training': config, 'final_epoch': EPOCHS}
    torch.save(checkpoint, out / 'checkpoint.pt')
    torch.save(optimizer.state_dict(), out / 'optimizer.pt')
    loaded, _ = load_gate(out / 'checkpoint.pt', start)
    assert digest(loaded.head) == digest(model.head)
    result = {'completed': True, 'frozen_parameters_unchanged': True, 'frozen_parameter_sha256': frozen,
              'updates': updates, 'seconds': time.monotonic() - began, 'final': record,
              'checkpoint_sha256': sha(out / 'checkpoint.pt'), 'optimizer_sha256': sha(out / 'optimizer.pt'),
              'fitting_gate_passed': bool(record['train']['balanced_accuracy'] >= .8 and record['train']['recall'] >= .7)}
    write(out / 'training-result.json', result)
    return result
