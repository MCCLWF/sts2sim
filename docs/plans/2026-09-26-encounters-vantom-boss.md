# Vantom Boss encounter

## Goal

Port `VANTOM_BOSS` and the Vantom combat behavior from game v0.111.0, including Slippery, its four-move loop, and Dismember's Wounds.

## Tasks

- [x] Add Vantom with source-matched HP, ascension values, Slippery, move transitions, and effects.
- [x] Port the generic Wound status card required by Dismember.
- [x] Add the Boss encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle.
- [x] Update encounter coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- `VANTOM_BOSS` executes the source move order and damage values.
- A0/A10 L3 traces match the oracle at every state, including Wound generation and Slippery depletion.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Source review confirms a deterministic four-move loop. Vantom's progress music and animation extension are presentation-only; `ShouldDisappearFromDoom = false` is retained as combat behavior.
- L3 A0 passed 4/4 combats and 114/114 states; A10 passed 2/2 combats and 44/44 states, with no unported behavior or divergence.
