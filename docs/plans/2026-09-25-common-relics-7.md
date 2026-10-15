# Shared relics batch 7

## Goal

Continue the shared relic pool with four effects supported by existing hooks: attack rewards for an empty hand, turn-seven damage, retained block, and colorless reward-pool additions.

## Scope

- Port `ScreamingFlagon`, `StoneCalendar`, `SturdyClamp`, and `DingyRug` from v0.111.0.
- Add focused tests for timing, targeting, retained block cap, and card-pool option conditions.
- Regenerate model dispatch and update shared relic coverage in README and roadmap.

## Acceptance

- Trigger conditions, amounts, and owner checks match source.
- New relic tests pass.
- `scripts/check.sh` passes the build, unit, smoke, and Oracle comparison gates.
- Report records the updated remaining shared relic list count.

## Files

- `game/Models/Relics/*.h`
- `game/generated/AllModels.h`, `game/generated/Dispatch.cpp`
- `tests/test_relics.cpp`
- `docs/roadmap.md`, `README.md`
- `docs/reports/2026-09-25-common-relics-7.md`
