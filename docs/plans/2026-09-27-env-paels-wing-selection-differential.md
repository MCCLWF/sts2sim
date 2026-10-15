# Pael's Wing selected sacrifice differential

Date: 2026-09-27. Add an opt-in original oracle policy that chooses the first two available `SACRIFICE` card reward alternatives. Keep the default policy untouched. Replay the resulting trace through the generic Env with per-decision snapshot restore, checking the relic counter, reward completion and relic grant after the second sacrifice. Record the exact verified path and any blocker; no CombatSolver work.
