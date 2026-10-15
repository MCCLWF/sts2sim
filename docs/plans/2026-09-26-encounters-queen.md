# Queen Boss encounter

Port QueenBoss, Queen, TorchHeadAmalgam, ChainsOfBindingPower and Bound from v0.111.0. Preserve slot order, binding history/turn counts, bound-card play limits, and Queen's phase change on Amalgam death.

- [x] Encounter and both monsters with complete move machines.
- [x] Chains of Binding/Bound and current-turn affliction history counter.
- [x] Generate dispatch and compile.
- [x] A0/A10 oracle comparison and phase-change/Binding probes.
- [x] Quick gate, progress docs and commit.

Acceptance: exact card affliction order and clear-at-turn-end semantics; duplicate plays do not consume the bound-card allowance; Amalgam death updates the conditional branch and immediately replaces Burn Bright For Me with Enrage when appropriate.
