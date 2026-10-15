# Spirit Grafter event in the generic Env

Date: 2026-09-27. CombatSolver remains frozen.

## Source and target

Use the installed v0.111.0 `Models/Events/SpiritGrafter.cs` as the rule source. The original Ironclad `SEED0003` policy 1 Act 2 trace reaches this event at decision 279/602, then chooses Rejection and upgrades a deck card.

## Work

Implement both original options with their dynamic values and action order: Let It In heals then adds Metamorphosis; Rejection selects one upgradable deck card, upgrades it, then deals unblockable and unpowered HP loss. Keep the pending card choice resumable through the Env's existing selector. Generate dispatch, rebuild, and replay the original trace with per-decision snapshot restore. Report any subsequent unsupported rule without replacing it with an approximation.
