"""Offline combat updates accepted by measured full-batch public policy KL.

The router and deployed actor are unchanged. Parameter rollback is exclusively
an optimizer operation; no environment action is undone or searched.
"""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch

from .encounter_credit import collect, combat_models
from .episodic_strategy import START_SHA, digest, pack, update as original_update
from .model import save

MAX_LR = .000133 / 4
MIN_LR = .000133 / 128
KL_CAP = .01


def write(path, obj):
    path.write_text(json.dumps(obj, indent=2)+'\n')


def check_time(deadline):
    if time.monotonic() >= deadline:
        raise TimeoutError('registered update deadline')


def adam_digest(optimizer, *, include_lr=True):
    """Digest all moments, step counters and optimizer options deterministically."""
    h = hashlib.sha256()

    def add(obj):
        h.update(type(obj).__name__.encode()+b':')
        if torch.is_tensor(obj):
            a = obj.detach().cpu().contiguous().numpy()
            h.update(str(a.dtype).encode()+str(a.shape).encode()+a.tobytes())
        elif isinstance(obj, dict):
            for key in sorted(obj, key=str):
                if key == 'lr' and not include_lr:
                    continue
                add(key);add(obj[key])
        elif isinstance(obj, (list, tuple)):
            for value in obj:
                add(value)
        else:
            h.update(repr(obj).encode())

    add(optimizer.state_dict())
    return h.hexdigest()


def old_distributions(model, data, device, deadline):
    count = len(data['public'])
    if not count:
        raise ValueError('no trustworthy combat rows')
    stored = []
    for begin in range(0, count, 256):
        check_time(deadline)
        ix = np.arange(begin, min(count, begin+256))
        batch = pack(data['public'], ix, device)
        with torch.no_grad():
            logits, _ = model(*batch)
            mask = torch.arange(logits.shape[1], device=device)[None, :] < batch[3][:, 1:2]
            if not torch.isfinite(logits[mask]).all() or not torch.isneginf(logits[~mask]).all():
                raise ValueError('old public legal logits invalid')
            stored.append(logits.float().log_softmax(-1).cpu())
    return stored


def policy_change(model, data, stored, device, deadline):
    kl, changed, entropy = [], [], []
    for part, begin in enumerate(range(0, len(data['public']), 256)):
        check_time(deadline)
        ix = np.arange(begin, min(len(data['public']), begin+256))
        batch = pack(data['public'], ix, device)
        with torch.no_grad():
            logits, values = model(*batch)
            old = stored[part].to(device)
            if old.shape != logits.shape:
                raise ValueError('public action table changed during optimization')
            mask = torch.arange(logits.shape[1], device=device)[None, :] < batch[3][:, 1:2]
            if (not torch.isfinite(logits[mask]).all() or not torch.isneginf(logits[~mask]).all()
                    or not torch.isfinite(values).all()):
                raise ValueError('updated public output invalid')
            new = logits.float().log_softmax(-1)
            distance = (old.exp()*torch.where(mask, old-new, torch.zeros_like(old))).sum(-1)
            kl.extend(distance.cpu().tolist())
            changed.extend((old.argmax(-1) != new.argmax(-1)).cpu().tolist())
            entropy.extend(torch.distributions.Categorical(logits=logits.float()).entropy().cpu().tolist())
    if not np.isfinite(kl).all() or not np.isfinite(entropy).all() or min(kl) < -1e-5:
        raise ValueError('invalid full-distribution policy change')
    return {'rows': len(kl), 'exact_kl_mean': float(np.mean(kl)),
            'exact_kl_p95': float(np.quantile(kl, .95)), 'exact_kl_max': float(max(kl)),
            'argmax_changed_fraction': float(np.mean(changed)), 'entropy_mean': float(np.mean(entropy))}


def checked_update(model, optimizer, data, device, rng, deadline, *, cap=None):
    """One proposal; either retain it or restore parameters and complete Adam state."""
    if cap is not None and not (np.isfinite(cap) and cap > 0):
        raise ValueError('acceptance cap must be positive and finite')
    began = time.monotonic()
    before = digest(model);before_adam = adam_digest(optimizer, include_lr=False)
    rate = optimizer.param_groups[0]['lr']
    stored = old_distributions(model, data, device, deadline)
    model_state = copy.deepcopy(model.state_dict()) if cap is not None else None
    adam_state = copy.deepcopy(optimizer.state_dict()) if cap is not None else None
    try:
        stats = original_update(model, optimizer, data, device, rng)
        check_time(deadline)
        candidate = policy_change(model, data, stored, device, deadline)
    except (ValueError, TimeoutError):
        # An unfinished/invalid proposal is never exported as an accepted actor.
        if cap is not None:
            model.load_state_dict(model_state)
            optimizer.load_state_dict(adam_state)
            assert digest(model) == before and adam_digest(optimizer, include_lr=False) == before_adam
        raise
    proposed = digest(model);candidate_adam = adam_digest(optimizer, include_lr=False)
    accepted = cap is None or candidate['exact_kl_mean'] <= cap
    if cap is not None:
        if accepted:
            kl = candidate['exact_kl_mean']
            next_rate = rate/1.5 if kl > .008 else rate*1.5 if kl < .0025 else rate
        else:
            model.load_state_dict(model_state)
            optimizer.load_state_dict(adam_state)
            assert digest(model) == before and adam_digest(optimizer, include_lr=False) == before_adam
            next_rate = rate/2
        for group in optimizer.param_groups:
            group['lr'] = min(MAX_LR, max(MIN_LR, next_rate))
    retained = candidate if accepted else policy_change(model, data, stored, device, deadline)
    if not accepted:
        assert abs(retained['exact_kl_mean']) < 1e-7 and retained['argmax_changed_fraction'] == 0
    return {**stats, 'accepted': accepted, 'kl_cap': cap, 'candidate': candidate, 'retained': retained,
            'before_parameter_sha256': before, 'candidate_parameter_sha256': proposed,
            'after_parameter_sha256': digest(model), 'before_adam_without_lr_sha256': before_adam,
            'candidate_adam_without_lr_sha256': candidate_adam,
            'after_adam_without_lr_sha256': adam_digest(optimizer, include_lr=False),
            'next_lr': optimizer.param_groups[0]['lr'],
            'retained_optimizer_steps': stats['optimizer_steps'] if accepted else 0,
            'update_seconds_including_audit': time.monotonic()-began}


def train(checkpoint, out, device, seed, arm):
    if arm not in ('U', 'B'):
        raise ValueError('expected preregistered U or B arm')
    out = Path(out);out.mkdir(parents=True, exist_ok=False)
    torch.manual_seed(seed);rng = np.random.default_rng(seed)
    outside, combat, policy, _ = combat_models(checkpoint, device)
    frozen = digest(outside)
    optimizer = torch.optim.Adam(combat.parameters(), lr=MAX_LR if arm == 'B' else .000133)
    config = {'source_commit': subprocess.check_output(['git','rev-parse','HEAD'], text=True).strip(),
              'checkpoint_sha256': START_SHA, 'seed': seed, 'arm': arm, 'credit': 'C',
              'update_attempts': 64, 'episodes_per_update': 32, 'seconds_limit': 900, 'max_steps': 30000,
              'prefix': 'NQ4ABTRAIN', 'device': device, 'precision': 'float32', 'actor_filter': 1,
              'outside_parameter_sha256': frozen, 'outside_policy': 'frozen greedy X1',
              'gamma': 1, 'reward': 'existing floor/HP(.2/70)/boss(.3)/victory', 'epochs': 2, 'minibatch': 256, 'clip': .2, 'entropy': .05,
              'value_coef': .5, 'grad_norm': .5, 'initial_lr': optimizer.param_groups[0]['lr'],
              'kl_cap': KL_CAP if arm == 'B' else None, 'min_lr': MIN_LR if arm == 'B' else None,
              'max_lr': MAX_LR if arm == 'B' else None}
    write(out/'config.json', config)
    began = time.monotonic();deadline = began+900
    attempts = faults = decisions = rows = steps = retained_steps = updates = accepted = 0
    error = None
    with (out/'train.jsonl').open('w') as log, (out/'episodes.jsonl').open('w') as episodes:
        try:
            for iteration in range(64):
                check_time(deadline)
                seeds = [f'NQ4ABTRAIN{seed}{iteration:04d}{slot:02d}' for slot in range(32)]
                data = collect(policy, seeds, device, sample=True, credit='C', deadline=deadline)
                for row in data['outcomes']:
                    episodes.write(json.dumps(row)+'\n')
                episodes.flush()
                attempts += 32;faults += sum(not r['mc_qualified'] for r in data['outcomes'])
                decisions += data['decisions']
                if faults:
                    raise ValueError('unqualified normal training trajectory')
                stats = checked_update(combat, optimizer, data, device, rng, deadline,
                                       cap=KL_CAP if arm == 'B' else None)
                assert digest(outside) == frozen
                updates += 1;accepted += stats['accepted'];rows += stats['rows']
                steps += stats['optimizer_steps'];retained_steps += stats['retained_optimizer_steps']
                row = {'iteration': updates, 'attempts': attempts, 'faults': faults, 'decisions': decisions,
                       'wall_seconds': time.monotonic()-began, 'sample_seconds': data['seconds'],
                       'floor_mean': float(np.mean([r['floor'] for r in data['outcomes']])),
                       'victories': sum(r['end'] == 'victory' for r in data['outcomes']), **stats}
                log.write(json.dumps(row)+'\n');log.flush();print(json.dumps(row), flush=True)
        except (TimeoutError, ValueError) as failure:
            error = str(failure)
    assert digest(outside) == frozen
    status = 'complete' if updates == 64 and attempts == 2048 and not error else 'incomplete'
    save(combat, out/'checkpoint.pt', actor_filter=1, outside_checkpoint_sha256=START_SHA,
         outside_parameter_sha256=frozen, learning_scope='combat only; must deploy with frozen outside X1',
         iteration=updates, decisions=decisions, combat_rows=rows, credit='C', bounded_update=arm == 'B')
    torch.save(optimizer.state_dict(), out/'optimizer.pt')
    result = {'status': status, 'updates': updates, 'accepted_updates': accepted,
              'rejected_updates': updates-accepted, 'attempts': attempts, 'faults': faults,
              'decisions': decisions, 'combat_rows': rows, 'optimizer_steps': steps,
              'retained_optimizer_steps': retained_steps, 'wall_seconds': time.monotonic()-began,
              'outside_parameters_unchanged': True, 'outside_parameter_sha256': frozen,
              'combat_parameter_sha256': digest(combat),
              'optimizer_without_lr_sha256': adam_digest(optimizer, include_lr=False),
              'final_lr': optimizer.param_groups[0]['lr'], 'error': error}
    write(out/'summary.json', result)
    return result
