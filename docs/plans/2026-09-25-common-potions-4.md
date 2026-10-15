# Common potions batch 4

- Branch: `feat/common-potions-4`
- Scope: add the three standard card-choice potions (Attack, Power, Skill) and Blessing of the Forge.
- Choice potions use the existing `CardSelectCmd` prefix/replay path and generate three distinct cards from the owning character pool, with a temporary zero cost.
- Acceptance: potion effects are usable through `PotionModel::OnUseWrapper`, choice tests replay a selected card, Blessing upgrades every upgradable hand card, and the full gate remains green.
- Colorless Potion remains separate because it can surface currently unported colorless card models; no cross-role card path is silently substituted.
