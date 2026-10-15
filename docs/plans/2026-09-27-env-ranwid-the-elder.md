# Ranwid the Elder event

Date: 2026-09-27. Work is on the generic Env; CombatSolver remains frozen.

## Source and trace

Use installed v0.111.0 `Models/Events/RanwidTheElder.cs` and `Commands/RelicCmd.cs`. The original Ironclad `SEED0003` policy 1 Act 2 trace reaches this event at 328/602 decisions and chooses its potion trade.

## Work and verification

Add the potion use lock while the event is open, select the offered potion and relic with the event RNG in source order, and implement the potion, gold and relic trade callbacks through normal commands. Preserve selected model identities in option arguments across snapshot restore. Add `RelicCmd::Remove` with the shipped `AfterRemoved` hook for the relic trade. Generate dispatch, rebuild, replay the fixed trace with snapshot verification, then run the full regression gate. The current numeric DynamicVar representation cannot expose the original StringVar titles; record this as an API gap.
