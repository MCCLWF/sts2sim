# Crystal Sphere generic Env plan

Date: 2026-09-28

## Source contract

Implement the v0.111.0 `Models/Events/CrystalSphere.cs` path and its
`Events/Custom/CrystalSphereEvent` dependencies without CombatSolver code.
The event must preserve the event RNG, the 11x11 fog grid and item placement,
small/big divination tools, cell clearing order, immediate Doubt curse reveal,
and the nested rewards screen. Crystal Sphere's reward items use the event RNG
as their source RNG in the original game; the Env must retain that stream.

## Implementation

- Add source-backed vars and initial options for Uncover Future and Payment
  Plan, including the 1..49 additive cost roll and all-player 100 gold / act > 0 gate.
- Store the minigame's logical grid in `EventState` so snapshots and hashes
  include hidden cells, placements, selected tool, remaining divinations and
  revealed item bits.
- Expose each minigame cell and tool toggle as an ordinary `ChoiceRequest` with
  source `crystal_sphere`, then run source-order horizontal, vertical,
diagonal and self clearing for big divinations.
- Preserve source item counts, sizes, placement and reveal
  effects. Build nested gold/card/potion/relic rewards after the final
  divination and keep the event RNG override through reward population.

## Acceptance

- Focused C++ event coverage checks vars, option keys, event RNG, board sizes,
  small/big clearing, curse and reward effects, and nested choice replay.
- Build, CTest and the source-first inventory check pass. A fixed oracle route
  that reaches Crystal Sphere must stop only at unrelated content, with no
  `SIM_NOT_PORTED` from this event.
