# Ceremonial Beast Boss encounter

## Goal

Port `CEREMONIAL_BEAST_BOSS` from game v0.111.0, including Plow threshold stun, second-phase move order, and Ringing card affliction behavior.

## Tasks

- [x] Add Ceremonial Beast moves, phase state, and encounter slot.
- [x] Add PlowPower and RingingPower/Ringing affliction behavior required by the boss.
- [x] Regenerate model dispatch and compile.
- [x] Compare A0/A10 against Sts2Oracle through the stun and post-stun loops.
- [x] Update coverage and reports; run the quick gate.

## Acceptance

- Plow accumulates from stamping, and qualifying damage stuns the boss, clears Strength, and starts its post-stun sequence.
- Ringing applies to deck cards while the debuff is active and clears after the player's side turn.
- A0/A10 L3 traces match the oracle.
- `scripts/check.sh --quick` passes.
