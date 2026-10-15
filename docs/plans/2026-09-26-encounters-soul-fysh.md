# Soul Fysh Boss encounter

## Goal

Port `SOUL_FYSH_BOSS`, including Beckon generation and turn-end HP loss, Intangible, and the five-move cycle.

## Tasks

- [x] Add Beckon card's turn-end HP-loss behavior.
- [x] Add Intangible's damage cap and enemy-turn duration.
- [x] Add Soul Fysh moves and source-matched ascension values.
- [x] Add the Boss encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle.
- [x] Update encounter coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- A0/A10 L3 traces match exactly through Beckon generation, its turn-end effect, Intangible, and the move cycle.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Source review confirms Beckon adds one card to a random draw-pile position and a second to discard; Gaze adds one to discard. Fade applies two Intangible and the power decrements at the end of the enemy side turn.
- L3 A0 passed 4/4 combats and 96/96 states; A10 passed 2/2 combats and 43/43 states with no unported behavior or divergence.
- `scripts/check.sh --quick` passed: build, 8/8 CTest tests, and whole-run smoke checks.
