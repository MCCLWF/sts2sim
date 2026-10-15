# Battleworn Dummy event encounters

## Goal

Port the three formal Battleworn Dummy event encounters (`V1`, `V2`, `V3`) and their shared passive combat behavior from v0.111.0.

## Tasks

- Add three stationary Battle Friend monsters with source health values 75, 150, and 300.
- Add the shared three-enemy-turn limit power, including decrement and escape behavior.
- Add the three encounter generators and preserve their no-rewards metadata from exported content.
- Regenerate dispatch, build, and compare all three encounters with the oracle at A0/A10.
- Update README, roadmap, and aggregate coverage report.

## Acceptance

- Each encounter starts with its matching Battle Friend and 3-stack time limit.
- L3 shows identical combat state through victory or timeout at A0/A10.
- Targeted builds and `scripts/check.sh --quick` pass.

## Scope

This item ports the encounters' combat behavior. The Battleworn Dummy event's post-combat narrative and reward selection remain run/event-model work and are tracked separately from formal encounter generation.

## Progress (2026-09-26)

- ✅ Three Battle Friend variants and three encounter generators added; all get the 3-turn time limit.
- ✅ All six A0/A10 L3 batches pass: 18/18 combats and 207/207 decision states.
- ✅ `scripts/check.sh --quick`: 8/8 unit tests and all three smoke runs passed.
- ⏳ Full L2/L3/L4 gate and post-combat event resume/reward behavior remain outstanding.
