# Aeonglass Boss encounter

## Goal

Port `AEONGLASS_BOSS` from game v0.111.0, including Withering Presence, the generated Wither status card, its fake upgrades, and the boss's repeating move cycle.

## Tasks

- [x] Add Aeonglass Boss encounter and monster model with its opening powers and escalating loop.
- [x] Add WitheringPresencePower and Wither status behavior, including card-generation scaling.
- [x] Regenerate model dispatch and compile.
- [x] Compare A0/A10 against Sts2Oracle through card-count triggers, Wither damage, and repeated Intensity cycles.
- [x] Update coverage and reports; run the quick gate.

## Acceptance

- Every six cards played by a target player generates a Wither in hand.
- Intensity applies the source-matched number of Withers, upgrades existing Withers, and increases Aeonglass Strength cumulatively.
- A0/A10 L3 traces match the oracle.
- `scripts/check.sh --quick` passes.
