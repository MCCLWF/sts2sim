"""Experimental public rule features with whole-run learned positive weights.

These are approximate printed-rule features, not an engine preview. Inference
reads only the public observation and the frozen card table. No Env call occurs.
Nested choices and all non-applicable decisions retain the exact X1 scores.
"""
from pathlib import Path
import json

import numpy as np
import torch
from torch import nn

from .endturn import START_SHA, baseline

FORMAT = 'sts2.nosl.public-tactical.v1'
FEATURES = ('hp_damage', 'kills', 'kill_shown_attack', 'effective_block', 'cards',
            'energy', 'self_power', 'poison', 'weak_shown_attack', 'heal',
            'negative_energy_cost', 'negative_star_cost', 'negative_hp_loss',
            'negative_play_cost', 'potion_danger', 'negative_potion_cost')
PRIOR = np.array([1, 8, 1, 1.4, 2, 4, 5, 2, 1.2, 2, 1, .2, 2, .25, .6, 3], np.float64)
POWERS = {'strength': 226, 'dexterity': 60, 'weak': 262, 'frail': 89,
          'vulnerable': 260, 'poison': 168, 'intangible': 123}
# DaggerSpray.OnPlay explicitly uses WithHitCount(2), not a RepeatVar.
DAGGER_SPRAY = 129


def active(global_, actions, counts):
    legal = torch.arange(actions.shape[1], device=actions.device)[None] < counts[:, 1:2]
    end = legal & (actions[..., 0] == 3)
    known = (actions[..., 0] == 2) | (actions[..., 0] == 3) | (actions[..., 0] == 10) | (actions[..., 0] == 11)
    return ((global_[:, 11] != 0) & (end.sum(1) == 1) & (counts[:, 1] > 1) &
            (known | ~legal).all(1)), legal


def feature_table(base, global_, tokens, actions, counts):
    """[B,A,16] features; amounts are heuristic estimates, all current/public."""
    g, t, a = global_.long(), tokens.long(), actions.long()
    batch, token_count = t.shape[:2]
    valid = torch.arange(token_count, device=t.device)[None] < counts[:, :1]
    enemy = valid & (t[..., 0] == 6) & (t[..., 15] > 0)
    source_index = a[..., 1].clamp(0, token_count - 1)
    source = t.gather(1, source_index[..., None].expand(-1, -1, t.shape[-1]))
    card = source[..., 1].clamp(0, base.card_numbers.shape[0] - 1)
    level = source[..., 8].clamp(0, 1)
    numbers = base.card_numbers[card, level].double()
    flags = base.card_flags[card, level]
    power_ids = base.card_powers[card, level].long()
    play = (a[..., 0] == 2) & (a[..., 1] >= 0) & (source[..., 0] == 1)
    attack = source[..., 11] == 1
    # Owner ordinals are public. Slot zero is the player; enemy n6 + 1.
    pools = {}
    owner = t[..., 9].clamp(0, token_count)
    for name, identity in POWERS.items():
        amount = torch.where(valid & (t[..., 0] == 7) & (t[..., 5] == identity), t[..., 8], 0).double()
        pool = torch.zeros(batch, token_count + 1, dtype=torch.float64, device=t.device)
        pool.scatter_add_(1, owner, amount);pools[name] = pool
    shown = (t[..., 11].clamp(min=0) * t[..., 12].clamp(min=1) * enemy).double()
    unblocked = (shown.sum(1) - g[:, 9]).clamp(min=0)
    block = (numbers[..., 4] + pools['dexterity'][:, :1] * (numbers[..., 4] > 0)).clamp(min=0)
    block *= torch.where(pools['frail'][:, :1] > 0, .75, 1.)
    block = torch.minimum(block.floor(), unblocked[:, None])
    damage = (numbers[..., 3] + pools['strength'][:, :1] * attack * (numbers[..., 3] > 0)).clamp(min=0)
    damage *= torch.where((pools['weak'][:, :1] > 0) & attack, .75, 1.)
    repeat = numbers[..., 5].clamp(min=1)
    repeat = torch.where(card == DAGGER_SPRAY, 2., repeat)
    # Pack the actual visible living enemies without truncating their count.
    width = int(enemy.sum(1).max())
    positions = torch.arange(token_count, device=t.device)[None].expand(batch, -1)
    indices = torch.where(enemy, positions, token_count).sort(1).values[:, :width]
    evalid = indices < token_count;indices = indices.clamp(max=token_count - 1)
    et = t.gather(1, indices[..., None].expand(-1, -1, t.shape[-1]))
    eowner = (et[..., 14] + 1).clamp(0, token_count)
    hp, armor = et[..., 8].double(), et[..., 10].double()
    hit = damage[..., None] * torch.where(pools['vulnerable'].gather(1, eowner)[:, None] > 0, 1.5, 1.)
    hit = hit.floor()
    hit = torch.where(pools['intangible'].gather(1, eowner)[:, None] > 0, hit.clamp(max=1), hit)
    hp_damage = (hit * repeat[..., None] - armor[:, None]).clamp(min=0)
    hp_damage = torch.minimum(hp_damage, hp[:, None].clamp(min=0))
    all_enemies, random_enemy = flags[..., 21] > 0, flags[..., 22] > 0
    targeted = a[..., 2:3] == indices[:, None]
    affected = (targeted | all_enemies[..., None] | random_enemy[..., None]) & evalid[:, None]
    factor = affected.double() / torch.where(random_enemy, evalid.sum(1)[:, None].clamp(min=1), 1)[..., None]
    kills = (hp_damage >= hp[:, None]) & (hp[:, None] > 0) & (hit * repeat[..., None] > 0)
    threat = shown.gather(1, indices)
    effective_damage = (hp_damage * factor).sum(2)
    kill_count = (kills * factor).sum(2)
    kill_threat = (kills * threat[:, None] * factor).sum(2)
    # Only printed power ids/amounts are used; stacking/effect completeness is not claimed.
    amount = numbers[..., 11:13].clamp(min=0)
    poison = (amount * (power_ids == POWERS['poison'])).sum(2)
    weak_amount = (amount * (power_ids == POWERS['weak'])).sum(2)
    weak_value = (factor * threat[:, None] * (pools['weak'].gather(1, eowner)[:, None] <= 0)).sum(2) * .25
    weak_value *= weak_amount > 0
    self_target = (flags[..., 19] > 0) | (flags[..., 23] > 0)
    self_power = (source[..., 11] == 3).double() + self_target * amount.sum(2).clamp(max=20) / 10
    cards = torch.minimum(numbers[..., 6].clamp(min=0), (11 - g[:, 17]).clamp(min=0)[:, None].double())
    heal = torch.minimum(numbers[..., 8].clamp(min=0), (g[:, 8] - g[:, 7]).clamp(min=0)[:, None].double())
    cost = torch.where(numbers[..., 1] > 0, g[:, 12:13], source[..., 9].clamp(min=0)).double()
    danger = (unblocked - g[:, 7] * .25).clamp(min=0)
    danger += (.5 - g[:, 7].double() / g[:, 8].clamp(min=1)).clamp(min=0) * 20
    potion = a[..., 0] == 10
    zero = torch.zeros_like(effective_damage)
    components = [effective_damage, kill_count, kill_threat, block, cards, numbers[..., 7].clamp(min=0),
                  self_power, poison, weak_value, heal, -cost, -source[..., 10].clamp(min=0).double(),
                  -numbers[..., 9].clamp(min=0), -torch.ones_like(cost), zero, zero]
    features = torch.stack(components, -1) * play[..., None]
    features[..., 14] = potion * danger[:, None]
    features[..., 15] = -potion.double()
    if not torch.isfinite(features).all():raise ValueError('nonfinite public tactical features')
    return features


class TacticalPolicy(nn.Module):
    def __init__(self, base, weights=PRIOR):
        super().__init__();self.base = base.eval().requires_grad_(False)
        weights = np.asarray(weights, np.float64)
        if weights.shape != (16,) or not np.isfinite(weights).all() or not (weights > 0).all():
            raise ValueError('tactical weights must be 16 finite positive numbers')
        self.register_buffer('weights', torch.tensor(weights, dtype=torch.float64))

    def forward(self, global_, tokens, actions, counts):
        old, values = self.base(global_, tokens, actions, counts)
        use, legal = active(global_, actions, counts)
        scores = old.double().clone()
        if use.any():
            features = feature_table(self.base, global_[use], tokens[use], actions[use], counts[use])
            # Elementwise, in a fixed feature order: no batch-dependent GEMM summation.
            changed = torch.zeros_like(features[..., 0])
            for column in range(16):changed += features[..., column] * self.weights[column]
            changed = torch.round(changed * 1e8) / 1e8
            changed = torch.where(actions[use, :, 0] == 11, -1e6, changed)
            changed = changed.masked_fill(~legal[use], float('-inf'))
            scores[use] = changed
        return scores, values


def save(path, weights, training=None):
    weights = np.asarray(weights, np.float64)
    blob = {'format': FORMAT, 'schema': 1, 'actor_filter': 1, 'baseline_checkpoint_sha256': START_SHA,
            'features': list(FEATURES), 'powers': POWERS, 'dagger_spray': DAGGER_SPRAY,
            'weights': weights.tolist(), 'training': training, 'score_precision': 'float64_sequential_quantized_1e-8'}
    Path(path).write_text(json.dumps(blob, indent=2) + '\n')


def load(path, start):
    blob = json.loads(Path(path).read_text())
    if (blob.get('format') != FORMAT or blob.get('schema') != 1 or blob.get('actor_filter') != 1 or
        blob.get('baseline_checkpoint_sha256') != START_SHA or blob.get('features') != list(FEATURES) or
        blob.get('powers') != POWERS or blob.get('dagger_spray') != DAGGER_SPRAY or
        blob.get('score_precision') != 'float64_sequential_quantized_1e-8'):
        raise ValueError('incompatible tactical checkpoint')
    return TacticalPolicy(baseline(start), blob.get('weights')), blob
