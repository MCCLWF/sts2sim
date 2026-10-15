# The Kin Boss encounter

## Goal

Port `THE_KIN_BOSS` from game v0.111.0 with its two Kin Followers, Kin Priest, move loops, and opening variant.

## Tasks

- [x] Add Kin Follower and Kin Priest source-matched stats, powers, attacks, and move order.
- [x] Add The Kin Boss formation and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle through the move loops and minion behavior.
- [x] Update encounter coverage, README, roadmap, aggregate report, and this report; run the quick gate.

## Acceptance

- Encounter order and slots match source; slot 1 Follower opens with Power Dance while slot 2 opens Quick Slash.
- A0/A10 combat states, damage, status powers, and Minion behavior match the oracle.
- `scripts/check.sh --quick` passes.

## Files

- `game/Models/Monsters/KinFollower.h`
- `game/Models/Monsters/KinPriest.h`
- `game/Models/Encounters/TheKinBoss.h`
- Generated dispatch, coverage docs, and batch report.

## Progress (2026-09-26)

- L3 A0 passed 4/4 combats and 77/77 states; A10 passed 2/2 combats and 35/35 states, with no unported behavior or divergence.
- Both follower positions use MinionPower; the slot 1 instance opens with Power Dance while slot 2 opens with Quick Slash.
- `scripts/check.sh --quick` passed: build, 8/8 CTest tests, and whole-run smoke checks.
