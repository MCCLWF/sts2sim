# The Lost and The Forgotten encounter

## Goal

Port `THE_LOST_AND_FORGOTTEN_NORMAL` and its two monsters from game v0.111.0, including their attribute theft and death-time restoration.

## Tasks

- Add The Lost and The Forgotten health, move loops, damage intents, and effects.
- Add the single-player state needed by Possess Strength and Possess Speed to restore stolen stats when their owner dies.
- Add the fixed two-monster encounter generator and regenerate model dispatch.
- Run A0/A10 Oracle L3 combat comparisons, including combat paths where a stolen-stat monster dies.
- Update aggregate coverage, roadmap, and README.

## Acceptance

- Both enemies start with their source-defined possession powers and alternate their fixed moves.
- Oracle L3 decision states match at A0/A10 with no unported behavior.
- Targeted build and quick gate pass.

## Progress (2026-09-26)

- ✅ The Lost, The Forgotten, Possess Strength, and Possess Speed are registered.
- ✅ A0/A10 L3 pass: 6/6 combats and 72/72 states with 10 upgraded Bludgeons plus Akabeko and Tungsten Rod.
- ✅ At A0, the two policy seeds exercise both enemy deaths; L3 verifies the stolen Strength and Dexterity return.
- ✅ `scripts/check.sh --quick`: 8/8 unit tests and all three smoke runs passed.
- ⏳ Full-repository L2/L3/L4 gate remains outstanding.
