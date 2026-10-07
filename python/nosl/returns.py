"""Training-only rewards and advantage estimates over public run observations.

These are learning targets, not game rules or deployment actor features.
Reward arithmetic preserves the existing PPO floor/HP/boss configuration.
"""
from dataclasses import dataclass

import numpy as np

from . import native

FLOOR_SCALE = 50.0
G_IN_COMBAT, G_ROOM_TYPE, ROOM_BOSS = 11, 26, 3
TOKEN_ENEMY, T_HP, T_MAX_HP, T_ALIVE = 6, 8, 9, 15


@dataclass
class RewardState:
    floor: np.ndarray
    potential: np.ndarray
    inside_boss: np.ndarray
    enemy_hp: np.ndarray
    enemy_max_hp: np.ndarray


def reward_state(global_, tokens, counts, hp_potential=.2, hp_denominator=70.0):
    """Own the small public summaries before a sampler step overwrites buffers."""
    used = max(1, int(counts[:, 0].max()))
    shown = tokens[:, :used]
    living = ((shown[..., 0] == TOKEN_ENEMY) & (shown[..., T_ALIVE] > 0) &
              (np.arange(used)[None, :] < counts[:, :1]))
    denominator = hp_denominator if hp_denominator > 0 else np.maximum(global_[:, native.G_MAX_HP], 1)
    return RewardState(global_[:, native.G_TOTAL_FLOOR].copy(),
                       hp_potential * global_[:, native.G_HP] / denominator,
                       (global_[:, G_IN_COMBAT] > 0) & (global_[:, G_ROOM_TYPE] == ROOM_BOSS),
                       (shown[..., T_HP] * living).sum(axis=1),
                       (shown[..., T_MAX_HP] * living).sum(axis=1))


class RunRewards:
    def __init__(self, runs, gamma=1.0, boss_damage=.3):
        self.gamma, self.boss_damage = gamma, boss_damage
        self.boss_ref = np.zeros(runs, np.float64)
        self.boss_paid = np.zeros(runs, np.float64)

    def step(self, before, after, results, seeds_before):
        opened = before.inside_boss & (self.boss_ref == 0)
        self.boss_ref[opened] = np.maximum(np.maximum(before.enemy_max_hp, before.enemy_hp), 1)[opened]
        self.boss_paid[opened] = 0
        # Preserve the original float32 base assignment and in-place bonus.
        reward = ((after.floor - before.floor) / FLOOR_SCALE +
                  self.gamma * after.potential - before.potential).astype(np.float32)
        ended = np.zeros(len(reward), bool)
        for row in results:
            slot = row['slot']
            if slot >= 0 and row['seed'] == seeds_before[slot]:
                ended[slot] = True
        fighting = (self.boss_ref > 0) & ~ended
        won = fighting & ~after.inside_boss
        share = np.where(won, 1.0 - self.boss_paid,
                         np.minimum(np.maximum(before.enemy_hp - after.enemy_hp, 0) /
                                    np.maximum(self.boss_ref, 1), 1.0 - self.boss_paid))
        share = np.where(fighting, share, 0.0)
        reward += self.boss_damage * share
        self.boss_paid += share
        fights = [(1.0, True) for _ in np.flatnonzero(won)]
        self.boss_ref[won] = 0
        for row in results:
            slot = row['slot']
            if slot < 0 or row['seed'] != seeds_before[slot]:
                continue
            if self.boss_ref[slot] > 0:
                victory = row['status'] == native.VICTORY
                if victory or row['status'] == native.DEATH:
                    fights.append((1.0 if victory else float(self.boss_paid[slot]), victory))
                self.boss_ref[slot] = 0
                bonus = self.boss_damage * (1.0 - self.boss_paid[slot]) if victory else 0.0
            else:
                bonus = 0.0
            if row['status'] in (native.VICTORY, native.DEATH):
                reward[slot] = ((row['floor'] - before.floor[slot]) / FLOOR_SCALE +
                                (row['status'] == native.VICTORY) - before.potential[slot] + bonus)
            else:
                reward[slot] = 0
        return reward, fights


def gae(reward, value, done, gamma=1.0, lam=.95):
    """Float32 iterative GAE; true terminals cancel successor bootstrapping."""
    advantage = np.zeros_like(reward, dtype=np.float32)
    last = np.zeros(reward.shape[1], np.float32)
    for t in reversed(range(len(reward))):
        alive = 1.0 - done[t]
        delta = reward[t] + gamma * value[t + 1] * alive - value[t]
        last = delta + gamma * lam * alive * last
        advantage[t] = last
    return advantage


def store_observation(target, array):
    """Lossless sampler-to-rollout copy; trailing token/action slots are padding."""
    if target.dtype != np.int32 or array.dtype != np.int32:
        raise ValueError('rollout public tables must stay int32')
    target[:] = 0
    if target.ndim == array.ndim and target.shape == array.shape:
        target[:] = array
    else:
        target[:, :array.shape[1]] = array
