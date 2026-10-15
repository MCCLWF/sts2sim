# Mecha Knight Elite encounter

## Goal

Port `MECHA_KNIGHT_ELITE` and the Mecha Knight combat behavior from game v0.111.0, including its A0/A10 attack values, Artifact, Burn generation, and fixed move cycle.

## Tasks

- [x] Add the Mecha Knight monster model with source-matched stats, move transitions, and effects.
- [x] Port the generic Burn turn-end effect required by Flamethrower.
- [x] Add the single-monster elite encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle.
- [x] Update encounter coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- `MECHA_KNIGHT_ELITE` runs through combat with the source move order and values.
- A0/A10 L3 traces match the oracle at every state.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Source review confirms a deterministic four-move sequence. `IsWoundUp` only selects presentation animation branches and has no combat-state effect, so it is omitted from this headless model.
- The first oracle run stopped at Burn's missing end-turn Hook; after porting that shared status-card behavior, A0 passed 4/4 combats and 81/81 states, and A10 passed 2/2 combats and 35/35 states.
