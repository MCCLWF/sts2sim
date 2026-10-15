# The Insatiable Boss encounter

## Goal

Port `THE_INSATIABLE_BOSS` from game v0.111.0, including Sandpit's turn timer, generated Frantic Escapes, and the finisher if the timer expires.

## Tasks

- [x] Add The Insatiable's move cycle and Sandpit/Frantic Escape behavior.
- [x] Add the Boss encounter and generated dispatch.
- [x] Compare A0/A10 Oracle traces through card generation, timer ticks/extensions, and expiration or boss death.
- [x] Update encounter coverage, README, roadmap, aggregate report, and this report; run the quick gate.

## Acceptance

- Six Frantic Escapes are inserted in order: three random draw-pile positions, then three random discard-pile positions.
- Sandpit decrements at the enemy turn start, Frantic Escape extends it and raises its combat cost, and expiration kills the player only while the boss and target remain alive.
- `scripts/check.sh --quick` passes.

## Files

- `game/Models/Monsters/TheInsatiable.h`
- `game/Models/Powers/SandpitPower.h`
- `game/Models/Cards/FranticEscape.h`
- `game/Models/Encounters/TheInsatiableBoss.h`
- Generated dispatch, coverage docs, and batch report.

## Progress (2026-09-26)

- Standard L3 A0 passed 4/4 combats and 88/88 states; A10 passed 2/2 combats and 39/39 states.
- A0 20-Defend timer probe passed 2/2 combats and 47/47 states, covering Sandpit expiration.
- `scripts/check.sh --quick` passed: build, 8/8 CTest tests, and whole-run smoke checks.
