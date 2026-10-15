# Generic Env: Pael's Tears

Date: 2026-09-27.

## Goal

Port the v0.111.0 `PaelsTears` relic hooks to the generic Env, preserving its per-relic `HadLeftoverEnergy` state and hook timing.

## Source behavior

- `BeforeSideTurnEnd`: if the owner's creature participates, remember whether the owner's combat energy is greater than zero. If the owner does not participate, leave the remembered value unchanged.
- `AfterSideTurnStart`: if the owner's creature participates and remembered leftover energy is true, flash and gain the relic's `Energy` dynamic variable (2 at its canonical value). Do not clear the remembered value here.
- `AfterCombatEnd`: clear the remembered value.

## Implementation

- Add `game/Models/Relics/PaelsTears.h` with the three source hooks and a single boolean state slot corresponding to `HadLeftoverEnergy`.
- Use the existing relic `extra(0)` per-instance state storage for that mutable boolean and `PlayerCmd::GainEnergy` with `DynamicVars().Energy().BaseValue()`.
- Do not add dispatch/generated changes or implement any unrelated Pael relic.

## Acceptance

- Hook participation tests, state writes, energy gain, and combat-end reset match the source order and conditions.
- The state stays true after a qualifying side-turn start until a later participating side-turn end overwrites it or combat ends.
- Only the new relic header and this plan/report are edited; build and Oracle validation remain with the integrating agent.
