# The Obscura Normal encounter

## Goal

Port `THE_OBSCURA_NORMAL` from game v0.111.0, including The Obscura's summon, Parafright, Illusion/Minion behavior, and randomized nonrepeating moves.

## Tasks

- [x] Add The Obscura and Parafright with source-matched stats, powers, moves, and summon behavior.
- [x] Add the encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle, including the summon, illusion revival, and randomized move branches.
- [x] Update encounter coverage, README, roadmap, aggregate report, and this report; run the quick gate.

## Acceptance

- The first move summons Parafright in the illusion slot; it attacks and uses the shared Illusion/Minion behavior.
- The Obscura's subsequent moves match source damage/block/Strength and nonrepeat branching at A0/A10.
- `scripts/check.sh --quick` passes.

## Files

- `game/Models/Monsters/TheObscura.h`
- `game/Models/Monsters/Parafright.h`
- `game/Models/Encounters/TheObscuraNormal.h`
- Generated dispatch, coverage docs, and batch report.

## Progress (2026-09-26)

- L3 A0 passed 4/4 combats and 78/78 states; A10 passed 2/2 combats and 32/32 states, with no unported behavior or divergence.
- Parafright revives through the shared IllusionPower after death; its temporary `REVIVE_MOVE` mirrors the source follow-up to the last logged move.
- `scripts/check.sh --quick` passed: build, 8/8 CTest tests, and whole-run smoke checks.
