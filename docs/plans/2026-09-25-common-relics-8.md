# Shared relics batch 8

## Goal

Port four shared combat relics that rely on existing card, block, and turn hooks: one skill-exhaust copy, Pocketwatch draw threshold, Vambrace's first block card, and Ripple Basin's attack-free turn block.

## Scope

- Port `BurningSticks`, `Pocketwatch`, `Vambrace`, and `RippleBasin` from v0.111.0.
- Store counters and card references in the relic's existing fixed-size extra state.
- Add focused tests for use limits, card-count thresholds, the triggering card, and per-turn resets.
- Regenerate dispatch and update shared relic coverage in README and roadmap.

## Acceptance

- Counter resets and eligibility conditions match source behavior.
- New relic tests pass.
- `scripts/check.sh` passes the build, unit, smoke, and Oracle comparison gates.
- Report records the current shared relic model coverage and remaining headers.

## Files

- `game/Models/Relics/*.h`
- `game/generated/AllModels.h`, `game/generated/Dispatch.cpp`
- `tests/test_relics.cpp`
- `docs/roadmap.md`, `README.md`
- `docs/reports/2026-09-25-common-relics-8.md`
