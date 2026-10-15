# Generic Env: Fur Coat map quest and combat hooks

Date: 2026-09-27. Fixed source: v0.111.0 `Models/Relics/FurCoat.cs`. This was the first concrete third-act blocker on the original `SEED0004` policy 1 full-run trace (738/1123 decisions matched).

## Required generic work

1. Model map-point quest membership and saved marked coordinates as ordinary POD state, including snapshot validation and logical serialization. Do not infer marked rooms later from the seed; restore the saved list.
2. Reproduce the relic-local `new Rng(owner, relic.Id, 0uL)` seed derivation, source candidate enumeration and `UnstableShuffle`. Do not substitute a run RNG stream or adjust draws to fit one seed.
3. Add the original `AfterObtained` and `ModifyGeneratedMapLate` map behavior and the `BeforeCombatStart` / `AfterCreatureAddedToCombat` effects, including current map-point history. Preserve Hook order and `CreatureCmd.SetCurrentHp` effects.
4. Generate dispatch and replay the fixed third-act original trace with per-decision snapshot restore. Add a targeted fresh trace for a marked monster room and spawned enemy; compare map quest facts, HP, RNG and outcome. Keep unknown branches explicitly Unported.

## Acceptance

The Env implementation is complete for the observed Fur Coat path: the original `SEED0004` policy 1 trace passes 814/1123 decisions with per-decision snapshot restore and stops at the next explicit blocker, `Events.BattlewornDummy.GenerateInitialOptions`, after the TinkerTime/Mad Science event path was added. The saved map marker/coordinates and HP effects are included in state validation and logical snapshots. A dedicated unmarked-versus-marked combat fixture remains a follow-up before calling this relic fully branch-complete. The implementation remains generic; CombatSolver stays frozen.
