# Ovicopter Normal encounter

## Goal

Port `OVICOPTER_NORMAL`, including Ovicopter's egg cap/slot selection, spawned Tough Eggs, their hatch timer and hatchling HP roll, and the repeated attack cycle.

## Tasks

- [x] Add HatchPower countdown behavior.
- [x] Add Tough Egg hatch and Nibble behavior with the Niche HP roll.
- [x] Add Ovicopter move cycle and source-matched egg slot selection.
- [x] Add the encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle, including a scenario where eggs hatch.
- [x] Update encounter coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- A0/A10 L3 traces match the oracle, including egg spawn order, HatchPower countdown, and hatchling HP.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Source review shows Ovicopter chooses the last free `egg1`–`egg5` slot each time it lays an egg; Tough Eggs hatch through their first move and replace their HP with a Niche-RNG roll.
- Standard A0/A10 traces and a no-attack egg-hatching trace all matched the oracle at every state.
