# Shared relics batch 6

## Goal

Add four shared relics with self-contained combat or pickup effects, including the temporary-strength power required by Reptile Trinket.

## Scope

- Port `LizardTail`, `Kunai`, `ReptileTrinket`, and `LoomingFruit` from v0.111.0.
- Add `ReptileTrinketPower` with temporary-strength behavior matching its source base class.
- Add focused tests for one-time death prevention, max-HP gain, attack counting/reset, and temporary strength expiration.
- Regenerate dispatch and update shared relic coverage in README and roadmap.

## Acceptance

- Owner, turn, attack-count, death-prevention, and temporary-power expiration behavior match source.
- New relic and power tests pass.
- `scripts/check.sh` passes the build, unit, smoke, and Oracle comparison gates.
- Report lists the remaining shared relic models without claiming the pool complete.

## Files

- `game/Models/Relics/*.h`, `game/Models/Powers/ReptileTrinketPower.h`
- `game/generated/AllModels.h`, `game/generated/Dispatch.cpp`
- `tests/test_relics.cpp`
- `docs/roadmap.md`, `README.md`
- `docs/reports/2026-09-25-common-relics-6.md`
