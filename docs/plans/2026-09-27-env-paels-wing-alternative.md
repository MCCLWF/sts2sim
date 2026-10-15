# Pael's Wing and generic card reward alternatives

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Relics/PaelsWing.cs`, `Entities/CardRewardAlternatives/CardRewardAlternative.cs`, and `Rewards/CardReward.cs`.

The Ironclad `SEED0004` policy 2 Act 2 trace selects Pael's Wing and stops at its ShowCounter. Implement saved sacrifice count, counter and reward alternative in the normal card reward choice machinery. Preserve source alternative order, mark Sacrifice as a completed reward, and obtain one ordinary next relic after every second sacrifice. Other unported modifier hooks remain explicit failures. Use the original trace to validate option exposure, selection, RNG and snapshot restore. CombatSolver remains frozen.
