# Shared relics batch 9

## Goal

Port three relic effects involving combat-reward upgrades, combat-pile upgrades, and card reward enchantment using existing hooks and RNG streams.

## Scope

- Port `LavaLamp`, `StoneCracker`, and `WingCharm` from v0.111.0.
- Preserve damage qualification, stable shuffle ordering, and the Niche RNG draw for eligible reward enchantment.
- Add focused tests for damage tracking, eligible upgrade counts, and reward-option enchantment.
- Regenerate dispatch and update shared relic coverage in README and roadmap.

## Acceptance

- Trigger conditions and card selection behavior match the source.
- New relic tests pass.
- `scripts/check.sh` passes the build, unit, smoke, and Oracle comparison gates.
- Report records the new count and remaining shared relic headers.

## Files

- `game/Models/Relics/*.h`
- `game/generated/AllModels.h`, `game/generated/Dispatch.cpp`
- `tests/test_relics.cpp`
- `docs/roadmap.md`, `README.md`
- `docs/reports/2026-09-25-common-relics-9.md`
