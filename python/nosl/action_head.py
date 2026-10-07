"""Cache public frozen encodings and fit only the action scoring head.

No new game data is collected here. Every non-scoring parameter remains
byte-identical to the frozen student, including its value encoder/head.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
import torch.nn.functional as F

from .branch_credit import KEYS, inputs, kl
from .model import grow, load, save
from .teacher_distill import sha

START_SHA = '6fb5f1955a11d55175d348659460ce459017baa018f7cf9242a671ba19266703'
DATA_SHA = '8d30a2c7e3ce51c7c0bb73551d062671a4892b5f5d3f241959864d0d74599f29'


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def frozen_digest(model):
    digest = hashlib.sha256()
    for name, value in model.state_dict().items():
        if name.startswith(('policy.', 'action_interaction.')):
            continue
        digest.update(name.encode())
        digest.update(value.detach().cpu().contiguous().numpy().tobytes())
    return digest.hexdigest()


def prepare(start, data, out, device='cpu'):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    assert not (out / 'cache.json').exists(), 'refuse to replace frozen cache'
    assert sha(start) == START_SHA
    data = Path(data)
    manifest = json.loads((data / 'dataset.json').read_text())
    assert manifest['export_gate_passed'] and manifest['actor_filter'] == 1
    assert manifest['checkpoint_sha256'] == START_SHA and manifest['public_tables_sha256'] == DATA_SHA
    assert sha(data / 'public-tables.npz') == DATA_SHA
    with np.load(data / 'public-tables.npz', allow_pickle=False) as original:
        teacher = {key: original[key] for key in (*KEYS, 'label', 'held', 'group')}
        anchors = {key: original['anchor_' + key] for key in (*KEYS, 'held')}
    model, _ = load(start, device)
    model.eval()
    cached = {}
    worst = 0.
    began = time.monotonic()
    for prefix, tables in (('', teacher), ('anchor_', anchors)):
        features, logits = [], []
        for first in range(0, len(tables['counts']), 128):
            ids = np.arange(first, min(first + 128, len(tables['counts'])))
            batch = tuple(t.long() for t in inputs(tables, ids, device))
            with torch.no_grad():
                run, entities = model.encode(batch[0], batch[1], batch[3])
                feature = model.action_features(run, entities, batch[2], True)
                score = model.score_actions(feature, batch[3])
                normal, _ = model(*batch)
                mask = torch.isfinite(normal)
                worst = max(worst, float((normal[mask] - score[mask]).abs().max()))
                assert torch.equal(normal, score), f'cached path differs from ordinary forward: max={worst}, batch={first}'
                assert torch.isfinite(feature).all()
            # inputs() trims each batch to its largest live action count.
            # Retain the dataset's common padded width before concatenating;
            # padding carries no action and its baseline logit remains -inf.
            padding = tables['actions'].shape[1] - feature.shape[1]
            features.append(np.pad(feature.cpu().numpy(), ((0, 0), (0, padding), (0, 0))))
            logits.append(np.pad(score.cpu().numpy(), ((0, 0), (0, padding)), constant_values=-np.inf))
        cached[prefix + 'features'] = np.concatenate(features)
        cached[prefix + 'baseline'] = np.concatenate(logits)
        for key in ('counts', 'held'):
            cached[prefix + key] = tables[key]
        cached[prefix + 'kind'] = tables['global'][:, 1]
    cached.update(label=teacher['label'], group=teacher['group'])
    np.savez_compressed(out / 'cache.npz', **cached)
    result = {'start_sha256': START_SHA, 'data_sha256': DATA_SHA, 'cache_sha256': sha(out / 'cache.npz'),
              'frozen_parameter_sha256': frozen_digest(model), 'encoder_device': device,
              'teacher_rows': len(teacher['label']), 'anchor_rows': len(anchors['counts']),
              'train_roots': len(set(teacher['group'][~teacher['held']])),
              'held_roots': len(set(teacher['group'][teacher['held']])),
              'cache_forward_max_difference': worst, 'seconds': time.monotonic() - began}
    assert result['train_roots'] == 55 and result['held_roots'] == 2
    write(out / 'cache.json', result)
    return result


def train(start, cache, out, seed=1, interaction=False, epochs=64, device='cpu'):
    out, cache = Path(out), Path(cache)
    out.mkdir(parents=True, exist_ok=True)
    assert not (out / 'config.json').exists(), 'refuse to restart a head learning arm'
    manifest = json.loads((cache / 'cache.json').read_text())
    assert sha(start) == START_SHA and manifest['start_sha256'] == START_SHA
    assert manifest['data_sha256'] == DATA_SHA and sha(cache / 'cache.npz') == manifest['cache_sha256']
    with np.load(cache / 'cache.npz', allow_pickle=False) as loaded:
        data = {key: loaded[key] for key in loaded.files}
    model, original = load(start, device)
    model = grow(model, model.features, '', False, action_interaction=interaction)
    assert frozen_digest(model) == manifest['frozen_parameter_sha256']
    model.requires_grad_(False)
    model.policy.requires_grad_(True)
    if model.action_interaction is not None:
        model.action_interaction.requires_grad_(True)
    initial_frozen = frozen_digest(model)
    torch.manual_seed(seed)
    rng = np.random.default_rng(seed)
    optimizer = torch.optim.Adam([p for p in model.parameters() if p.requires_grad], lr=3e-4)
    tensors = {key: torch.from_numpy(value).to(device) for key, value in data.items()}
    rows, held = np.flatnonzero(~data['held']), np.flatnonzero(data['held'])
    anchor_rows, anchor_held = np.flatnonzero(~data['anchor_held']), np.flatnonzero(data['anchor_held'])
    groups = collections.Counter(int(data['group'][i]) for i in rows)
    weights = torch.ones(len(data['label']), device=device)
    for index in rows:
        weights[index] = len(rows) / (len(groups) * groups[int(data['group'][index])])
    config = {'start_sha256': START_SHA, 'data_sha256': DATA_SHA, 'cache_sha256': manifest['cache_sha256'],
              'seed': seed, 'action_interaction': interaction, 'epochs': epochs, 'lr': 3e-4,
              'minibatch': 128, 'kl_coef': .5, 'grad_norm': 1., 'device': device,
              'train_rows': len(rows), 'held_rows': len(held), 'train_roots': len(groups),
              'frozen_parameter_sha256': initial_frozen,
              'trainable_parameters': [name for name, p in model.named_parameters() if p.requires_grad]}
    write(out / 'config.json', config)

    def score(ids, prefix=''):
        return model.score_actions(tensors[prefix + 'features'][ids], tensors[prefix + 'counts'][ids])

    def metrics(ids, prefix='', labels=False):
        total_ce = hits = total_kl = seen = 0
        kinds = collections.defaultdict(lambda: {'rows': 0, 'same_action': 0, 'kl': 0.})
        with torch.no_grad():
            for first in range(0, len(ids), 128):
                chunk = ids[first:first + 128]
                now, old = score(chunk, prefix), tensors[prefix + 'baseline'][chunk]
                total_kl += float(kl(now, old)) * len(chunk)
                seen += len(chunk)
                if labels:
                    target = tensors['label'][chunk]
                    total_ce += float(F.cross_entropy(now, target)) * len(chunk)
                    hits += int((now.argmax(-1) == target).sum())
                for kind in np.unique(data[prefix + 'kind'][chunk]):
                    selected = data[prefix + 'kind'][chunk] == kind
                    entry = kinds[int(kind)]
                    entry['rows'] += int(selected.sum())
                    entry['same_action'] += int((now[selected].argmax(-1) == old[selected].argmax(-1)).sum())
                    entry['kl'] += float(kl(now[selected], old[selected])) * int(selected.sum())
        return {'rows': seen, 'ce': total_ce / seen if labels and seen else None,
                'agreement': hits / seen if labels and seen else None,
                'kl': total_kl / seen if seen else None,
                'decision_kinds': {kind: {'rows': v['rows'], 'kl': v['kl'] / v['rows'],
                    'baseline_action_agreement': v['same_action'] / v['rows']} for kind, v in kinds.items()}}

    began = time.monotonic()
    for epoch in range(epochs + 1):
        if epoch:
            for first in range(0, len(rows), 128):
                if first == 0:
                    order = rng.permutation(rows)
                chunk = order[first:first + 128]
                now = score(chunk)
                objective = (F.cross_entropy(now, tensors['label'][chunk], reduction='none') * weights[chunk]).mean()
                sampled = rng.choice(anchor_rows, min(128, len(anchor_rows)), replace=False)
                objective += .5 * (kl(now, tensors['baseline'][chunk]) +
                                   kl(score(sampled, 'anchor_'), tensors['anchor_baseline'][sampled]))
                assert torch.isfinite(objective), 'nonfinite scoring objective'
                optimizer.zero_grad(set_to_none=True)
                objective.backward()
                torch.nn.utils.clip_grad_norm_([p for p in model.parameters() if p.requires_grad], 1., error_if_nonfinite=True)
                optimizer.step()
        record = {'epoch': epoch, 'seconds': time.monotonic() - began, 'train': metrics(rows, labels=True),
                  'held': metrics(held, labels=True), 'anchors': metrics(anchor_rows, 'anchor_'),
                  'held_anchors': metrics(anchor_held, 'anchor_')}
        with (out / 'training.jsonl').open('a') as stream:
            stream.write(json.dumps(record) + '\n')
        if epoch in (0, epochs):
            print(json.dumps(record), flush=True)
    assert frozen_digest(model) == initial_frozen, 'non-scoring parameter changed'
    assert all(torch.isfinite(p).all() for p in model.parameters()), 'nonfinite learned parameters'
    extra = {key: original[key] for key in ('iteration', 'decisions', 'actor_filter', 'reward') if key in original}
    save(model, out / 'checkpoint.pt', **extra, action_head_training=config, teacher_epoch=epochs)
    result = {'frozen_parameters_unchanged': True, 'frozen_parameter_sha256': frozen_digest(model),
              'updates': epochs * ((len(rows) + 127) // 128), 'seconds': time.monotonic() - began,
              'checkpoint_sha256': sha(out / 'checkpoint.pt'), 'final': record}
    write(out / 'training-result.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('cache', 'train'))
    parser.add_argument('--start', required=True)
    parser.add_argument('--data')
    parser.add_argument('--cache')
    parser.add_argument('--out', required=True)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--interaction', action='store_true')
    parser.add_argument('--epochs', type=int, default=64)
    parser.add_argument('--device', default='cpu')
    args = parser.parse_args()
    torch.set_num_threads(1)
    result = prepare(args.start, args.data, args.out, args.device) if args.mode == 'cache' else \
        train(args.start, args.cache, args.out, args.seed, args.interaction, args.epochs, args.device)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
