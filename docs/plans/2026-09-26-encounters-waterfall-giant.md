# Waterfall Giant Boss encounter

## Goal

Port `WATERFALL_GIANT_BOSS` from game v0.111.0, including Steam Eruption, pressure buildup, and the delayed death blow.

## Tasks

- [x] Add Waterfall Giant's move loop and stateful damage/pressure tracking.
- [x] Add SteamEruptionPower death interception and the Boss encounter.
- [x] Compare A0/A10 traces against Sts2Oracle through ordinary death, pressure buildup, and eruption.
- [x] Update encounter coverage, README, roadmap, aggregate report, and this report; run the quick gate.

## Acceptance

- Damage, Weak, Strength-independent pressure values, healing, and the escalating Pressure Gun match source at A0/A10.
- Killing the giant while Steam Eruption is active triggers the source-matched delayed explosion and then ends combat.
- `scripts/check.sh --quick` passes.

## Files

- `game/Models/Monsters/WaterfallGiant.h`
- `game/Models/Powers/SteamEruptionPower.h`
- `game/Models/Encounters/WaterfallGiantBoss.h`
- Generated dispatch, coverage docs, and batch report.

## Progress (2026-09-26)

- L3 A0 passed 4/4 combats and 132/132 states; A10 passed 2/2 combats and 67/67 states, with no unported behavior or divergence.
- Traces reached the Steam Eruption death interception and the delayed explosion; Pressure Gun escalation also matched.
- `scripts/check.sh --quick` passed: build, 8/8 CTest tests, and whole-run smoke checks.
