# M3 action surface

- Branch: `feat/m3-action-surface`
- Scope: expose manual potion use/discard in the environment decision loop and expose a fixed action mask through C ABI/Python.
- Acceptance:
  - `UsePotion` and `DiscardPotion` are legal only for owned, usable, manually actionable potions.
  - target enumeration follows potion `TargetType` for self, player, enemy, ally, all-target, and no-creature effects.
  - potion effects reuse `PotionModel::OnUseWrapper`, so removal and hooks stay in one path and pending choices can be replayed.
  - C ABI and ctypes expose the new action kinds and a fixed 512-slot mask.
  - focused C++ and Python tests pass; full `scripts/check.sh` remains green.
- Known follow-up M3 gaps: full hidden future-state determinization, clone performance evidence, and a richer globally reversible action encoding.
