# Shared relics batch 5

## Goal

Continue the Ironclad run-content work by porting shared relics whose gameplay behavior fits existing simulator hooks. This batch covers turn-start card retention, two existing combat triggers, card upgrades, reward additions, and one combat-start potion.

## Scope

- Port `RingingTriangle`, `TheAbacus`, `UnceasingTop`, `MiniatureTent`, `RazorTooth`, `PrayerWheel`, `PetrifiedToad`, and `TinyMailbox` from the v0.111.0 source.
- Add focused relic tests for the observable behavior of each model.
- Regenerate model dispatch and update shared relic coverage in README and roadmap.
- Record the exact check results and any remaining limitations.

## Acceptance

- Hook timing and conditions match the source implementations.
- New and existing relic unit tests pass.
- `scripts/check.sh` passes, including build, CTest, and the configured oracle comparisons.
- Current shared relic model coverage is recorded without implying that remaining pool entries are complete.

## Files

- `game/Models/Relics/*.h`
- `game/generated/AllModels.h`, `game/generated/Dispatch.cpp`
- `tests/test_relics.cpp`
- `docs/roadmap.md`, `README.md`
- `docs/reports/2026-09-25-common-relics-5.md`
