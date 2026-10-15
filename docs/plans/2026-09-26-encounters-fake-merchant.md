# Fake Merchant Event Encounter

## Goal

Port `FAKE_MERCHANT_EVENT_ENCOUNTER` and `FakeMerchantMonster` from the v0.111.0 source. Keep the monster's weighted no-repeat move branches and validate combat states against `Sts2Oracle` at A0 and A10.

## Tasks

- Add the Fake Merchant monster's health, ascension-scaled attack values, exact move graph, and effects.
- Add encounter dispatch for the fixed `merchant` slot and use the exported encounter metadata for its 300-gold reward.
- Regenerate model dispatch and build Env/CLI targets.
- Run L3 combat comparisons at A0 and A10; record the exact combat/state counts.
- Update encounter coverage in the aggregate report, roadmap, and README.

## Progress (2026-09-26)

- ✅ Added the encounter and Fake Merchant move graph, including the three-turn cooldown on Enrage.
- ✅ A0 L3: 4/4 fights and 98/98 states; A10 L3: 2/2 fights and 47/47 states.
- ✅ `scripts/check.sh --quick`: 8/8 unit tests and all three smoke runs passed.
- ⏳ Full-repository L2/L3/L4 gate remains outstanding.

## Acceptance

- Env can initialize `FAKE_MERCHANT_EVENT_ENCOUNTER` with the source-defined Fake Merchant.
- L3 reports zero unported behavior and no divergent decision states for the selected A0/A10 tests.
- The targeted C++ build and `scripts/check.sh --quick` pass.
