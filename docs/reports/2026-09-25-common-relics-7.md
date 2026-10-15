# Shared relics batch 7 report

## Scope and source

Ported `ScreamingFlagon`, `StoneCalendar`, `SturdyClamp`, and `DingyRug` from v0.111.0. The implementations use the existing turn-end, block-clear, and card-reward-option hooks.

The shared relic pool contains 118 entries. C++ relic model headers now exist for 102; the 16 without headers are: `BurningSticks`, `Cauldron`, `DollysMirror`, `GnarledHammer`, `LavaLamp`, `MembershipCard`, `Orrery`, `Pocketwatch`, `PunchDagger`, `RippleBasin`, `RoyalStamp`, `StoneCracker`, `Toolbox`, `UnsettlingLamp`, `Vambrace`, and `WingCharm`.

## Verification

| Check | Result |
|---|---|
| `cmake --build build -j2` | Passed |
| `./build/test_relics` | Passed, including empty-hand damage, turn-seven damage, block retention capped at 10, and Dingy Rug pool-condition checks |
| `./scripts/check.sh` build and unit tests | Passed; 8/8 CTest cases |
| Whole-run smoke | Passed; observed 4 boss clears, deaths, and controlled unported stops; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- L4 is a broad integration regression; direct model tests cover these relic hooks because random runs do not guarantee acquisition of each one.
- The 16 relics without headers remain unported shared content. Cross-character cards in Ironclad runs remain unimplemented.
