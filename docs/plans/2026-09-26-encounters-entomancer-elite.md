# Entomancer Elite encounter

## Goal

Port `ENTOMANCER_ELITE`, the Entomancer, and Personal Hive behavior from game v0.111.0.

## Tasks

- Add the Entomancer's elite health, Bees/Spear/Spit move cycle, and scaling.
- Add Personal Hive stacking and its Dazed generation when the Entomancer takes powered attack damage.
- Register the fixed elite encounter and regenerate dispatch.
- Run A0/A10 Oracle L3 comparisons that exercise Dazed generation.
- Update coverage documentation and run the quick gate.

## Acceptance

- Encounter generation and combat intent cycle match the source.
- Dazed cards are generated in the player's draw pile with oracle-matching order/RNG.
- A0/A10 L3 and `scripts/check.sh --quick` pass.

## Progress (2026-09-26)

- ✅ Entomancer, Personal Hive, Dazed generation, and the elite encounter are registered.
- ✅ A0 L3: 4/4 combats and 85/85 states; A10 L3: 2/2 and 44/44.
- ✅ `scripts/check.sh --quick`: 8/8 unit tests and all three smoke runs passed.
- ⏳ Full-repository L2/L3/L4 gate remains outstanding.
