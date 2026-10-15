# Knights Elite encounter

## Goal

Port `KNIGHTS_ELITE` and its complete combat behavior from game v0.111.0: fixed Flail Knight, Spectral Knight, and Magi Knight; Hex/Hexed; Dampen including downgrade and restoration of upgraded combat cards.

## Tasks

- ✅ Add Spectral Knight and Magi Knight moves, HP, ascension values, and fixed encounter registration.
- ✅ Implement HexPower and Hexed lifecycle, including Ethereal while HexPower exists and removal when its applier dies.
- ✅ Implement CardModel downgrade semantics needed by Dampen and preserve original upgrade levels across the debuff.
- ✅ Implement Dampen caster tracking, downgrade on first application, and restoration when the final caster dies.
- ✅ Compare A0/A10 combat traces against Sts2Oracle; include upgraded Ironclad cards with `AfterDowngraded` state in the validation.
- ✅ Update encounter/content coverage, README, roadmap, and report; run the quick gate.

## Acceptance

- Default Ironclad L3 A0/A10 traces for `KNIGHTS_ELITE` match the oracle.
- An upgraded-card scenario verifies Dampen downgrades cards and restores their original upgrade levels after all Magi Knight casters die.
- Hexed cards gain Ethereal while HexPower is active and clear when it is removed.
- `scripts/check.sh --quick` passes.

## Progress (2026-09-26)

- Knights Elite and all three fixed monsters are registered; Hex/Hexed and Dampen are implemented.
- During Dampen validation, a Rampage trace exposed a duplicate `OnUpgrade` delta. The generator already exported the same level changes, so the table-only `OnUpgrade` overrides are now left to the tables.
- Final A0/A10 L3, upgraded-card Dampen restoration, and quick-gate results are recorded in [`reports/2026-09-26-encounters-knights-elite.md`](../reports/2026-09-26-encounters-knights-elite.md).
