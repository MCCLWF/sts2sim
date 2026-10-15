# Lagavulin Matriarch Boss encounter

## Goal

Port `LAGAVULIN_MATRIARCH_BOSS`, including its asleep/plating opening, damage-triggered wake-up stun, timeout wake-up, and attack cycle.

## Tasks

- [x] Implement AsleepPower damage, turn-end, and Plating lifecycle hooks.
- [x] Add Lagavulin Matriarch moves and source values.
- [x] Add the Boss encounter and generated dispatch.
- [x] Compare A0/A10 combat traces against Sts2Oracle, including an undamaged sleep-expiry route.
- [x] Update encounter coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- A0/A10 L3 traces match exactly, including damage awakening, turn-count awakening, wake-up stun, Plating removal, and the repeated attack cycle.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Source review found that AsleepPower wakes on unblocked damage or after three participating side turns; it also removes Plating just before the final sleeping player's turn ends.
- Default A0/A10 traces and a no-attack A0 trace all matched the oracle, covering both wake paths and Plating timing.
