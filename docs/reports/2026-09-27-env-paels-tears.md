# Pael's Tears relic port

Date: 2026-09-27.

## Preconditions

- Read `AGENTS.md` and `docs/audits/pitfalls.md`.
- Inspected v0.111.0 `Models/Relics/PaelsTears.cs`, the turn-side hook signatures, `PlayerCmd.GainEnergy`, and existing relic state-storage patterns.
- The provided trace identifies Ironclad SEED0004, policy 1, choosing PaelsTears at Pael.

## Change

Added `game/Models/Relics/PaelsTears.h`. `BeforeSideTurnEnd` updates the remembered energy flag only when the owner's creature participates. `AfterSideTurnStart` checks owner participation and that flag, then flashes and grants the canonical dynamic energy amount without clearing the flag. `AfterCombatEnd` clears the flag.

The source stores `HadLeftoverEnergy` in a private mutable boolean backing field; this port stores that value in relic instance state slot `extra(0)`. The v0.111.0 source does not decorate this private property with `[SavedProperty]`; the implementation mirrors its mutable per-instance state and setter behavior rather than introducing an unrelated serialization hook.

## Verification

- Source hook conditions, mutation points, grant amount, and reset timing: inspected against `PaelsTears.cs`.
- Generated dispatch and Release build passed in integration. The original Ironclad `SEED0004` policy 1 Act 2 trace advanced from 290/737 to 317/737 decisions without difference and with per-decision snapshot restoration. It then stopped at Infested Automaton, subsequently ported.
- Python trailing-whitespace scan of the three added files: `clean`.

## Limits

Only the PaelsTears relic effect is ported. No generated dispatch, build, Oracle, or CombatSolver changes are included.
